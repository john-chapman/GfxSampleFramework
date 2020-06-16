#include "AppSample.h"

#include <frm/core/platform.h>
#include <frm/core/math.h>
#include <frm/core/memory.h>
#include <frm/core/ArgList.h>
#include <frm/core/App.h>
#include <frm/core/File.h>
#include <frm/core/FileSystem.h>
#include <frm/core/Framebuffer.h>
#include <frm/core/GlContext.h>
#include <frm/core/Input.h>
#include <frm/core/Json.h>
#include <frm/core/Log.h>
#include <frm/core/Mesh.h>
#include <frm/core/Profiler.h>
#include <frm/core/Properties.h>
#include <frm/core/Shader.h>
#include <frm/core/Texture.h>
#include <frm/core/Window.h>
#ifdef FRM_PLATFORM_WIN
	#include <frm/core/win.h>
	#include <frm/core/extern/renderdoc_app.h>
#endif

#include <imgui/imgui.h>

#include <cstring>

using namespace frm;

static Log        g_Log(100);
static AppSample* g_Current;

void AppLogCallback(const char* _msg, LogType _type)
{
	g_Log.addMessage(_msg, _type);
}

static ImU32 kColor_Log[LogType_Count];
static float kStatusBarLogWidth;
static int   kStatusBarFlags;

/*******************************************************************************

                                   AppSample

*******************************************************************************/

static void FileChangeNotification(const char* _path, FileSystem::FileAction _action)
{
 // some applications (e.g. Photoshop) write to a temporary file and then do a delete/rename, hence we need to check both _Modified and _Created actions
	if (_action == FileSystem::FileAction_Modified || _action == FileSystem::FileAction_Created) 
	{
	 // shader
		if (FileSystem::Matches("*.glsl", _path)) 
		{
			//FRM_LOG_DBG("Reload shader '%s'", _path);
			Shader::FileModified(_path);
			return;
		}

	 // texture
		if (FileSystem::MatchesMulti({"*.bmp", "*.dds", "*.exr", "*.hdr", "*.png", "*.tga", "*.jpg", "*.gif", "*.psd"}, _path)) 
		{
			//FRM_LOG_DBG("Reload texture '%s'", _path);
			Texture::FileModified(_path);
			return;
		}
	}
}

// PUBLIC

AppSample* AppSample::GetCurrent()
{
	FRM_ASSERT(g_Current);
	return g_Current;
}

