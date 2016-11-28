#include <frm/AppSample3d.h>

#include <frm/def.h>
#include <frm/gl.h>
#include <frm/geom.h>
#include <frm/GlContext.h>
#include <frm/Mesh.h>
#include <frm/MeshData.h>
#include <frm/Profiler.h>
#include <frm/Shader.h>
#include <frm/Scene.h>
#include <frm/Window.h>
#include <frm/XForm.h>

#include <frm/im3d.h>


using namespace frm;
using namespace apt;

// \hack we override some callbacks for Im3d, hence need to do call them manually
// \todo allow multiple callbacks on Window
static Window::Callbacks s_appSampleCallbacks;

// PUBLIC

bool AppSample3d::init(const apt::ArgList& _args)
{
	if (!AppSample::init(_args)) {
		return false;
	}

	
	Im3d::SetCurrentContext(m_im3dCtx);
	if (!Im3d_Init()) {
		return false;
	}

 // set Im3d callbacks
	s_appSampleCallbacks = getWindow()->getCallbacks();
	Window::Callbacks cb = s_appSampleCallbacks;
	cb.m_OnMouseButton = Im3d_OnMouseButton;
	cb.m_OnKey = Im3d_OnKey;
	getWindow()->setCallbacks(cb);


 // create default scene root
	Node* defaultScene = m_scene.createNode("SceneDefault", Node::kTypeRoot);
	m_sceneRoots.push_back(defaultScene);
	m_currentScene = 0;

 // create default camera
	Camera* defaultCamera = m_scene.createCamera(Camera(), defaultScene);
	Node* defaultCameraNode = defaultCamera->getNode();
	defaultCameraNode->setSelected(true);
	XForm* freeCam = XForm::Create("FreeCameraXForm");
	((FreeCameraXForm*)freeCam)->m_position = vec3(0.0f, 5.0f, 22.5f);
	defaultCameraNode->addXForm(freeCam);

	return true;
}

void AppSample3d::shutdown()
{
	Im3d_Shutdown();
	AppSample::shutdown();
}

bool AppSample3d::update()
{
	if (!AppSample::update()) {
		return false;
	}
	Im3d::SetCurrentContext(m_im3dCtx);
	Im3d_Update(this);

	m_scene.update((float)m_deltaTime, Node::kStateActive | Node::kStateDynamic);
	#ifdef frm_Scene_ENABLE_EDIT
		if (*m_showSceneEditor) {
			m_sceneEditor.edit();
		}
	#endif

	Camera* currentCamera = m_scene.getDrawCamera();
	if (currentCamera->isSymmetric()) {
	 // update aspect ratio to match window size
		Window* win = getWindow();
		int winX = win->getWidth();
		int winY = win->getHeight();
		if (winX != 0 && winY != 0) {
			float aspect = (float)winX / (float)winY;
			if (currentCamera->getAspect() != aspect) {
				currentCamera->setAspect(aspect);
			}
		}
	}

 // keyboard shortcuts
	Keyboard* keyb = Input::GetKeyboard();
	if (keyb->wasPressed(Keyboard::kF2)) {
		*m_showHelpers = !*m_showHelpers;
		m_sceneEditor.m_showCameras = !m_sceneEditor.m_showCameras;
	}
	if (ImGui::IsKeyPressed(Keyboard::kO) && ImGui::IsKeyDown(Keyboard::kLCtrl)) {
		*m_showSceneEditor = !*m_showSceneEditor;
	}
	if (ImGui::IsKeyPressed(Keyboard::kC) && ImGui::IsKeyDown(Keyboard::kLCtrl) && ImGui::IsKeyDown(Keyboard::kLShift)) {
		if (m_dbgCullCamera) {
			m_scene.destroyCamera(m_dbgCullCamera);
			m_scene.setCullCamera(m_scene.getDrawCamera());
		} else {
			m_dbgCullCamera = m_scene.createCamera(*m_scene.getCullCamera());
			Node* node = m_dbgCullCamera->getNode();
			node->setName("DEBUG CULL CAMERA");
			node->setDynamic(false);
			node->setActive(false);
			node->setLocalMatrix(m_scene.getCullCamera()->getWorldMatrix());
			m_scene.setCullCamera(m_dbgCullCamera);
		}
	}

	if (*m_showHelpers) {
		const int   kGridSize = 20;
		const float kGridHalf = (float)kGridSize * 0.5f;
		Im3d::SetSize(1.0f);
		Im3d::BeginLines();
			for (int x = 0; x <= kGridSize; ++x) {
				Im3d::Vertex(-kGridHalf, 0.0f, (float)x - kGridHalf,  Im3d::Color(0.0f, 0.0f, 0.0f));
				Im3d::Vertex( kGridHalf, 0.0f, (float)x - kGridHalf,  Im3d::Color(1.0f, 0.0f, 0.0f));
			}
			for (int z = 0; z <= kGridSize; ++z) {
				Im3d::Vertex((float)z - kGridHalf, 0.0f, -kGridHalf,  Im3d::Color(0.0f, 0.0f, 0.0f));
				Im3d::Vertex((float)z - kGridHalf, 0.0f,  kGridHalf,  Im3d::Color(0.0f, 0.0f, 1.0f));
			}
		Im3d::End();
	}

	return true;
}

