#pragma once

#include <frm/core/frm.h>
#include <frm/core/math.h>
#include <frm/core/Camera.h>
#include <frm/core/Viewport.h>
#include <frm/vr/VRInput.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// VRContext
//
// \todo
// - Allow the context to be created in a null state in case the HMD isn't
//   connected.
// - Exit crash, see VRContext::Impl::shutdown()
// - Handle tracking issues (check tracking status when polling HMD).
////////////////////////////////////////////////////////////////////////////////
class VRContext
{
public:

	enum Eye_
	{
		Eye_Left,
		Eye_Right,

		Eye_Count
	};
	typedef int Eye;

	enum Hand_
	{
		Hand_Left,
		Hand_Right,

		Hand_Count
	};
	typedef int Hand;

	enum Layer_
	{
		Layer_Main, // Main scene view.
		Layer_Text, // High quality view for text/UI rendering.

		Layer_Count
	};
	typedef int Layer;

	struct PoseData
	{
		mat4 pose                = identity;
		vec3 linearVelocity      = vec3(0.f); // ms^-1
		vec3 linearAcceleration  = vec3(0.f); // ms^-2
		vec3 angularVelocity     = vec3(0.f); // Euler, rs^-1
		vec3 angularAcceleration = vec3(0.f); // Euler, rs^-2

		vec3 getPosition() const      { return pose[3].xyz();  }
		vec3 getForwardVector() const { return -pose[2].xyz(); }
		vec3 getUpVector() const      { return pose[1].xyz();  }
	};

	struct TrackedData
	{
		PoseData headPose;
		PoseData handPoses[Hand_Count];
		mat4     eyePoses[Eye_Count];

		vec3     headOffset;
	};


	// Create/destroy a VR context. After successful init, the returned context is current.
	static VRContext*  Create();
	static void        Destroy(VRContext*& _ctx_);

	// Get/set the current VR context.
	static VRContext*  GetCurrent();
	static bool        MakeCurrent(VRContext* _ctx);

	// Update tracked state and controller inputs. Return false if the application should quit.
	bool               update(float _dt, const vec3& _userPosition = vec3(0.f), const quat& _userOrientation = quat(0.f, 0.f, 0.f, 1.f));

	// Call prior to accessing any per-frame context state (framebuffer, viewport, etc.).
	void               beginDraw();

	// Call after all rendering to layer framebuffers is complete (submit layer framebuffers to the VR compositor).
	void               endDraw();

	// Get the eye framebuffer for a layer.
	Framebuffer*       getFramebuffer(Eye _eye, Layer _layer) const;

	// Get the eye viewport for a layer.
	Viewport           getViewport(Eye _eye, Layer _layer) const;

	// Get the eye stencil rectangle for a layer (visible region).
	Viewport           getStencilRect(Eye _eye, Layer _layer) const;

	// Draw a mesh representing the non-visible area of the viewport at the near plane.
	void               primeDepthBuffer(Eye _eye, float _depthValue = 0.f);

	// Return the refresh rate of the HMD.
	float              getHMDRefreshRate() const;
	
	// Return true if the HMD is connected and mounted (the application should render to the headset).
	bool               isActive() const;

	// Return true if the application has VR focus (e.g. false if Oculus dash is on).
	bool               isFocused() const;

	// Get tracked poses and velocity/acceleration data for head/hands.
	const TrackedData& getTrackedData() const              { return m_trackedData; }

	// Get eye camera.
	Camera*            getEyeCamera(Eye _eye)              { return &m_eyeCameras[_eye]; }

	// Get VR input device.
	VRInput&           getInputDevice()                    { return m_input; }


private:

	struct Transform
	{
		vec3 position    = vec3(0.f);
		quat orientation = quat(0.f, 0.f, 0.f, 1.f);
	};

	struct       Impl;
	Impl*        m_impl                       = nullptr;

	Transform    m_userTransform;
	Transform    m_prevUserTransform;

	TrackedData  m_trackedData;
	Camera       m_eyeCameras[Eye_Count];
	VRInput      m_input;
	uint64       m_frameIndex                 = 0;
	mat4         m_handPoses[Hand_Count]      = { identity };
	bool         m_showMirror                 = true;
	Shader*      m_shPrimeDepth               = nullptr;

	
	       VRContext() = default;
          ~VRContext() = default;
	
	bool   init();
	void   shutdown();
	void   pollHmd(float _dt);
	void   pollInput(float _dt);
};

} // namespace frm
