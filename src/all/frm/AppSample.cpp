#include <frm/AppSample.h>

#include <frm/math.h>
#include <frm/App.h>
#include <frm/Framebuffer.h>
#include <frm/GlContext.h>
#include <frm/Input.h>
#include <frm/Mesh.h>
#include <frm/Profiler.h>
#include <frm/Shader.h>
#include <frm/Texture.h>
#include <frm/Window.h>
#include <frm/ui/Log.h>
#include <frm/ui/ProfilerViewer.h>
#include <frm/ui/TextureViewer.h>
#include <frm/ui/ShaderViewer.h>

#include <apt/ArgList.h>
#include <apt/File.h>
#include <apt/FileSystem.h>
#include <apt/IniFile.h>

#include <imgui/imgui.h>

#include <cstring>

using namespace frm;
using namespace apt;

static ui::Log            s_log(32, 512);
static ui::ProfilerViewer s_profilerViewer;
static ui::TextureViewer  s_textureViewer;
static ui::ShaderViewer   s_shaderViewer;
static AppSample*         s_current;

void AppLogCallback(const char* _msg, LogType _type)
{
	s_log.addMessage(_msg, _type);
}

/*******************************************************************************

                                   AppSample

*******************************************************************************/

// PUBLIC

AppSample* AppSample::GetCurrent()
{
	APT_ASSERT(s_current);
	return s_current;
}