bool AppSample::init(const frm::ArgList& _args)
{
	if (frm::GetLogCallback() == nullptr) // don't override an existing callback
	{
		frm::SetLogCallback(AppLogCallback);
	}
	if (!App::init(_args)) 
	{
		return false;
	}

	if (_args.find("renderdoc")) 
	{
		initRenderdoc();
	}

	m_hiddenMode = _args.find("hidden") != nullptr;

	FileSystem::AddRoot("");
	m_rootCommon = FileSystem::AddRoot("common");
	m_rootApp    = FileSystem::AddRoot((const char*)m_name);
	FileSystem::SetDefaultRoot(m_rootApp);

	FileSystem::BeginNotifications(FileSystem::GetRoot(m_rootCommon), &FileChangeNotification);
	FileSystem::BeginNotifications(FileSystem::GetRoot(m_rootApp),    &FileChangeNotification);

	if (!m_hiddenMode) 
	{	
		g_Log.setOutput((const char*)String<64>("%s.log", (const char*)m_name)); // need to set after the application root
	}
	g_Log.addMessage((const char*)String<64>("'%s' %s", (const char*)m_name, Time::GetDateTime().asString()));
	FRM_LOG("System info:\n%s", (const char*)GetPlatformInfoString());

 	m_configPath.setf("%s.json", (const char*)m_name);
	readConfig(m_configPath.c_str());

	Properties::PushGroup("AppSample");

	ivec2 windowSize     = m_hiddenMode ? ivec2(1) : *Properties::Find("WindowSize")->get<ivec2>();
	m_window             = Window::Create(windowSize.x, windowSize.y, (const char*)m_name);
	m_windowSize         = ivec2(m_window->getWidth(), m_window->getHeight());
		
	Properties::PushGroup("GlContext");
	ivec2 glVersion      = *Properties::Find("GlVersion",       "GlContext")->get<ivec2>();
	bool glCompatibility = *Properties::Find("GlCompatibility", "GlContext")->get<bool>();
	bool glDebug         = *Properties::Find("GlDebug",         "GlContext")->get<bool>();
	bool glHDR           = *Properties::Find("HDR",             "GlContext")->get<bool>();
	Properties::PopGroup();

	GlContext::CreateFlags ctxFlags = 0
		| (glCompatibility ? GlContext::CreateFlags_Compatibility : 0)
		| (glDebug         ? GlContext::CreateFlags_Debug         : 0)
		| (glHDR           ? GlContext::CreateFlags_HDR           : 0)
		;
	m_glContext          = GlContext::Create(m_window, glVersion.x, glVersion.y, ctxFlags);
	m_glContext->setVsync((GlContext::Vsync)(m_vsyncMode - 1));

	m_imguiIniPath       = FileSystem::MakePath("imgui.ini");
	ImGui::GetIO().IniFilename = (const char*)m_imguiIniPath;
	if (!ImGui_Init(this)) 
	{
		return false;
	}

	ivec2 resolution = *Properties::Find("Resolution")->get<ivec2>();
	m_resolution.x   = resolution.x == -1 ? m_windowSize.x : resolution.x;
	m_resolution.y   = resolution.y == -1 ? m_windowSize.y : resolution.y;

	Properties::PopGroup();

	kColor_Log[LogType_Log]   = 0xff999999;
	kColor_Log[LogType_Error] = 0xff1943ff;
	kColor_Log[LogType_Debug] = 0xffffaa33;
	kStatusBarLogWidth        = 0.4f; // fraction of window width
	kStatusBarFlags           = 0
		| ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoScrollbar
		| ImGuiWindowFlags_NoScrollWithMouse
		| ImGuiWindowFlags_NoCollapse
		| ImGuiWindowFlags_NoSavedSettings
		| ImGuiWindowFlags_NoBringToFrontOnFocus
		;

 // set ImGui callbacks
 // \todo poll input directly = easier to use proxy devices
	Window::Callbacks cb = m_window->getCallbacks();
	cb.m_OnDpiChange     = ImGui_OnDpiChange;
	cb.m_OnMouseButton   = ImGui_OnMouseButton;
	cb.m_OnMouseWheel    = ImGui_OnMouseWheel;
	cb.m_OnKey           = ImGui_OnKey;
	cb.m_OnChar          = ImGui_OnChar;
	m_window->setCallbacks(cb);

	FRM_VERIFY(AppSample::update());

	if (!m_hiddenMode) 
	{
		m_window->show();
	
	 // splash screen
		m_glContext->setFramebufferAndViewport(0);
		glAssert(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
		glAssert(glClear(GL_COLOR_BUFFER_BIT));
		ImGui::SetNextWindowSize(ImVec2(sizeof("Loading") * ImGui::GetFontSize(), ImGui::GetFrameHeightWithSpacing()));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::Begin(
			"Loading", 0, 0
			| ImGuiWindowFlags_NoTitleBar
			| ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoMove
			| ImGuiWindowFlags_NoSavedSettings
			| ImGuiWindowFlags_AlwaysAutoResize
			);
		ImGui::Text("Loading");
		ImGui::End();
		ImGui::PopStyleColor();
		AppSample::draw();
	}

	return true;
}

void AppSample::shutdown()
{	
	FileSystem::EndNotifications(FileSystem::GetRoot(m_rootCommon));
	FileSystem::EndNotifications(FileSystem::GetRoot(m_rootApp));

	ImGui_Shutdown(this);
	
	if (m_glContext) 
	{
		GlContext::Destroy(m_glContext);
	}
	if (m_window) 
	{
		Window::Destroy(m_window);
	}
	
	if (!m_hiddenMode) 
	{
		writeConfig(m_configPath.c_str());
	}

	App::shutdown();

	frm::SetLogCallback(nullptr);
}

bool AppSample::update()
{
	if (!App::update()) {
		return false;
	}

	PROFILER_MARKER_CPU("#AppSample::update");
	if (!m_window->hasFocus()) 
	{
	 // \todo keyboard/mouse input events aren't received when the window doesn't have focus which leads to an invalid device state
		Input::ResetKeyboard();
		Input::ResetMouse();
	}

	{	PROFILER_MARKER_CPU("#Poll Events");
		if (!m_window->pollEvents()) // dispatches callbacks to ImGui
		{
			return false;
		}
	}
	{	PROFILER_MARKER_CPU("#Dispatch File Notifications");
		FileSystem::DispatchNotifications();
	}

 // skip the default UI in hidden mode
	if (m_hiddenMode) 
	{
		return true;
	}

	Window* window = getWindow();
	m_windowSize = ivec2(m_window->getWidth(), m_window->getHeight());
	ImGui::GetIO().MousePos = ImVec2(-1.0f, -1.0f);
	if (window->hasFocus()) 
	{
		int x, y;
		window->getWindowRelativeCursor(&x, &y);
		ImGui::GetIO().MousePos = ImVec2((float)x, (float)y);
	}
	overrideInput(); // must call after Input::PollAllDevices (App::update()) but before ImGui_Update
	ImGui_Update(this);

 // keyboard shortcuts
	Keyboard* keyboard = Input::GetKeyboard();
    if (keyboard->isDown(Keyboard::Key_LShift) && keyboard->wasPressed(Keyboard::Key_Escape)) 
	{
		return false;
	}
	if (keyboard->isDown(Keyboard::Key_LCtrl) && keyboard->isDown(Keyboard::Key_LShift) && keyboard->wasPressed(Keyboard::Key_P) || keyboard->wasPressed(Keyboard::Key_Pause)) 
	{
		Profiler::SetPause(!Profiler::GetPause());	
	}
	if (keyboard->wasPressed(Keyboard::Key_F1)) 
	{
		m_showMenu = !m_showMenu;
	}
	if (keyboard->wasPressed(Keyboard::Key_F8)) 
	{
		m_glContext->clearTextureBindings();
		Texture::ReloadAll();
	}
	if (keyboard->wasPressed(Keyboard::Key_F9)) 
	{
		m_glContext->setShader(nullptr);
		Shader::ReloadAll();
	}
	if (ImGui::IsKeyPressed(Keyboard::Key_1) && ImGui::IsKeyDown(Keyboard::Key_LCtrl)) 
	{
		m_showProfilerViewer = !m_showProfilerViewer;
	}
	if (ImGui::IsKeyPressed(Keyboard::Key_2) && ImGui::IsKeyDown(Keyboard::Key_LCtrl)) 
	{
		m_showTextureViewer = !m_showTextureViewer;
	}
	if (ImGui::IsKeyPressed(Keyboard::Key_3) && ImGui::IsKeyDown(Keyboard::Key_LCtrl)) 
	{
		m_showShaderViewer = !m_showShaderViewer;
	}
	
	ImGuiIO& io = ImGui::GetIO();
	if (m_showMenu) 
	{ 
		drawMainMenuBar();
		drawStatusBar();
	}
	else 
	{
		drawNotifications();	 
	}

	if (m_showPropertyEditor)
	{
		static ImGuiTextFilter propertiesFilter;
		ImGui::Begin("Properties", &m_showPropertyEditor);
			propertiesFilter.Draw();
			Properties::GetCurrent()->edit(propertiesFilter.InputBuf);
		ImGui::End();
	}
	if (m_showProfilerViewer) 
	{
		ImGui::SetNextWindowPos(vec2(0.0f, 16.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(vec2(m_resolution.x * 1.0f, m_resolution.y * 1.0f/3.0f), ImGuiCond_FirstUseEver);
		Profiler::DrawUi();
	}
	if (m_showTextureViewer)
	{
		ImGui::SetNextWindowPos(vec2(0.0f, 16.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(vec2(m_resolution.x * 2.0f/3.0f, m_resolution.y * 2.0f/3.0f), ImGuiCond_FirstUseEver);
		Texture::ShowTextureViewer(&m_showTextureViewer);
	}
	if (m_showShaderViewer) 
	{
		ImGui::SetNextWindowPos(vec2(0.0f, 0.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(vec2(m_resolution.x * 2.0f/3.0f, m_resolution.y * 2.0f/3.0f), ImGuiCond_FirstUseEver);
		Shader::ShowShaderViewer(&m_showShaderViewer);
	}
	
	ImGui::BeginInvisible("OverlayWindow", vec2(0.0f), vec2((float)m_windowSize.x, (float)m_windowSize.y - (m_showMenu ? ImGui::GetFrameHeightWithSpacing() : 0.0f)));
		Profiler::DrawPinnedValues();
	ImGui::EndInvisible();

	return true;
}

void AppSample::draw()
{
	if (m_hiddenMode) 
	{
		ImGui::EndFrame();
	}
	else 
	{
		PROFILER_MARKER("#AppSample::draw");
		m_glContext->setFramebufferAndViewport(m_fbDefault);
		ImGui::GetIO().UserData = m_glContext;
		ImGui::Render();
	}

	{	PROFILER_MARKER("#VSYNC");
		m_glContext->setFramebufferAndViewport(0); // this is required if you want to use e.g. fraps
		m_glContext->present();
	}
}

void AppSample::drawNdcQuad()
{
	m_glContext->drawNdcQuad();
}


// PROTECTED

AppSample::AppSample(const char* _name)
	: App()
	, m_name(_name)
{
	FRM_ASSERT(g_Current == nullptr); // don't support multiple apps (yet)
	g_Current = this;

	
	Properties::PushGroup("AppSample");

		//              name                     default                     min           max                           storage
		Properties::Add("Resolution",            ivec2(-1),                  ivec2(1),     ivec2(32768)                                         );
		Properties::Add("WindowSize",            ivec2(-1),                  ivec2(1),     ivec2(32768)                                         );
		Properties::Add("VsyncMode",             m_vsyncMode,                0,            (int)GlContext::Vsync_On3,    &m_vsyncMode           );
		Properties::Add("ShowMenu",              m_showMenu,                                                             &m_showMenu            );
		Properties::Add("ShowLog",               m_showLog,                                                              &m_showLog             );
		Properties::Add("ShowLogNotifications",  m_showLogNotifications,                                                 &m_showLogNotifications);
		Properties::Add("ShowPropertyEditor",    m_showPropertyEditor,                                                   &m_showPropertyEditor  );
		Properties::Add("ShowProfiler",          m_showProfilerViewer,                                                   &m_showProfilerViewer  );
		Properties::Add("ShowTextureViewer",     m_showTextureViewer,                                                    &m_showTextureViewer   );
		Properties::Add("ShowShaderViewer",      m_showShaderViewer,                                                     &m_showShaderViewer    );

		Properties::PushGroup("Font");
			//                  name                 default                     min           max                           storage
			Properties::AddPath("FontPath",          ""                                                                              );
			Properties::Add    ("FontSize",          13.0f,                      4.0f,         64.0f                                 );
			Properties::Add    ("FontOversample",    1,                          1,            8                                     );
			Properties::Add    ("FontEnableScaling", true                                                                            );
		Properties::PopGroup(); // Font

		Properties::PushGroup("GlContext");
			//              name                 default                     min           max                           storage
			Properties::Add("GlVersion",         ivec2(-1, -1),              ivec2(-1),    ivec2(99)                             );
			Properties::Add("GlCompatibility",   false                                                                           );
			Properties::Add("GlDebug",           false                                                                           );
			Properties::Add("HDR",               false                                                                           );
		Properties::PopGroup(); // GlContext

	Properties::PopGroup(); // AppSample	
}

AppSample::~AppSample()
{
	//shutdown(); \todo it's not safe to call shutdown() twice

	Properties::PushGroup("AppSample");
		Properties::InvalidateStorage("Resolution");
		Properties::InvalidateStorage("WindowSize");
		Properties::InvalidateStorage("VsyncMode");
		Properties::InvalidateStorage("ShowMenu");
		Properties::InvalidateStorage("ShowLog");
		Properties::InvalidateStorage("ShowLogNotifications");
		Properties::InvalidateStorage("ShowPropertyEditor");
		Properties::InvalidateStorage("ShowProfiler");
		Properties::InvalidateStorage("ShowTextureViewer");
		Properties::InvalidateStorage("ShowShaderViewer");

		Properties::PushGroup("Font");
			Properties::InvalidateStorage("FontPath");
			Properties::InvalidateStorage("FontSize");
			Properties::InvalidateStorage("FontOversample");
			Properties::InvalidateStorage("FontEnableScaling");
		Properties::PopGroup(); // Font

		Properties::PushGroup("GlContext");
			Properties::InvalidateStorage("GlVersion");
			Properties::InvalidateStorage("GlCompatibility");
			Properties::InvalidateStorage("GlDebug");
			Properties::InvalidateStorage("HDR");
		Properties::PopGroup(); // GlContext

	Properties::PopGroup(); // AppSample	
}


bool AppSample::readConfig(const char* _path, int _root)
{
	Json json;
	if (Json::Read(json, _path, _root)) 
	{
		SerializerJson serializer(json, SerializerJson::Mode_Read);
		return Serialize(serializer, *Properties::GetCurrent());
	}
	return false;
}

bool AppSample::writeConfig(const char* _path, int _root)
{
	Json json;
	SerializerJson serializer(json, SerializerJson::Mode_Write);
	if (!Serialize(serializer, *Properties::GetCurrent()))
	{
		return false;
	}
	return Json::Write(json, _path, _root);
}

// PRIVATE

void AppSample::drawMainMenuBar()
{
	if (ImGui::BeginMainMenuBar()) 
	{
		if (ImGui::BeginMenu("Tools")) 
		{
			if (ImGui::MenuItem("Properties",     nullptr,    m_showPropertyEditor)) m_showPropertyEditor = !m_showPropertyEditor;
			if (ImGui::MenuItem("Profiler",       "Ctrl+1",   m_showProfilerViewer)) m_showProfilerViewer = !m_showProfilerViewer;
			if (ImGui::MenuItem("Texture Viewer", "Ctrl+2",   m_showTextureViewer))  m_showTextureViewer  = !m_showTextureViewer;
			if (ImGui::MenuItem("Shader Viewer",  "Ctrl+3",   m_showShaderViewer))   m_showShaderViewer   = !m_showShaderViewer;
			
			ImGui::EndMenu();
		}
		float vsyncWidth = (float)sizeof("Adaptive") * ImGui::GetFontSize();
		ImGui::PushItemWidth(vsyncWidth);
		float cursorX = ImGui::GetCursorPosX();
		ImGui::SetCursorPosX(ImGui::GetContentRegionAvailWidth() - vsyncWidth);
		if (ImGui::Combo("VSYNC", &m_vsyncMode, "Adaptive\0Off\0On\0On1\0On2\0On3\0")) 
		{
			getGlContext()->setVsync((GlContext::Vsync)(m_vsyncMode - 1));
		}
		ImGui::PopItemWidth();
		ImGui::SetCursorPosX(cursorX);
			
		ImGui::EndMainMenuBar();
	}
}

void AppSample::drawStatusBar()
{
	auto& io = ImGui::GetIO();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ImGui::GetStyle().WindowPadding.x, 2.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0,0));
	ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, ImGui::GetFrameHeightWithSpacing()));
	ImGui::SetNextWindowPos(ImVec2(0.0f, io.DisplaySize.y - ImGui::GetFrameHeightWithSpacing()));
	ImGui::Begin("##StatusBar", 0, kStatusBarFlags);
		ImGui::AlignTextToFramePadding();
		
		float logPosX = io.DisplaySize.x - io.DisplaySize.x * kStatusBarLogWidth + ImGui::GetStyle().WindowPadding.x;
		float cursorPosX = ImGui::GetCursorPosX();
		const Log::Message* logMsg = g_Log.getLastMessage();
		if (logMsg) 
		{
			ImGui::SetCursorPosX(logPosX);
			ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(kColor_Log[logMsg->m_type]), (const char*)logMsg->m_str);
			if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && ImGui::GetMousePos().x > logPosX) 
			{
				m_showLog = !m_showLog;
			}
			ImGui::SameLine();
			ImGui::SetCursorPosX(cursorPosX);
		}
		
	ImGui::End();
	ImGui::PopStyleVar(3);
	
	if (m_showLog) 
	{
		float logPosY = io.DisplaySize.y * 0.7f;
		ImGui::SetNextWindowPos(ImVec2(logPosX, logPosY));
		ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - logPosX, io.DisplaySize.y - logPosY - ImGui::GetFrameHeightWithSpacing()));
		ImGui::Begin("Log", 0, 0
			| ImGuiWindowFlags_NoTitleBar
			| ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoMove
			| ImGuiWindowFlags_NoSavedSettings
			);
	
			auto appTime = Time::GetApplicationElapsed().getRaw();
			auto msgTime = FRM_DATA_TYPE_MAX(sint64);
			for (int i = 0; i < LogType_Count; ++i) 
			{
				if (g_Log.getLastMessage(i)) 
				{
					msgTime = FRM_MIN(msgTime, appTime - g_Log.getLastMessage(i)->m_time.getRaw());
				}
			}
			bool autoScroll = ImGui::IsWindowAppearing() || Timestamp(msgTime).asSeconds() < 0.1;
			
			for (int i = 0, n = g_Log.getMessageCount(); i < n; ++i) 
			{
				auto msg = g_Log.getMessage(i);
				ImGui::PushStyleColor(ImGuiCol_Text, kColor_Log[msg->m_type]);
					ImGui::TextWrapped((const char*)msg->m_str);
				ImGui::PopStyleColor();
				
				if (autoScroll && msg == g_Log.getLastMessage(/*LogType_Error*/)) 
				{
					ImGui::SetScrollHere();
					autoScroll = false;
				}
			}
			if (autoScroll) {
				ImGui::SetScrollHere();
			}
			
			
		ImGui::End();
	}
}

