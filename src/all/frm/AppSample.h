#pragma once
#ifndef frm_AppSample_h
#define frm_AppSample_h

#include <frm/def.h>
#include <frm/App.h>
#include <frm/Property.h>

#include <apt/FileSystem.h>

#include <imgui/imgui.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// AppSample
// Base class for graphics samples. Provides a window, OpenGL context + ImGui
// integration. 
////////////////////////////////////////////////////////////////////////////////
class AppSample: public App
{
public:

	static AppSample* AppSample::GetCurrent();

	virtual bool        init(const apt::ArgList& _args) override;
	virtual void        shutdown() override;
	virtual bool        update() override;
	virtual void        draw();
	void                drawNdcQuad();
	
	// Get/set the framebuffer to which UI/overlays are drawn (a null ptr means the context backbuffer).
	const Framebuffer*  getDefaultFramebuffer() const                 { return m_fbDefault; }
	void                setDefaultFramebuffer(const Framebuffer* _fb) { m_fbDefault = _fb; }

	Properties&         getProperties()               { return m_props; }
	const Properties&   getProperties() const         { return m_props; }
	const ivec2&        getResolution() const         { return m_resolution; }
	const ivec2&        getWindowSize() const         { return m_windowSize; }
	Window*             getWindow()                   { return m_window; }
	const Window*       getWindow() const             { return m_window; }
	GlContext*          getGlContext()                { return m_glContext; }
	const GlContext*    getGlContext() const          { return m_glContext; }
	uint64              getFrameIndex() const         { return m_frameIndex; }

protected:		
	typedef apt::FileSystem::PathStr PathStr;

	AppSample(const char* _title);
	virtual ~AppSample();

	virtual void overrideInput() {}

	Properties m_props;
	bool readProps(const char* _path, apt::FileSystem::RootType _rootHint = apt::FileSystem::RootType_Application);
	bool writeProps(const char* _path, apt::FileSystem::RootType _rootHint = apt::FileSystem::RootType_Application);

	ivec2  m_resolution;
	ivec2  m_windowSize;
	int    m_vsyncMode;
	bool   m_showMenu;
	bool   m_showLog;
	bool   m_showLogNotifications;
	bool   m_showPropertyEditor;
	bool   m_showProfilerViewer;
	bool   m_showTextureViewer;
	bool   m_showShaderViewer;
	uint64 m_frameIndex = 0;

private:
	apt::String<32>    m_name;
	Window*            m_window    = nullptr;
	GlContext*         m_glContext = nullptr;
	const Framebuffer* m_fbDefault = nullptr; // where to draw overlays, or default backbuffer if 0

	apt::FileSystem::PathStr m_propsPath;

	void drawMainMenuBar();
	void drawStatusBar();
	void drawNotifications();

	apt::FileSystem::PathStr m_imguiIniPath;
	static bool ImGui_Init();
	static void ImGui_InitStyle();
	static void ImGui_Shutdown();
	static void ImGui_Update(AppSample* _app);
	static void ImGui_RenderDrawLists(ImDrawData* _drawData);
	static bool ImGui_OnMouseButton(Window* _window, unsigned _button, bool _isDown);
	static bool ImGui_OnMouseWheel(Window* _window, float _delta);
	static bool ImGui_OnKey(Window* _window, unsigned _key, bool _isDown);
	static bool ImGui_OnChar(Window* _window, char _char);

}; // class AppSample


} // namespace frm

#endif // frm_AppSample_h
