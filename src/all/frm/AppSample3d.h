#pragma once
#ifndef frm_AppSample3d_h
#define frm_AppSample3d_h

#include <frm/def.h>
#include <frm/geom.h>
#include <frm/AppSample.h>
#include <frm/Camera.h>
#include <frm/Scene.h>

#include <frm/im3d.h>

#include <vector>

namespace frm {

class Scene;

////////////////////////////////////////////////////////////////////////////////
/// \class AppSample3d
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

	/// Get a world/view space ray corresponding to the cursor position (by default
	/// the window-relative mouse position).
	virtual Ray getCursorRayW() const;
	virtual Ray getCursorRayV() const;

protected:		
	AppSample3d(const char* _title, const char* _appDataPath = 0);
	virtual ~AppSample3d();

	Scene               m_scene;           //< Contains all sub-scenes, this allows default cameras to be shared between scenes.
	std::vector<Node*>  m_sceneRoots;      //< Ptrs to sub-scene root nodes inside m_scene.
	uint                m_currentScene;    //< Index m_sceneRoots.
	Camera*             m_dbgCullCamera;
#ifdef frm_Scene_ENABLE_EDIT
	SceneEditor	        m_sceneEditor;
#endif
	
	bool* m_showHelpers;
	bool* m_showSceneEditor;

	Im3d::Context m_im3dCtx;

	static bool Im3d_Init();
	static void Im3d_Shutdown();
	static void Im3d_Render(Im3d::Context& _im3dCtx, const Camera& _cam, bool _depthTest = false);

}; // class AppSample3d

} // namespace frm

#endif // frm_AppSample3d_h