void AppSample::drawNotifications()
{
	if (!m_showLogNotifications)
	{
		return;
	}

	auto& io = ImGui::GetIO();

 // error/debug log notifications
	auto logMsg = g_Log.getLastMessage();
	if (logMsg) 
	{
		float logAge = (float)(Time::GetApplicationElapsed() - logMsg->m_time).asSeconds();
		if (logAge < 3.0f) 
		{
			float logAlpha = 1.0f;
			if (logAge > 2.5f) 
			{
				logAlpha = 1.0f - (logAge - 2.5f) / 0.5f;
			}
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetColorU32(ImGuiCol_WindowBg, 0.8f * logAlpha));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ImGui::GetStyle().WindowPadding.x, 2.0f));
			ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - io.DisplaySize.x * kStatusBarLogWidth, io.DisplaySize.y - ImGui::GetFrameHeightWithSpacing()));
			ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * kStatusBarLogWidth, ImGui::GetFrameHeightWithSpacing()));
			ImGui::Begin("##Notifications", 0, kStatusBarFlags | ImGuiWindowFlags_NoFocusOnAppearing);
				ImGui::AlignTextToFramePadding();
				ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(IM_COLOR_ALPHA(kColor_Log[logMsg->m_type], logAlpha)), (const char*)logMsg->m_str);
				if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered()) 
				{
					m_showMenu = true;
					m_showLog = true;
				}
			ImGui::End();
			ImGui::PopStyleVar(2);
			ImGui::PopStyleColor();
		}
	}
}