void AppSample3d::drawMainMenuBar()
{
	if (ImGui::BeginMenu("Scene")) {
		ImGui::MenuItem("Scene Editor",      "Ctrl+O",       m_showSceneEditor);
		ImGui::MenuItem("Show Helpers",      "F2",           m_showHelpers);
		ImGui::MenuItem("Pause Cull Camera", "Ctrl+Shift+C", m_showSceneEditor);

		ImGui::EndMenu();
	}
}

void AppSample3d::drawStatusBar()
{
}

void AppSample3d::draw()
{
	getGlContext()->setFramebufferAndViewport(getDefaultFramebuffer());
	Im3d_Render(m_im3dCtx, *m_scene.getDrawCamera());

	AppSample::draw();
}

Ray AppSample3d::getCursorRayW() const
{
	Ray ret = getCursorRayV();
	ret.transform(m_scene.getDrawCamera()->getWorldMatrix());
	return ret;
}

Ray AppSample3d::getCursorRayV() const
{
	int mx, my;
	getWindow()->getWindowRelativeCursor(&mx, &my);
	vec2 mpos  = vec2((float)mx, (float)my);
	vec2 wsize = vec2((float)getWindow()->getWidth(), (float)getWindow()->getHeight());
	mpos = (mpos / wsize) * 2.0f - 1.0f;
	mpos.y = -mpos.y; // the cursor position is top-left relative
	float tanHalfFov = m_scene.getDrawCamera()->getTanFovUp();
	float aspect = m_scene.getDrawCamera()->getAspect();
	return Ray(vec3(0.0f), normalize(vec3(mpos.x * tanHalfFov * aspect, mpos.y * tanHalfFov, -1.0f)));
	
	//return Ray(vec3(0.0f), vec3(0.0f));
}

// PROTECTED

AppSample3d::AppSample3d(const char* _title, const char* _appDataPath)
	: AppSample(_title, _appDataPath)
	, m_showHelpers(0)
	, m_showSceneEditor(0)
	, m_dbgCullCamera(0)
#ifdef frm_Scene_ENABLE_EDIT
	, m_sceneEditor(&m_scene)
#endif
{

	AppPropertyGroup& props = m_properties.addGroup("AppSample3d");
	//                                name                   display name                     default              min    max    hidden
	m_showHelpers     = props.addBool("ShowHelpers",         "Helpers",                       true,                              true);
	m_showSceneEditor = props.addBool("ShowSceneEditor",     "Scene Editor",                  false,                             true);
}

AppSample3d::~AppSample3d()
{
	m_sceneRoots.clear();
}

// PRIVATE


/*******************************************************************************

                                   Im3d

*******************************************************************************/

