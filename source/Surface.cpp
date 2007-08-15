#include "Surface.h"

#include "OgreVertexIndexData.h"

namespace Ogre
{
	Surface::Surface(const String& material)
	{
	   mRenderOp.vertexData = new VertexData();
	   mRenderOp.indexData = new IndexData();

	   this->setMaterial(material);
	}

	Surface::~Surface(void)
	{
	   delete mRenderOp.vertexData;
	   delete mRenderOp.indexData;
	}

	void Surface::setGeometry(std::vector<Vertex> verticesToSet, std::vector<Triangle> indicesToSet)
	{
				
		//LogManager::getSingleton().logMessage("In setGeometry()");
	   // Initialization stuff
	   mRenderOp.vertexData->vertexCount = verticesToSet.size();
	   mRenderOp.vertexData->vertexStart = 0;
	   mRenderOp.operationType = RenderOperation::OT_TRIANGLE_LIST; // OT_LINE_LIST, OT_LINE_STRIP
	   mRenderOp.useIndexes = true;
	   mRenderOp.indexData->indexStart = 0;
	   mRenderOp.indexData->indexCount = indicesToSet.size()*3;

	   //LogManager::getSingleton().logMessage("Finished initialisaing stuff");

	   VertexDeclaration *decl = mRenderOp.vertexData->vertexDeclaration;
	   VertexBufferBinding *bind = mRenderOp.vertexData->vertexBufferBinding;

	   //FIXME - this should be moved to constructor?
	   //LogManager::getSingleton().logMessage("Creating Vertex Declaration");
	   decl->removeAllElements();
	   decl->addElement(0, 0, VET_FLOAT3, VES_POSITION);
	   decl->addElement(0, 3 * sizeof(float), VET_FLOAT3, VES_NORMAL);

	   //LogManager::getSingleton().logMessage("Creating Vertex Buffer");
	   HardwareVertexBufferSharedPtr vbuf =
		  HardwareBufferManager::getSingleton().createVertexBuffer(
			 decl->getVertexSize(0),
			 mRenderOp.vertexData->vertexCount,
			 HardwareBuffer::HBU_STATIC_WRITE_ONLY);

	   bind->setBinding(0, vbuf);

	   //LogManager::getSingleton().logMessage("Creating Index Buffer");
	   HardwareIndexBufferSharedPtr ibuf =
		   HardwareBufferManager::getSingleton().createIndexBuffer(
				HardwareIndexBuffer::IT_16BIT, // type of index
				mRenderOp.indexData->indexCount * 3, // number of indexes
				HardwareBuffer::HBU_STATIC_WRITE_ONLY, // usage
				false); // no shadow buffer	

	   mRenderOp.indexData->indexBuffer = ibuf;

	   
	   // Drawing stuff
	   int size = verticesToSet.size();
	   Vector3 vaabMin = verticesToSet[0].position;
	   Vector3 vaabMax = verticesToSet[0].position;

	   //LogManager::getSingleton().logMessage("Setting Vertex Data of size " + StringConverter::toString(size));

	   Real *prPos = static_cast<Real*>(vbuf->lock(HardwareBuffer::HBL_DISCARD));

	   for(int i = 0; i < size; i++)
	   {
		  *prPos++ = verticesToSet[i].position.x;
		  *prPos++ = verticesToSet[i].position.y;
		  *prPos++ = verticesToSet[i].position.z;

		  *prPos++ = verticesToSet[i].normal.x;
		  *prPos++ = verticesToSet[i].normal.y;
		  *prPos++ = verticesToSet[i].normal.z;

		  if(verticesToSet[i].position.x < vaabMin.x)
			 vaabMin.x = verticesToSet[i].position.x;
		  if(verticesToSet[i].position.y < vaabMin.y)
			 vaabMin.y = verticesToSet[i].position.y;
		  if(verticesToSet[i].position.z < vaabMin.z)
			 vaabMin.z = verticesToSet[i].position.z;

		  if(verticesToSet[i].position.x > vaabMax.x)
			 vaabMax.x = verticesToSet[i].position.x;
		  if(verticesToSet[i].position.y > vaabMax.y)
			 vaabMax.y = verticesToSet[i].position.y;
		  if(verticesToSet[i].position.z > vaabMax.z)
			 vaabMax.z = verticesToSet[i].position.z;
	   }

	   vbuf->unlock();

	   mBox.setExtents(vaabMin, vaabMax);

	   unsigned short* pIdx = static_cast<unsigned short*>(ibuf->lock(HardwareBuffer::HBL_DISCARD));
	   for(int i = 0; i < indicesToSet.size(); i++)
	   {
		   *pIdx = indicesToSet[i].v0;
		   pIdx++;
		   *pIdx = indicesToSet[i].v1;
		   pIdx++;
		   *pIdx = indicesToSet[i].v2;
		   pIdx++;
	   }
	   ibuf->unlock();
	}

	Real Surface::getSquaredViewDepth(const Camera *cam) const
	{
	   Vector3 vMin, vMax, vMid, vDist;
	   vMin = mBox.getMinimum();
	   vMax = mBox.getMaximum();
	   vMid = ((vMin - vMax) * 0.5) + vMin;
	   vDist = cam->getDerivedPosition() - vMid;

	   return vDist.squaredLength();
	}

	Real Surface::getBoundingRadius(void) const
	{
		return Math::Sqrt((std::max)(mBox.getMaximum().squaredLength(), mBox.getMinimum().squaredLength()));
	   //return mRadius;
	}
	/*
	void Line3D::getWorldTransforms(Matrix4 *xform) const
	{
	   // return identity matrix to prevent parent transforms
	   *xform = Matrix4::IDENTITY;
	}
	*/
	const Quaternion &Surface::getWorldOrientation(void) const
	{
	   return Quaternion::IDENTITY;
	}

	const Vector3 &Surface::getWorldPosition(void) const
	{
	   return Vector3::ZERO;
	}
}