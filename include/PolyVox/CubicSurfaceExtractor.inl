/*******************************************************************************
* The MIT License (MIT)
*
* Copyright (c) 2015 David Williams and Matthew Williams
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*******************************************************************************/

#include "Impl/Timer.h"

namespace PolyVox
{
	// This constant defines the maximum number of quads which can share a vertex in a cubic style mesh.
	//
	// We try to avoid duplicate vertices by checking whether a vertex has already been added at a given position.
	// However, it is possible that vertices have the same position but different materials. In this case, the
	// vertices are not true duplicates and both must be added to the mesh. As far as I can tell, it is possible to have
	// at most eight vertices with the same position but different materials. For example, this worst-case scenario
	// happens when we have a 2x2x2 group of voxels, all with different materials and some/all partially transparent.
	// The vertex position at the center of this group is then going to be used by all eight voxels all with different
	// materials.
	const uint32_t MaxVerticesPerPosition = 8;

	////////////////////////////////////////////////////////////////////////////////
	// Data structures
	////////////////////////////////////////////////////////////////////////////////

	enum FaceNames
	{
		PositiveX,
		PositiveY,
		PositiveZ,
		NegativeX,
		NegativeY,
		NegativeZ,
		NoOfFaces
	};

	struct Quad
	{
		Quad(uint32_t v0, uint32_t v1, uint32_t v2, uint32_t v3)
		{
			vertices[0] = v0;
			vertices[1] = v1;
			vertices[2] = v2;
			vertices[3] = v3;
		}

		uint32_t vertices[4];
	};

	template<typename VolumeType>
	struct IndexAndMaterial
	{
		int32_t iIndex;
		typename VolumeType::VoxelType uMaterial;
		uint8_t ambientOcclusion;
	};

	////////////////////////////////////////////////////////////////////////////////
	// Vertex encoding/decoding
	////////////////////////////////////////////////////////////////////////////////

	inline Vector3DFloat decodePosition(const Vector3DUint8& encodedPosition)
	{
		Vector3DFloat result(encodedPosition.getX(), encodedPosition.getY(), encodedPosition.getZ());
		result -= 0.5f; // Apply the required offset
		return result;
	}

	template<typename DataType>
	Vertex<DataType> decodeVertex(const CubicVertex<DataType>& cubicVertex)
	{
		Vertex<DataType> result;
		result.position = decodePosition(cubicVertex.encodedPosition);
		result.normal.setElements(0.0f, 0.0f, 0.0f); // Currently not calculated
		result.data = cubicVertex.data; // Data is not encoded
		result.ambientOcclusion = cubicVertex.ambientOcclusion;
		return result;
	}

	////////////////////////////////////////////////////////////////////////////////
	// Surface extraction
	////////////////////////////////////////////////////////////////////////////////

	template<typename VertexType>
	bool isSameVertex(const VertexType& v1, const VertexType& v2)
	{
		return v1.data == v2.data && v1.ambientOcclusion == v2.ambientOcclusion;
	}

	template<typename MeshType>
	bool mergeQuads(Quad& q1, Quad& q2, MeshType* m_meshCurrent)
	{
		const typename MeshType::VertexType& v11 = m_meshCurrent->getVertex(q1.vertices[0]);
		const typename MeshType::VertexType& v21 = m_meshCurrent->getVertex(q2.vertices[0]);
		const typename MeshType::VertexType& v12 = m_meshCurrent->getVertex(q1.vertices[1]);
		const typename MeshType::VertexType& v22 = m_meshCurrent->getVertex(q2.vertices[1]);
		const typename MeshType::VertexType& v13 = m_meshCurrent->getVertex(q1.vertices[2]);
		const typename MeshType::VertexType& v23 = m_meshCurrent->getVertex(q2.vertices[2]);
		const typename MeshType::VertexType& v14 = m_meshCurrent->getVertex(q1.vertices[3]);
		const typename MeshType::VertexType& v24 = m_meshCurrent->getVertex(q2.vertices[3]);
		if (isSameVertex(v11, v21) && isSameVertex(v12, v22) && isSameVertex(v13, v23) && isSameVertex(v14, v24)) {
			//Now check whether quad 2 is adjacent to quad one by comparing vertices.
			//Adjacent quads must share two vertices, and the second quad could be to the
			//top, bottom, left, of right of the first one. This gives four combinations to test.
			if ((q1.vertices[0] == q2.vertices[1]) && ((q1.vertices[3] == q2.vertices[2])))
			{
				q1.vertices[0] = q2.vertices[0];
				q1.vertices[3] = q2.vertices[3];
				return true;
			}
			else if ((q1.vertices[3] == q2.vertices[0]) && ((q1.vertices[2] == q2.vertices[1])))
			{
				q1.vertices[3] = q2.vertices[3];
				q1.vertices[2] = q2.vertices[2];
				return true;
			}
			else if ((q1.vertices[1] == q2.vertices[0]) && ((q1.vertices[2] == q2.vertices[3])))
			{
				q1.vertices[1] = q2.vertices[1];
				q1.vertices[2] = q2.vertices[2];
				return true;
			}
			else if ((q1.vertices[0] == q2.vertices[3]) && ((q1.vertices[1] == q2.vertices[2])))
			{
				q1.vertices[0] = q2.vertices[0];
				q1.vertices[1] = q2.vertices[1];
				return true;
			}
		}

		//Quads cannot be merged.
		return false;
	}

