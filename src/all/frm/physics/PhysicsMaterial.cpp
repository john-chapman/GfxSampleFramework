#include "PhysicsMaterial.h"
#include "PhysicsInternal.h"

#include <frm/core/memory.h>
#include <frm/core/FileSystem.h>
#include <frm/core/Json.h>
#include <frm/core/Serializer.h>

#include <imgui/imgui.h>

using namespace frm;

// PUBLIC

PhysicsMaterial* PhysicsMaterial::Create(const char* _path)
{
	Id id = GetHashId(_path);
	PhysicsMaterial* ret = Find(id);
	if (!ret) 
	{
		ret = FRM_NEW(PhysicsMaterial(id, FileSystem::GetFileName(_path).c_str()));
		ret->m_path.set(_path);
	}
	return ret;
}

PhysicsMaterial* PhysicsMaterial::Create(float _staticFriction, float _dynamicFriction, float _restitution, const char* _name)
{
	Id id = GetUniqueId();
	String<32> name = _name ? _name : String<32>("PhysicsMaterial%llu", id);
	PhysicsMaterial* ret = FRM_NEW(PhysicsMaterial(id, name.c_str()));
	return ret;
}

PhysicsMaterial* PhysicsMaterial::Create(Serializer& _serializer_)
{
	PhysicsMaterial* ret = Create(0.5f, 0.5f, 0.5f); // create unique, calls Use()
	ret->serialize(_serializer_);
	return ret;
}

void PhysicsMaterial::Destroy(PhysicsMaterial*& _inst_)
{
	FRM_DELETE(_inst_);
}

bool PhysicsMaterial::reload()
{
	if (!m_path.isEmpty())
	{
		File file;
		if (!FileSystem::Read(file, m_path.c_str()))
		{
			return false;
		}
		m_path = file.getPath(); // use f.getPath() to include the root - this is required for reload to work correctly
	
		Json json;
		if (!Json::Read(json, file))
		{
			return false;
		}
	
		SerializerJson serializer(json, SerializerJson::Mode_Read);
		if (!serialize(serializer))
		{
			return false;
		}
	}

	updateImpl();

	setState(State_Loaded);
	
	return true;
}

bool PhysicsMaterial::edit()
{
	bool ret = false;

	ImGui::PushID(this);
	String<32> buf = m_name;
	if (ImGui::InputText("Name", &buf[0], buf.getCapacity(), ImGuiInputTextFlags_EnterReturnsTrue) && buf[0] != '\0')
	{
		m_name.set(buf.c_str());
		ret = true;
	}
	ret |= ImGui::SliderFloat("Static Friction", &m_staticFriction, 0.0f, 1.0f);
	ret |= ImGui::SliderFloat("Dynamic Friction", &m_dynamicFriction, 0.0f, 1.0f);
	ret |= ImGui::SliderFloat("Restitution", &m_restitution, 0.0f, 1.0f);

	ImGui::Spacing();
	bool save = false;
	if (!m_path.isEmpty())
	{
		if (ImGui::Button("Save"))
		{
			save = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Reload"))
		{
			reload();
			ret = false;
		}
		ImGui::SameLine();
	}
	if (ImGui::Button("Save As"))
	{
		if (FileSystem::PlatformSelect(m_path, { "*.physmat" }))
		{
			save = true;
			FileSystem::SetExtension(m_path, "physmat");
		}
	}
	if (save)
	{
		Json json;
		SerializerJson serializer(json, SerializerJson::Mode_Write);
		if (serialize(serializer))
		{
			Json::Write(json, m_path.c_str());
		}
	}

	ImGui::PopID();

	m_staticFriction = Max(m_staticFriction, m_dynamicFriction); // matches behavior of PxMaterial

	if (ret)
	{
		updateImpl();
	}
	return ret;
}

bool PhysicsMaterial::serialize(Serializer& _serializer_)
{
	bool ret = true;
	ret &= Serialize(_serializer_, m_staticFriction,  "m_staticFriction");
	ret &= Serialize(_serializer_, m_dynamicFriction, "m_dynamicFriction");
	ret &= Serialize(_serializer_, m_restitution,     "m_restitution");
	       Serialize(_serializer_, m_name,            "m_name"); // optional
	setState(ret ? State_Unloaded : State_Error);
	return ret;
}

// PRIVATE

PhysicsMaterial::PhysicsMaterial(uint64 _id, const char* _name)
	: Resource<PhysicsMaterial>(_id, _name)
{
}

PhysicsMaterial::~PhysicsMaterial()
{
	if (m_impl)
	{
		((physx::PxMaterial*)m_impl)->release();
		m_impl = nullptr;
	}
}

void PhysicsMaterial::updateImpl()
{
	if (!m_impl)
	{
		m_impl = g_pxPhysics->createMaterial(m_staticFriction, m_dynamicFriction, m_restitution);
	}

	physx::PxMaterial* pxMaterial = (physx::PxMaterial*)m_impl;
	pxMaterial->setRestitution(m_restitution);
	pxMaterial->setDynamicFriction(m_dynamicFriction);
	pxMaterial->setStaticFriction(m_staticFriction);
}