/*******************************************************************************

                                   ImGui

*******************************************************************************/

static Shader*     s_shImGui;
static Mesh*       s_msImGui;
static Texture*    s_txImGui;
static TextureView s_txViewImGui; // default texture view for the ImGui texture
static Shader*     s_shTextureView[frm::internal::kTextureTargetCount]; // shader per texture type
static Texture*    s_txRadar;

bool AppSample::initRenderdoc()
{
#ifndef FRM_PLATFORM_WIN
	return false;
#else
	HMODULE dll = LoadLibraryA("extern/renderdoc.dll");
	if (!dll)
	{
		FRM_LOG_ERR("Failed to load RenderDoc");
		return false;
	}
	pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(dll, "RENDERDOC_GetAPI");
	RENDERDOC_API_1_4_0* renderdoc = nullptr;
	if (RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_4_0, (void**)&renderdoc) != 1)
	{
		FRM_LOG_ERR("Failed to get RenderDoc API");
		return false;
	}
	renderdoc->MaskOverlayBits(0, eRENDERDOC_Overlay_Default);
	renderdoc->SetCaptureFilePathTemplate("RenderDoc/GfxSampleFramework");
	RENDERDOC_InputButton cfrmureKey = RENDERDOC_InputButton::eRENDERDOC_Key_F11;
	renderdoc->SetCaptureKeys(&cfrmureKey, 1);

	return true;
