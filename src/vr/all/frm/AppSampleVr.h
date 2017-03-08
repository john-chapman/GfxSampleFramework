#pragma once
#ifndef frm_AppSampleVr_h
#define frm_AppSampleVr_h

#include <frm/def.h>
#include <frm/geom.h>
#include <frm/AppSample3d.h>
#include <frm/Camera.h>
#include <frm/Input.h>
#include <frm/Scene.h>

namespace frm {

class AppSampleVr: public AppSample3d
{
public:
	enum Eye
	{
		Eye_Left,
		Eye_Right,
		
		Eye_Count
	};

	enum Layer
	{
		Layer_Main,
		Layer_Text,

		Layer_Count
	};
	
	virtual bool init(const apt::ArgList& _args) override;
	virtual void shutdown() override;
	virtual bool update() override;
	virtual void draw() override;

	virtual void drawMainMenuBar() override;
	virtual void drawStatusBar() override;

	virtual Ray getCursorRayW() const override;
	virtual Ray getCursorRayV() const override;

protected:
	AppSampleVr(const char* _title);
	virtual ~AppSampleVr();

	virtual void overrideInput() override;

	bool isVrMode() const                 { return m_vrMode; }

	const Camera&  getEyeCamera(Eye _eye) { return m_eyeCameras[_eye]; }

	// Current texture for the specified _eye/_layer.
	const Texture* getEyeTexture(Eye _eye, Layer _layer);

	// Current framebuffer for the specified _eye/_layer.
	const Framebuffer* getEyeFramebuffer(Eye _eye, Layer _layer);

	// Signal that rendering is complete for the specified _eye/_layer.
	// \note UI rendering to Layer_Text occurs here.
	void commitEye(Eye _eye, Layer _layer);

	// Poll the HMD position, update eye poses.
	void pollHmd();

	// Recent origin to HMD position (+ vertical offset).
	void recenter();

private:
	bool         m_vrMode;
	bool         m_disableRender;
	bool         m_showGazeCursor;

	ProxyGamepad m_proxyGamepad;
	ProxyMouse   m_proxyMouse;

	float        m_eyeFovScale;         // Global fov scale (use with caution).
	float        m_clipNear, m_clipFar;

	Camera       m_eyeCameras[2];
	Camera*      m_vrDrawCamera;        // Combined eye cameras, override scene cull/draw camera for Im3d, etc.
	Camera*      m_sceneDrawCamera;     // Store/restore when entering/leaving VR mode.
	Node*        m_nodeOrigin;          // VR origin (parent of head).
	Node*        m_nodeHead;
	float*       m_userHeight;          // User height property, meters.
	
	vec2         m_headRotationDelta; // \todo encapsulte in gaze cursor?

	Texture*     m_txHmdMirror;
	Shader*      m_shHmdMirror;

	Texture*     m_txVuiScreen;
	Framebuffer* m_fbVuiScreen;
	Shader*      m_shVuiScreen;
	Plane        m_vuiScreenPlane;
	mat4         m_vuiScreenWorldMatrix;
	vec3*        m_vuiScreenOrigin;
	float*       m_vuiScreenDistance;
	float*       m_vuiScreenSize;
	float*       m_vuiScreenAspect;
	Texture*     m_txVuiScreenButtonMove;
	Texture*     m_txVuiScreenButtonScale;
	Texture*     m_txVuiScreenButtonDistance;
	bool         m_moveVuiScreen, m_scaleVuiScreen, m_distVuiScreen;

	bool         m_showVuiScreen;
	bool*        m_showVrOptions;
	bool*        m_showTrackingFrusta;


	struct VrContext;
	VrContext* m_vrCtx;

	bool initVr();
	void shutdownVr();

	void drawVuiScreen(const Camera& _cam);

}; // class AppSampleVr


} // namespace frm

#endif // frm_AppSampleVr_h
