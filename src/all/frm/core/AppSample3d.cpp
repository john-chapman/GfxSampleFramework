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
#include <frm/core/Scene.h>
#include <frm/core/Texture.h>
#include <frm/core/Viewport.h>
#include <frm/core/Window.h>
#include <frm/core/XForm.h>

#if FRM_MODULE_PHYSICS
	#include <frm/physics/Physics.h>
#endif

#include <im3d/im3d.h>

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

	m_scene = new Scene;
	Scene::SetCurrent(m_scene);

	#if FRM_MODULE_PHYSICS
		if (!Physics::Init()) {
			return false;
		}
	#endif

	if (!Scene::Load((const char*)m_scenePath, *m_scene)) 
	{
 		Camera* defaultCamera = m_scene->createCamera(Camera());
		defaultCamera->setPerspective(Radians(45.0f), 1.0f, 0.1f, 1000.0f, Camera::ProjFlag_Infinite);
		defaultCamera->updateGpuBuffer(); // alloc the gpu buffer
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
	Scene::SetCurrent(nullptr);
	delete m_scene;

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

	Im3d_Update(this);

	Scene& scene = *Scene::GetCurrent();
	scene.update((float)m_deltaTime, Node::State_Active | Node::State_Dynamic);
	#ifdef frm_Scene_ENABLE_EDIT
		if (m_showSceneEditor)
		{
			Scene::GetCurrent()->edit();
		}
	#endif
	#if FRM_MODULE_PHYSICS
	 // update *after* scene to capture kinematic targets and then override world matrices
		Physics::Update((float)m_deltaTime);
	#endif

	Camera* currentCamera = scene.getDrawCamera();
	if (!currentCamera->getProjFlag(Camera::ProjFlag_Asymmetrical))
	{
	 // update aspect ratio to match window size
		Window* win = getWindow();
		int winX = win->getWidth();
		int winY = win->getHeight();
		if (winX != 0 && winY != 0)
		{
			float aspect = (float)winX / (float)winY;
			if (currentCamera->m_aspectRatio != aspect)
			{
				currentCamera->setAspectRatio(aspect);
			}
		}
	}

	drawMainMenuBar();

 // keyboard shortcuts
	Keyboard* keyb = Input::GetKeyboard();
	if (keyb->wasPressed(Keyboard::Key_F2))
	{
		m_showHelpers = !m_showHelpers;
	}
	if (ImGui::IsKeyPressed(Keyboard::Key_0) && ImGui::IsKeyDown(Keyboard::Key_LCtrl))
	{
		m_showSceneEditor = !m_showSceneEditor;
	}
	if (ImGui::IsKeyPressed(Keyboard::Key_C) && ImGui::IsKeyDown(Keyboard::Key_LCtrl) && ImGui::IsKeyDown(Keyboard::Key_LShift))
	{
		if (m_dbgCullCamera)
		{
			scene.destroyCamera(m_dbgCullCamera);
			scene.setCullCamera(scene.getDrawCamera());
		}
		else
		{
			m_dbgCullCamera = scene.createCamera(*scene.getCullCamera());
			Node* node = m_dbgCullCamera->m_parent;
			node->setName("#DEBUG CULL CAMERA");
			node->setDynamic(false);
			node->setActive(false);
			node->setLocalMatrix(scene.getCullCamera()->m_world);
			scene.setCullCamera(m_dbgCullCamera);
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

		 // scene cameras
			for (int i = 0; i < scene.getCameraCount(); ++i)
			{
				Camera* camera = scene.getCamera(i);
				if (camera == scene.getDrawCamera())
				{
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

void AppSample3d::draw()
{
	Im3d::EndFrame();
	if (!m_hiddenMode)
	{	
		PROFILER_MARKER("#AppSample3d::draw");
		getGlContext()->setFramebufferAndViewport(getDefaultFramebuffer());
		drawIm3d(Scene::GetDrawCamera(), nullptr, Viewport(), m_txIm3dDepth);
	}
	AppSample::draw();
}

Ray AppSample3d::getCursorRayW(const Camera* _camera) const
{
	_camera = _camera ? _camera : Scene::GetDrawCamera();
	Ray ret = getCursorRayV(_camera);
	ret.transform(_camera->m_world);
	return ret;
}

Ray AppSample3d::getCursorRayV(const Camera* _camera) const
{
	_camera = _camera ? _camera : Scene::GetDrawCamera();
	int mx, my;
	getWindow()->getWindowRelativeCursor(&mx, &my);
	vec2 wsize = vec2((float)getWindow()->getWidth(), (float)getWindow()->getHeight());
	vec2 mpos  = vec2((float)mx, (float)my) / wsize;
	Ray ret;
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
	Properties::PushGroup("AppSample3d");
		//                  name                 default              min     max     storage
		Properties::Add    ("ShowHelpers",       m_showHelpers,                       &m_showHelpers);
		Properties::Add    ("ShowSceneEditor",   m_showSceneEditor,                   &m_showSceneEditor);
		Properties::AddPath("ScenePath",         m_scenePath,                         &m_scenePath);
	Properties::PopGroup(); // AppSample3d
}

AppSample3d::~AppSample3d()
{
	Properties::InvalidateGroup("AppSampleVR");
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
		if (ImGui::BeginMenu("Scene"))
		{
			if (ImGui::MenuItem("Load..."))
			{
				if (FileSystem::PlatformSelect(m_scenePath, { "*.json" }))
				{
					m_scenePath = FileSystem::MakeRelative((const char*)m_scenePath);
					Scene::Load((const char*)m_scenePath, *m_scene);
				}
			}
			if (ImGui::MenuItem("Save"))
			{
				Scene::Save((const char*)m_scenePath, *m_scene);
			}
			if (ImGui::MenuItem("Save As..."))
			{
				if (FileSystem::PlatformSelect(m_scenePath, { "*.json" }))
				{
					m_scenePath = FileSystem::MakeRelative((const char*)m_scenePath);
					Scene::Save((const char*)m_scenePath, *m_scene);
				}
			}

			ImGui::Separator();

			ImGui::MenuItem("Scene Editor",      "Ctrl+O",       m_showSceneEditor);
			ImGui::MenuItem("Show Helpers",      "F2",           m_showHelpers);
			ImGui::MenuItem("Pause Cull Camera", "Ctrl+Shift+C", m_showSceneEditor);

			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}
}


/*******************************************************************************

                                   Im3d

*******************************************************************************/

static Shader* s_shIm3dPrimitives[Im3d::DrawPrimitive_Count][2]; // shader per primtive type (points, lines, tris), with/without depth test
static Mesh*   s_msIm3dPrimitives[Im3d::DrawPrimitive_Count];

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
}

void AppSample3d::Im3d_Update(AppSample3d* _app)
{
	PROFILER_MARKER_CPU("#Im3d_Update");

	Im3d::AppData& ad = Im3d::GetAppData();

	ad.m_deltaTime = (float)_app->m_deltaTime;
	ad.m_viewportSize = vec2((float)_app->getWindow()->getWidth(), (float)_app->getWindow()->getHeight());
	ad.m_projScaleY = Scene::GetDrawCamera()->m_up - Scene::GetDrawCamera()->m_down;
	ad.m_projOrtho = Scene::GetDrawCamera()->getProjFlag(Camera::ProjFlag_Orthographic);
	ad.m_viewOrigin = Scene::GetDrawCamera()->getPosition();
	ad.m_viewDirection = Scene::GetDrawCamera()->getViewVector();
	
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

	ad.m_snapTranslation = ctrlDown ? 0.1f : 0.0f;
	ad.m_snapRotation    = ctrlDown ? Radians(15.0f) : 0.0f;
	ad.m_snapScale       = ctrlDown ? 0.5f : 0.0f;

	Im3d::NewFrame();
}

void AppSample3d::Im3d_Draw(Camera* _camera, Texture* _txDepth)
{
	/*ImDrawList* imDrawList = ImGui::GetWindowDrawList();
	const mat4& viewProj = _camera->m_viewProj;
	for (unsigned i = 0; i < Im3d::GetTextDrawListCount(); ++i) 
	{
		const Im3d::TextDrawList& textDrawList = Im3d::GetTextDrawLists()[i];
		
		for (unsigned j = 0; j < textDrawList.m_textDataCount; ++j)
		{
			const Im3d::TextData& textData = textDrawList.m_textData[j];
			if (textData.m_positionSize.w == 0.0f || textData.m_color.getA() == 0.0f)
			{
				continue;
			}

			// Project world -> screen space.
			vec4 clip = viewProj * vec4(textData.m_positionSize.x, textData.m_positionSize.y, textData.m_positionSize.z, 1.0f);
			vec2 screen = vec2(clip.x / clip.w, clip.y / clip.w);
	
			// Cull text which falls offscreen. Note that this doesn't take into account text size but works well enough in practice.
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
			vec2 textSize = ImGui::CalcTextSize(text, text + textData.m_textLength);
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
			imDrawList->AddText(nullptr, textData.m_positionSize.w * ImGui::GetFontSize(), screen, textData.m_color.getABGR(), text, text + textData.m_textLength);
		}
	}

	ImGui::End();
	ImGui::PopStyleColor();*/
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

	// \todo text rendering
}

void AppSample3d::drawIm3d(Camera* _camera, Framebuffer* _framebuffer, Viewport _viewport, Texture* _depthTexture)
{
	FRM_ASSERT(_camera);

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
			_viewport = { 0, 0, m_resolution.x, m_resolution.y };
		}
	}

	drawIm3d({ _camera }, { _framebuffer }, { _viewport }, { _depthTexture });
}