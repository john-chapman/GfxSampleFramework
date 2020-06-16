#include "AppSampleVR.h"

#include <frm/core/frm.h>
#include <frm/core/BasicRenderer.h>
#include <frm/core/Component.h>
#include <frm/core/Framebuffer.h>
#include <frm/core/GlContext.h>
#include <frm/core/Profiler.h>
#include <frm/core/Properties.h>
#include <frm/core/Scene.h>
#include <frm/vr/VRContext.h>
#include <frm/vr/VRInput.h>

#if FRM_MODULE_PHYSICS
	#include <frm/physics/Physics.h>
	#include <frm/physics/PhysicsConstraint.h>
	#include <frm/physics/PhysicsGeometry.h>
#endif

#include <im3d/im3d.h>
#include <imgui/imgui.h>

namespace frm {

// PUBLIC

bool AppSampleVR::init(const ArgList& _args)
{
	if (!AppSample3d::init(_args))
	{
		return false;
	}
	
	m_vrContext = VRContext::Create();
	if (!m_vrContext)
	{
		return false;
	}

	// \todo 
	// - TAA requires previous frame, therefore some render targets need to be full stereo.
	// - Configurable render resolution + properties. Need to update the viewports on the VR context if doing dynamic res!
	Viewport viewport = m_vrContext->getViewport(VRContext::Eye_Left, VRContext::Layer_Main);
	m_renderer = BasicRenderer::Create(viewport.w, viewport.h);
	m_renderer->setFlag(BasicRenderer::Flag_FXAA, true);
	m_renderer->setFlag(BasicRenderer::Flag_TAA, false);
	m_renderer->setFlag(BasicRenderer::Flag_WriteToBackBuffer, false);
	m_renderer->motionBlurTargetFps = m_vrContext->getHMDRefreshRate() * 0.5f; // From adhoc testing; ~50% is comfortable without introducing too much 'false' blur when rotating objects in sync with the HMD.

	// Hand models.
	for (int handIndex = 0; handIndex < VRContext::Hand_Count; ++handIndex)
	{
		m_handNodes[handIndex] = Scene::GetCurrent()->createNode(Node::Type_Object);
		m_handNodes[handIndex]->setNamef("#VR%sHand", handIndex == VRContext::Hand_Left ? "Left" : "Right");
	}
	initHands();

	return true;
}

void AppSampleVR::shutdown()
{
	BasicRenderer::Destroy(m_renderer);
	VRContext::Destroy(m_vrContext);

	AppSample3d::shutdown();
}

bool AppSampleVR::update()
{
	vec3 userPosition = m_vrController.getPosition();
	quat userOrientation = RotationQuaternion(vec3(0.f, 1.f, 0.f), Radians(m_vrController.getOrientation()));
	if (!m_vrContext->update((float)getDeltaTime(), userPosition, userOrientation))
	{
		return false;
	}

	m_vrActive = m_vrContext->isActive();

	VRInput& input = m_vrContext->getInputDevice();

	// Select dominant hand based on last press of trigger/grip.
	if (m_vrActive)
	{
		if (input.wasPressed(VRInput::Button_RTrigger) || input.wasPressed(VRInput::Button_RGrip))
		{
			m_dominantHand = VRContext::Hand_Right;
		}
		else if (input.wasPressed(VRInput::Button_LTrigger) || input.wasPressed(VRInput::Button_LGrip))
		{
			m_dominantHand = VRContext::Hand_Left;
		}
	}

	if (!AppSample3d::update())
	{
		return false;
	}

	updateHands();
	m_vrController.update((float)getDeltaTime(), m_vrContext);

	ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNode("Renderer"))
	{
		m_renderer->edit();
		ImGui::TreePop();
	}

	return true;
}

