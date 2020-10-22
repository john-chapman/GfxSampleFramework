#include "StreamingQuadtreeTest.h"

#include <frm/core/frm.h>
#include <frm/core/Camera.h>
#include <frm/core/StreamingQuadtree.h>
#include <frm/core/world/World.h>

#include <imgui/imgui.h>
#include <im3d/im3d.h>

using namespace frm;

static StreamingQuadtreeTest s_inst;

StreamingQuadtreeTest::StreamingQuadtreeTest()
	: AppBase("StreamingQuadtree") 
{
}

StreamingQuadtreeTest::~StreamingQuadtreeTest()
{
}

bool StreamingQuadtreeTest::init(const frm::ArgList& _args)
{
	if (!AppBase::init(_args))
	{
		return false;
	}

	m_streamingQuadtree = FRM_NEW(StreamingQuadtree(8));

	return true;
}

void StreamingQuadtreeTest::shutdown()
{
	FRM_DELETE(m_streamingQuadtree);

	AppBase::shutdown();
}

bool StreamingQuadtreeTest::update()
{
	if (!AppBase::update())
	{
		return false;
	}

	Camera* cullCamera = World::GetCullCamera();
		
	static mat4 quadtreeToWorld = TransformationMatrix(vec3(0.0f), RotationQuaternion(vec3(1.0f, 0.0f, 0.0f), Radians(90.0f)), vec3(16.0f, 16.0f, 1.0f));
	Im3d::Gizmo("quadtreeToWorld", (float*)&quadtreeToWorld);
	static mat4 worldToQuadtree = Inverse(quadtreeToWorld);

	vec3 pivotQ = TransformPosition(worldToQuadtree, cullCamera->getPosition());
	pivotQ.z = 0.0f;
	vec3 directionQ = TransformDirection(worldToQuadtree, cullCamera->getViewVector());
	m_streamingQuadtree->setPivot(pivotQ, directionQ);
	m_streamingQuadtree->update();
	m_streamingQuadtree->drawDebug(quadtreeToWorld);

	ImGui::SliderInt("Max load/frame", &m_maxLoadPerFrame, 0, 32);
	ImGui::SliderInt("Max release/frame", &m_maxReleasePerFrame, 0, 32);

	int releaseCount = 0;
	while (releaseCount < m_maxReleasePerFrame) // could be some unload condition based on available resources
	{
		StreamingQuadtree::NodeIndex nodeIndex = m_streamingQuadtree->popReleaseQueue();
		if (nodeIndex == StreamingQuadtree::NodeIndex_Invalid)
		{
			break;
		}
		m_streamingQuadtree->setNodeData(nodeIndex, nullptr);
		++releaseCount;
	}

	int loadCount = 0;
	while (loadCount < m_maxLoadPerFrame)
	{
		StreamingQuadtree::NodeIndex nodeIndex = m_streamingQuadtree->popLoadQueue();
		if (nodeIndex == StreamingQuadtree::NodeIndex_Invalid)
		{
			break;
		}
		m_streamingQuadtree->setNodeData(nodeIndex, (void*)1);
		++loadCount;
	}

	return true;
}

void StreamingQuadtreeTest::draw()
{
 // code here

	AppBase::draw();
}