bool AppSample::init(const apt::ArgList& _args)
{
	apt::SetLogCallback(AppLogCallback);

	if (!App::init(_args)) {
		return false;
	}
	
 // get FileSystem roots from _args
	const Arg* arg;
	if (arg = _args.find("CommonDataPath")) {
		if (arg->getValueCount() == 1) {
			FileSystem::SetRoot(FileSystem::kCommon, arg->getValue(0).asString());
		} else {
			APT_LOG_ERR("'CommonDataPath' invalid argument format (usage: '-CommonDataPath \"commonData\"')");
		}
	}
	if (arg = _args.find("AppDataPath")) {
		if (arg->getValueCount() == 1) {
			FileSystem::SetRoot(FileSystem::kApplication, arg->getValue(0).asString());
		} else {
			APT_LOG_ERR("'AppDataPath' invalid argument format (usage: '-AppDataPath \"appData\"')");
		}
	}

 // init FileSystem roots
	FileSystem::SetRoot(FileSystem::kCommon, "common");
	FileSystem::SetRoot(FileSystem::kApplication, (const char*)m_appDataPath);
	

	
 // load settings from ini
	m_properties.setIniPath(PathStr("%s.ini", (const char*)m_name));
	m_properties.load();

	 // \todo support string properties
		/*IniFile::Property p = ini.getProperty("AppDataPath", "AppSample");
		if (!p.isNull()) {
			if (p.getCount() == 1) {
				FileSystem::SetRoot(FileSystem::kApplication, p.asString(0));
			} else {
				APT_LOG_ERR("'AppDataPath' invalid property (usage: 'AppDataPath = \"appData\"')");
			}
		}
		p = ini.getProperty("CommonDataPath", "AppSample");
		if (!p.isNull()) {
			if (p.getCount() == 1) {
				FileSystem::SetRoot(FileSystem::kCommon, p.asString(0));
			} else {
				APT_LOG_ERR("'CommonDataPath' invalid property (usage: 'CommonDataPath = \"commonData\"')");
			}
		}
		p = ini.getProperty("LogFile", "AppSample");
		if (!p.isNull()) {
			if (p.getCount() == 1) {
				m_logPath.set(p.asString(0));
			} else {
				APT_LOG_ERR("'LogFile' invalid property (usage: 'LogFile = \"log.txt\"')");
			}
		}*/

 // command line args overwrite settings from the ini
	m_properties.setValues(_args);
	if (arg = _args.find("AppDataPath")) {
		if (arg->getValueCount() == 1) {
			FileSystem::SetRoot(FileSystem::kApplication, arg->getValue().asString());
		} else {
			APT_LOG_ERR("'AppDataPath' invalid argument format (usage: '-AppDataPath \"appData\"')");
		}
	}
	if (arg = _args.find("CommonDataPath")) {
		if (arg->getValueCount() == 1) {
			FileSystem::SetRoot(FileSystem::kCommon, arg->getValue().asString());
		} else {
			APT_LOG_ERR("'CommonDataPath' invalid argument format (usage: '-CommonDataPath \"commonData\"')");
		}
	}
	if (arg = _args.find("LogFile")) {
		if (arg->getValueCount() == 1) {
			m_logPath.set(arg->getValue().asString());
		} else {
			APT_LOG_ERR("'LogFile' invalid argument format (usage: 'LogFile = \"log.txt\"')");
		}
	}


 // init the app
	AppPropertyGroup& props = m_properties["AppSample"];
	const ivec2& winSize = props["WindowSize"].getValue<ivec2>();
	const ivec2& glVersion = props["GlVersion"].getValue<ivec2>();
	bool glCompatibility = props["GlCompatibility"].getValue<bool>();
	m_window    = Window::Create(winSize.x, winSize.y, m_name);
	m_glContext = GlContext::Create(m_window, glVersion.x, glVersion.y, glCompatibility);
	
	m_glContext->setVsyncMode((GlContext::VsyncMode)(*m_vsyncMode - 1));
	
	FileSystem::MakePath(m_imguiIniPath, "imgui.ini", FileSystem::kApplication);
	ImGui::GetIO().IniFilename = (const char*)m_imguiIniPath;
	if (!ImGui_Init()) {
		return false;
	}

	MeshDesc quadDesc;
	quadDesc.setPrimitive(MeshDesc::kTriangleStrip);
	quadDesc.addVertexAttr(VertexAttr::kPositions, 2, DataType::kFloat32);
	//quadDesc.addVertexAttr(VertexAttr::kTexcoords, 2, DataType::kFloat32);
	float quadVertexData[] = { 
		-1.0f, -1.0f, //0.0f, 0.0f,
		 1.0f, -1.0f, //1.0f, 0.0f,
		-1.0f,  1.0f, //0.0f, 1.0f,
		 1.0f,  1.0f, //1.0f, 1.0f
	};
	MeshData* quadData = MeshData::Create(quadDesc, 4, 0, quadVertexData);
	m_msQuad = Mesh::Create(*quadData);
	MeshData::Destroy(quadData);
	
 // set ImGui callbacks
	Window::Callbacks cb = m_window->getCallbacks();
	cb.m_OnMouseButton = ImGui_OnMouseButton;
	cb.m_OnMouseWheel  = ImGui_OnMouseWheel;
	cb.m_OnKey         = ImGui_OnKey;
	cb.m_OnChar        = ImGui_OnChar;
	m_window->setCallbacks(cb);


	m_window->show();

 // splash screen
	APT_VERIFY(AppSample::update());
	m_glContext->setFramebufferAndViewport(0);
	glAssert(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
	glAssert(glClear(GL_COLOR_BUFFER_BIT));
	ImGui::SetNextWindowPosCenter();
	ImGui::SetNextWindowSize(ImVec2(sizeof("Loading") * ImGui::GetFontSize(), ImGui::GetItemsLineHeightWithSpacing()));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::Begin(
		"Loading", 0, 
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_AlwaysAutoResize
		);
	ImGui::Text("Loading");
	ImGui::End();
	ImGui::PopStyleColor();
	AppSample::draw();
	
	return true;
}

void AppSample::shutdown()
{	
	ImGui_Shutdown();
	if (m_msQuad) {
		Mesh::Destroy(m_msQuad);
	}

	if (m_glContext) {
		GlContext::Destroy(m_glContext);
	}
	if (m_window) {
		Window::Destroy(m_window);
	}
	
	m_properties.save();

	App::shutdown();
}

bool AppSample::update()
{
	App::update();

	CPU_AUTO_MARKER("AppSample::update");

	if (!m_window->pollEvents()) { // dispatches callbacks to ImGui
		return false;
	}
	// prepare ImGui for next frame
	Window* window = getWindow();
	ImGui::GetIO().MousePos = ImVec2(-1.0f, -1.0f);
	if (window->hasFocus()) {
		int x, y;
		window->getWindowRelativeCursor(&x, &y);
		ImGui::GetIO().MousePos = ImVec2((float)x, (float)y);
	}
	ImGui_OverrideIo();
	ImGui_Update(this);

 // keyboard shortcuts
	Keyboard* keyboard = Input::GetKeyboard();
	if (keyboard->wasPressed(Keyboard::kEscape)) {
		return false;
	}
	if (keyboard->wasPressed(Keyboard::kF1)) {
		*m_showMenu = !*m_showMenu;
	}
	if (keyboard->wasPressed(Keyboard::kF8)) {
		m_glContext->clearTextureBindings();
		Texture::ReloadAll();
	}
	if (keyboard->wasPressed(Keyboard::kF9)) {
		m_glContext->setShader(0);
		Shader::ReloadAll();
	}

	if (ImGui::IsKeyPressed(Keyboard::kP) && ImGui::IsKeyDown(Keyboard::kLCtrl)) {
		*m_showPropertyEditor = !*m_showPropertyEditor;
	}
	if (ImGui::IsKeyPressed(Keyboard::k1) && ImGui::IsKeyDown(Keyboard::kLCtrl)) {
		*m_showProfilerViewer = !*m_showProfilerViewer;
	}
	if (ImGui::IsKeyPressed(Keyboard::k2) && ImGui::IsKeyDown(Keyboard::kLCtrl)) {
		*m_showTextureViewer = !*m_showTextureViewer;
	}
	
 // AppSample UI
	const float kStatusBarHeight = ImGui::GetItemsLineHeightWithSpacing();// + 4.0f;
	const ImGuiWindowFlags kStatusBarFlags = 
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus
			;
	ImGuiIO& io = ImGui::GetIO();
	if (*m_showMenu) {
	 // status bar
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.0f, 2.0f));
		ImGui::SetNextWindowPos(ImVec2(0.0f, io.DisplaySize.y - kStatusBarHeight));
		ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, kStatusBarHeight));
		ImGui::Begin("Status Bar", 0, kStatusBarFlags);
			ImGui::AlignFirstTextHeightToWidgets();
			
			float cpuAvgFrameDuration = (float)Timestamp(Profiler::GetCpuAvgFrameDuration()).asMilliseconds();
			float gpuAvgFrameDuration = (float)Timestamp(Profiler::GetGpuAvgFrameDuration()).asMilliseconds();
			ImGui::Text("CPU %-4.2fms GPU %-4.2fms", cpuAvgFrameDuration, gpuAvgFrameDuration);
			*m_showProfilerViewer = ImGui::IsItemClicked() ? !*m_showProfilerViewer : *m_showProfilerViewer;
			
			ImGui::SameLine();
			float cursorPosX = ImGui::GetCursorPosX();
			float logPosX = ImGui::GetContentRegionMax().x - ImGui::GetContentRegionAvailWidth() * 0.3f;
			const ui::Log::Message* logMsg = s_log.getLastErr();
			if (!logMsg) {
				logMsg = s_log.getLastDbg();
			}
			if (!logMsg) {
				logMsg = s_log.getLastLog();
			}
			ImGui::SetCursorPosX(logPosX);
			ImGui::TextColored(ImColor(logMsg->m_col), logMsg->m_txt);
			*m_showLog = ImGui::IsItemClicked() ? !*m_showLog : *m_showLog;
			
			ImGui::SameLine();
			ImGui::SetCursorPosX(cursorPosX);

		 // draw app items
			drawStatusBar();

		ImGui::End();
		ImGui::PopStyleVar(2);

		if (*m_showLog) {
			float logPosY = io.DisplaySize.y * 0.7f;
			ImGui::SetNextWindowPos(ImVec2(logPosX, logPosY));
			ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - logPosX, io.DisplaySize.y - logPosY - kStatusBarHeight));
			ImGui::Begin("Log", 0, 
				ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoSavedSettings
				);
			s_log.draw();
			ImGui::End();
		}
	 // main menu
		if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("Tools")) {
				ImGui::MenuItem("Properties",     "Ctrl+Shift+P", m_showPropertyEditor);
				ImGui::MenuItem("Profiler",       "Ctrl+1",       m_showProfilerViewer);
				ImGui::MenuItem("Texture Viewer", "Ctrl+2",       m_showTextureViewer);
				ImGui::MenuItem("Shader Viewer",  0,              m_showShaderViewer);
				
				ImGui::EndMenu();
			}
			float vsyncWidth = (float)sizeof("Adaptive") * ImGui::GetFontSize();
			ImGui::PushItemWidth(vsyncWidth);
			float cursorX = ImGui::GetCursorPosX();
			ImGui::SetCursorPosX(ImGui::GetContentRegionAvailWidth() - vsyncWidth);
			if (ImGui::Combo("VSYNC", m_vsyncMode, "Adaptive\0Off\0On\0On1\0On2\0On3\0")) {
				getGlContext()->setVsyncMode((GlContext::VsyncMode)(*m_vsyncMode - 1));
			}
			ImGui::PopItemWidth();
			ImGui::SameLine();
			ImGui::SetCursorPosX(cursorX);
			
		 // draw app items
			drawMainMenuBar();
			
			ImGui::EndMainMenuBar();
		}
	}

	if (*m_showPropertyEditor) {
		m_properties.edit("Properties");
	}
	if (*m_showProfilerViewer) {
		s_profilerViewer.draw(m_showProfilerViewer);
	}
	if (*m_showTextureViewer) {
		s_textureViewer.draw(m_showTextureViewer);
	}
	if (*m_showShaderViewer) {
		s_shaderViewer.draw(m_showShaderViewer);
	}
	

	return true;
}

