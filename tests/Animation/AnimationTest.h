#pragma once

#include <frm/core/AppSample3d.h>

typedef frm::AppSample3d AppBase;

class AnimationTest: public AppBase
{
public:
	AnimationTest();
	virtual ~AnimationTest();

	virtual bool init(const frm::ArgList& _args) override;
	virtual void shutdown() override;
	virtual bool update() override;
	virtual void draw() override;

protected:

	frm::PathStr                    m_meshPath      = "";
	frm::PathStr                    m_animPath      = "";
	float                           m_animSpeed     = 1.0f;
	float                           m_animTime      = 0.0f;
	frm::mat4                       m_world         = frm::identity;
	frm::Node*                      m_node          = nullptr;
	frm::Component_BasicRenderable* m_renderable    = nullptr;
	frm::Mesh*                      m_mesh          = nullptr;
	frm::BasicMaterial*             m_material      = nullptr;
	frm::SkeletonAnimation*         m_anim          = nullptr;
	frm::BasicRenderer*             m_basicRenderer = nullptr;

	bool initMesh();
	void shutdownMesh();

	bool initAnim();
	void shutdownAnim(); 

};
