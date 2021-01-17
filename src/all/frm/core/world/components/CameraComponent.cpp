#include "CameraComponent.h"

#include <frm/core/Profiler.h>
#include <frm/core/Serializable.inl>

#include <imgui/imgui.h>
#include <im3d/im3d.h>

namespace frm {

FRM_COMPONENT_DEFINE(CameraComponent, 0);

// PUBLIC

void CameraComponent::SetDrawCamera(CameraComponent* _cameraComponent)
{
	s_drawCamera[1] = s_drawCamera[0];
	s_drawCamera[0] = _cameraComponent;
}

void CameraComponent::SetCullCamera(CameraComponent* _cameraComponent)
{
	s_cullCamera[1] = s_cullCamera[0];
	s_cullCamera[0] = _cameraComponent;
}

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
		component->draw();
	}
}

void CameraComponent::lookAt(const vec3& _from, const vec3& _to, const vec3& _up)
{
	m_camera.lookAt(_from, _to, _up);
	getParentNode()->setLocal(m_camera.m_world);
}

// PRIVATE

CameraComponent* CameraComponent::s_drawCamera[2] = { nullptr };
CameraComponent* CameraComponent::s_cullCamera[2] = { nullptr };

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
	if (!s_drawCamera[0])
	{
		s_drawCamera[0] = this;
	}
	
	if (!s_cullCamera[0])
	{
		s_cullCamera[0] = this;
	}

	m_camera.updateGpuBuffer(); // \hack force allocation of GPU buffer.

	return true;
}

void CameraComponent::shutdownImpl()
{
	for (CameraComponent*& camera : s_drawCamera)
	{
		camera = (camera == this) ? nullptr : camera;
	}
	
	for (CameraComponent*& camera : s_cullCamera)
	{
		camera = (camera == this) ? nullptr : camera;
	}
}

bool CameraComponent::editImpl()
{
	draw();
	
	const bool isDrawCamera = s_drawCamera[0] == this;
	const bool isCullCamera = s_cullCamera[0] == this;

	ImGui::PushStyleColor(ImGuiCol_Text, isDrawCamera ? (ImVec4)ImColor(0xff3380ff) : ImGui::GetStyle().Colors[ImGuiCol_Text]);
	if (ImGui::Button(ICON_FA_VIDEO_CAMERA " Set Draw Camera"))
	{
		if (isDrawCamera && s_drawCamera[1])
		{
			SetDrawCamera(s_drawCamera[1]);
		}
		else
		{		
			SetDrawCamera(this);
		}
	}
	ImGui::PopStyleColor();

	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Text, isCullCamera ? (ImVec4)ImColor(0xff3380ff) : ImGui::GetStyle().Colors[ImGuiCol_Text]);
	if (ImGui::Button(ICON_FA_CUBES " Set Cull Camera"))
	{
		if (isCullCamera && s_cullCamera[1])
		{
			SetCullCamera(s_cullCamera[1]);
		}
		else
		{		
			SetCullCamera(this);
		}
	}
	ImGui::PopStyleColor();

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