static Shader *s_shIm3dPoints, *s_shIm3dLines, *s_shIm3dTriangles;
static Mesh   *s_msIm3dPoints, *s_msIm3dLines, *s_msIm3dTriangles;

bool AppSample3d::Im3d_Init()
{
 // io
	Im3d::Context& im3d = Im3d::GetCurrentContext();
	im3d.m_keyMap[Im3d::Context::kMouseLeft]   = 0;
	im3d.m_keyMap[Im3d::Context::kMouseRight]  = 1;
	im3d.m_keyMap[Im3d::Context::kMouseMiddle] = 2;
	im3d.m_keyMap[Im3d::Context::kKeyCtrl]     = Keyboard::kLCtrl + 3;
	im3d.m_keyMap[Im3d::Context::kKeyShift]    = Keyboard::kLShift + 3;
	im3d.m_keyMap[Im3d::Context::kKeyAlt]      = Keyboard::kLShift + 3;
	im3d.m_keyMap[Im3d::Context::kKeyT]        = Keyboard::kT + 3;
	im3d.m_keyMap[Im3d::Context::kKeyR]        = Keyboard::kR + 3;
	im3d.m_keyMap[Im3d::Context::kKeyS]        = Keyboard::kS + 3;

	
 // render resources
	ShaderDesc sd;
	sd.setPath(GL_VERTEX_SHADER,   "shaders/Im3d_vs.glsl");
	sd.setPath(GL_FRAGMENT_SHADER, "shaders/Im3d_fs.glsl");
	sd.addGlobalDefine("POINTS");
	APT_VERIFY(s_shIm3dPoints = Shader::Create(sd));
	s_shIm3dPoints->setName("#Im3d_POINTS");

	sd.clearDefines();
	sd.addGlobalDefine("TRIANGLES");
	APT_VERIFY(s_shIm3dTriangles = Shader::Create(sd));
	s_shIm3dTriangles->setName("#Im3d_TRIANGLES");

	sd.clearDefines();
	sd.setPath(GL_GEOMETRY_SHADER, "shaders/Im3d_gs.glsl");
	sd.addGlobalDefine("LINES");
	APT_VERIFY(s_shIm3dLines = Shader::Create(sd));
	s_shIm3dLines->setName("#Im3d_LINES");

	MeshDesc meshDesc(MeshDesc::kPoints);
	meshDesc.addVertexAttr(VertexAttr::kPositions, 4, DataType::kFloat32);
	meshDesc.addVertexAttr(VertexAttr::kColors,    4, DataType::kUint8N);
	APT_ASSERT(meshDesc.getVertexSize() == sizeof(struct Im3d::Vertex));
	s_msIm3dPoints = Mesh::Create(meshDesc);

	meshDesc.setPrimitive(MeshDesc::kLines);
	s_msIm3dLines= Mesh::Create(meshDesc);

	meshDesc.setPrimitive(MeshDesc::kTriangles);
	s_msIm3dTriangles= Mesh::Create(meshDesc);

	return s_shIm3dPoints && s_msIm3dPoints;
}

void AppSample3d::Im3d_Shutdown()
{
	if (s_shIm3dPoints)     Shader::Destroy(s_shIm3dPoints);
	if (s_shIm3dLines)      Shader::Destroy(s_shIm3dLines);
	if (s_shIm3dTriangles)  Shader::Destroy(s_shIm3dTriangles);
	if (s_msIm3dPoints)     Mesh::Destroy(s_msIm3dPoints);
	if (s_msIm3dLines)      Mesh::Destroy(s_msIm3dLines);
	if (s_msIm3dTriangles)  Mesh::Destroy(s_msIm3dTriangles);
}

