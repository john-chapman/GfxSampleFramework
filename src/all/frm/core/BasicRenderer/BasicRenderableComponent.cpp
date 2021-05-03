#include "BasicRenderableComponent.h"
#include "BasicMaterial.h"

#include <frm/core/DrawMesh.h>
#include <frm/core/Profiler.h>
#include <frm/core/Serializable.inl>
#include <frm/core/SkeletonAnimation.h>

#include <imgui/imgui.h>
#include <im3d/im3d.h>

namespace frm {

FRM_COMPONENT_DEFINE(BasicRenderableComponent, 0);

// PUBLIC

void BasicRenderableComponent::Update(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase)
{
	PROFILER_MARKER_CPU("BasicRenderableComponent::Update");

	if (_phase != World::UpdatePhase::PreRender)
	{
		return;
	}

	for (; _from != _to; ++_from)
	{
		BasicRenderableComponent* component = (BasicRenderableComponent*)*_from;

		component->m_prevWorld = component->m_world;
		component->m_world = component->getParentNode()->getWorld();
	}
}

eastl::span<BasicRenderableComponent*> BasicRenderableComponent::GetActiveComponents()
{
	static ComponentList& activeList = (ComponentList&)Component::GetActiveComponents(StringHash("BasicRenderableComponent"));
	return eastl::span<BasicRenderableComponent*>(*((eastl::vector<BasicRenderableComponent*>*)&activeList));
}

BasicRenderableComponent* BasicRenderableComponent::Create(DrawMesh* _mesh, BasicMaterial* _material)
{
	BasicRenderableComponent* ret = (BasicRenderableComponent*)Component::Create(StringHash("BasicRenderableComponent"));
	ret->m_mesh = _mesh;
	ret->m_meshPath = _mesh->getPath();
	ret->m_materials.push_back(_material);
	ret->m_materialPaths.push_back(_material->getPath());

	return ret;
}

void BasicRenderableComponent::setPose(const Skeleton& _skeleton)
{
	const mat4* pose = _skeleton.getPose();
	const size_t boneCount = _skeleton.getBoneCount();
	if (m_mesh)
	{
		//FRM_ASSERT(m_mesh->getBindPose() && m_mesh->getBindPose()->getBoneCount() == boneCount);
		if (m_mesh->getBindPoseSize() != boneCount)
		{
			return;
		}

	// \todo apply the bind pose during Skeleton::resolve()?
		const mat4* bindPose = m_mesh->getBindPose();

		swap(m_pose, m_prevPose);
		m_pose.clear();
		m_pose.reserve(boneCount);
		for (size_t i = 0; i < boneCount; ++i)
		{
			m_pose.push_back(pose[i] * bindPose[i]);
		}

		if (m_prevPose.size() != m_pose.size())
		{
			m_prevPose.assign(m_pose.begin(), m_pose.end());
		}
	}
}

void BasicRenderableComponent::clearPose()
{
	m_pose.clear();
}

// PROTECTED

bool BasicRenderableComponent::initImpl()
{
	bool ret = true;

	// Mesh.
	if (!m_mesh)
	{
		if (m_meshPath.isEmpty())
		{
			m_meshPath = "models/Gear_1.gltf";
		}
		m_mesh = DrawMesh::Create(m_meshPath.c_str());
	}
	else
	{
		// Need to explicitly call Use here (Mesh::Create does it by default).
		DrawMesh::Use(m_mesh);
	}
	if (!CheckResource(m_mesh))
	{
		DrawMesh::Release(m_mesh);
		ret = false;
	}

	// Materials.
	//if (m_materialPaths.size() != m_mesh->getSubmeshCount())
	{
		m_materialPaths.resize(m_mesh->getSubmeshCount());

		while (m_materials.size() > m_materialPaths.size())
		{
			BasicMaterial::Release(m_materials.back());
			m_materials.pop_back();
		}

		while (m_materials.size() < m_materialPaths.size())
		{
			m_materials.push_back(nullptr);
		}
	}
	bool hasMaterialPaths = false;
	for (PathStr& materialPath : m_materialPaths)
	{
		if (!materialPath.isEmpty())
		{
			hasMaterialPaths = true;
			break;
		}
	}
	if (!hasMaterialPaths)
	{
		if (m_materialPaths.empty())
		{
			m_materialPaths.push_back("");
			m_materials.push_back(nullptr);
		}
		m_materialPaths[0] = "materials/BasicMaterial.mat";
	}
	for (size_t i = 0; i < m_materialPaths.size(); ++i)
	{
		if (!m_materialPaths[i].isEmpty())
		{
			m_materials[i] = BasicMaterial::Create(m_materialPaths[i].c_str());
			if (!CheckResource(m_materials[i]))
			{
				BasicMaterial::Release(m_materials[i]);
				ret = false;
			}
		}
	}

	return ret;
}

bool BasicRenderableComponent::postInitImpl()
{
	return true;
}

void BasicRenderableComponent::shutdownImpl()
{
	for (BasicMaterial*& material : m_materials)
	{
		BasicMaterial::Release(material);
		material = nullptr;
	}
	DrawMesh::Release(m_mesh);
	m_mesh = nullptr;

	m_pose.clear();
	m_prevPose.clear();
}

bool BasicRenderableComponent::editImpl()
{
	// \hack Static state for popup material editor.
	struct MaterialEditorState
	{
		bool show = false;
		int  editIndex = -1;                                  // Index into m_materials/m_materialPaths.
		BasicRenderableComponent* callingComponent = nullptr; // Last component which called BasicMaterial::Edit().
	};
	static MaterialEditorState materialEditorState;

	bool ret = false;

	ret |= ImGui::ColorEdit3("Color", &m_colorAlpha.x);
	ret |= ImGui::SliderFloat("Alpha", &m_colorAlpha.w, 0.0f, 1.0f);
	ret |= ImGui::Checkbox("Cast Shadows", &m_castShadows);

	ImGui::Spacing();
	ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNode("Mesh"))
	{
		if (DrawMesh::Select(m_mesh, "Mesh", { "*.mesh", "*.gltf" }))
		{
			m_meshPath = m_mesh->getPath();
			if (ret && m_materialPaths.size() != m_mesh->getSubmeshCount())
			{
				m_materialPaths.resize(m_mesh->getSubmeshCount());

				while (m_materials.size() > m_materialPaths.size())
				{
					BasicMaterial::Release(m_materials.back());
					m_materials.pop_back();
				}

				while (m_materials.size() < m_materialPaths.size())
				{
					m_materials.push_back(nullptr);
				}
			}

			ret = true;
		}	
		ImGui::SameLine();
		ImGui::Text(m_meshPath.c_str());

		ImGui::SliderInt("LOD Override", &m_lodOverride, -1, m_mesh->getLODCount() - 1);
		ImGui::SliderInt("Submesh Override", &m_subMeshOverride, -1, m_mesh->getSubmeshCount() - 1);

		ImGui::TreePop();
	}

