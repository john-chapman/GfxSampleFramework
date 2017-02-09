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

	if (!Scene::Load(m_scenePath, Scene::GetCurrent())) {
 		Camera* defaultCamera = Scene::GetCurrent().createCamera(Camera());
		Node* defaultCameraNode = defaultCamera->getNode();
		defaultCameraNode->setStateMask(Node::kStateActive | Node::kStateDynamic | Node::kStateSelected);
		XForm* freeCam = XForm::Create("XForm_FreeCamera");
		((XForm_FreeCamera*)freeCam)->m_position = vec3(0.0f, 5.0f, 22.5f);
		defaultCameraNode->addXForm(freeCam);
	}

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

	Scene& scene = Scene::GetCurrent();
	scene.update((float)m_deltaTime, Node::kStateActive | Node::kStateDynamic);
	#ifdef frm_Scene_ENABLE_EDIT
		if (*m_showSceneEditor) {
			Scene::GetCurrent().edit();
		}
	#endif

	Camera* currentCamera = scene.getDrawCamera();
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
	}
	if (ImGui::IsKeyPressed(Keyboard::kO) && ImGui::IsKeyDown(Keyboard::kLCtrl)) {
		*m_showSceneEditor = !*m_showSceneEditor;
	}
	if (ImGui::IsKeyPressed(Keyboard::kC) && ImGui::IsKeyDown(Keyboard::kLCtrl) && ImGui::IsKeyDown(Keyboard::kLShift)) {
		if (m_dbgCullCamera) {
			scene.destroyCamera(m_dbgCullCamera);
			scene.setCullCamera(scene.getDrawCamera());
		} else {
			m_dbgCullCamera = scene.createCamera(*scene.getCullCamera());
			Node* node = m_dbgCullCamera->getNode();
			node->setName("#DEBUG CULL CAMERA");
			node->setDynamic(false);
			node->setActive(false);
			node->setLocalMatrix(scene.getCullCamera()->getWorldMatrix());
			scene.setCullCamera(m_dbgCullCamera);
		}
	}

	if (*m_showHelpers) {
		const int   kGridSize = 20;
		const float kGridHalf = (float)kGridSize * 0.5f;
		Im3d::PushDrawState();
			Im3d::SetAlpha(1.0f);
			Im3d::SetSize(1.0f);

		 // origin XZ grid
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

		 // scene cameras
			for (int i = 0; i < scene.getCameraCount(); ++i) {
				Camera* camera = scene.getCamera(i);
				if (camera == scene.getDrawCamera()) {
					continue;
				}
				Im3d::PushMatrix();
					Im3d::MulMatrix(camera->getWorldMatrix());
					Im3d::DrawXyzAxes();
				Im3d::PopMatrix();
				DrawFrustum(camera->getWorldFrustum());
			}
		Im3d::PopDrawState();
	}

	return true;
}

void AppSample3d::drawMainMenuBar()
{
	if (ImGui::BeginMenu("Scene")) {
		if (ImGui::MenuItem("Load...")) {
			FileSystem::PathStr newPath;
			if (FileSystem::PlatformSelect(newPath, "*.json")) {
				FileSystem::MakeRelative(newPath);
				if (Scene::Load(newPath, Scene::GetCurrent())) {
					strncpy(m_scenePath, newPath, AppProperty::kMaxStringLength);
				}
			}
		}
		if (ImGui::MenuItem("Save")) {
			Scene::Save(m_scenePath, Scene::GetCurrent());
		}
		if (ImGui::MenuItem("Save As...")) {
			FileSystem::PathStr newPath = m_scenePath;
			if (FileSystem::PlatformSelect(newPath, "*.json")) {
				FileSystem::MakeRelative(newPath);
				if (Scene::Save(newPath, Scene::GetCurrent())) {
					strncpy(m_scenePath, newPath, AppProperty::kMaxStringLength);
				}
			}
		}

		ImGui::Separator();

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
	Im3d_Render(m_im3dCtx, *Scene::GetDrawCamera());

	AppSample::draw();
}

Ray AppSample3d::getCursorRayW() const
{
	Ray ret = getCursorRayV();
	ret.transform(Scene::GetDrawCamera()->getWorldMatrix());
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
	float tanHalfFov = Scene::GetDrawCamera()->getTanFovUp();
	float aspect = Scene::GetDrawCamera()->getAspect();
	return Ray(vec3(0.0f), normalize(vec3(mpos.x * tanHalfFov * aspect, mpos.y * tanHalfFov, -1.0f)));
	
	//return Ray(vec3(0.0f), vec3(0.0f));
}

// PROTECTED

void AppSample3d::DrawFrustum(const Frustum& _frustum)
{
	const vec3* verts = _frustum.m_vertices;

 // edges
	Im3d::SetColor(0.5f, 0.5f, 0.5f);
	Im3d::BeginLines();
		Im3d::Vertex(verts[0]); Im3d::Vertex(verts[4]);
		Im3d::Vertex(verts[1]); Im3d::Vertex(verts[5]);
		Im3d::Vertex(verts[2]); Im3d::Vertex(verts[6]);
		Im3d::Vertex(verts[3]); Im3d::Vertex(verts[7]);
	Im3d::End();

 // near plane
	Im3d::SetColor(1.0f, 1.0f, 0.25f);
	Im3d::BeginLineLoop();
		Im3d::Vertex(verts[0]); 
		Im3d::Vertex(verts[1]);
		Im3d::Vertex(verts[2]);
		Im3d::Vertex(verts[3]);
	Im3d::End();

 // far plane
	Im3d::SetColor(1.0f, 0.25f, 1.0f);
	Im3d::BeginLineLoop();
		Im3d::Vertex(verts[4]); 
		Im3d::Vertex(verts[5]);
		Im3d::Vertex(verts[6]);
		Im3d::Vertex(verts[7]);
	Im3d::End();
}

AppSample3d::AppSample3d(const char* _title)
	: AppSample(_title)
	, m_showHelpers(0)
	, m_showSceneEditor(0)
	, m_dbgCullCamera(0)
{

	AppPropertyGroup& props = m_properties.addGroup("AppSample3d");
	//                                name                   display name                   default              min    max    hidden
	m_showHelpers     = props.addBool("ShowHelpers",         "Helpers",                     true,                              true);
	m_showSceneEditor = props.addBool("ShowSceneEditor",     "Scene Editor",                false,                             true);
	m_scenePath       = props.addPath("ScenePath",           "Scene Path",                  "Scene.json",                      false);
}

AppSample3d::~AppSample3d()
{
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
	Shader::Release(s_shIm3dPoints);
	Shader::Release(s_shIm3dLines);
	Shader::Release(s_shIm3dTriangles);
	Mesh::Release(s_msIm3dPoints);
	Mesh::Release(s_msIm3dLines);
	Mesh::Release(s_msIm3dTriangles);
}

void AppSample3d::Im3d_Update(AppSample3d* _app)
{
	Im3d::Context& im3d = Im3d::GetCurrentContext();

	Ray cursorRayW = _app->getCursorRayW();
	im3d.m_cursorRayOriginW = cursorRayW.m_origin;
	im3d.m_cursorRayDirectionW = cursorRayW.m_direction;
	im3d.m_deltaTime = (float)_app->getDeltaTime();
	im3d.m_tanHalfFov = Scene::GetDrawCamera()->getTanFovUp();
	im3d.m_viewOriginW = Scene::GetDrawCamera()->getPosition();
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