void AppSample::draw()
{
	m_glContext->setFramebufferAndViewport(m_fbDefault);
	ImGui::GetIO().UserData = m_glContext;
	ImGui::Render();
	{	AUTO_MARKER("#GlContext::present");
		m_glContext->setFramebufferAndViewport(0); // this is required if you want to use e.g. fraps
		m_glContext->present();
	}
	++m_frameIndex;
}

void AppSample::drawNdcQuad()
{
	m_glContext->setMesh(m_msQuad);
	m_glContext->draw();
}


// PROTECTED

AppSample::AppSample(const char* _name, const char* _appDataPath)
	: App()
	, m_name(_name)
	, m_appDataPath(_appDataPath ? _appDataPath : _name)
	, m_window(0)
	, m_glContext(0)
	, m_frameIndex(0)
	, m_fbDefault(0)
	, m_msQuad(0)
	, m_showMenu(0)
	, m_showLog(0)
	, m_showPropertyEditor(0)
	, m_showProfilerViewer(0)
	, m_showTextureViewer(0)
	, m_showShaderViewer(0)
{
	APT_ASSERT(s_current == 0); // don't support multiple apps (yet)
	s_current = this;

	AppPropertyGroup& props = m_properties.addGroup("AppSample");
	//                                    name                   display name                     default              min    max    hidden
	                       props.addVec2i("Resolution",          "Resolution",                    ivec2(-1, -1),       1,     8192,  false);
	                       props.addVec2i("WindowSize",          "Window Size",                   ivec2(-1, -1),      -1,     8192,  true);
	                       props.addVec2i("GlVersion",           "OpenGL Version",                ivec2(4, 5),         1,     5,     true);
	                       props.addBool ("GlCompatibility",     "OpenGL Compatibility Profile",  false,                             true);
	m_vsyncMode          = props.addInt  ("VsyncMode",           "Vsync Mode",                    0,                   0,     5,     true);
	m_showMenu           = props.addBool ("ShowMenu",            "Menu",                          true,                              true);
	m_showLog            = props.addBool ("ShowLog",             "Log",                           false,                             true);
	m_showPropertyEditor = props.addBool ("ShowPropertyEditor",  "Property Editor",               false,                             true);
	m_showProfilerViewer = props.addBool ("ShowProfiler",        "Profiler",                      false,                             true);
	m_showTextureViewer  = props.addBool ("ShowTextureViewer",   "Texture Viewer",                false,                             true);
	m_showShaderViewer   = props.addBool ("ShowShaderViewer",    "Shader Viewer",                 false,                             true);

}

