#include <frm/core/frm.h>
#include <frm/core/Camera.h>
#include <frm/core/DrawMesh.h>
#include <frm/core/FileSystem.h>
#include <frm/core/GlContext.h>
#include <frm/core/Properties.h>
#include <frm/core/Shader.h>
#include <frm/core/StringHash.h>
#include <frm/core/Texture.h>
#include <frm/core/Window.h>
#include <frm/core/BasicRenderer/BasicLightComponent.h>
#include <frm/core/BasicRenderer/BasicMaterial.h>
#include <frm/core/BasicRenderer/BasicRenderer.h>
#include <frm/core/BasicRenderer/BasicRenderableComponent.h>
#include <frm/core/BasicRenderer/ImageLightComponent.h>
#include <frm/core/world/World.h>
#include <frm/core/world/components/Component.h>
#include <frm/core/world/components/CameraComponent.h>
#include <frm/core/world/components/OrbitLookComponent.h>

#include "MeshViewer.h"

static MeshViewer s_inst;

MeshViewer::MeshViewer()
	: AppBase("MeshViewer") 
{

	Properties::PushGroup("MeshViewer");
		//                  name                 default             min     max              storage
		Properties::AddPath("m_meshPath",        m_meshPath,                                  &m_meshPath);
		Properties::AddPath("m_materialPath",    m_materialPath,                              &m_materialPath);
		Properties::AddPath("m_environmentPath", m_environmentPath,                           &m_environmentPath);
		Properties::Add    ("m_overlayMode",     m_overlayMode,      0,      (int)Mode_Count, &m_overlayMode);
		Properties::Add    ("m_overlayAlpha",    m_overlayAlpha,     0.0f,   1.0f,            &m_overlayAlpha);
		Properties::Add    ("m_wirframe",        m_wireframe,                                 &m_wireframe);
		Properties::Add    ("m_lodOverride",     m_lodOverride,      -1,     10,              &m_lodOverride);
		Properties::Add    ("m_submeshOverride", m_submeshOverride,  -1,     10,              &m_submeshOverride);
	Properties::PopGroup();
}

MeshViewer::~MeshViewer()
{
	Properties::InvalidateGroup("MeshViewer");
}

bool MeshViewer::init(const frm::ArgList& _args)
{
	if (!AppBase::init(_args))
	{
		return false;
	}
	
	m_shOverlay = Shader::CreateVsFs("shaders/MeshViewer.glsl", "shaders/MeshViewer.glsl");
	FRM_ASSERT(CheckResource(m_shOverlay));

	m_shWireframe = Shader::CreateVsFs("shaders/MeshViewer.glsl", "shaders/MeshViewer.glsl", { "WIREFRAME" });
	FRM_ASSERT(CheckResource(m_shWireframe));

	m_basicRenderer = BasicRenderer::Create();
	m_basicRenderer->setFlag(BasicRenderer::Flag::WriteToBackBuffer, false);
	m_basicRenderer->settings.motionBlurTargetFps = 0.0f;

	FRM_VERIFY(initScene());
	FRM_VERIFY(initMeshMaterial());

	return true;
}

void MeshViewer::shutdown()
{
	Shader::Release(m_shOverlay);
	Shader::Release(m_shWireframe);

	BasicRenderer::Destroy(m_basicRenderer);

	AppBase::shutdown();
}

