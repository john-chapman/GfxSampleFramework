#pragma once
#ifndef frm_AppSample_h
#define frm_AppSample_h

#include <frm/def.h>
#include <frm/App.h>
#include <frm/AppProperty.h>

#include <apt/FileSystem.h>

#include <imgui/imgui.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
/// \class AppSample
/// Base class for graphics samples. Provides a window, OpenGL context + ImGui
/// integration. 
////////////////////////////////////////////////////////////////////////////////
class AppSample: public App
{
public:

	/// \return Current app instance. 
	static AppSample* AppSample::GetCurrent();

	virtual bool        init(const apt::ArgList& _args) override;
	virtual void        shutdown() override;
	virtual bool        update() override;
	virtual void        draw();

	virtual void        drawMainMenuBar()             {}
	virtual void        drawStatusBar()               {}
	
	void                drawNdcQuad();
	
	Window*             getWindow()                   { return m_window; }
	const Window*       getWindow() const             { return m_window; }
	GlContext*          getGlContext()                { return m_glContext; }
	const GlContext*    getGlContext() const          { return m_glContext; }

	/// Get/set the framebuffer to which UI/overlays are drawn (a null ptr means
	/// the context backbuffer).
	const Framebuffer*  getDefaultFramebuffer() const                 { return m_fbDefault; }
	void                setDefaultFramebuffer(const Framebuffer* _fb) { m_fbDefault = _fb; }

	uint                getFrameIndex() const         { return m_frameIndex; }

protected:		
	typedef apt::FileSystem::PathStr PathStr;

	AppSample(const char* _title, const char* _appDataPath = 0);
	virtual ~AppSample();

	virtual void ImGui_OverrideIo() {}

	AppProperties      m_properties;
	ivec2*             m_resolution;
	ivec2*             m_windowSize;

private:
	PathStr            m_name;

	// \todo support string properties
	PathStr            m_appDataPath;
	PathStr            m_imguiIniPath;
	PathStr            m_logPath;

	Window*            m_window;
	GlContext*         m_glContext;
	uint64             m_frameIndex;

	const Framebuffer* m_fbDefault; //< Where to draw overlays, or m_glContext backbuffer if 0.
	Mesh*              m_msQuad;    //< NDC quad.

	int*               m_vsyncMode;
	bool*              m_showMenu;
	bool*              m_showLog;
	bool*              m_showPropertyEditor;
	bool*              m_showProfilerViewer;
	bool*              m_showTextureViewer;
	bool*              m_showShaderViewer;

	static bool ImGui_Init();
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
