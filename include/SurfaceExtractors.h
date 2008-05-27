#pragma region License
/******************************************************************************
This file is part of the PolyVox library
Copyright (C) 2006  David Williams

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
******************************************************************************/
#pragma endregion

#ifndef __PolyVox_SurfaceExtractors_H__
#define __PolyVox_SurfaceExtractors_H__

#pragma region Headers
#include "PolyVoxForwardDeclarations.h"
#include "TypeDef.h"

#include "boost/cstdint.hpp"

#include <list>
#pragma endregion

namespace PolyVox
{
	POLYVOX_API std::list<RegionGeometry> getChangedRegionGeometry(VolumeChangeTracker& volume);
	void genMultiFromSingle(IndexedSurfacePatch* singleMaterialPatch, IndexedSurfacePatch* multiMaterialPatch);

	POLYVOX_API void generateRoughMeshDataForRegion(BlockVolume<boost::uint8_t>* volumeData, Region region, IndexedSurfacePatch* singleMaterialPatch, IndexedSurfacePatch* multiMaterialPatch);
	POLYVOX_API Vector3DFloat computeNormal(BlockVolume<boost::uint8_t>* volumeData, const Vector3DFloat& position, NormalGenerationMethod normalGenerationMethod);

	POLYVOX_API void generateSmoothMeshDataForRegion(BlockVolume<boost::uint8_t>* volumeData, Region region, IndexedSurfacePatch* singleMaterialPatch, IndexedSurfacePatch* multiMaterialPatch);
	POLYVOX_API Vector3DFloat computeSmoothNormal(BlockVolume<boost::uint8_t>* volumeData, const Vector3DFloat& position, NormalGenerationMethod normalGenerationMethod);
}

#endif