AppSample::~AppSample()
{
	//shutdown(); \todo it's not safe to call shutdown() twice
}


// PRIVATE

/*******************************************************************************

                                   ImGui

*******************************************************************************/

static Shader*     s_shImGui;
static Shader*     s_shTextureView[frm::internal::kTextureTargetCount]; // shader per texture type
static Mesh*       s_msImGui;
static Texture*    s_txImGui;
static TextureView s_txViewImGui; // default texture view for the ImGui texture

	

bool AppSample::ImGui_Init()
{
	ImGuiIO& io = ImGui::GetIO();
	
 // mesh
 	if (s_msImGui) {
		Mesh::Destroy(s_msImGui);
	}	
	MeshDesc meshDesc(MeshDesc::kTriangles);
	meshDesc.addVertexAttr(VertexAttr::kPositions, 2, DataType::kFloat32);
	meshDesc.addVertexAttr(VertexAttr::kTexcoords, 2, DataType::kFloat32);
	meshDesc.addVertexAttr(VertexAttr::kColors,    4, DataType::kUint8N);
	APT_ASSERT(meshDesc.getVertexSize() == sizeof(ImDrawVert));
	s_msImGui = Mesh::Create(meshDesc);

 // shaders
	if (s_shImGui) {
		Shader::Destroy(s_shImGui);
	}
	APT_VERIFY(s_shImGui = Shader::CreateVsFs("shaders/ImGui_vs.glsl", "shaders/ImGui_fs.glsl"));
	s_shImGui->setName("#ImGui");

	ShaderDesc desc;
	desc.setPath(GL_VERTEX_SHADER,   "shaders/ImGui_vs.glsl");
	desc.setPath(GL_FRAGMENT_SHADER, "shaders/TextureView_fs.glsl");
	for (int i = 0; i < internal::kTextureTargetCount; ++i) {
		desc.clearDefines();
		desc.addDefine(GL_FRAGMENT_SHADER, internal::GlEnumStr(internal::kTextureTargets[i]) + 3); // \hack +3 removes the 'GL_', which is reserved in the shader
		APT_VERIFY(s_shTextureView[i] = Shader::Create(desc));
		s_shTextureView[i]->setNamef("#TextureViewer_%s", internal::GlEnumStr(internal::kTextureTargets[i]) + 3);
	}

 // fonts texture
	if (s_txImGui) {
		Texture::Destroy(s_txImGui);
	}
	unsigned char* buf;
	int txX, txY;
	//ImFontConfig fontCfg;
	//fontCfg.OversampleH = fontCfg.OversampleV = 3;
	//io.Fonts->AddFontFromFileTTF("DroidSans.ttf", 14.0f, &fontCfg);
	io.Fonts->GetTexDataAsAlpha8(&buf, &txX, &txY);
	s_txImGui = Texture::Create2d(txX, txY, GL_R8);
	s_txImGui->setName("#ImGuiFont");
	s_txImGui->setData(buf, GL_RED, GL_UNSIGNED_BYTE);
	s_txViewImGui = TextureView(s_txImGui);
	io.Fonts->TexID = (void*)&s_txViewImGui;

	
 // init ImGui state
	io.KeyMap[ImGuiKey_Tab]        = Keyboard::kTab;
    io.KeyMap[ImGuiKey_LeftArrow]  = Keyboard::kLeft;
    io.KeyMap[ImGuiKey_RightArrow] = Keyboard::kRight;
    io.KeyMap[ImGuiKey_UpArrow]	   = Keyboard::kUp;
    io.KeyMap[ImGuiKey_DownArrow]  = Keyboard::kDown;
	io.KeyMap[ImGuiKey_PageUp]	   = Keyboard::kPageUp;
    io.KeyMap[ImGuiKey_PageDown]   = Keyboard::kPageDown;
    io.KeyMap[ImGuiKey_Home]	   = Keyboard::kHome;
    io.KeyMap[ImGuiKey_End]		   = Keyboard::kEnd;
    io.KeyMap[ImGuiKey_Delete]	   = Keyboard::kDelete;
	io.KeyMap[ImGuiKey_Backspace]  = Keyboard::kBackspace;
    io.KeyMap[ImGuiKey_Enter]	   = Keyboard::kReturn;
	io.KeyMap[ImGuiKey_Escape]	   = Keyboard::kEscape;
    io.KeyMap[ImGuiKey_A]		   = Keyboard::kA;
    io.KeyMap[ImGuiKey_C]		   = Keyboard::kC;
    io.KeyMap[ImGuiKey_V]		   = Keyboard::kV;
    io.KeyMap[ImGuiKey_X]		   = Keyboard::kX;
    io.KeyMap[ImGuiKey_Y]		   = Keyboard::kY;
    io.KeyMap[ImGuiKey_Z]		   = Keyboard::kZ;
	io.DisplayFramebufferScale     = ImVec2(1.0f, 1.0f);
	io.RenderDrawListsFn           = ImGui_RenderDrawLists;
	io.IniSavingRate               = -1.0f; // never save automatically

	return true;
}