	template<typename MeshType>
	bool performQuadMerging(std::list<Quad>& quads, MeshType* m_meshCurrent)
	{
		bool bDidMerge = false;
		for (typename std::list<Quad>::iterator outerIter = quads.begin(); outerIter != quads.end(); outerIter++)
		{
			typename std::list<Quad>::iterator innerIter = outerIter;
			innerIter++;
			while (innerIter != quads.end())
			{
				Quad& q1 = *outerIter;
				Quad& q2 = *innerIter;

				bool result = mergeQuads(q1, q2, m_meshCurrent);

				if (result)
				{
					bDidMerge = true;
					innerIter = quads.erase(innerIter);
				}
				else
				{
					innerIter++;
				}
			}
		}

		return bDidMerge;
	}

	// https://0fps.net/2013/07/03/ambient-occlusion-for-minecraft-like-worlds/
	// 0 is the darkest
	// 3 is no occlusion at all
	inline uint8_t vertexAmbientOcclusion(bool side1, bool side2, bool corner)
	{
		if (side1 && side2) {
			return 0;
		}
		return 3 - (side1 + side2 + corner);
	}

	template<typename VolumeType, typename MeshType, typename ContributeToAO>
	int32_t addVertex(uint32_t uX, uint32_t uY, uint32_t uZ, typename VolumeType::VoxelType uMaterialIn, Array<3, IndexAndMaterial<VolumeType> >& existingVertices, MeshType* m_meshCurrent, const typename VolumeType::VoxelType& face1, const typename VolumeType::VoxelType& face2, const typename VolumeType::VoxelType& corner, ContributeToAO contributeToAO)
	{
		for (uint32_t ct = 0; ct < MaxVerticesPerPosition; ct++)
		{
			IndexAndMaterial<VolumeType>& rEntry = existingVertices(uX, uY, ct);

			const uint8_t ambientOcclusion = vertexAmbientOcclusion(contributeToAO(face1), contributeToAO(face2), contributeToAO(corner));

			if (rEntry.iIndex == -1)
			{
				//No vertices matched and we've now hit an empty space. Fill it by creating a vertex. The 0.5f offset is because vertices set between voxels in order to build cubes around them.
				CubicVertex<typename VolumeType::VoxelType> cubicVertex;
				cubicVertex.encodedPosition.setElements(static_cast<uint8_t>(uX), static_cast<uint8_t>(uY), static_cast<uint8_t>(uZ));
				cubicVertex.data = uMaterialIn;
				cubicVertex.ambientOcclusion = ambientOcclusion;
				rEntry.iIndex = m_meshCurrent->addVertex(cubicVertex);
				rEntry.uMaterial = uMaterialIn;
				rEntry.ambientOcclusion = ambientOcclusion;

				return rEntry.iIndex;
			}

			//If we have an existing vertex and the material matches then we can return it.
			if (rEntry.uMaterial == uMaterialIn && rEntry.ambientOcclusion == ambientOcclusion)
			{
				return rEntry.iIndex;
			}
		}

		// If we exit the loop here then apparently all the slots were full but none of them matched.
		// This shouldn't ever happen, so if it does it is probably a bug in PolyVox. Please report it to us!
		POLYVOX_THROW(std::runtime_error, "All slots full but no matches during cubic surface extraction. This is probably a bug in PolyVox");
		return -1; //Should never happen.
	}

