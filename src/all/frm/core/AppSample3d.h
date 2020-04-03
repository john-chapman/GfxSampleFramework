#pragma once

#include <frm/core/frm.h>
#include <frm/core/geom.h>
#include <frm/core/AppSample.h>
#include <frm/core/Camera.h>
#include <frm/core/FileSystem.h>
#include <frm/core/Scene.h>

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

	Camera*  m_dbgCullCamera    = nullptr;
	Scene*   m_scene            = nullptr;

	bool     m_showHelpers      = false;
	bool     m_showSceneEditor  = false;
	PathStr  m_scenePath        = "Scene.json";
	
private:

	void drawMainMenuBar();

	static bool Im3d_Init(AppSample3d* _app);
	static void Im3d_Shutdown(AppSample3d* _app);
	static void Im3d_Update(AppSample3d* _app);
	static void Im3d_Draw(const Im3d::DrawList& _drawList);
	
}; // class AppSample3d

} // namespace frm
