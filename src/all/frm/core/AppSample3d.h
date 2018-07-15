#pragma once
#ifndef frm_AppSample3d_h
#define frm_AppSample3d_h

#include <frm/def.h>
#include <frm/geom.h>
#include <frm/AppSample.h>
#include <frm/Camera.h>
#include <frm/Scene.h>

#include <apt/FileSystem.h>

#include <im3d/im3d.h>

namespace frm {

class Scene;

////////////////////////////////////////////////////////////////////////////////
// AppSample3d
////////////////////////////////////////////////////////////////////////////////
class AppSample3d: public AppSample
{
public:
	virtual bool init(const apt::ArgList& _args) override;
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

	Camera*      m_dbgCullCamera;
	Scene*       m_scene;

	bool         m_showHelpers;
	bool         m_showSceneEditor;
	apt::PathStr m_scenePath;
	
private:

	void drawMainMenuBar();

	static bool Im3d_Init();
	static void Im3d_Shutdown();
	static void Im3d_Update(AppSample3d* _app);
	static void Im3d_Draw(const Im3d::DrawList& _drawList);
	
}; // class AppSample3d

} // namespace frm

#endif // frm_AppSample3d_h