#endif
}

bool AppSample::ImGui_Init(AppSample* _app)
{
	ImGuiIO& io = ImGui::GetIO();
	io.MemAllocFn = frm::internal::malloc;
	io.MemFreeFn = frm::internal::free;

	if (_app->m_hiddenMode) 
	{
		unsigned char* buf;
		int txX, txY;
		io.Fonts->GetTexDataAsAlpha8(&buf, &txX, &txY);
		io.Fonts->TexID = nullptr;
		return true;
	}
	
 // mesh
 	if (s_msImGui) 
	{
		Mesh::Release(s_msImGui);
	}	
	MeshDesc meshDesc(MeshDesc::Primitive_Triangles);
	meshDesc.addVertexAttr(VertexAttr::Semantic_Positions, DataType_Float32, 2);
	meshDesc.addVertexAttr(VertexAttr::Semantic_Texcoords, DataType_Float32, 2);
	meshDesc.addVertexAttr(VertexAttr::Semantic_Colors,    DataType_Uint32,  1);
	FRM_ASSERT(meshDesc.getVertexSize() == sizeof(ImDrawVert));
	s_msImGui = Mesh::Create(meshDesc);

 // shaders
	if (s_shImGui) 
	{
		Shader::Release(s_shImGui);
	}
	FRM_VERIFY(s_shImGui = Shader::CreateVsFs("shaders/ImGui.glsl", "shaders/ImGui.glsl"));
	s_shImGui->setName("#ImGui");

	ShaderDesc desc;
	desc.setPath(GL_VERTEX_SHADER,   "shaders/ImGui.glsl");
	desc.setPath(GL_FRAGMENT_SHADER, "shaders/TextureView_fs.glsl");
	for (int i = 0; i < internal::kTextureTargetCount; ++i) 
	{
		desc.clearDefines();
		desc.addDefine(GL_FRAGMENT_SHADER, internal::GlEnumStr(internal::kTextureTargets[i]) + 3); // \hack +3 removes the 'GL_', which is reserved in the shader
		FRM_VERIFY(s_shTextureView[i] = Shader::Create(desc));
		s_shTextureView[i]->setNamef("#TextureViewer_%s", internal::GlEnumStr(internal::kTextureTargets[i]) + 3);
	}

 // radar texture
	if (s_txRadar) 
	{
		Texture::Release(s_txRadar);
	}
	s_txRadar = Texture::Create("textures/radar.tga");
	s_txRadar->setName("#TextureViewer_radar");

	ImGui_InitFont(_app);

	if (s_txImGui)
	{
		Texture::Release(s_txImGui);
	}
	unsigned char* buf;
	int txX, txY;
	io.Fonts->GetTexDataAsAlpha8(&buf, &txX, &txY);
	s_txImGui = Texture::Create2d(txX, txY, GL_R8);
	s_txImGui->setFilter(GL_NEAREST);
	s_txImGui->setName("#ImGuiFont");
	s_txImGui->setData(buf, GL_RED, GL_UNSIGNED_BYTE);
	s_txViewImGui = TextureView(s_txImGui, s_shImGui);
	io.Fonts->TexID = (void*)&s_txViewImGui; // need a TextureView ptr for rendering

	
 // init ImGui state
	io.KeyMap[ImGuiKey_Tab]        = Keyboard::Key_Tab;
    io.KeyMap[ImGuiKey_LeftArrow]  = Keyboard::Key_Left;
    io.KeyMap[ImGuiKey_RightArrow] = Keyboard::Key_Right;
    io.KeyMap[ImGuiKey_UpArrow]	   = Keyboard::Key_Up;
    io.KeyMap[ImGuiKey_DownArrow]  = Keyboard::Key_Down;
	io.KeyMap[ImGuiKey_PageUp]	   = Keyboard::Key_PageUp;
    io.KeyMap[ImGuiKey_PageDown]   = Keyboard::Key_PageDown;
    io.KeyMap[ImGuiKey_Home]	   = Keyboard::Key_Home;
    io.KeyMap[ImGuiKey_End]		   = Keyboard::Key_End;
    io.KeyMap[ImGuiKey_Delete]	   = Keyboard::Key_Delete;
	io.KeyMap[ImGuiKey_Backspace]  = Keyboard::Key_Backspace;
    io.KeyMap[ImGuiKey_Enter]	   = Keyboard::Key_Return;
	io.KeyMap[ImGuiKey_Escape]	   = Keyboard::Key_Escape;
    io.KeyMap[ImGuiKey_A]		   = Keyboard::Key_A;
    io.KeyMap[ImGuiKey_C]		   = Keyboard::Key_C;
    io.KeyMap[ImGuiKey_V]		   = Keyboard::Key_V;
    io.KeyMap[ImGuiKey_X]		   = Keyboard::Key_X;
    io.KeyMap[ImGuiKey_Y]		   = Keyboard::Key_Y;
    io.KeyMap[ImGuiKey_Z]		   = Keyboard::Key_Z;
	io.DisplayFramebufferScale     = ImVec2(1.0f, 1.0f);
	io.RenderDrawListsFn           = ImGui_RenderDrawLists;
	io.IniSavingRate               = -1.0f; // never save automatically

	ImGui_InitStyle();

	return true;
}