	/// The CubicSurfaceExtractor creates a mesh in which each voxel appears to be rendered as a cube
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Introduction
	/// ------------
	/// Games such as Minecraft and Voxatron have a unique graphical style in which each voxel in the world appears to be rendered as a single cube. Actually rendering a cube for each voxel would be very expensive, but in practice the only faces which need to be drawn are those which lie on the boundary between solid and empty voxels. The CubicSurfaceExtractor can be used to create such a mesh from PolyVox volume data. As an example, images from Minecraft and Voxatron are shown below:
	///
	/// \image html MinecraftAndVoxatron.jpg
	///
	/// Before we get into the specifics of the CubicSurfaceExtractor, it is useful to understand the principles which apply to *all* PolyVox surface extractors and which are described in the Surface Extraction document (ADD LINK). From here on, it is assumed that you are familier with PolyVox regions and how they are used to limit surface extraction to a particular part of the volume. The principles of allowing dynamic terrain are also common to all surface extractors and are described here (ADD LINK).
	///
	/// Basic Operation
	/// ---------------
	/// At its core, the CubicSurfaceExtractor works by by looking at pairs of adjacent voxels and determining whether a quad should be placed between then. The most simple situation to imagine is a binary volume where every voxel is either solid or empty. In this case a quad should be generated whenever a solid voxel is next to an empty voxel as this represents part of the surface of the solid object. There is no need to generate a quad between two solid voxels (this quad would never be seen as it is inside the object) and there is no need to generate a quad between two empty voxels (there is no object here). PolyVox allows the principle to be extended far beyond such simple binary volumes but they provide a useful starting point for understanding how the algorithm works.
	///
	/// As an example, lets consider the part of a volume shown below. We are going to explain the principles in only two dimensions as this makes it much simpler to illustrate, so you will need to mentally extend the process into the third dimension. Hopefully you will find this intuitive. The diagram below shows a small part of a larger volume (as indicated by the voxel coordinates on the axes) which contains only solid and empty voxels represented by solid and hollow circles respectively. The region on which we are running the surface extractor is marked in pink, and for the purpose of this example it corresponds to the whole of the diagram.
	///
	/// \image html CubicSurfaceExtractor1.png
	///
	/// The output of the surface extractor is the mesh marked in red. As you can see, this forms a closed object which corrsponds to the shape of the underlying voxel data. We won't describe the rendering of such meshes here - for details of this please see (SOME LINK HERE).
	///
	/// Working with Regions
	/// --------------------
	/// So far the behaviour is easy to understand, but let's look at what happens when the extraction is limited to a particular region of the volume. The figure below shows the same data set as the previous figure, but the extraction region (still marked in pink) has been limited to 13 to 16 in x and 47 to 51 in y:
	///
	/// \image html CubicSurfaceExtractor2.png
	/// 
	/// As you can see, the extractor continues to generate a number of quads as indicated by the solid red lines. However, you can also see that the shape is no longer closed. This is because the solid voxels actually extend outside the region which is being processed, and so the extractor does not encounter a boundary between solid and empty voxels. Although this may initially appear problematic, the hole in the mesh does not actually matter because it will be hidden by the mesh corresponding to the region adjacent to it (see next diagram).
	///
	/// More interestingly, the diagram also contains a couple of dotted red lines lying on the bottom and right hand side of the extracted region. These are present to illustrate a common point of confusion, which is that *no quads are generated at this position even though it is a boundary between solid and empty voxels*. This is indeed somewhat counter intuitive but there is a rational reasaoning behind it.
	/// If you consider the dashed line on the righthand side of the extracted region, then it is clear that this lies on a boundary between solid and empty voxels and so we do need to create quads here. But what is not so clear is whether these quads should be assigned to the mesh which corresponds to the region in pink, or whether they should be assigned to the region to the right of it which is marked in blue in the diagram below:
	///
	/// \image html CubicSurfaceExtractor3.png
	///
	/// We could choose to add the quads to *both* regions, but this can cause confusion when one of the region is modified (causing the face to disappear or a new one to be created) as *both* regions need to have their mesh regenerated to correctly represent the new state of the volume data. Such pairs of coplanar quads can also cause problems with physics engines, and may prevent transparent voxels from rendering correctly. Therefore we choose to instead only add the quad to one of the the regions and we always choose the one with the greater coordinate value in the direction in which they differ. In the above example the regions differ by the 'x' component of their position, and so the quad is added to the region with the greater 'x' value (the one marked in blue).
	///
	/// **Note:** *This behaviour has changed recently (September 2012). Earlier versions of PolyVox tried to be smart about this problem by looking beyond the region which was being processed, but this complicated the code and didn't work very well. Ultimatly we decided to simply stick with the convention outlined above.*
	///
	/// One of the practical implications of this is that when you modify a voxel *you may have to re-extract the mesh for regions other than region which actually contains the voxel you modified.* This happens when the voxel lies on the upper x,y or z face of a region. Assuming that you have some management code which can mark a region as needing re-extraction when a voxel changes, you should probably extend this to mark the regions of neighbouring voxels as invalid (this will have no effect when the voxel is well within a region, but will mark the neighbouring region as needing an update if the voxel lies on a region face).
	///
	/// Another scenario which sometimes results in confusion is when you wish to extract a region which corresponds to the whole volume, partcularly when solid voxels extend right to the edge of the volume.  
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	template<typename VolumeType, typename IsQuadNeeded, typename ContributeToAO>
	Mesh<CubicVertex<typename VolumeType::VoxelType> > extractCubicMesh(VolumeType* volData, Region region, IsQuadNeeded isQuadNeeded, ContributeToAO contributeToAO, bool bMergeQuads)
	{
		Mesh< CubicVertex<typename VolumeType::VoxelType> > result;
		extractCubicMeshCustom(volData, region, &result, isQuadNeeded, contributeToAO, bMergeQuads);
		return result;
	}