void AppSample3d::Im3d_Update(AppSample3d* _app)
{
	Im3d::Context& im3d = Im3d::GetCurrentContext();

	Ray cursorRayW = _app->getCursorRayW();
	im3d.m_cursorRayOriginW = cursorRayW.m_origin;
	im3d.m_cursorRayDirectionW = cursorRayW.m_direction;
	im3d.m_deltaTime = (float)_app->getDeltaTime();
	im3d.m_tanHalfFov = _app->m_scene.getDrawCamera()->getTanFovUp();
	im3d.m_viewOriginW = _app->m_scene.getDrawCamera()->getPosition();
	im3d.m_displaySize = Im3d::Vec2((float)_app->getWindow()->getWidth(), (float)_app->getWindow()->getHeight());

	im3d.reset();
}

void AppSample3d::Im3d_Render(Im3d::Context& _im3dCtx, const Camera& _cam, bool _depthTest)
{
	AUTO_MARKER("Im3d::Render");

	GlContext* ctx = GlContext::GetCurrent();
	Im3d::Context& im3dCtx = _im3dCtx;

	glAssert(glEnable(GL_BLEND));
    glAssert(glBlendEquation(GL_FUNC_ADD));
    glAssert(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    glAssert(glDisable(GL_CULL_FACE));
	if (_depthTest) {
		glAssert(glEnable(GL_DEPTH_TEST));
	}
    
	vec2 viewport = vec2(ctx->getViewportWidth(), ctx->getViewportHeight());

 // triangles
	s_msIm3dTriangles->setVertexData(im3dCtx.getTriangleData(), im3dCtx.getTriangleCount(), GL_STREAM_DRAW);
	ctx->setShader(s_shIm3dTriangles);
	ctx->setUniform("uViewProjMatrix", _cam.getViewProjMatrix());
	ctx->setUniform("uViewport", viewport);
	ctx->setMesh(s_msIm3dTriangles);
	ctx->draw();

 // lines
	s_msIm3dLines->setVertexData(im3dCtx.getLineData(), im3dCtx.getLineCount(), GL_STREAM_DRAW);
	ctx->setShader(s_shIm3dLines);
	ctx->setUniform("uViewProjMatrix", _cam.getViewProjMatrix());
	ctx->setUniform("uViewport", viewport);
	ctx->setMesh(s_msIm3dLines);
	ctx->draw();

 // points
	glAssert(glEnable(GL_PROGRAM_POINT_SIZE));
	s_msIm3dPoints->setVertexData(im3dCtx.getPointData(), im3dCtx.getPointCount(), GL_STREAM_DRAW);
	ctx->setShader(s_shIm3dPoints);
	ctx->setUniform("uViewProjMatrix", _cam.getViewProjMatrix());
	ctx->setUniform("uViewport", viewport);
	ctx->setMesh(s_msIm3dPoints);
	ctx->draw();
	glAssert(glDisable(GL_PROGRAM_POINT_SIZE));

	if (_depthTest) {
		glAssert(glDisable(GL_DEPTH_TEST));
	}
	glAssert(glDisable(GL_BLEND));
}

bool AppSample3d::Im3d_OnMouseButton(Window* _window, unsigned _button, bool _isDown)
{
	Im3d::Context& im3d = Im3d::GetCurrentContext();

	APT_ASSERT(_button < 3); // button index out of bounds
	switch ((Mouse::Button)_button) {
		case Mouse::kLeft:    im3d.m_keyDown[0] = _isDown; break;
		case Mouse::kRight:   im3d.m_keyDown[1] = _isDown; break;
		case Mouse::kMiddle:  im3d.m_keyDown[2] = _isDown; break;
		default: break;
	};
	
	return s_appSampleCallbacks.m_OnMouseButton(_window, _button, _isDown);
}

bool AppSample3d::Im3d_OnKey(Window* _window, unsigned _key, bool _isDown)
{
	Im3d::Context& im3d = Im3d::GetCurrentContext();
	unsigned key = _key + 3; // reserve mouse buttons
	APT_ASSERT(key < APT_ARRAY_COUNT(im3d.m_keyDown)); // key index out of bounds
	im3d.m_keyDown[key] = _isDown;

	return s_appSampleCallbacks.m_OnKey(_window, _key, _isDown);
}
