#pragma once

#include <frm/core/frm.h>
#include <frm/core/geom.h>
#include <frm/core/AppSample.h>
#include <frm/core/Viewport.h>

#include <im3d/im3d.h>

namespace frm {

class Scene;

////////////////////////////////////////////////////////////////////////////////
// AppSample3d
////////////////////////////////////////////////////////////////////////////////
class AppSample3d: public AppSample
{
public:

	virtual bool init(const ArgList& _args) override;
	virtual void shutdown() override;
	virtual bool update() override;
	virtual void draw() override;

	// Get a world/view space ray corresponding to the cursor position (by default the window-relative mouse position).
	// By default Scene::GetDrawCamera() is used (if _camera == nullptr).
	virtual Ray getCursorRayW(const Camera* _camera = nullptr) const;
	virtual Ray getCursorRayV(const Camera* _camera = nullptr) const;

protected:

	static void DrawFrustum(const Frustum& _frustum);

	AppSample3d(const char* _title);
	virtual ~AppSample3d();

	void setIm3dDepthTexture(Texture* _tx);

	
	World* m_world = nullptr;
	PathStr  m_worldPath = "";
	WorldEditor* m_worldEditor = nullptr;

	bool     m_showHelpers      = false;
	bool     m_showWorldEditor = false;
	
	
	// Draw Im3d to multiple views.
	void drawIm3d(
		std::initializer_list<Camera*>      _cameras,
		std::initializer_list<Framebuffer*> _framebuffers,
		std::initializer_list<Viewport>     _viewports,
		std::initializer_list<Texture*>     _depthTextures
		);

	void drawIm3d(Camera* _camera, Framebuffer* _framebuffer = nullptr, Viewport _viewport = Viewport(), Texture* _depthTexture = nullptr);
	
private:

	void drawMainMenuBar();

	CameraComponent* m_restoreCullCamera = nullptr;
	void createDebugCullCamera();
	void destroyDebugCullCamera();

	Texture* m_txIm3dDepth = nullptr;
	static bool Im3d_Init(AppSample3d* _app);
	static void Im3d_Shutdown(AppSample3d* _app);
	static void Im3d_Update(AppSample3d* _app);

}; // class AppSample3d

} // namespace frm