void AppSample::ImGui_Shutdown()
{
	for (int i = 0; i < internal::kTextureTargetCount; ++i) {
		Shader::Destroy(s_shTextureView[i]);
	}
	if (s_shImGui) Shader::Destroy(s_shImGui);
	if (s_msImGui) Mesh::Destroy(s_msImGui); 
	if (s_txImGui) Texture::Destroy(s_txImGui);
	
	ImGui::Shutdown();
}

void AppSample::ImGui_Update(AppSample* _app)
{
 // consume keybaord/mouse input
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureKeyboard) {
		Input::ResetKeyboard();
	}
	if (io.WantCaptureMouse) {
		Input::ResetMouse();
	}


	io.ImeWindowHandle = _app->getWindow()->getHandle();
	if (_app->getDefaultFramebuffer()) {
		io.DisplaySize = ImVec2((float)_app->getDefaultFramebuffer()->getWidth(), (float)_app->getDefaultFramebuffer()->getHeight());
	} else {
		io.DisplaySize = ImVec2((float)_app->getWindow()->getWidth(), (float)_app->getWindow()->getHeight());
	}
	io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
	io.DeltaTime = (float)_app->m_deltaTime;
	ImGui::NewFrame(); // must call after m_window->pollEvents()

}

void AppSample::ImGui_RenderDrawLists(ImDrawData* _drawData)
{
	AUTO_MARKER("ImGui::Render");

	ImGuiIO& io = ImGui::GetIO();
	GlContext* ctx = (GlContext*)io.UserData;

	if (_drawData->CmdListsCount == 0) {
		return;
	}	
	int fbX = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
    int fbY = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
    if (fbX == 0 || fbY == 0) {
        return;
	}
    _drawData->ScaleClipRects(io.DisplayFramebufferScale);

    glAssert(glEnable(GL_BLEND));
    glAssert(glBlendEquation(GL_FUNC_ADD));
    glAssert(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    glAssert(glDisable(GL_CULL_FACE));
    glAssert(glDisable(GL_DEPTH_TEST));
    glAssert(glEnable(GL_SCISSOR_TEST));
    glAssert(glActiveTexture(GL_TEXTURE0));

	glAssert(glViewport(0, 0, (GLsizei)fbX, (GLsizei)fbY));
	mat4 ortho = mat4(
		 2.0f/io.DisplaySize.x, 0.0f,                   0.0f, 0.0f,
		 0.0f,                  2.0f/-io.DisplaySize.y, 0.0f, 0.0f,
		 0.0f,                  0.0f,                  -1.0f, 0.0f,
		-1.0f,                  1.0f,                   0.0f, 1.0f
		);
	ctx->setMesh(s_msImGui);

	for (int i = 0; i < _drawData->CmdListsCount; ++i) {
		const ImDrawList* drawList = _drawData->CmdLists[i];
		uint indexOffset = 0;

	 // upload vertex/index data
		s_msImGui->setVertexData((GLvoid*)&drawList->VtxBuffer.front(), (GLsizeiptr)drawList->VtxBuffer.size(), GL_STREAM_DRAW);
		APT_STATIC_ASSERT(sizeof(ImDrawIdx) == sizeof(uint16)); // need to change the index data type if this fails
		s_msImGui->setIndexData(DataType::kUint16, (GLvoid*)&drawList->IdxBuffer.front(), (GLsizeiptr)drawList->IdxBuffer.size(), GL_STREAM_DRAW);
	
	 // dispatch draw commands
		for (const ImDrawCmd* pcmd = drawList->CmdBuffer.begin(); pcmd != drawList->CmdBuffer.end(); ++pcmd) {
			if (pcmd->UserCallback) {
				pcmd->UserCallback(drawList, pcmd);
			} else {
				TextureView* txView = (TextureView*)pcmd->TextureId;
				const Texture* tx = txView->m_texture;
				Shader* sh = s_shImGui;
				if (txView != &s_txViewImGui) {
				 // select a shader based on the texture type
					sh = s_shTextureView[internal::TextureTargetToIndex(tx->getTarget())];
				}
				ctx->setShader(sh);
				ctx->setUniform ("uProjMatrix", ortho);
				ctx->setUniform ("uBiasUv",     txView->getNormalizedOffset());
				ctx->setUniform ("uScaleUv",    txView->getNormalizedSize());
				ctx->setUniform ("uLayer",      (float)txView->m_array);
				ctx->setUniform ("uMip",        (float)txView->m_mip);
				ctx->setUniform ("uRgbaMask",   uvec4(txView->m_rgbaMask[0], txView->m_rgbaMask[1], txView->m_rgbaMask[2], txView->m_rgbaMask[3]));
				ctx->bindTexture("txTexture",   txView->m_texture);

                glAssert(glScissor((int)pcmd->ClipRect.x, (int)(fbY - pcmd->ClipRect.w), (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y)));
				glAssert(glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, GL_UNSIGNED_SHORT, (GLvoid*)indexOffset));
			}
			indexOffset += pcmd->ElemCount * sizeof(ImDrawIdx);
		}
	}

	glAssert(glDisable(GL_SCISSOR_TEST));
	glAssert(glDisable(GL_BLEND));
	ctx->setShader(0);
}

bool AppSample::ImGui_OnMouseButton(Window* _window, unsigned _button, bool _isDown)
{
	ImGuiIO& io = ImGui::GetIO();
	APT_ASSERT(_button < APT_ARRAY_COUNT(io.MouseDown)); // button index out of bounds
	switch ((Mouse::Button)_button) {
		case Mouse::kLeft:    io.MouseDown[0] = _isDown; break;
		case Mouse::kRight:   io.MouseDown[1] = _isDown; break;
		case Mouse::kMiddle:  io.MouseDown[2] = _isDown; break;
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
	APT_ASSERT(_key < APT_ARRAY_COUNT(io.KeysDown)); // key index out of bounds
	io.KeysDown[_key] = _isDown;

	// handle modifiers
	switch ((Keyboard::Button)_key) {
		case Keyboard::kLCtrl:
		case Keyboard::kRCtrl:
			io.KeyCtrl = _isDown;
			break;
		case Keyboard::kLShift:
		case Keyboard::kRShift:
			io.KeyShift = _isDown;
			break;			
		case Keyboard::kLAlt:
		case Keyboard::kRAlt:
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
	if (_char > 0 && _char < 0x10000) {
		io.AddInputCharacter((unsigned short)_char);
	}
	return true;
}
