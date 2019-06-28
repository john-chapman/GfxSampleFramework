#include "Component.h"

#include <frm/core/BasicMaterial.h>
#include <frm/core/Mesh.h>
#include <frm/core/Scene.h>
#include <frm/core/Texture.h>

#include <apt/log.h>
#include <apt/FileSystem.h>
#include <apt/Serializer.h>

#include <imgui/imgui.h>
#include <im3d/im3d.h>

using namespace frm;
using namespace apt;

/*******************************************************************************

                                  Component

*******************************************************************************/

APT_FACTORY_DEFINE(Component);


/*******************************************************************************

                            Component_BasicRenderable

*******************************************************************************/

APT_FACTORY_REGISTER_DEFAULT(Component, Component_BasicRenderable);

eastl::vector<Component_BasicRenderable*> Component_BasicRenderable::s_instances;

bool Component_BasicRenderable::init()
{
	shutdown();

	bool ret = true;
	
	m_mesh = Mesh::Create(m_meshPath.c_str());
	if (!m_mesh)
	{
		return false;
	}

	m_materials.resize(m_materialPaths.size());
	for (size_t i = 0; i < m_materialPaths.size(); ++i)
	{
		if (m_materialPaths[i].isEmpty())
		{
			m_materials[i] = nullptr;
			continue;
		}
		m_materials[i] = BasicMaterial::Create(m_materialPaths[i].c_str());
		ret &= m_materials[i] != nullptr;
	}

	s_instances.push_back(this);

	return ret;
}

void Component_BasicRenderable::shutdown()
{
	auto it = eastl::find(s_instances.begin(), s_instances.end(), this);
	if (it != s_instances.end())
	{
		s_instances.erase_unsorted(it);
	}

	for (BasicMaterial* material : m_materials)
	{
		BasicMaterial::Release(material);
	}
	Mesh::Release(m_mesh);
}

void Component_BasicRenderable::update(float _dt)
{
}

bool Component_BasicRenderable::edit()
{
	bool ret = false;
	ImGui::PushID(this);

	if (ImGui::Button("Mesh"))
	{
		PathStr path = m_meshPath;
		if (FileSystem::PlatformSelect(path, { "*.obj", "*.md5" }))
		{
			path = FileSystem::MakeRelative(path.c_str());
			if (path != m_meshPath)
			{
				Mesh* mesh = Mesh::Create(path.c_str());
				if (mesh)
				{
					Mesh::Release(m_mesh);
					m_mesh = mesh;
					m_meshPath = path;
					ret = true;

					if (m_materialPaths.size() < m_mesh->getSubmeshCount())
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
				}
			}
		}
	}
	ImGui::SameLine();
	ImGui::Text(m_meshPath.c_str());

	ImGui::PushID("Materials");
	ImGui::Spacing();
	ImGui::Text("Materials");
	for (size_t i = 0; i < m_materialPaths.size(); ++i)
	{
		ImGui::PushID(i);
		String<16> label(i == 0 ? "Global" : "Submesh %u", i - 1);
		if (ImGui::Button(label.c_str()))
		{
			PathStr path = m_materialPaths[i];
			if (FileSystem::PlatformSelect(path, { "*.json" }))
			{
				path = FileSystem::MakeRelative(path.c_str());
				if (path != m_materialPaths[i])
				{
					BasicMaterial* material = BasicMaterial::Create(path.c_str());
					if (material)
					{
						BasicMaterial::Release(m_materials[i]);
						m_materials[i] = material;
						m_materialPaths[i] = path;
						ret = true;
					}
				}	
			}
		}
		ImGui::SameLine();
		ImGui::Text(m_materialPaths[i].c_str());
		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_TIMES))
		{
			BasicMaterial::Release(m_materials[i]);
			m_materialPaths[i] = "";
			ret = true;
		}
		ImGui::PopID();

	 // if the global material is set don't show the submesh slots
		if (i == 0 && !m_materialPaths[0].isEmpty())
		{
			break;
		}
	}
	ImGui::PopID();

	ImGui::PopID();
	return ret;
}

bool Component_BasicRenderable::serialize(apt::Serializer& _serializer_)
{
	Serialize(_serializer_, m_meshPath, "Mesh");
	uint materialCount = m_materialPaths.size();
	if (_serializer_.beginArray(materialCount, "Material"))
	{
		m_materialPaths.resize(materialCount);
		for (uint i = 0; i < m_materialPaths.size(); ++i)
		{
			Serialize(_serializer_, m_materialPaths[i]);
		}

		_serializer_.endArray();
	}

	return true;
}
