#include <frm/AppSample3d.h>

#include <frm/def.h>
#include <frm/gl.h>
#include <frm/geom.h>
#include <frm/GlContext.h>
#include <frm/Input.h>
#include <frm/Mesh.h>
#include <frm/MeshData.h>
#include <frm/Profiler.h>
#include <frm/Shader.h>
#include <frm/Scene.h>
#include <frm/Window.h>
#include <frm/XForm.h>

#include <im3d/im3d.h>

using namespace frm;
using namespace apt;

// PUBLIC

bool AppSample3d::init(const apt::ArgList& _args)
{
	if (!AppSample::init(_args)) {
		return false;
	}
	if (!Im3d_Init()) {
		return false;
	}

	if (!Scene::Load(m_scenePath, Scene::GetCurrent())) {
 		Camera* defaultCamera = Scene::GetCurrent().createCamera(Camera());
		Node* defaultCameraNode = defaultCamera->m_parent;
		defaultCameraNode->setStateMask(Node::State_Active | Node::State_Dynamic | Node::State_Selected);
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
	Im3d_Update(this);

	Scene& scene = Scene::GetCurrent();
	scene.update((float)m_deltaTime, Node::State_Active | Node::State_Dynamic);
	#ifdef frm_Scene_ENABLE_EDIT
		if (m_showSceneEditor) {
			Scene::GetCurrent().edit();
		}
	#endif

	Camera* currentCamera = scene.getDrawCamera();
	if (!currentCamera->getProjFlag(Camera::ProjFlag_Asymmetrical)) {
	 // update aspect ratio to match window size
		Window* win = getWindow();
		int winX = win->getWidth();
		int winY = win->getHeight();
		if (winX != 0 && winY != 0) {
			float aspect = (float)winX / (float)winY;
			if (currentCamera->m_aspectRatio != aspect) {
				currentCamera->setAspectRatio(aspect);
			}
		}
	}

 // keyboard shortcuts
	Keyboard* keyb = Input::GetKeyboard();
	if (keyb->wasPressed(Keyboard::Key_F2)) {
		m_showHelpers = !m_showHelpers;
	}
	if (ImGui::IsKeyPressed(Keyboard::Key_O) && ImGui::IsKeyDown(Keyboard::Key_LCtrl)) {
		m_showSceneEditor = !m_showSceneEditor;
	}
	if (ImGui::IsKeyPressed(Keyboard::Key_C) && ImGui::IsKeyDown(Keyboard::Key_LCtrl) && ImGui::IsKeyDown(Keyboard::Key_LShift)) {
		if (m_dbgCullCamera) {
			scene.destroyCamera(m_dbgCullCamera);
			scene.setCullCamera(scene.getDrawCamera());
		} else {
			m_dbgCullCamera = scene.createCamera(*scene.getCullCamera());
			Node* node = m_dbgCullCamera->m_parent;
			node->setName("#DEBUG CULL CAMERA");
			node->setDynamic(false);
			node->setActive(false);
			node->setLocalMatrix(scene.getCullCamera()->m_world);
			scene.setCullCamera(m_dbgCullCamera);
		}
	}

	if (m_showHelpers) {
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
					Im3d::MulMatrix(camera->m_world);
					Im3d::DrawXyzAxes();
				Im3d::PopMatrix();
				DrawFrustum(camera->m_worldFrustum);
			}
		Im3d::PopDrawState();
	}

	return true;
}

