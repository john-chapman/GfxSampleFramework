#include "AppSample3d.h"

#include <frm/core/frm.h>
#include <frm/core/interpolation.h>
#include <frm/core/gl.h>
#include <frm/core/geom.h>
#include <frm/core/Framebuffer.h>
#include <frm/core/GlContext.h>
#include <frm/core/Input.h>
#include <frm/core/Mesh.h>
#include <frm/core/MeshData.h>
#include <frm/core/Profiler.h>
#include <frm/core/Properties.h>
#include <frm/core/Shader.h>
#include <frm/core/Texture.h>
#include <frm/core/Viewport.h>
#include <frm/core/Window.h>
#include <frm/core/world/World.h>
#include <frm/core/world/WorldEditor.h>
#include <frm/core/world/components/Component.h>
#include <frm/core/world/components/CameraComponent.h>
#include <frm/core/XForm.h>

#if FRM_MODULE_PHYSICS
	#include <frm/physics/Physics.h>
#endif

#include <im3d/im3d.h>
#include <imgui/imgui_ext.h>

using namespace frm;

// PUBLIC

bool AppSample3d::init(const frm::ArgList& _args)
{
	if (!AppSample::init(_args))
	{
		return false;
	}
	if (!Im3d_Init(this))
	{
		return false;
	}

	#if FRM_MODULE_PHYSICS
		if (!Physics::Init()) 
		{
			return false;
		}
	#endif

	m_world = World::Create(m_worldPath.c_str());
	FRM_VERIFY(m_world->init() && m_world->postInit());

	m_worldEditor = FRM_NEW(WorldEditor());
	m_worldEditor->setWorld(m_world);

	return true;
}

void AppSample3d::shutdown()
{
	destroyDebugCullCamera();

	FRM_DELETE(m_worldEditor); // \todo Detect pending changes which might need to be saved.

	m_worldPath = m_world->getPath();
	m_world->shutdown();
	World::Destroy(m_world);

	#if FRM_MODULE_PHYSICS
		Physics::Shutdown();
	#endif

	if (m_txIm3dDepth)
	{
		Texture::Release(m_txIm3dDepth);
	}
	Im3d_Shutdown(this);

	AppSample::shutdown();
}

bool AppSample3d::update()
{
	if (!AppSample::update())
	{
		return false;
	}

	PROFILER_MARKER_CPU("#AppSample3d::update");

	const float dt = (float)getDeltaTime();

	Im3d_Update(this);

	{	PROFILER_MARKER_CPU("#World update");
		World* world = World::GetCurrent();
		Component::ClearActiveComponents();
		world->update(dt, World::UpdatePhase::GatherActive);
		world->update(dt, World::UpdatePhase::PrePhysics);
		world->update(dt, World::UpdatePhase::Hierarchy);
		#if FRM_MODULE_PHYSICS
			Physics::Update(dt);
		#endif
		world->update(dt, World::UpdatePhase::Physics);
		world->update(dt, World::UpdatePhase::PostPhysics);
		world->update(dt, World::UpdatePhase::PreRender);
	}

	// Update draw camera aspect ratio to match window.
	Camera* drawCamera = World::GetDrawCamera();
	if (drawCamera && !drawCamera->getProjFlag(Camera::ProjFlag_Asymmetrical))
	{
		const Window* win = getWindow();
		const int winX = win->getWidth();
		const int winY = win->getHeight();
		if (winX != 0 && winY != 0)
		{
			float aspect = (float)winX / (float)winY;
			if (drawCamera->m_aspectRatio != aspect)
			{
				drawCamera->setAspectRatio(aspect);
			}
		}
	}

	if (m_showWorldEditor)
	{
		m_worldEditor->edit();
	}

	drawMainMenuBar();

	// Keyboard shortcuts
	Keyboard* keyb = Input::GetKeyboard();
	if (keyb->wasPressed(Keyboard::Key_F2))
	{
		m_showHelpers = !m_showHelpers;
	}
	if (ImGui::IsKeyPressed(Keyboard::Key_0) && ImGui::IsKeyDown(Keyboard::Key_LCtrl))
	{
		m_showWorldEditor = !m_showWorldEditor;
	}
	if (ImGui::IsKeyPressed(Keyboard::Key_C) && ImGui::IsKeyDown(Keyboard::Key_LCtrl) && ImGui::IsKeyDown(Keyboard::Key_LShift))
	{
		if (m_restoreCullCamera)
		{
			destroyDebugCullCamera();
		}
		else
		{
			createDebugCullCamera();
		}
	}

	if (m_showHelpers)
	{
		const int   kGridSize = 20;
		const float kGridHalf = (float)kGridSize * 0.5f;
		Im3d::PushDrawState();
			Im3d::SetAlpha(1.0f);
			Im3d::SetSize(1.0f);

		 // origin XZ grid
			Im3d::BeginLines();
				for (int x = 0; x <= kGridSize; ++x)
				{
					Im3d::Vertex(-kGridHalf, 0.0f, (float)x - kGridHalf,  Im3d::Color(0.0f, 0.0f, 0.0f));
					Im3d::Vertex( kGridHalf, 0.0f, (float)x - kGridHalf,  Im3d::Color(1.0f, 0.0f, 0.0f));
				}
				for (int z = 0; z <= kGridSize; ++z)
				{
					Im3d::Vertex((float)z - kGridHalf, 0.0f, -kGridHalf,  Im3d::Color(0.0f, 0.0f, 0.0f));
					Im3d::Vertex((float)z - kGridHalf, 0.0f,  kGridHalf,  Im3d::Color(0.0f, 0.0f, 1.0f));
				}
			Im3d::End();
		Im3d::PopDrawState();
	}

	#if FRM_MODULE_PHYSICS
		Physics::DrawDebug();
	#endif

	return true;
}