void AppSample::ImGui_InitStyle()
{
#if 1
	ImGui::StyleColorsDark();
#else
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = style.ChildRounding = style.FrameRounding = style.GrabRounding = 4.0f;
	style.ScrollbarRounding = 16.0f;
	style.ScrollbarSize = 12.0f;
	style.FramePadding = ImVec2(4.0f, 2.0f);
	style.ItemSpacing = ImVec2(8.0f, 4.0f);
	style.Alpha = 1.0f;

	style.Colors[ImGuiCol_Text]                     = ImColor(0xffdedede);
    style.Colors[ImGuiCol_TextDisabled]             = ImColor(0xd0dedede);
    style.Colors[ImGuiCol_WindowBg]                 = ImColor(0xdc242424);
    style.Colors[ImGuiCol_ChildBg]                  = ImColor(0xff242424);
    style.Colors[ImGuiCol_PopupBg]                  = ImColor(0xdc101010);
    style.Colors[ImGuiCol_Border]                   = ImColor(0x080a0a0a);
    style.Colors[ImGuiCol_BorderShadow]             = ImColor(0x04040404);
    style.Colors[ImGuiCol_FrameBg]                  = ImColor(0xff604328);
    style.Colors[ImGuiCol_FrameBgHovered]           = ImColor(0xff705338);
    style.Colors[ImGuiCol_FrameBgActive]			= ImColor(0xff705338);
    style.Colors[ImGuiCol_TitleBg]                  = ImColor(0xdc242424);
    style.Colors[ImGuiCol_TitleBgCollapsed]			= ImColor(0x7f242424);
    style.Colors[ImGuiCol_TitleBgActive]			= ImColor(0xff0a0a0a);
    style.Colors[ImGuiCol_MenuBarBg]				= ImColor(0xdc242424);
	style.Colors[ImGuiCol_ScrollbarBg]              = ImColor(0xdc0b0b0b);
    style.Colors[ImGuiCol_ScrollbarGrab]            = ImColor(0xef4f4f4f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered]     = ImColor(0xff4f4f4f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]      = ImColor(0xff4f4f4f);
    //style.Colors[ImGuiCol_CheckMark]
    style.Colors[ImGuiCol_SliderGrab]               = ImColor(0xffe0853d);
    style.Colors[ImGuiCol_SliderGrabActive]         = ImColor(0xfff0954d);
    style.Colors[ImGuiCol_Button]                   = ImColor(0xff714c2b);
    style.Colors[ImGuiCol_ButtonHovered]            = ImColor(0xff815c3b);
    style.Colors[ImGuiCol_ButtonActive]             = ImColor(0xff815c3b);
    style.Colors[ImGuiCol_Header]                   = ImColor(0xdc242424);
    style.Colors[ImGuiCol_HeaderHovered]            = ImColor(0xdc343434);
    style.Colors[ImGuiCol_HeaderActive]             = ImColor(0xdc242424);
    //style.Colors[ImGuiCol_Column]
    //style.Colors[ImGuiCol_ColumnHovered]
    //style.Colors[ImGuiCol_ColumnActive]
    //style.Colors[ImGuiCol_ResizeGrip]
    //style.Colors[ImGuiCol_ResizeGripHovered]
    //style.Colors[ImGuiCol_ResizeGripActive]
    style.Colors[ImGuiCol_CloseButton]              = ImColor(0xff353535);
    style.Colors[ImGuiCol_CloseButtonHovered]       = ImColor(0xff454545);
    style.Colors[ImGuiCol_CloseButtonActive]        = ImColor(0xff454545);
    style.Colors[ImGuiCol_PlotLines]                = ImColor(0xff0485fd);
    style.Colors[ImGuiCol_PlotLinesHovered]         = ImColor(0xff0485fd);
    style.Colors[ImGuiCol_PlotHistogram]            = ImColor(0xff0485fd);
    style.Colors[ImGuiCol_PlotHistogramHovered]     = ImColor(0xff0485fd);
    //style.Colors[ImGuiCol_TextSelectedBg]
    //style.Colors[ImGuiCol_ModalWindowDarkening]
#endif

	ImGui::SetColorEditOptions(0
		| ImGuiColorEditFlags_NoOptions
		| ImGuiColorEditFlags_AlphaPreview
		| ImGuiColorEditFlags_AlphaBar
		);
}

