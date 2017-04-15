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

	virtual void drawMainMenuBar() override;
	virtual void drawStatusBar() override;

	// Get a world/view space ray corresponding to the cursor position (by default the window-relative mouse position).
	virtual Ray getCursorRayW() const;
	virtual Ray getCursorRayV() const;

protected:		
	static void DrawFrustum(const Frustum& _frustum);

	AppSample3d(const char* _title);
	virtual ~AppSample3d();

	Camera* m_dbgCullCamera;

	bool m_showHelpers;
	bool m_showSceneEditor;
	apt::FileSystem::PathStr m_scenePath;

	Im3d::Context m_im3dCtx;
	static bool Im3d_Init();
	static void Im3d_Shutdown();
	static void Im3d_Update(AppSample3d* _app);
	static void Im3d_Draw(const Im3d::DrawList& _drawList);
	
}; // class AppSample3d

} // namespace frm

#endif // frm_AppSample3d_h