bool MeshViewer::update()
{
	if (!AppBase::update())
	{
		return false;
	}

	auto fileDropList = getWindow()->getFileDropList();
	for (auto& filePath : fileDropList)
	{
		if (FileSystem::CompareExtension("gltf", filePath.c_str()) || FileSystem::CompareExtension("mesh", filePath.c_str()))
		{		
			m_meshPath = FileSystem::MakeRelative(filePath.c_str());
			initMeshMaterial();
		}
		else if (FileSystem::CompareExtension("mat", filePath.c_str()))
		{
			m_materialPath = FileSystem::MakeRelative(filePath.c_str());
			initMeshMaterial();
		}

		return true;
	}

	ImGui::Combo("Overlay Mode", &m_overlayMode, kOverlayModeStr, Mode_Count);
	ImGui::SliderFloat("Overlay Alpha", &m_overlayAlpha, 0.0f, 1.0f);
	ImGui::Checkbox("Wireframe", &m_wireframe);

	ImGui::Spacing();

	ImGui::SliderInt("LOD Override", &m_lodOverride, -1, m_mesh->getLODCount() - 1);
	m_renderable->setLODOverride(m_lodOverride);

	ImGui::SliderInt("Submesh Override", &m_submeshOverride, -1, m_mesh->getSubmeshCount() - 1);
	m_renderable->setSubmeshOverride(m_submeshOverride);
	
	if (m_mesh)
	{
		// Calculate the projected size of the bs.
		const Sphere bs = m_mesh->getBoundingSphere();
		const float d = Length(m_camera->getPosition() - bs.m_origin);
		const float r = bs.m_radius;
		const float fov = m_camera->m_up; // tan(fov/2)
		#if 0
			const float p = 1.0f / fov * r / d;
		#else
			// https://stackoverflow.com/questions/21648630/radius-of-projected-sphere-in-screen-space
			const float p = 1.0f / fov * r / sqrtf(d * d - r * r);
		#endif

		ImGui::Text("Projected size %1.3f (%1.3f px)", p, p * m_resolution.y);
		if (ImGui::Button("Reset Camera"))
		{
			resetCamera();
		}
	}

	if (ImGui::TreeNode("Camera"))
	{
		m_camera->edit();
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Environment"))
	{
		if (m_environment->edit())
		{
			m_environmentPath = m_environment->getTexturePath();
		}
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Renderer"))
	{
		m_basicRenderer->edit();
		ImGui::TreePop();
	}

	return true;
}

void MeshViewer::draw()
{
	GlContext* ctx = GlContext::GetCurrent();
	Camera* drawCamera = World::GetDrawCamera();
	Camera* cullCamera = World::GetCullCamera();

	m_basicRenderer->nextFrame((float)getDeltaTime(), drawCamera, cullCamera);
	m_basicRenderer->draw((float)getDeltaTime(), drawCamera, cullCamera);

	if (m_mesh && m_showHelpers)
	{
		const AlignedBox bb = m_mesh->getBoundingBox();
		const Sphere bs = m_mesh->getBoundingSphere();
		Im3d::PushDrawState();
			Im3d::SetColor(Im3d::Color_Gold);
			Im3d::SetAlpha(0.5f);
			Im3d::SetSize(3.0f);
			Im3d::DrawAlignedBox(bb.m_min, bb.m_max);
			Im3d::DrawSphere(bs.m_origin, bs.m_radius, 32);
		Im3d::PopDrawState();
	}

	if (m_mesh)
	{	
		glScopedEnable(GL_CULL_FACE, GL_FALSE);
		glScopedEnable(GL_BLEND, GL_TRUE);
		
		Texture* txDepth = m_basicRenderer->renderTargets[BasicRenderer::Target_GBufferDepthStencil].getTexture();

		ctx->setFramebufferAndViewport(m_basicRenderer->fbFinal);
		ctx->setShader(m_shOverlay);
		ctx->setMesh(m_mesh, m_renderable->getSelectedLOD(), Max(0, m_submeshOverride));
		ctx->bindBuffer(m_basicRenderer->sceneCamera.m_gpuBuffer); // NB we MUST use the renderer's camera here to account for TAA since we rely on manual depth testing.
		ctx->bindTexture("txDepth", txDepth);
		ctx->setUniform("uWorld", m_renderable->getParentNode()->getWorld());
		ctx->setUniform("uAlpha", m_overlayAlpha);
		ctx->setUniform("uMode", m_overlayMode);
		ctx->draw();

		if (m_wireframe && m_overlayMode != Mode_None)
		{
			//glScopedEnable(GL_CULL_FACE, GL_FALSE);
			glAssert(glPolygonMode(GL_FRONT_AND_BACK, GL_LINE));
			glAssert(glLineWidth(3.0f)); 
			ctx->setShader(m_shWireframe);
			ctx->setMesh(m_mesh, m_renderable->getSelectedLOD(), Max(0, m_submeshOverride));
			ctx->bindBuffer(m_basicRenderer->sceneCamera.m_gpuBuffer); // NB we MUST use the renderer's camera here to account for TAA since we rely on manual depth testing.
			ctx->bindTexture("txDepth", txDepth);
			ctx->setUniform("uWorld", m_renderable->getParentNode()->getWorld());
			ctx->setUniform("uAlpha", m_overlayAlpha);
			ctx->setUniform("uMode", m_overlayMode);
			ctx->draw();
			glAssert(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));			
		}
	}	
	
	ctx->blitFramebuffer(m_basicRenderer->fbFinal, nullptr);

	AppBase::draw();
}

// PROTECTED

const char* MeshViewer::kOverlayModeStr[Mode_Count] =
{
	"None",
	"Normals",
	"Tangents",
	"Colors",
	"MaterialUVs",
	"LightmapUVs",
	"BoneWeights",
	"BoneIndices",
};

void MeshViewer::resetCamera()
{
	if (m_cameraController)
	{
		const Sphere bs = m_mesh->getBoundingSphere();
		const float r = bs.m_radius;
		const float fov = m_camera->m_up; // tan(fov/2)
		
		m_cameraController->setTarget(vec3(0.0f));
		m_cameraController->setTranslateRate(r / 200.0f);

		const float projectedScale = 0.6f;
		const float d = 1.0f / (projectedScale / (r / fov));
		m_cameraController->setRadius(d);

		// \todo This doesn't work as expected?
		if ((d - m_camera->m_near) < r)
		{
			m_camera->m_near = Max(1e-2f, d - r);
		}
	}
}

bool MeshViewer::initScene()
{
	World* world = World::GetCurrent();
	Scene* scene = world->getRootScene();

 // Camera

	SceneNode* cameraNode = scene->createTransientNode("OrbitCamera");

	m_cameraController = (OrbitLookComponent*)Component::Create(StringHash("OrbitLookComponent"));
	FRM_ASSERT(m_cameraController);
	world->setInputConsumer((Component*)m_cameraController);
	cameraNode->addComponent((Component*)m_cameraController);

	CameraComponent* cameraComponent = (CameraComponent*)Component::Create(StringHash("CameraComponent"));
	FRM_ASSERT(cameraComponent);
	world->setDrawCameraComponent(cameraComponent);
	world->setCullCameraComponent(cameraComponent);
	cameraNode->addComponent(cameraComponent);
	m_camera = &cameraComponent->getCamera();
	m_camera->setPerspective(Radians(25.0f), 1.0f, 0.05f, 1000.0f, Camera::ProjFlag_Default);

	FRM_VERIFY(cameraNode->init() && cameraNode->postInit());

 // Lights

	SceneNode* environmentNode = scene->createTransientNode("Environment");
	m_environment = ImageLightComponent::Create(m_environmentPath.c_str());
	environmentNode->addComponent(m_environment);
	FRM_VERIFY(environmentNode->init() && environmentNode->postInit());
	m_environment->setIsBackground(true);
	m_environment->setBackgroundLod(0.25f * m_environment->getTexture()->getMipCount());

	//SceneNode* sunLightNode = scene->createTransientNode("SunLight");
	//BasicLightComponent* sunLight = BasicLightComponent::CreateDirect(vec3(1.0f), 4.0f, true);
	//sunLightNode->addComponent(sunLight);
	//sunLightNode->setInitial(LookAt(vec3(0.0f), vec3(5.0f, 10.0f, 10.0f)));
	//FRM_VERIFY(sunLightNode->init() && sunLightNode->postInit());

	return true;
}

bool MeshViewer::initMeshMaterial()
{	
	World* world = World::GetCurrent();
	Scene* scene = world->getRootScene();

	m_mesh = DrawMesh::Create(m_meshPath.c_str());
	FRM_ASSERT(CheckResource(m_mesh));

	m_material = BasicMaterial::Create(m_materialPath.c_str());
	FRM_ASSERT(CheckResource(m_material));

	if (m_renderable)
	{
		m_renderable->setMesh(m_mesh);
		m_renderable->setMaterial(m_material);
	}
	else
	{
		SceneNode* meshNode = scene->createTransientNode("Mesh");
		m_renderable = BasicRenderableComponent::Create(m_mesh, m_material);
		meshNode->addComponent(m_renderable);
		
		FRM_VERIFY(meshNode->init() && meshNode->postInit());
	}

	resetCamera();

	return true;
}