void AppSampleVR::draw()
{
	GlContext* ctx = GlContext::GetCurrent();
	const float dt = (float)getDeltaTime();

	if (m_drawDebug)
	{
		debugDrawHand(VRContext::Hand_Left);
		debugDrawHand(VRContext::Hand_Right);
		if (!m_vrActive)
		{
			debugDrawHead();
		}
	}

	// \todo Noticed a lot of coil whine on the GPU if beginDraw()/endDraw() is not called.
	m_vrContext->beginDraw();

	if (m_vrActive)
	{
		PROFILER_MARKER("AppSampleVR::draw()");

		ctx->setVsync(GlContext::Vsync_Off);

		// Draw the main view via the renderer.
		// \todo Combined left/right culling frustum.
		m_renderer->nextFrame(dt, m_vrContext->getEyeCamera(0), m_vrContext->getEyeCamera(0));
		for (int eyeIndex = 0; eyeIndex < VRContext::Eye_Count; ++eyeIndex)
		{
			m_currentEye = (VRContext::Eye)eyeIndex;
			Camera* eyeCamera = m_vrContext->getEyeCamera(m_currentEye);
			m_renderer->draw((float)getDeltaTime(), eyeCamera, eyeCamera);

			Framebuffer* fbEye = m_vrContext->getFramebuffer(m_currentEye, VRContext::Layer_Main);
			Viewport     vpEye = m_vrContext->getViewport(m_currentEye, VRContext::Layer_Main);
			Framebuffer* fbSrc = m_renderer->fbFinal;
			ctx->blitFramebuffer(fbSrc, fbSrc->getViewport(), fbEye, vpEye, GL_COLOR_BUFFER_BIT, GL_LINEAR);
		}
		
		// Draw Im3d to each eye. It's more efficient to do this in a single step to avoid copying the geometry to the GPU twice.
		// \todo Need to create a combined depth buffer, only the right eye is valid at this point. Note that a combined texture is required since ovrLayerEyeFovDepth doesn't support a single color texture per-eye depth textures.
		{
			m_vrController.draw((float)getDeltaTime(), m_vrContext);

			Im3d::EndFrame();
			drawIm3d(
				{ m_vrContext->getEyeCamera(VRContext::Eye_Left),                          m_vrContext->getEyeCamera(VRContext::Eye_Right)                          },
				{ m_vrContext->getFramebuffer(VRContext::Eye_Left, VRContext::Layer_Main), m_vrContext->getFramebuffer(VRContext::Eye_Right, VRContext::Layer_Main) },
				{ m_vrContext->getViewport(VRContext::Eye_Left, VRContext::Layer_Main),    m_vrContext->getViewport(VRContext::Eye_Right, VRContext::Layer_Main)    },
				{ nullptr, nullptr }
				);
			Im3d::NewFrame();
		}

		// \todo Options for mirroring to the primary view.
		// - No mirror.
		// - Copy visible subrect of a single eye.
		// - Both eyes.
		ctx->blitFramebuffer(m_vrContext->getFramebuffer(VRContext::Eye_Left, VRContext::Layer_Main), nullptr, GL_COLOR_BUFFER_BIT, GL_LINEAR);

	}
	else
	{
		ctx->setVsync(GlContext::Vsync_On);

		Camera* drawCamera = Scene::GetDrawCamera();
		Camera* cullCamera = Scene::GetCullCamera();
		m_renderer->nextFrame(dt, drawCamera, cullCamera);
		m_renderer->draw(dt, drawCamera, cullCamera);
		ctx->blitFramebuffer(m_renderer->fbFinal, nullptr);
	}
	
	m_vrContext->endDraw();

	AppSample3d::draw();
}

Ray AppSampleVR::getCursorRayW(const Camera* _camera) const
{
	if (m_vrActive)
	{
		const mat4 handPose = m_vrContext->getTrackedData().handPoses[m_dominantHand].pose;
		return Ray(handPose[3].xyz(), -handPose[2].xyz());
	}
	else
	{
		return AppSample3d::getCursorRayW(_camera);
	}
}

Ray AppSampleVR::getCursorRayV(const Camera* _camera) const
{
	if (m_vrActive)
	{
		// \todo combined view space?
		FRM_ASSERT(false);
		return Ray();
	}
	else
	{
		return AppSample3d::getCursorRayV(_camera);
	}
}

// PROTECTED

AppSampleVR::AppSampleVR(const char* _title)
	: AppSample3d(_title)
{
	Properties::PushGroup("AppSampleVR");
		//                  name                 default              min     max     storage
		Properties::Add    ("m_drawDebug",       m_drawDebug,                         &m_drawDebug);
		Properties::Add    ("m_dominantHand",    m_dominantHand,      0,      1,      &m_dominantHand);
	Properties::PopGroup(); // AppSampleVR
}

AppSampleVR::~AppSampleVR()
{
	Properties::InvalidateGroup("AppSampleVR");
}


bool AppSampleVR::initHands()
{
	#if FRM_MODULE_PHYSICS
		PhysicsGeometry* spherePhysicsGeometry = PhysicsGeometry::CreateSphere(0.05f);
	#endif

	for (int handIndex = 0; handIndex < VRContext::Hand_Count; ++handIndex)
	{
		// Create a separate node for the renderable + physics component, attach to the tracked hands via physics constraints.
		Node* handNode = m_scene->createNode(Node::Type_Object, m_handNodes[handIndex]);
		handNode->setNamef("#VR%sHandChild", handIndex == 0 ? "Left" : "Right");

		Component_BasicRenderable* renderable = (Component_BasicRenderable*)Component::Create("Component_BasicRenderable");
		switch (handIndex)
		{
			default:
			case VRContext::Hand_Left:
				renderable->m_meshPath = "models/RiftS/LeftTouchController.gltf";
				renderable->m_materialPaths.push_back("models/RiftS/Left_material.json");
				break;
			case VRContext::Hand_Right:
				renderable->m_meshPath = "models/RiftS/RightTouchController.gltf";
				renderable->m_materialPaths.push_back("models/RiftS/Right_material.json");
				break;
		};
		renderable->m_castShadows = false; // Disable shadow-casting by default; need to fix polygon offset/bias and improve shadow quality in BasicRenderer.
		
		handNode->addComponent(renderable);
	
		#if FRM_MODULE_PHYSICS
		{
			Component_Physics* physics = Component_Physics::Create(spherePhysicsGeometry, Physics::GetDefaultMaterial(), 100.0f, identity, Physics::Flag::Kinematic);
			if (physics)
			{
				handNode->addComponent(physics);
			}
		
			// \todo 
			// Distance constraints aren't useful here, need a custom IK constraint.
			// Probably also want to suppress collisions during snap motion.
			/*PhysicsConstraint::Distance jointData;
			jointData.minDistance = 0.f;
			jointData.maxDistance = 0.0f;
			jointData.spring.stiffness = 100.f;
			jointData.spring.damping = 1.0f;
			m_handNodeJoints[handIndex] = PhysicsConstraint::CreateDistance(nullptr, identity, physics, identity, jointData);*/
		}
		#endif
	}

	return true;
}