bool AppSample::ImGui_InitFont(AppSample* _app)
{
	Properties::PushGroup("Font");

	ImGuiIO& io = ImGui::GetIO();

	PathStr& fontPath       = *Properties::Find("FontPath")->get<PathStr>();
	float    fontSize       = *Properties::Find("FontSize")->get<float>();
	int      fontOversample = *Properties::Find("FontOversample")->get<int>();

	ImFontConfig fontCfg;
	fontCfg.OversampleH = fontCfg.OversampleV = fontOversample;
	fontCfg.SizePixels  = fontSize;
	if (*Properties::Find("FontEnableScaling")->get<bool>()) 
	{
		fontCfg.SizePixels = Ceil(fontCfg.SizePixels * _app->getWindow()->getScaling());
	}
	fontCfg.PixelSnapH  = true;
	
	io.Fonts->Clear();
	if (fontPath.isEmpty())
	{
		io.Fonts->AddFontDefault(&fontCfg);
	}
	else 
	{
		io.Fonts->AddFontFromFileTTF((const char*)fontPath, fontSize, &fontCfg);
	}
	fontCfg.MergeMode = true;
	const ImWchar glyphRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
	io.Fonts->AddFontFromFileTTF("common/fonts/" FONT_ICON_FILE_NAME_FA, fontSize, &fontCfg, glyphRanges);
	
	if (s_txImGui)
	{
		Texture::Release(s_txImGui);
	}
	unsigned char* buf;
	int txX, txY;
	io.Fonts->GetTexDataAsAlpha8(&buf, &txX, &txY);
	s_txImGui = Texture::Create2d(txX, txY, GL_R8);
	s_txImGui->setFilter(GL_NEAREST);
	s_txImGui->setName("#ImGuiFont");
	s_txImGui->setData(buf, GL_RED, GL_UNSIGNED_BYTE);
	s_txViewImGui = TextureView(s_txImGui, s_shImGui);
	io.Fonts->TexID = (void*)&s_txViewImGui; // need a TextureView ptr for rendering

	Properties::PopGroup();
	return true;
}

void AppSample::ImGui_Shutdown(AppSample* _app)
{
	for (int i = 0; i < internal::kTextureTargetCount; ++i)
	{
		Shader::Release(s_shTextureView[i]);
	}
	Shader::Release(s_shImGui);
	Mesh::Release(s_msImGui); 
	Texture::Release(s_txRadar);
	Texture::Release(s_txImGui);
	
	ImGui::Shutdown();
}

void AppSample::ImGui_Update(AppSample* _app)
{
	PROFILER_MARKER_CPU("#ImGui_Update");

	ImGuiIO& io = ImGui::GetIO();

 // extract keyboard/mouse input
	/*Mouse* mouse = Input::GetMouse();
	io.MouseDown[0] = mouse->isDown(Mouse::Button_Left);
	io.MouseDown[1] = mouse->isDown(Mouse::Button_Right);
	io.MouseDown[2] = mouse->isDown(Mouse::Button_Middle);
	io.MouseWheel   = mouse->getAxisState(Mouse::Axis_Wheel) / 120.0f;

	Keyboard* keyb = Input::GetKeyboard();
	for (int i = 0, n = FRM_MIN((int)FRM_ARRAY_COUNT(io.KeysDown), (int)Keyboard::Key_Count); i < n; ++i) {
		io.KeysDown[i] = keyb->isDown(i);
	}
	io.KeyCtrl  = keyb->isDown(Keyboard::Key_LCtrl) | keyb->isDown(Keyboard::Key_RCtrl);
	io.KeyAlt   = keyb->isDown(Keyboard::Key_LAlt) | keyb->isDown(Keyboard::Key_RAlt);
	io.KeyShift = keyb->isDown(Keyboard::Key_LShift) | keyb->isDown(Keyboard::Key_RShift);
	*/

 // consume keyboard/mouse input
	if (io.WantCfrmureKeyboard)
	{
		Input::ResetKeyboard();
	}
	if (io.WantCfrmureMouse)
	{
		Input::ResetMouse();
	}

	io.ImeWindowHandle = _app->getWindow()->getHandle();
	if (_app->getDefaultFramebuffer())
	{
		io.DisplaySize = ImVec2((float)_app->getDefaultFramebuffer()->getWidth(), (float)_app->getDefaultFramebuffer()->getHeight());
	}
	else
	{
		io.DisplaySize = ImVec2((float)_app->getWindow()->getWidth(), (float)_app->getWindow()->getHeight());
	}
	io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
	io.DeltaTime = (float)_app->m_deltaTime;

	if (!_app->getWindow()->hasFocus())
	{
	 // \todo keyboard/mouse input events aren't received when the window doesn't have focus which leads to an invalid device state
		memset(io.KeysDown, 0, sizeof(io.KeysDown));
		io.KeyAlt = io.KeyCtrl = io.KeyShift = false;
	}

	ImGui::NewFrame(); // must call after m_window->pollEvents()

	//ImGui::ShowTestWindow();
}