	ImGui::Spacing();
	ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNode("Materials"))
	{
		for (size_t i = 0; i < m_materialPaths.size(); ++i)
		{
			ImGui::PushID((int)i);
			String<16> label(i == 0 ? "Global.." : "Submesh %u..", i); 

			if (BasicMaterial::Select(m_materials[i], label.c_str(), { "*.mat" }))
			{
				m_materialPaths[i] = m_materials[i]->getPath();
				ret = true;
			}

			ImGui::SameLine();
			ImGui::Text(m_materialPaths[i].c_str());
			if (m_materials[i])
			{
				ImGui::SameLine();
				if (ImGui::Button(ICON_FA_TIMES "##delete"))
				{
					BasicMaterial::Release(m_materials[i]);
					m_materials[i] = nullptr;
					m_materialPaths[i] = "";
					ret = true;
				}
				ImGui::SameLine();
				if (ImGui::Button(ICON_FA_EXTERNAL_LINK "##edit"))
				{
					materialEditorState.show = true;
					materialEditorState.editIndex = (int)i;
					materialEditorState.callingComponent = this;
				}
			}
			ImGui::PopID();

		 // if the global material is set don't show the submesh slots
			if (i == 0 && !m_materialPaths[0].isEmpty())
			{
				break;
			}
		}

		ImGui::TreePop();
	}

	// \hack Call the popup material editor.
	if (materialEditorState.callingComponent == this && materialEditorState.show && BasicMaterial::Edit(m_materials[materialEditorState.editIndex], &materialEditorState.show))
	{		
		if (m_materialPaths[materialEditorState.editIndex] != m_materials[materialEditorState.editIndex]->getPath())
		{
			m_materialPaths[materialEditorState.editIndex] = m_materials[materialEditorState.editIndex]->getPath();
			ret = true;
		}
		
		if (!materialEditorState.show) // the window was closed
		{
			materialEditorState.callingComponent = nullptr;
		}
	}

	return ret;
}

bool BasicRenderableComponent::serializeImpl(Serializer& _serializer_)
{
	if (!SerializeAndValidateClass(_serializer_))
	{
		return false;
	}

	Serialize(_serializer_, m_castShadows, "m_castShadows");
	Serialize(_serializer_, m_colorAlpha,  "m_colorAlpha");
	Serialize(_serializer_, m_meshPath,    "m_meshPath");

	uint materialCount = m_materialPaths.size();
	if (_serializer_.beginArray(materialCount, "m_materialPaths"))
	{
		m_materialPaths.resize(materialCount);
		for (uint i = 0; i < m_materialPaths.size(); ++i)
		{
			Serialize(_serializer_, m_materialPaths[i]);
		}

		_serializer_.endArray();
	}

	return _serializer_.getError() == nullptr;
}

} // namespace frm