void AppSampleVR::updateHands()
{
	const VRContext::TrackedData& trackedData = m_vrContext->getTrackedData();
	for (int handIndex = 0; handIndex < VRContext::Hand_Count; ++handIndex)
	{
		m_handNodes[handIndex]->setLocalMatrix(trackedData.handPoses[handIndex].pose);
		#if FRM_MODULE_PHYSICS
			if (m_handNodeJoints[handIndex])
			{
				m_handNodeJoints[handIndex]->setComponentFrame(0, trackedData.handPoses[handIndex].pose);
			}
		#endif
	}
}

void AppSampleVR::debugDrawHand(VRContext::Hand _hand)
{
	const VRContext::PoseData& handPose = m_vrContext->getTrackedData().handPoses[_hand];
	const vec3& handPosition = handPose.getPosition();

	Im3d::PushDrawState();

	// Hand pose.
	Im3d::PushMatrix(handPose.pose * ScaleMatrix(vec3(0.1f)));
		Im3d::SetSize(2.0f);
		Im3d::DrawXyzAxes();
	Im3d::PopMatrix();

	// Linear motion.
	Im3d::SetColor(Im3d::Color_Gray);
	Im3d::SetSize(8.0f);
	Im3d::SetAlpha(1.0f);
	Im3d::DrawArrow(handPosition, handPosition + handPose.linearVelocity);

	// Angular motion.
	Im3d::SetSize(8.0f);
	Im3d::SetAlpha(1.0f);
	Im3d::SetColor(Im3d::Color_Red);
	Im3d::DrawArrow(handPosition, handPosition + vec3(1.f, 0.f, 0.f) * handPose.angularVelocity.x * 0.1f);
	Im3d::SetColor(Im3d::Color_Green);
	Im3d::DrawArrow(handPosition, handPosition + vec3(0.f, 1.f, 0.f) * handPose.angularVelocity.y * 0.1f);
	Im3d::SetColor(Im3d::Color_Blue);
	Im3d::DrawArrow(handPosition, handPosition + vec3(0.f, 0.f, 1.f) * handPose.angularVelocity.z * 0.1f);

	Im3d::PopDrawState();
}

void AppSampleVR::debugDrawHead()
{
	const VRContext::PoseData& headPose = m_vrContext->getTrackedData().headPose;
	const vec3& headPosition = headPose.getPosition();

	Im3d::PushDrawState();

	// Head pose.
	Im3d::PushMatrix();
	Im3d::SetMatrix(headPose.pose * ScaleMatrix(vec3(0.2f)));
	Im3d::PushSize();
		Im3d::SetSize(4.0f);
		Im3d::DrawXyzAxes();
	Im3d::PopSize();
	Im3d::PopMatrix();

	// Eye frusta.
	Im3d::PushAlpha(0.5f);
		DrawFrustum(m_vrContext->getEyeCamera(VRContext::Eye_Left)->m_worldFrustum);
		DrawFrustum(m_vrContext->getEyeCamera(VRContext::Eye_Right)->m_worldFrustum); 
	Im3d::PopAlpha();

	// Angular motion.
	Im3d::SetSize(8.0f);
	Im3d::SetAlpha(1.0f);
	Im3d::SetColor(Im3d::Color_Red);
	Im3d::DrawArrow(headPosition, headPosition + vec3(1.f, 0.f, 0.f) * headPose.angularVelocity.x * 0.1f);
	Im3d::SetColor(Im3d::Color_Green);
	Im3d::DrawArrow(headPosition, headPosition + vec3(0.f, 1.f, 0.f) * headPose.angularVelocity.y * 0.1f);
	Im3d::SetColor(Im3d::Color_Blue);
	Im3d::DrawArrow(headPosition, headPosition + vec3(0.f, 0.f, 1.f) * headPose.angularVelocity.z * 0.1f);

	Im3d::PopDrawState();
}

// PRIVATE

} // namespace frm
