#include "CameraComponent.h"

#include <frm/core/Profiler.h>
#include <frm/core/Serializable.inl>
#include <frm/core/World/World.h>

#include <imgui/imgui.h>
#include <im3d/im3d.h>

namespace frm {

// Store the previous draw/cull cameras for toggling. Note that this should technically be per world.
static CameraComponent* s_prevDrawCameraComponent = nullptr;
static CameraComponent* s_prevCullCameraComponent = nullptr;

FRM_COMPONENT_DEFINE(CameraComponent, 0);

// PUBLIC

void CameraComponent::Update(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase)
{
	PROFILER_MARKER_CPU("CameraComponent::Update");

	if (_phase != World::UpdatePhase::PreRender)
	{
		return;
	}

	for (; _from != _to; ++_from)
	{
		CameraComponent* component = (CameraComponent*)*_from;
		
		component->m_camera.m_world = component->getParentNode()->getWorld();
		component->m_camera.update();
	}
}

void CameraComponent::lookAt(const vec3& _from, const vec3& _to, const vec3& _up)
{
	m_camera.lookAt(_from, _to, _up);
	getParentNode()->setLocal(m_camera.m_world);
}

// PRIVATE

void CameraComponent::draw() const
{
	const vec3* verts = m_camera.m_worldFrustum.m_vertices;

	Im3d::PushColor();

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

	Im3d::PopColor();

	Im3d::PushMatrix(m_camera.m_world);
		Im3d::DrawXyzAxes();
	Im3d::PopMatrix();
}

bool CameraComponent::initImpl()
{
	m_camera.updateGpuBuffer(); // \hack force allocation of GPU buffer.
	return true;
}

void CameraComponent::shutdownImpl()
{
}

bool CameraComponent::editImpl()
{
	draw();	

	World* parentWorld = m_parentNode->getParentWorld();
	const bool isDrawCamera = parentWorld->getDrawCameraComponent() == this;
	ImGui::PushStyleColor(ImGuiCol_Text, isDrawCamera ? (ImVec4)ImColor(0xff3380ff) : ImGui::GetStyle().Colors[ImGuiCol_Text]);
	if (ImGui::Button(ICON_FA_VIDEO_CAMERA " Set Draw Camera"))
	{
		if (isDrawCamera && s_prevDrawCameraComponent)
		{
			parentWorld->setDrawCameraComponent(s_prevDrawCameraComponent);
		}
		else
		{		
			s_prevDrawCameraComponent = parentWorld->getDrawCameraComponent();
			parentWorld->setDrawCameraComponent(this);
		}
	}
	ImGui::PopStyleColor();

	ImGui::SameLine();

	const bool isCullCamera = parentWorld->getCullCameraComponent() == this;
	ImGui::PushStyleColor(ImGuiCol_Text, isCullCamera ? (ImVec4)ImColor(0xff3380ff) : ImGui::GetStyle().Colors[ImGuiCol_Text]);
	if (ImGui::Button(ICON_FA_CUBES " Set Cull Camera"))
	{
		if (isCullCamera && s_prevCullCameraComponent)
		{
			parentWorld->setCullCameraComponent(s_prevCullCameraComponent);
		}
		else
		{		
			s_prevCullCameraComponent = parentWorld->getCullCameraComponent();
			parentWorld->setCullCameraComponent(this);
		}
	}
	ImGui::PopStyleColor();

	ImGui::SameLine();

	ImGui::Spacing();

	return m_camera.edit();
}

bool CameraComponent::serializeImpl(Serializer& _serializer_)
{
	if (!SerializeAndValidateClass(_serializer_))
	{
		return false;
	}

	return Serialize(_serializer_, m_camera);
}

} // namespace frm