void AppSample3d::drawMainMenuBar()
{
	if (ImGui::BeginMenu("Scene")) {
		if (ImGui::MenuItem("Load...")) {
			if (FileSystem::PlatformSelect(m_scenePath, "*.json")) {
				FileSystem::MakeRelative(m_scenePath);
				Scene::Load(m_scenePath, Scene::GetCurrent());
			}
		}
		if (ImGui::MenuItem("Save")) {
			Scene::Save(m_scenePath, Scene::GetCurrent());
		}
		if (ImGui::MenuItem("Save As...")) {
			if (FileSystem::PlatformSelect(m_scenePath, "*.json")) {
				FileSystem::MakeRelative(m_scenePath);
				Scene::Save(m_scenePath, Scene::GetCurrent());
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
	Im3d::Draw();
	AppSample::draw();
}

Ray AppSample3d::getCursorRayW() const
{
	Ray ret = getCursorRayV();
	ret.transform(Scene::GetDrawCamera()->m_world);
	return ret;
}

Ray AppSample3d::getCursorRayV() const
{
	int mx, my;
	getWindow()->getWindowRelativeCursor(&mx, &my);
	vec2 mpos  = vec2((float)mx, (float)my);
	vec2 wsize = vec2((float)getWindow()->getWidth(), (float)getWindow()->getHeight());
	mpos = (mpos / wsize);
	const Camera&  cam = *Scene::GetDrawCamera();
	Ray ret;
	if (cam.getProjFlag(Camera::ProjFlag_Orthographic)) {
		ret.m_origin.x  = mix(cam.m_left, cam.m_right, mpos.x);
		ret.m_origin.y  = mix(cam.m_up,   cam.m_down,  mpos.y);
		ret.m_origin.z  = 0.0f;
		ret.m_direction = vec3(0.0f, 0.0f, -1.0f);
	} else {
		ret.m_origin      = vec3(0.0f);
		ret.m_direction.x = mix(cam.m_left, cam.m_right, mpos.x);
		ret.m_direction.y = mix(cam.m_up,   cam.m_down,  mpos.y);
		ret.m_direction.z = -1.0f;
		ret.m_direction   = normalize(ret.m_direction);
	}

	return ret;
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

 // plane normals
 //  4------------5
 //  |\          /|
 //  7-\--------/-6
 //   \ 0------1 /
 //    \|      |/
 //     3------2
	/*struct FrustumPlane { int m_index; int m_vertices[4]; };
	static const FrustumPlane fplanes[6] = {
		{ Frustum::Plane_Near,   { 0, 1, 3, 2 } },
		{ Frustum::Plane_Far,    { 4, 5, 7, 6 } },
		{ Frustum::Plane_Right,  { 1, 5, 2, 6 } },
		{ Frustum::Plane_Left,   { 0, 4, 3, 7 } },
		{ Frustum::Plane_Top,    { 4, 5, 0, 1 } },
		{ Frustum::Plane_Bottom, { 7, 6, 3, 2 } }
	};
	Im3d::PushColor(Im3d::Color(1.0f, 0.2f, 0.1f, 0.75f));
	Im3d::PushSize(4.0f);
	for (int i = 0; i < 6; ++i) {
		const FrustumPlane& fp = fplanes[i];
		vec3 origin = mix(mix(verts[fp.m_vertices[0]], verts[fp.m_vertices[1]], 0.5f), mix(verts[fp.m_vertices[2]], verts[fp.m_vertices[3]], 0.5f), 0.5f);
		Im3d::DrawArrow(origin, origin + _frustum.m_planes[fp.m_index].m_normal, 0.15f);
	}
	Im3d::PopSize();
	Im3d::PopColor();
	*/
}

AppSample3d::AppSample3d(const char* _title)
	: AppSample(_title)
{

	PropertyGroup& propGroup = m_props.addGroup("AppSample3d");
	//                name                     default        min     max     storage
	propGroup.addBool("Show Helpers",        false,                         &m_showHelpers);
	propGroup.addBool("Show Scene Editor",   false,                         &m_showSceneEditor);
	propGroup.addPath("Scene Path",          "Scene.json",                  &m_scenePath);
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
	s_shIm3dPoints = Shader::CreateVsFs("shaders/Im3d_vs.glsl", "shaders/Im3d_fs.glsl", "POINTS\0");
	s_shIm3dPoints->setName("#Im3d_POINTS");
	s_shIm3dLines = Shader::CreateVsGsFs("shaders/Im3d_vs.glsl", "shaders/Im3d_gs.glsl", "shaders/Im3d_fs.glsl", "LINES\0");
	s_shIm3dLines->setName("#Im3d_POINTS");
	s_shIm3dLines->setName("#Im3d_LINES");
	s_shIm3dTriangles = Shader::CreateVsFs("shaders/Im3d_vs.glsl", "shaders/Im3d_fs.glsl", "TRIANGLES\0");
	s_shIm3dTriangles->setName("#Im3d_TRIANGLES");


	MeshDesc meshDesc(MeshDesc::Primitive_Points);
	meshDesc.addVertexAttr(VertexAttr::Semantic_Positions, DataType::Float32, 4);
	meshDesc.addVertexAttr(VertexAttr::Semantic_Colors,    DataType::Uint8N, 4);
	APT_ASSERT(meshDesc.getVertexSize() == sizeof(struct Im3d::VertexData));
	s_msIm3dPoints = Mesh::Create(meshDesc);
	meshDesc.setPrimitive(MeshDesc::Primitive_Lines);
	s_msIm3dLines= Mesh::Create(meshDesc);
	meshDesc.setPrimitive(MeshDesc::Primitive_Triangles);
	s_msIm3dTriangles= Mesh::Create(meshDesc);

	Im3d::GetAppData().drawCallback = Im3d_Draw;

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
	CPU_AUTO_MARKER("Im3d_Update");

	Im3d::AppData& ad = Im3d::GetAppData();

	ad.m_deltaTime = (float)_app->m_deltaTime;
	ad.m_viewportSize = vec2((float)_app->getWindow()->getWidth(), (float)_app->getWindow()->getHeight());
	ad.m_projScaleY = Scene::GetDrawCamera()->m_up - Scene::GetDrawCamera()->m_down;
	ad.m_projOrtho = Scene::GetDrawCamera()->getProjFlag(Camera::ProjFlag_Orthographic);
	ad.m_viewOrigin = Scene::GetDrawCamera()->getPosition();
	
	Ray cursorRayW = _app->getCursorRayW();
	ad.m_cursorRayOrigin = cursorRayW.m_origin;
	ad.m_cursorRayDirection = cursorRayW.m_direction;
	ad.m_worldUp = vec3(0.0f, 1.0f, 0.0f);

	Mouse* mouse = Input::GetMouse();	
	ad.m_keyDown[Im3d::Mouse_Left/*Im3d::Action_Select*/] = mouse->isDown(Mouse::Button_Left);

	Keyboard* keyb = Input::GetKeyboard();
	bool ctrlDown = keyb->isDown(Keyboard::Key_LCtrl);
	ad.m_keyDown[Im3d::Key_L/*Action_GizmoLocal*/]       = ctrlDown && keyb->wasPressed(Keyboard::Key_L);
	ad.m_keyDown[Im3d::Key_T/*Action_GizmoTranslation*/] = ctrlDown && keyb->wasPressed(Keyboard::Key_T);
	ad.m_keyDown[Im3d::Key_R/*Action_GizmoRotation*/]    = ctrlDown && keyb->wasPressed(Keyboard::Key_R);
	ad.m_keyDown[Im3d::Key_S/*Action_GizmoScale*/]       = ctrlDown && keyb->wasPressed(Keyboard::Key_S);

	Im3d::NewFrame();
}

void AppSample3d::Im3d_Draw(const Im3d::DrawList& _drawList)
{
	AUTO_MARKER("Im3d_Draw");

	Im3d::AppData& ad = Im3d::GetAppData();

	glAssert(glEnable(GL_BLEND));
    glAssert(glBlendEquation(GL_FUNC_ADD));
    glAssert(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    glAssert(glDisable(GL_CULL_FACE));
	glAssert(glEnable(GL_PROGRAM_POINT_SIZE));
    
	Mesh* ms;
	Shader* sh;
	switch (_drawList.m_primType) {
		case Im3d::DrawPrimitive_Points:
			ms = s_msIm3dPoints;
			sh = s_shIm3dPoints;
			break;
		case Im3d::DrawPrimitive_Lines:
			ms = s_msIm3dLines;
			sh = s_shIm3dLines;
			break;
		case Im3d::DrawPrimitive_Triangles:
			ms = s_msIm3dTriangles;
			sh = s_shIm3dTriangles;
			break;
		default:
			APT_ASSERT(false); // unsupported primitive type?
	};

	ms->setVertexData(_drawList.m_vertexData, _drawList.m_vertexCount, GL_STREAM_DRAW);
	
	GlContext* ctx = GlContext::GetCurrent();
	ctx->setShader(sh);
	ctx->setUniform("uViewProjMatrix", Scene::GetDrawCamera()->m_viewProj);
	ctx->setUniform("uViewport", vec2(ctx->getViewportWidth(), ctx->getViewportHeight()));
	ctx->setMesh(ms);
	ctx->draw();

	glAssert(glDisable(GL_PROGRAM_POINT_SIZE));
	glAssert(glDisable(GL_BLEND));
}
