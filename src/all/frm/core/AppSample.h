#pragma once

#include <frm/core/frm.h>
#include <frm/core/App.h>
#include <frm/core/FileSystem.h>

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
	static void FileChangeNotification(const char* _path, FileSystem::FileAction _action);

	virtual bool        init(const ArgList& _args) override;
	virtual void        shutdown() override;
	virtual bool        update() override;
	virtual void        draw();
	void                drawNdcQuad();
	
	// Get/set the framebuffer to which UI/overlays are drawn (a null ptr means the context backbuffer).
	const Framebuffer*  getDefaultFramebuffer() const                 { return m_fbDefault; }
	void                setDefaultFramebuffer(const Framebuffer* _fb) { m_fbDefault = _fb; }

	const ivec2&        getResolution() const                         { return m_resolution; }
	const ivec2&        getWindowSize() const                         { return m_windowSize; }
	Window*             getWindow()                                   { return m_window; }
	const Window*       getWindow() const                             { return m_window; }
	GlContext*          getGlContext()                                { return m_glContext; }
	const GlContext*    getGlContext() const                          { return m_glContext; }

	bool                canSetWindowCursorType() const                { return m_canSetWindowCursorType; } 
	void                setCanSetWindowCursorType(bool _value)        { m_canSetWindowCursorType = true; }

protected:

	AppSample(const char* _title);
	virtual ~AppSample();

	virtual void overrideInput() {}

	bool readConfig(const char* _path,  int _root = FileSystem::GetDefaultRoot());
	bool writeConfig(const char* _path, int _root = FileSystem::GetDefaultRoot());

	ivec2              m_resolution                 = ivec2(-1);
	ivec2              m_windowSize                 = ivec2(-1);
	int                m_vsyncMode                  = 1;//GlContext::Vsync_On;
	bool               m_showMenu                   = true;
	bool               m_showLog                    = false;
	bool               m_showLogNotifications       = true;
	bool               m_showPropertyEditor         = false;
	bool               m_showProfilerViewer         = false;
	bool               m_showTextureViewer          = false;
	bool               m_showShaderViewer           = false;
	bool               m_showResourceViewer         = false;
	bool               m_hiddenMode                 = false;  // don't display the app window, disable ImGui

	static void ImGui_RenderDrawLists(ImDrawData* _drawData);
private:

	String<32>         m_name                       = "";
	Window*            m_window                     = nullptr;
	GlContext*         m_glContext                  = nullptr;
	const Framebuffer* m_fbDefault                  = nullptr; // where to draw overlays, or default backbuffer if 0
	PathStr            m_configPath                 = "";
	int                m_rootCommon                 = 0;
	int                m_rootApp                    = 0;
	bool               m_canSetWindowCursorType     = true; // whether ImGui can set the window cursor type

	void drawMainMenuBar();
	void drawStatusBar();
	void drawNotifications();

	bool initRenderdoc();

	PathStr m_imguiIniPath;
	static bool ImGui_Init(AppSample* _app);
	static void ImGui_InitStyle();
	static bool ImGui_InitFont(AppSample* _app);
	static void ImGui_Shutdown(AppSample* _app);
	static void ImGui_Update(AppSample* _app);
	static bool ImGui_OnMouseButton(Window* _window, unsigned _button, bool _isDown);
	static bool ImGui_OnMouseWheel(Window* _window, float _delta);
	static bool ImGui_OnKey(Window* _window, unsigned _key, bool _isDown);
	static bool ImGui_OnChar(Window* _window, char _char);
	static bool ImGui_OnDpiChange(Window* _window, int _dpiX, int _dpiY);

}; // class AppSample

} // namespace frm
