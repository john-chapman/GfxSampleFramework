#pragma once

#include <frm/core/frm.h>
#include <frm/core/AppSample3d.h>
#include <frm/vr/VRContext.h>
#include <frm/vr/VRController.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// AppSampleVR
// \todo
// - Default editor/debug mode which deriving applications can enable. Has
//   all debug drawing, controller model rendering, locomotion, etc. Manage some
//   behaviour (world raycasts?) via a callback.
////////////////////////////////////////////////////////////////////////////////
class AppSampleVR: public AppSample3d
{
public:

	virtual bool init(const ArgList& _args) override;
	virtual void shutdown() override;
	virtual bool update() override;
	virtual void draw() override;

	virtual Ray  getCursorRayW(const Camera* _camera) const override;
	virtual Ray  getCursorRayV(const Camera* _camera) const override;

protected:

	BasicRenderer*         m_renderer          = nullptr;
	VRContext*             m_vrContext         = nullptr;
	VRController           m_vrController;
	bool                   m_vrActive          = false;
	bool                   m_drawDebug         = false;
	VRContext::Hand        m_dominantHand      = VRContext::Hand_Right;
	VRContext::Eye         m_currentEye        = VRContext::Eye_Left;
	Node*                  m_handNodes[2]      = { nullptr };
	#if FRM_MODULE_PHYSICS
		PhysicsConstraint* m_handNodeJoints[2] = { nullptr };
	#endif
	

	             AppSampleVR(const char* _title);
	virtual      ~AppSampleVR();

	void         debugDrawHand(VRContext::Hand _hand);
	void         debugDrawHead();

	virtual bool initHands();
	virtual void updateHands();
};

} // namespace frm