void AppSample::ImGui_RenderDrawLists(ImDrawData* _drawData)
{
	PROFILER_MARKER("#ImGui_RenderDrawLists");

	ImGuiIO& io = ImGui::GetIO();
	GlContext* ctx = (GlContext*)io.UserData;

	if (_drawData->CmdListsCount == 0)
	{
		return;
	}	
	int fbX = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
    int fbY = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
    if (fbX == 0 || fbY == 0)
	{
        return;
	}
    _drawData->ScaleClipRects(io.DisplayFramebufferScale);

	glScopedEnable(GL_BLEND,        GL_TRUE);
	glScopedEnable(GL_SCISSOR_TEST, GL_TRUE);
	glScopedEnable(GL_CULL_FACE,    GL_FALSE);
	glScopedEnable(GL_DEPTH_TEST,   GL_FALSE);
    glAssert(glBlendEquation(GL_FUNC_ADD));
    glAssert(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    glAssert(glActiveTexture(GL_TEXTURE0));

	glAssert(glViewport(0, 0, (GLsizei)fbX, (GLsizei)fbY));
	mat4 ortho = mat4(
		vec4( 2.0f/io.DisplaySize.x, 0.0f,                   0.0f, 0.0f),
		vec4( 0.0f,                  2.0f/-io.DisplaySize.y, 0.0f, 0.0f),
		vec4( 0.0f,                  0.0f,                   1.0f, 0.0f),
		vec4(-1.0f,                  1.0f,                   0.0f, 1.0f)
		);

	for (int i = 0; i < _drawData->CmdListsCount; ++i)
	{
		const ImDrawList* drawList = _drawData->CmdLists[i];
		uint indexOffset = 0;

	 // upload vertex/index data
		s_msImGui->setVertexData((GLvoid*)&drawList->VtxBuffer.front(), (GLsizeiptr)drawList->VtxBuffer.size(), GL_STREAM_DRAW);
		FRM_STATIC_ASSERT(sizeof(ImDrawIdx) == sizeof(uint16)); // need to change the index data type if this fails
		s_msImGui->setIndexData(DataType_Uint16, (GLvoid*)&drawList->IdxBuffer.front(), (GLsizeiptr)drawList->IdxBuffer.size(), GL_STREAM_DRAW);
	
	 // dispatch draw commands
		for (const ImDrawCmd* pcmd = drawList->CmdBuffer.begin(); pcmd != drawList->CmdBuffer.end(); ++pcmd)
		{
			if (pcmd->UserCallback)
			{
				pcmd->UserCallback(drawList, pcmd);
			}
			else
			{
				TextureView* txView = (TextureView*)pcmd->TextureId;
				const Texture* tx   = txView->m_texture;
				Shader* sh          = txView->m_shader;
				if (!sh)
				{
					sh = s_shTextureView[internal::TextureTargetToIndex(tx->getTarget())]; // select a default shader based on the texture type
				}
				ctx->setShader  (sh);
				ctx->setMesh    (s_msImGui);
				ctx->setUniform ("uProjMatrix", ortho);
				ctx->setUniform ("uBiasUv",     txView->getNormalizedOffset());
				ctx->setUniform ("uScaleUv",    txView->getNormalizedSize());
				ctx->setUniform ("uLayer",      (float)txView->m_array);
				ctx->setUniform ("uMip",        (float)txView->m_mip);
				ctx->setUniform ("uRgbaMask",   uvec4(txView->m_rgbaMask[0], txView->m_rgbaMask[1], txView->m_rgbaMask[2], txView->m_rgbaMask[3]));
				ctx->setUniform ("uIsDepth",    (int)(txView->m_texture->isDepth()));
				ctx->bindTexture("txTexture",   txView->m_texture);
				ctx->bindTexture("txRadar",     s_txRadar);

                glAssert(glScissor((int)pcmd->ClipRect.x, (int)(fbY - pcmd->ClipRect.w), (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y)));
				glAssert(glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, GL_UNSIGNED_SHORT, (GLvoid*)indexOffset));
			}
			indexOffset += pcmd->ElemCount * sizeof(ImDrawIdx);
		}
	}

	ctx->setShader(0);
}

bool AppSample::ImGui_OnMouseButton(Window* _window, unsigned _button, bool _isDown)
{
	ImGuiIO& io = ImGui::GetIO();
	FRM_ASSERT(_button < FRM_ARRAY_COUNT(io.MouseDown)); // button index out of bounds
	switch ((Mouse::Button)_button)
	{
		case Mouse::Button_Left:    io.MouseDown[0] = _isDown; break;
		case Mouse::Button_Right:   io.MouseDown[1] = _isDown; break;
		case Mouse::Button_Middle:  io.MouseDown[2] = _isDown; break;
		default: break;
	};
	
	return true;
}
bool AppSample::ImGui_OnMouseWheel(Window* _window, float _delta)
{
	ImGuiIO& io = ImGui::GetIO();
	io.MouseWheel = _delta;	
	return true;
}
bool AppSample::ImGui_OnKey(Window* _window, unsigned _key, bool _isDown)
{
	ImGuiIO& io = ImGui::GetIO();
	FRM_ASSERT(_key < FRM_ARRAY_COUNT(io.KeysDown)); // key index out of bounds
	io.KeysDown[_key] = _isDown;

	// handle modifiers
	switch ((Keyboard::Key)_key)
	{
		case Keyboard::Key_LCtrl:
		case Keyboard::Key_RCtrl:
			io.KeyCtrl = _isDown;
			break;
		case Keyboard::Key_LShift:
		case Keyboard::Key_RShift:
			io.KeyShift = _isDown;
			break;			
		case Keyboard::Key_LAlt:
		case Keyboard::Key_RAlt:
			io.KeyAlt = _isDown;
			break;
		default: 
			break;
	};

	return true;
}
bool AppSample::ImGui_OnChar(Window* _window, char _char)
{
	ImGuiIO& io = ImGui::GetIO();
	if (_char > 0 && _char < 0x10000)
	{
		io.AddInputCharacter((unsigned short)_char);
	}
	return true;
}


bool AppSample::ImGui_OnDpiChange(Window* _window, int _dpiX, int _dpiY)
{
	ImGui_InitFont(AppSample::GetCurrent());
	return true;
}