	/// This version of the function performs the extraction into a user-provided mesh rather than allocating a mesh automatically.
	/// There are a few reasons why this might be useful to more advanced users:
	///
	///   1. It leaves the user in control of memory allocation and would allow them to implement e.g. a mesh pooling system.
	///   2. The user-provided mesh could have a different index type (e.g. 16-bit indices) to reduce memory usage.
	///   3. The user could provide a custom mesh class, e.g a thin wrapper around an openGL VBO to allow direct writing into this structure.
	///
	/// We don't provide a default MeshType here. If the user doesn't want to provide a MeshType then it probably makes
	/// more sense to use the other variant of this function where the mesh is a return value rather than a parameter.
	///
	/// Note: This function is called 'extractCubicMeshCustom' rather than 'extractCubicMesh' to avoid ambiguity when only three parameters
	/// are provided (would the third parameter be a controller or a mesh?). It seems this can be fixed by using enable_if/static_assert to emulate concepts,
	/// but this is relatively complex and I haven't done it yet. Could always add it later as another overload.
	template<typename VolumeType, typename MeshType, typename IsQuadNeeded, typename ContributeToAO>
	void extractCubicMeshCustom(VolumeType* volData, Region region, MeshType* result, IsQuadNeeded isQuadNeeded, ContributeToAO contributeToAO, bool bMergeQuads)
	{
		// This extractor has a limit as to how large the extracted region can be, because the vertex positions are encoded with a single byte per component.
		int32_t maxReionDimensionInVoxels = 255;
		POLYVOX_THROW_IF(region.getWidthInVoxels() > maxReionDimensionInVoxels, std::invalid_argument, "Requested extraction region exceeds maximum dimensions");
		POLYVOX_THROW_IF(region.getHeightInVoxels() > maxReionDimensionInVoxels, std::invalid_argument, "Requested extraction region exceeds maximum dimensions");
		POLYVOX_THROW_IF(region.getDepthInVoxels() > maxReionDimensionInVoxels, std::invalid_argument, "Requested extraction region exceeds maximum dimensions");

		Timer timer;
		result->clear();

		//Used to avoid creating duplicate vertices.
		Array<3, IndexAndMaterial<VolumeType> > m_previousSliceVertices(region.getUpperX() - region.getLowerX() + 2, region.getUpperY() - region.getLowerY() + 2, MaxVerticesPerPosition);
		Array<3, IndexAndMaterial<VolumeType> > m_currentSliceVertices(region.getUpperX() - region.getLowerX() + 2, region.getUpperY() - region.getLowerY() + 2, MaxVerticesPerPosition);

		//During extraction we create a number of different lists of quads. All the 
		//quads in a given list are in the same plane and facing in the same direction.
		std::vector< std::list<Quad> > m_vecQuads[NoOfFaces];


		memset(m_previousSliceVertices.getRawData(), 0xff, m_previousSliceVertices.getNoOfElements() * sizeof(IndexAndMaterial<VolumeType>));
		memset(m_currentSliceVertices.getRawData(), 0xff, m_currentSliceVertices.getNoOfElements() * sizeof(IndexAndMaterial<VolumeType>));

		m_vecQuads[NegativeX].resize(region.getUpperX() - region.getLowerX() + 2);
		m_vecQuads[PositiveX].resize(region.getUpperX() - region.getLowerX() + 2);

		m_vecQuads[NegativeY].resize(region.getUpperY() - region.getLowerY() + 2);
		m_vecQuads[PositiveY].resize(region.getUpperY() - region.getLowerY() + 2);

		m_vecQuads[NegativeZ].resize(region.getUpperZ() - region.getLowerZ() + 2);
		m_vecQuads[PositiveZ].resize(region.getUpperZ() - region.getLowerZ() + 2);

		typename VolumeType::Sampler volumeSampler(volData);

		for (int32_t z = region.getLowerZ(); z <= region.getUpperZ(); z++)
		{
			uint32_t regZ = z - region.getLowerZ();

			for (int32_t y = region.getLowerY(); y <= region.getUpperY(); y++)
			{
				uint32_t regY = y - region.getLowerY();

				volumeSampler.setPosition(region.getLowerX(), y, z);

				for (int32_t x = region.getLowerX(); x <= region.getUpperX(); x++)
				{
					uint32_t regX = x - region.getLowerX();

					/**
					 *
					 *
					 *                  [D]
					 *            8 ____________ 7
					 *             /|          /|
					 *            / |         / |              ABOVE [D] |
					 *           /  |    [F] /  |              BELOW [C]
					 *        5 /___|_______/ 6 |  [B]       y           BEHIND  [F]
					 *    [A]   |   |_______|___|              |      z  BEFORE [E] /
					 *          | 4 /       |   / 3            |   /
					 *          |  / [E]    |  /               |  /   . center
					 *          | /         | /                | /
					 *          |/__________|/                 |/________   LEFT  RIGHT
					 *        1               2                          x   [A] - [B]
					 *               [C]
					 */

					typename VolumeType::VoxelType material; //Filled in by callback

					typename VolumeType::VoxelType voxelCurrent                = volumeSampler.getVoxel();

					const typename VolumeType::VoxelType voxelLeft             = volumeSampler.peekVoxel1nx0py0pz();
					const typename VolumeType::VoxelType voxelBefore           = volumeSampler.peekVoxel0px0py1nz();
					const typename VolumeType::VoxelType voxelLeftBefore       = volumeSampler.peekVoxel1nx0py1nz();
					const typename VolumeType::VoxelType voxelRightBefore      = volumeSampler.peekVoxel1px0py1nz();
					const typename VolumeType::VoxelType voxelLeftBehind       = volumeSampler.peekVoxel1nx0py1pz();

					const typename VolumeType::VoxelType voxelAboveLeft        = volumeSampler.peekVoxel1nx1py0pz();
					const typename VolumeType::VoxelType voxelAboveBefore      = volumeSampler.peekVoxel0px1py1nz();
					const typename VolumeType::VoxelType voxelAboveLeftBefore  = volumeSampler.peekVoxel1nx1py1nz();
					const typename VolumeType::VoxelType voxelAboveRightBefore = volumeSampler.peekVoxel1px1py1nz();
					const typename VolumeType::VoxelType voxelAboveLeftBehind  = volumeSampler.peekVoxel1nx1py1pz();

					const typename VolumeType::VoxelType voxelBelow            = volumeSampler.peekVoxel0px1ny0pz();
					const typename VolumeType::VoxelType voxelBelowLeft        = volumeSampler.peekVoxel1nx1ny0pz();
					const typename VolumeType::VoxelType voxelBelowRight       = volumeSampler.peekVoxel1px1ny0pz();
					const typename VolumeType::VoxelType voxelBelowBefore      = volumeSampler.peekVoxel0px1ny1nz();
					const typename VolumeType::VoxelType voxelBelowBehind      = volumeSampler.peekVoxel0px1ny1pz();
					const typename VolumeType::VoxelType voxelBelowLeftBefore  = volumeSampler.peekVoxel1nx1ny1nz();
					const typename VolumeType::VoxelType voxelBelowRightBefore = volumeSampler.peekVoxel1px1ny1nz();
					const typename VolumeType::VoxelType voxelBelowLeftBehind  = volumeSampler.peekVoxel1nx1ny1pz();
					const typename VolumeType::VoxelType voxelBelowRightBehind = volumeSampler.peekVoxel1px1ny1pz();

					// X [A] LEFT
					if (isQuadNeeded(voxelCurrent, voxelLeft, material))
					{
						const uint32_t v_0_1 = addVertex(regX, regY,     regZ,     material, m_previousSliceVertices, result,
							voxelLeftBefore, voxelBelowLeft, voxelBelowLeftBefore, contributeToAO);
						const uint32_t v_1_4 = addVertex(regX, regY,     regZ + 1, material, m_currentSliceVertices,  result,
							voxelBelowLeft, voxelLeftBehind, voxelBelowLeftBehind, contributeToAO);
						const uint32_t v_2_8 = addVertex(regX, regY + 1, regZ + 1, material, m_currentSliceVertices,  result,
							voxelLeftBehind, voxelAboveLeft, voxelAboveLeftBehind, contributeToAO);
						const uint32_t v_3_5 = addVertex(regX, regY + 1, regZ,     material, m_previousSliceVertices, result,
							voxelAboveLeft, voxelLeftBefore, voxelAboveLeftBefore, contributeToAO);
						m_vecQuads[NegativeX][regX].push_back(Quad(v_0_1, v_1_4, v_2_8, v_3_5));
					}

					// X [B] RIGHT
					if (isQuadNeeded(voxelLeft, voxelCurrent, material)) {
						volumeSampler.moveNegativeX();

						const typename VolumeType::VoxelType _voxelRightBefore      = volumeSampler.peekVoxel1px0py1nz();
						const typename VolumeType::VoxelType _voxelRightBehind      = volumeSampler.peekVoxel1px0py1pz();

						const typename VolumeType::VoxelType _voxelAboveRight       = volumeSampler.peekVoxel1px1py0pz();
						const typename VolumeType::VoxelType _voxelAboveRightBefore = volumeSampler.peekVoxel1px1py1nz();
						const typename VolumeType::VoxelType _voxelAboveRightBehind = volumeSampler.peekVoxel1px1py1pz();

						const typename VolumeType::VoxelType _voxelBelowRight       = volumeSampler.peekVoxel1px1ny0pz();
						const typename VolumeType::VoxelType _voxelBelowRightBefore = volumeSampler.peekVoxel1px1ny1nz();
						const typename VolumeType::VoxelType _voxelBelowRightBehind = volumeSampler.peekVoxel1px1ny1pz();

						const uint32_t v_0_2 = addVertex(regX, regY,     regZ,     material, m_previousSliceVertices, result,
								_voxelBelowRight, _voxelRightBefore, _voxelBelowRightBefore, contributeToAO);
						const uint32_t v_1_3 = addVertex(regX, regY,     regZ + 1, material, m_currentSliceVertices,  result,
								_voxelBelowRight, _voxelRightBehind, _voxelBelowRightBehind, contributeToAO);
						const uint32_t v_2_7 = addVertex(regX, regY + 1, regZ + 1, material, m_currentSliceVertices,  result,
								_voxelAboveRight, _voxelRightBehind, _voxelAboveRightBehind, contributeToAO);
						const uint32_t v_3_6 = addVertex(regX, regY + 1, regZ,     material, m_previousSliceVertices, result,
								_voxelAboveRight, _voxelRightBefore, _voxelAboveRightBefore, contributeToAO);
						m_vecQuads[PositiveX][regX].push_back(Quad(v_0_2, v_3_6, v_2_7, v_1_3));

						volumeSampler.movePositiveX();
					}

					// Y [C] BELOW
					if (isQuadNeeded(voxelCurrent, voxelBelow, material)) {
						const uint32_t v_0_1 = addVertex(regX,     regY, regZ,     material, m_previousSliceVertices, result,
								voxelBelowBefore, voxelBelowLeft, voxelBelowLeftBefore, contributeToAO);
						const uint32_t v_1_2 = addVertex(regX + 1, regY, regZ,     material, m_previousSliceVertices, result,
								voxelBelowRight, voxelBelowBefore, voxelBelowRightBefore, contributeToAO);
						const uint32_t v_2_3 = addVertex(regX + 1, regY, regZ + 1, material, m_currentSliceVertices,  result,
								voxelBelowBehind, voxelBelowRight, voxelBelowRightBehind, contributeToAO);
						const uint32_t v_3_4 = addVertex(regX,     regY, regZ + 1, material, m_currentSliceVertices,  result,
								voxelBelowLeft, voxelBelowBehind, voxelBelowLeftBehind, contributeToAO);
						m_vecQuads[NegativeY][regY].push_back(Quad(v_0_1, v_1_2, v_2_3, v_3_4));
					}

					// Y [D] ABOVE
					if (isQuadNeeded(voxelBelow, voxelCurrent, material)) {
						volumeSampler.moveNegativeY();

						const typename VolumeType::VoxelType _voxelAboveLeft        = volumeSampler.peekVoxel1nx1py0pz();
						const typename VolumeType::VoxelType _voxelAboveRight       = volumeSampler.peekVoxel1px1py0pz();
						const typename VolumeType::VoxelType _voxelAboveBefore      = volumeSampler.peekVoxel0px1py1nz();
						const typename VolumeType::VoxelType _voxelAboveBehind      = volumeSampler.peekVoxel0px1py1pz();
						const typename VolumeType::VoxelType _voxelAboveLeftBefore  = volumeSampler.peekVoxel1nx1py1nz();
						const typename VolumeType::VoxelType _voxelAboveRightBefore = volumeSampler.peekVoxel1px1py1nz();
						const typename VolumeType::VoxelType _voxelAboveLeftBehind  = volumeSampler.peekVoxel1nx1py1pz();
						const typename VolumeType::VoxelType _voxelAboveRightBehind = volumeSampler.peekVoxel1px1py1pz();

						const uint32_t v_0_5 = addVertex(regX,     regY, regZ,     material, m_previousSliceVertices, result,
								_voxelAboveBefore, _voxelAboveLeft, _voxelAboveLeftBefore, contributeToAO);
						const uint32_t v_1_6 = addVertex(regX + 1, regY, regZ,     material, m_previousSliceVertices, result,
								_voxelAboveRight, _voxelAboveBefore, _voxelAboveRightBefore, contributeToAO);
						const uint32_t v_2_7 = addVertex(regX + 1, regY, regZ + 1, material, m_currentSliceVertices,  result,
								_voxelAboveBehind, _voxelAboveRight, _voxelAboveRightBehind, contributeToAO);
						const uint32_t v_3_8 = addVertex(regX,     regY, regZ + 1, material, m_currentSliceVertices,  result,
								_voxelAboveLeft, _voxelAboveBehind, _voxelAboveLeftBehind, contributeToAO);
						m_vecQuads[PositiveY][regY].push_back(Quad(v_0_5, v_3_8, v_2_7, v_1_6));

						volumeSampler.movePositiveY();
					}

					// Z [E] BEFORE
					if (isQuadNeeded(voxelCurrent, voxelBefore, material)) {
						const uint32_t v_0_1 = addVertex(regX,     regY,     regZ, material, m_previousSliceVertices, result,
								voxelBelowBefore, voxelLeftBefore, voxelBelowLeftBefore, contributeToAO);
						const uint32_t v_1_5 = addVertex(regX,     regY + 1, regZ, material, m_previousSliceVertices, result,
								voxelAboveBefore, voxelLeftBefore, voxelAboveLeftBefore, contributeToAO);
						const uint32_t v_2_6 = addVertex(regX + 1, regY + 1, regZ, material, m_previousSliceVertices, result,
								voxelAboveBefore, voxelRightBefore, voxelAboveRightBefore, contributeToAO);
						const uint32_t v_3_2 = addVertex(regX + 1, regY,     regZ, material, m_previousSliceVertices, result,
								voxelBelowBefore, voxelRightBefore, voxelBelowRightBefore, contributeToAO);
						m_vecQuads[NegativeZ][regZ].push_back(Quad(v_0_1, v_1_5, v_2_6, v_3_2));
					}

					// Z [F] BEHIND
					if (isQuadNeeded(voxelBefore, voxelCurrent, material)) {
						volumeSampler.moveNegativeZ();

						const typename VolumeType::VoxelType _voxelLeftBehind       = volumeSampler.peekVoxel1nx0py1pz();
						const typename VolumeType::VoxelType _voxelRightBehind      = volumeSampler.peekVoxel1px0py1pz();

						const typename VolumeType::VoxelType _voxelAboveBehind      = volumeSampler.peekVoxel0px1py1pz();
						const typename VolumeType::VoxelType _voxelAboveLeftBehind  = volumeSampler.peekVoxel1nx1py1pz();
						const typename VolumeType::VoxelType _voxelAboveRightBehind = volumeSampler.peekVoxel1px1py1pz();

						const typename VolumeType::VoxelType _voxelBelowBehind      = volumeSampler.peekVoxel0px1ny1pz();
						const typename VolumeType::VoxelType _voxelBelowLeftBehind  = volumeSampler.peekVoxel1nx1ny1pz();
						const typename VolumeType::VoxelType _voxelBelowRightBehind = volumeSampler.peekVoxel1px1ny1pz();

						const uint32_t v_0_4 = addVertex(regX,     regY,     regZ, material, m_previousSliceVertices, result,
								_voxelBelowBehind, _voxelLeftBehind, _voxelBelowLeftBehind, contributeToAO);
						const uint32_t v_1_8 = addVertex(regX,     regY + 1, regZ, material, m_previousSliceVertices, result,
								_voxelAboveBehind, _voxelLeftBehind, _voxelAboveLeftBehind, contributeToAO);
						const uint32_t v_2_7 = addVertex(regX + 1, regY + 1, regZ, material, m_previousSliceVertices, result,
								_voxelAboveBehind, _voxelRightBehind, _voxelAboveRightBehind, contributeToAO);
						const uint32_t v_3_3 = addVertex(regX + 1, regY,     regZ, material, m_previousSliceVertices, result,
								_voxelBelowBehind, _voxelRightBehind, _voxelBelowRightBehind, contributeToAO);
						m_vecQuads[PositiveZ][regZ].push_back(Quad(v_0_4, v_3_3, v_2_7, v_1_8));

						volumeSampler.movePositiveZ();
					}

					volumeSampler.movePositiveX();
				}
			}

			m_previousSliceVertices.swap(m_currentSliceVertices);
			memset(m_currentSliceVertices.getRawData(), 0xff, m_currentSliceVertices.getNoOfElements() * sizeof(IndexAndMaterial<VolumeType>));
		}

		for (uint32_t uFace = 0; uFace < NoOfFaces; uFace++)
		{
			std::vector< std::list<Quad> >& vecListQuads = m_vecQuads[uFace];

			for (uint32_t slice = 0; slice < vecListQuads.size(); slice++)
			{
				std::list<Quad>& listQuads = vecListQuads[slice];

				if (bMergeQuads)
				{
					//Repeatedly call this function until it returns
					//false to indicate nothing more can be done.
					while (performQuadMerging(listQuads, result)){}
				}

				typename std::list<Quad>::iterator iterEnd = listQuads.end();
				for (typename std::list<Quad>::iterator quadIter = listQuads.begin(); quadIter != iterEnd; quadIter++)
				{
					const Quad& quad = *quadIter;
					const CubicVertex<typename VolumeType::VoxelType>& v00 = result->getVertex(quad.vertices[3]);
					const CubicVertex<typename VolumeType::VoxelType>& v01 = result->getVertex(quad.vertices[0]);
					const CubicVertex<typename VolumeType::VoxelType>& v10 = result->getVertex(quad.vertices[2]);
					const CubicVertex<typename VolumeType::VoxelType>& v11 = result->getVertex(quad.vertices[1]);

					if (v00.ambientOcclusion + v11.ambientOcclusion > v01.ambientOcclusion + v10.ambientOcclusion) {
						result->addTriangle(quad.vertices[1], quad.vertices[2], quad.vertices[3]);
						result->addTriangle(quad.vertices[1], quad.vertices[3], quad.vertices[0]);
					} else {
						result->addTriangle(quad.vertices[0], quad.vertices[1], quad.vertices[2]);
						result->addTriangle(quad.vertices[0], quad.vertices[2], quad.vertices[3]);
					}
					result->addTriangle(quad.vertices[0], quad.vertices[2], quad.vertices[3]);
				}
			}
		}

		result->setOffset(region.getLowerCorner());
		result->removeUnusedVertices();

		POLYVOX_LOG_TRACE("Cubic surface extraction took ", timer.elapsedTimeInMilliSeconds(),
			"ms (Region size = ", m_regSizeInVoxels.getWidthInVoxels(), "x", m_regSizeInVoxels.getHeightInVoxels(),
			"x", m_regSizeInVoxels.getDepthInVoxels(), ")");
	}
}