void AppSample3d::draw()
{
	Im3d::EndFrame();
	if (!m_hiddenMode)
	{	
		PROFILER_MARKER("#AppSample3d::draw");
		getGlContext()->setFramebufferAndViewport(getDefaultFramebuffer());
		drawIm3d(World::GetDrawCamera(), nullptr, Viewport(), m_txIm3dDepth);
	}
	AppSample::draw();
}

Ray AppSample3d::getCursorRayW(const Camera* _camera) const
{
	Ray ret;
	
	_camera = _camera ? _camera : World::GetDrawCamera();
	if (_camera)
	{
		ret = getCursorRayV(_camera);
		ret.transform(_camera->m_world);
	}

	return ret;
}

Ray AppSample3d::getCursorRayV(const Camera* _camera) const
{
	Ray ret;
	
	_camera = _camera ? _camera : World::GetDrawCamera();
	if (_camera)
	{
		int mx, my;
		getWindow()->getWindowRelativeCursor(&mx, &my);
		vec2 wsize = vec2((float)getWindow()->getWidth(), (float)getWindow()->getHeight());
		vec2 mpos  = vec2((float)mx, (float)my) / wsize;
		if (_camera->getProjFlag(Camera::ProjFlag_Orthographic))
		{
			ret.m_origin.x    = lerp(_camera->m_left, _camera->m_right, mpos.x);
			ret.m_origin.y    = lerp(_camera->m_up,   _camera->m_down,  mpos.y);
			ret.m_origin.z    = 0.0f;
			ret.m_direction   = vec3(0.0f, 0.0f, -1.0f);
		}
		else
		{
			ret.m_origin      = vec3(0.0f);
			ret.m_direction.x = lerp(_camera->m_left, _camera->m_right, mpos.x);
			ret.m_direction.y = lerp(_camera->m_up,   _camera->m_down,  mpos.y);
			ret.m_direction.z = -1.0f;
			ret.m_direction   = normalize(ret.m_direction);
		}
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
	Properties::PushGroup("#AppSample3d");
		//                  name                 default              min     max     storage
		Properties::Add    ("m_showHelpers",     m_showHelpers,                       &m_showHelpers);
		Properties::Add    ("m_showWorldEditor", m_showWorldEditor,                   &m_showWorldEditor);
		Properties::AddPath("m_worldPath",       m_worldPath,                         &m_worldPath);
	Properties::PopGroup(); // AppSample3d
}

AppSample3d::~AppSample3d()
{
	Properties::InvalidateGroup("#AppSample3d");
}

void AppSample3d::setIm3dDepthTexture(Texture* _tx)
{
	if (m_txIm3dDepth == _tx)
	{
		return;
	}

	if (m_txIm3dDepth)
	{
		Texture::Release(m_txIm3dDepth);
	}

	m_txIm3dDepth = _tx;
	Texture::Use(m_txIm3dDepth);
}

// PRIVATE

void AppSample3d::drawMainMenuBar()
{
	if (m_showMenu && ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("World"))
		{
			if (ImGui::MenuItem("World Editor", "Ctrl+0", m_showWorldEditor))
			{
				m_showWorldEditor = !m_showWorldEditor;
			}

			if (ImGui::MenuItem("Show Helpers", "F2", m_showHelpers))
			{
				m_showHelpers = !m_showHelpers;
			}

			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}
}

void AppSample3d::createDebugCullCamera()
{
	if (m_restoreCullCamera)
	{
		destroyDebugCullCamera();
	}

	World* world = World::GetCurrent();
	Scene* rootScene = world->getRootScene();
	
	m_restoreCullCamera = world->getCullCameraComponent();
	if (!m_restoreCullCamera)
	{
		return;
	}

	SceneNode* cullCameraNode = rootScene->createTransientNode("#Debug Cull Camera");
	CameraComponent* cullCameraComponent = (CameraComponent*)Component::Create(StringHash("CameraComponent"));
	cullCameraComponent->getCamera().copyFrom(m_restoreCullCamera->getCamera());
	cullCameraNode->addComponent(cullCameraComponent);	
	cullCameraNode->setLocal(m_restoreCullCamera->getCamera().m_world);
	world->setCullCameraComponent(cullCameraComponent);

	FRM_VERIFY(cullCameraNode->init() && cullCameraNode->postInit());
}

void AppSample3d::destroyDebugCullCamera()
{
	if (!m_restoreCullCamera)
	{
		return;
	}
	
	World* world = World::GetCurrent();

	CameraComponent* cullCameraComponent = world->getCullCameraComponent();
	if (!cullCameraComponent)
	{
		return;
	}
	Scene* rootScene = World::GetCurrent()->getRootScene();
	rootScene->destroyNode(cullCameraComponent->getParentNode());

	world->setCullCameraComponent(m_restoreCullCamera);
	m_restoreCullCamera = nullptr;
}

/*******************************************************************************

                                   Im3d

*******************************************************************************/

static Shader*       s_shIm3dPrimitives[Im3d::DrawPrimitive_Count][2]; // Shader per primtive type (points, lines, tris), with/without depth test.
static Mesh*         s_msIm3dPrimitives[Im3d::DrawPrimitive_Count];    // Mesh per primitive type.
static ImGuiContext* s_im3dTextRenderContext = nullptr;                // Separate ImGui context for text rendering.

bool AppSample3d::Im3d_Init(AppSample3d* _app)
{
	if (_app->m_hiddenMode)
	{
		return true;
	}

	bool ret = true;

	constexpr const char* kPrimitiveNames[] = { "TRIANGLES", "LINES", "POINTS" };
	constexpr const char* kShaderPath = "shaders/Im3d.glsl";
	for (int primitiveType = 0; primitiveType < Im3d::DrawPrimitive_Count; ++primitiveType)
	{
		if (primitiveType == Im3d::DrawPrimitive_Lines)
		{
			s_shIm3dPrimitives[primitiveType][0] = Shader::CreateVsGsFs(kShaderPath, kShaderPath, kShaderPath, { kPrimitiveNames[primitiveType], "DEPTH" });
			s_shIm3dPrimitives[primitiveType][1] = Shader::CreateVsGsFs(kShaderPath, kShaderPath, kShaderPath, { kPrimitiveNames[primitiveType] });
		}
		else
		{
			s_shIm3dPrimitives[primitiveType][0] = Shader::CreateVsFs(kShaderPath, kShaderPath, { kPrimitiveNames[primitiveType], "DEPTH" });
			s_shIm3dPrimitives[primitiveType][1] = Shader::CreateVsFs(kShaderPath, kShaderPath, { kPrimitiveNames[primitiveType] });
		}

		s_shIm3dPrimitives[primitiveType][0]->setNamef("#Im3d_%s_DEPTH", kPrimitiveNames[primitiveType]);
		s_shIm3dPrimitives[primitiveType][1]->setNamef("#Im3d_%s", kPrimitiveNames[primitiveType]);

		ret &= s_shIm3dPrimitives[primitiveType][0] && s_shIm3dPrimitives[primitiveType][0]->getState() == Shader::State_Loaded;
		ret &= s_shIm3dPrimitives[primitiveType][1] && s_shIm3dPrimitives[primitiveType][0]->getState() == Shader::State_Loaded;
	}

	MeshDesc meshDesc(MeshDesc::Primitive_Points);
	meshDesc.addVertexAttr(VertexAttr::Semantic_Positions, DataType_Float32, 4);
	meshDesc.addVertexAttr(VertexAttr::Semantic_Colors,    DataType_Uint8N, 4);
	FRM_ASSERT(meshDesc.getVertexSize() == sizeof(struct Im3d::VertexData));
	s_msIm3dPrimitives[Im3d::DrawPrimitive_Points] = Mesh::Create(meshDesc);
	ret &= s_msIm3dPrimitives[Im3d::DrawPrimitive_Points] && s_msIm3dPrimitives[Im3d::DrawPrimitive_Points]->getState() == Mesh::State_Loaded;
	
	meshDesc.setPrimitive(MeshDesc::Primitive_Lines);
	s_msIm3dPrimitives[Im3d::DrawPrimitive_Lines] = Mesh::Create(meshDesc);
	ret &= s_msIm3dPrimitives[Im3d::DrawPrimitive_Lines] && s_msIm3dPrimitives[Im3d::DrawPrimitive_Lines]->getState() == Mesh::State_Loaded;

	meshDesc.setPrimitive(MeshDesc::Primitive_Triangles);
	s_msIm3dPrimitives[Im3d::DrawPrimitive_Triangles] = Mesh::Create(meshDesc);
	ret &= s_msIm3dPrimitives[Im3d::DrawPrimitive_Triangles] && s_msIm3dPrimitives[Im3d::DrawPrimitive_Triangles]->getState() == Mesh::State_Loaded;

	// Init separate ImGui context for Im3d text rendering.
	ImGuiContext* prevImGuiContext = ImGui::GetCurrentContext();
	s_im3dTextRenderContext = ImGui::CreateContext(ImGui::GetIO().Fonts); // Share main context font atlas.
	ImGui::SetCurrentContext(s_im3dTextRenderContext); // \todo Could avoid this by including imgui_internal.h directly.
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;
	io.UserData = _app->getGlContext();
	
	ImGui::SetCurrentContext(prevImGuiContext);

	return ret;
}

void AppSample3d::Im3d_Shutdown(AppSample3d* _app)
{
	for (int primitiveType = 0; primitiveType < Im3d::DrawPrimitive_Count; ++primitiveType)
	{
		Mesh::Release(s_msIm3dPrimitives[primitiveType]);
		Shader::Release(s_shIm3dPrimitives[primitiveType][0]);
		Shader::Release(s_shIm3dPrimitives[primitiveType][1]);
	}

	ImGui::DestroyContext(s_im3dTextRenderContext);
}

void AppSample3d::Im3d_Update(AppSample3d* _app)
{
	PROFILER_MARKER_CPU("#Im3d_Update");

	Im3d::AppData& ad = Im3d::GetAppData();

	const Camera* drawCamera = World::GetDrawCamera();
	ad.m_deltaTime           = (float)_app->m_deltaTime;
	ad.m_viewportSize        = vec2((float)_app->getWindow()->getWidth(), (float)_app->getWindow()->getHeight());
	ad.m_projScaleY          = drawCamera ? (drawCamera->m_up - drawCamera->m_down) : 1.0f;
	ad.m_projOrtho           = drawCamera ? drawCamera->getProjFlag(Camera::ProjFlag_Orthographic) : false;
	ad.m_viewOrigin          = drawCamera ? drawCamera->getPosition() : vec3(0.0f);
	ad.m_viewDirection       = drawCamera ? drawCamera->getViewVector() : vec3(0.0f, 0.0f, -1.0f);
	
	const Ray cursorRayW     = _app->getCursorRayW();
	ad.m_cursorRayOrigin     = cursorRayW.m_origin;
	ad.m_cursorRayDirection  = cursorRayW.m_direction;
	ad.m_worldUp             = vec3(0.0f, 1.0f, 0.0f);

	Mouse* mouse = Input::GetMouse();	
	ad.m_keyDown[Im3d::Mouse_Left/*Im3d::Action_Select*/] = mouse->isDown(Mouse::Button_Left);

	Keyboard* keyb = Input::GetKeyboard();
	bool ctrlDown = keyb->isDown(Keyboard::Key_LCtrl);
	ad.m_keyDown[Im3d::Key_L/*Action_GizmoLocal*/]       = ctrlDown && keyb->wasPressed(Keyboard::Key_L);
	ad.m_keyDown[Im3d::Key_T/*Action_GizmoTranslation*/] = ctrlDown && keyb->wasPressed(Keyboard::Key_T);
	ad.m_keyDown[Im3d::Key_R/*Action_GizmoRotation*/]    = ctrlDown && keyb->wasPressed(Keyboard::Key_R);
	ad.m_keyDown[Im3d::Key_S/*Action_GizmoScale*/]       = ctrlDown && keyb->wasPressed(Keyboard::Key_S);

	ad.m_snapTranslation = ctrlDown ? 0.1f : 0.0f;
	ad.m_snapRotation    = ctrlDown ? Radians(15.0f) : 0.0f;
	ad.m_snapScale       = ctrlDown ? 0.5f : 0.0f;

	Im3d::NewFrame();
}

void AppSample3d::drawIm3d(
	std::initializer_list<Camera*>      _cameras,
	std::initializer_list<Framebuffer*> _framebuffers,
	std::initializer_list<Viewport>     _viewports,
	std::initializer_list<Texture*>     _depthTextures
	)
{
	if (Im3d::GetDrawListCount() == 0)
	{
		return;
	}

	PROFILER_MARKER("#drawIm3d");

	size_t viewCount = _cameras.size();
	FRM_ASSERT(_framebuffers.size()  == viewCount);
	FRM_ASSERT(_viewports.size()     == viewCount);
	FRM_ASSERT(_depthTextures.size() == viewCount);
			
	glScopedEnable(GL_BLEND,              GL_TRUE);
	glScopedEnable(GL_PROGRAM_POINT_SIZE, GL_TRUE);
	glScopedEnable(GL_CULL_FACE,          GL_FALSE);
	glAssert(glBlendEquation(GL_FUNC_ADD));
	//glAssert(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
	glAssert(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE)); // preserve alpha
	
	GlContext* ctx = GlContext::GetCurrent();

	for (unsigned drawListIndex = 0; drawListIndex < Im3d::GetDrawListCount(); ++drawListIndex)
	{
		const Im3d::DrawList& drawList = Im3d::GetDrawLists()[drawListIndex];

		Mesh* ms = s_msIm3dPrimitives[drawList.m_primType];
		ms->setVertexData(drawList.m_vertexData, drawList.m_vertexCount, GL_STREAM_DRAW);

		for (size_t viewIndex = 0; viewIndex < viewCount; ++viewIndex)
		{
			Camera*      camera       = *(viewIndex + _cameras.begin());
			Framebuffer* framebuffer  = *(viewIndex + _framebuffers.begin());
			Viewport     viewport     = *(viewIndex + _viewports.begin());
			Texture*     depthTexture = *(viewIndex + _depthTextures.begin());
			Shader*      shader       = s_shIm3dPrimitives[drawList.m_primType][depthTexture ? 0 : 1];

			ctx->setShader(shader);
			ctx->setUniform("uViewProjMatrix", camera->m_viewProj);
			ctx->setUniform("uViewport", vec2((float)viewport.w, (float)viewport.h));	
			ctx->setFramebuffer(framebuffer);
			ctx->setViewport(viewport);
			if (depthTexture)
			{
				ctx->bindTexture("txDepth", depthTexture);
			}
			ctx->setMesh(ms);
			ctx->draw();
		}
	}

	// Early-out if there is no text to draw (avoid overhead of updating ImGui context).
	if (Im3d::GetTextDrawListCount() == 0)
	{
		return;
	}

	ImGui::PushContext(s_im3dTextRenderContext);

	Window* window = getWindow();
	ImGuiIO& io = ImGui::GetIO();
	io.ImeWindowHandle = window->getHandle();

	for (size_t viewIndex = 0; viewIndex < viewCount; ++viewIndex)
	{
		Camera*      camera       = *(viewIndex + _cameras.begin());
		Framebuffer* framebuffer  = *(viewIndex + _framebuffers.begin());
		Viewport     viewport     = *(viewIndex + _viewports.begin());
		Texture*     depthTexture = *(viewIndex + _depthTextures.begin());

		io.DisplaySize = ImVec2((float)viewport.w, (float)viewport.h);
		io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
		io.DeltaTime = (float)getDeltaTime();
		
		ImGui::NewFrame();
		ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32_BLACK_TRANS);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(io.DisplaySize);
		FRM_VERIFY(ImGui::Begin("###Im3dText", nullptr, 0
			| ImGuiWindowFlags_NoTitleBar
			| ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoScrollbar
			| ImGuiWindowFlags_NoInputs
			| ImGuiWindowFlags_NoSavedSettings
			| ImGuiWindowFlags_NoFocusOnAppearing
			| ImGuiWindowFlags_NoBringToFrontOnFocus
			));

		ImDrawList* imDrawList = ImGui::GetWindowDrawList();
		const mat4& viewProj = (*_cameras.begin())->m_viewProj;
		for (unsigned textDrawListIndex = 0; textDrawListIndex < Im3d::GetTextDrawListCount(); ++textDrawListIndex)
		{
			const Im3d::TextDrawList& textDrawList = Im3d::GetTextDrawLists()[textDrawListIndex];		

			for (unsigned textDataIndex = 0; textDataIndex < textDrawList.m_textDataCount; ++textDataIndex)
			{
				const Im3d::TextData& textData = textDrawList.m_textData[textDataIndex];
				if (textData.m_positionSize.w == 0.0f || textData.m_color.getA() == 0.0f)
				{
					continue;
				}

				// Project world -> screen space.
				const vec4 clip = viewProj * vec4(textData.m_positionSize.x, textData.m_positionSize.y, textData.m_positionSize.z, 1.0f);
				vec2 screen = vec2(clip.x / clip.w, clip.y / clip.w);
	
				// Cull text which falls offscreen. Note that this doesn't take into account text size but works well enough in practice.
				// \todo fade out near borders.
				if (clip.w < 0.0f || screen.x >= 1.0f || screen.y >= 1.0f)
				{
					continue;
				}

				// Pixel coordinates for the ImGuiWindow ImGui.
				screen = screen * vec2(0.5f) + vec2(0.5f);
				screen.y = 1.0f - screen.y; // screen space origin is reversed by the projection.
				screen = screen * (vec2)ImGui::GetWindowSize();

				// All text data is stored in a single buffer; each textData instance has an offset into this buffer.
				const char* text = textDrawList.m_textBuffer + textData.m_textBufferOffset;

				// Calculate the final text size in pixels to apply alignment flags correctly.
				ImGui::SetWindowFontScale(textData.m_positionSize.w); // NB no CalcTextSize API which takes a font/size directly...
				const vec2 textSize = ImGui::CalcTextSize(text, text + textData.m_textLength);
				ImGui::SetWindowFontScale(1.0f);

				// Generate a pixel offset based on text flags.
				vec2 textOffset = vec2(-textSize.x * 0.5f, -textSize.y * 0.5f); // default to center
				if ((textData.m_flags & Im3d::TextFlags_AlignLeft) != 0)
				{
					textOffset.x = -textSize.x;
				}
				else if ((textData.m_flags & Im3d::TextFlags_AlignRight) != 0)
				{
					textOffset.x = 0.0f;
				}

				if ((textData.m_flags & Im3d::TextFlags_AlignTop) != 0)
				{
					textOffset.y = -textSize.y;
				}
				else if ((textData.m_flags & Im3d::TextFlags_AlignBottom) != 0)
				{
					textOffset.y = 0.0f;
				}

				// Add text to the window draw list.
				screen = screen + textOffset;
				imDrawList->AddText(nullptr, textData.m_positionSize.w * ImGui::GetFontSize(), screen + vec2(1.f), IM_COL32_BLACK, text, text + textData.m_textLength); // shadow
				imDrawList->AddText(nullptr, textData.m_positionSize.w * ImGui::GetFontSize(), screen, textData.m_color.getABGR(), text, text + textData.m_textLength);
			}
		}
		
		ImGui::End();
		ImGui::PopStyleColor(1);
		ImGui::PopStyleVar(1);
		ImGui::Render(); // calls EndFrame();

		ImGui_RenderDrawLists(ImGui::GetDrawData());
	}

	ImGui::PopContext();
}

void AppSample3d::drawIm3d(Camera* _camera, Framebuffer* _framebuffer, Viewport _viewport, Texture* _depthTexture)
{
	if (!_camera)
	{
		return;
	}

	GlContext* ctx = GlContext::GetCurrent();

	if (!_framebuffer)
	{
		_framebuffer = (Framebuffer*)ctx->getFramebuffer();
	}

	if (_viewport.w == 0)
	{
		if (_framebuffer)
		{
			_viewport = _framebuffer->getViewport();
		}
		else
		{
			_viewport = { 0, 0, m_windowSize.x, m_windowSize.y };
		}
	}

	drawIm3d({ _camera }, { _framebuffer }, { _viewport }, { _depthTexture });
}