#include "PhysicsGeometry.h"
#include "PhysicsInternal.h"

#include <frm/core/hash.h>
#include <frm/core/log.h>
#include <frm/core/memory.h>
#include <frm/core/FileSystem.h>
#include <frm/core/Json.h>
#include <frm/core/MeshData.h>
#include <frm/core/Serializer.h>

#include <imgui/imgui.h>

#include <PxPhysicsAPI.h>

using namespace frm;

static const char* kTypeStr[PhysicsGeometry::Type_Count]
{
	"Sphere",       //Type_Sphere,
	"Box",          //Type_Box,
	"Plane",        //Type_Plane,
	"Capsule",      //Type_Capsule,
	"ConvexMesh",   //Type_ConvexMesh,
	"TriangleMesh", //Type_TriangleMesh,
	"Heightfield",  //Type_Heightfield,
	"Invalid"
};

// PUBLIC

PhysicsGeometry* PhysicsGeometry::Create(const char* _path)
{
	Id id = GetHashId(_path);
	PhysicsGeometry* ret = Find(id);
	if (!ret) 
	{
		ret = FRM_NEW(PhysicsGeometry(id, FileSystem::GetFileName(_path).c_str()));
		ret->m_path.set(_path);
	}
	return ret;
}

PhysicsGeometry* PhysicsGeometry::CreateSphere(float _radius, const char* _name)
{
	Id id = GetUniqueId();
	String<32> name = _name ? _name : String<32>("PhysicsSphere%llu", id);
	PhysicsGeometry* ret = FRM_NEW(PhysicsGeometry(id, name.c_str()));
	ret->m_type = Type_Sphere;
	ret->m_data.sphere.radius = _radius;
	return ret;
}

PhysicsGeometry* PhysicsGeometry::CreateBox(const vec3& _halfExtents, const char* _name)
{
	Id id = GetUniqueId();
	String<32> name = _name ? _name : String<32>("PhysicsBox%llu", id);
	PhysicsGeometry* ret = FRM_NEW(PhysicsGeometry(id, name.c_str()));
	ret->m_type = Type_Box;
	ret->m_data.box.halfExtents = _halfExtents;
	return ret;
}

PhysicsGeometry* PhysicsGeometry::CreateCapsule(float _radius, float _halfHeight, const char* _name)
{
	Id id = GetUniqueId();
	String<32> name = _name ? _name : String<32>("PhysicsCapsule%llu", id);
	PhysicsGeometry* ret = FRM_NEW(PhysicsGeometry(id, name.c_str()));
	ret->m_type = Type_Capsule;
	ret->m_data.capsule.radius = _radius;
	ret->m_data.capsule.halfHeight = _halfHeight;
	return ret;
}

PhysicsGeometry* PhysicsGeometry::CreatePlane(const vec3& _normal, const vec3& _origin, const char* _name)
{
	Id id = GetUniqueId();
	String<32> name = _name ? _name : String<32>("PhysicsPlane%llu", id);
	PhysicsGeometry* ret = FRM_NEW(PhysicsGeometry(id, name.c_str()));
	ret->m_type = Type_Plane;
	ret->m_data.plane.normal = Normalize(_normal);
	ret->m_data.plane.offset = Dot(_normal, _origin);
	return ret;
}

PhysicsGeometry* PhysicsGeometry::CreateConvexMesh(const char* _path, const char* _name)
{
	Id id = GetUniqueId();
	String<32> name = _name ? _name : String<32>("PhysicsConvexMesh%llu", id);
	PhysicsGeometry* ret = FRM_NEW(PhysicsGeometry(id, name.c_str()));
	ret->m_type = Type_ConvexMesh;
	ret->m_dataPath = _path;
	return ret;
}

PhysicsGeometry* PhysicsGeometry::CreateTriangleMesh(const char* _path, const char* _name)
{
	Id id = GetUniqueId();
	String<32> name = _name ? _name : String<32>("PhysicsTrianlgeMesh%llu", id);
	PhysicsGeometry* ret = FRM_NEW(PhysicsGeometry(id, name.c_str()));
	ret->m_type = Type_TriangleMesh;
	ret->m_dataPath = _path;
	return ret;
}

PhysicsGeometry* PhysicsGeometry::Create(Serializer& _serializer_)
{
	Id id = GetUniqueId();
	String<32> name("PhysicsGeometry%llu", id);
	PhysicsGeometry* ret = FRM_NEW(PhysicsGeometry(id, name.c_str()));
	ret->serialize(_serializer_);
	return ret;
}

void PhysicsGeometry::Destroy(PhysicsGeometry*& _inst_)
{
	FRM_DELETE(_inst_);
	_inst_ = nullptr;
}

bool PhysicsGeometry::reload()
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

	return initImpl();
}

bool PhysicsGeometry::edit()
{
	bool ret = false;

	ImGui::PushID(this);
	String<32> buf = m_name;
	if (ImGui::InputText("Name", &buf[0], buf.getCapacity(), ImGuiInputTextFlags_EnterReturnsTrue) && buf[0] != '\0')
	{
		m_name.set(buf.c_str());
		ret = true;
	}
	
	if (ImGui::Combo("Type", &m_type, kTypeStr, Type_Count))
	{
		ret = true;
		// \todo need to reinit any Component_Physics instances which reference this
	}
	switch (m_type)
	{
		default:
			break;
		case Type_Sphere:
			ret |= ImGui::SliderFloat("Radius", &m_data.sphere.radius, 1e-4f, 16.0f);
			break;
		case Type_Box:
			ret |= ImGui::SliderFloat3("Half Extents", &m_data.box.halfExtents.x, 1e-4f, 16.0f);
			break;
		case Type_Plane:
		{
			// \todo better editor for this?
			ret |= ImGui::SliderFloat3("Normal", &m_data.plane.normal.x, -1.0f, 1.0f); 
			m_data.plane.normal = Normalize(m_data.plane.normal);
			ret |= ImGui::DragFloat("Offset", &m_data.plane.offset);
			break;
		}
		case Type_ConvexMesh:
		case Type_TriangleMesh:
		{
			if (ImGui::Button("Mesh Data"))
			{
				PathStr dataPath = m_dataPath;
				if (FileSystem::PlatformSelect(dataPath))
				{
					m_dataPath = dataPath;
					ret = true;
				}
			}
			ImGui::SameLine();
			ImGui::Text(internal::StripPath(m_dataPath.c_str()));
			break;
		}
	};

	ImGui::Spacing();
	bool save = false;
	if (!m_path.isEmpty())
	{
		if (ImGui::Button(ret ? "Save*" : "Save"))
		{
			save = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Load"))
		{
			reload();
			ret = false;
		}
		ImGui::SameLine();
	}
	if (ImGui::Button(ret ? "Save As*" : "Save As"))
	{
		if (FileSystem::PlatformSelect(m_path, { "*.physgeo" }))
		{
			save = true;
			FileSystem::SetExtension(m_path, "physgeo");
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
	
	return ret;
}

bool PhysicsGeometry::serialize(Serializer& _serializer_)
{
	bool ret = true;

	ret |= SerializeEnum<Type, Type_Count>(_serializer_, m_type, kTypeStr, "Type");

	switch (m_type)
	{
		default:
		{
			FRM_LOG_ERR("PhysicsGeometry::serialize -- Invalid type (%d)", m_type);
			ret = false;
			break;
		}
		case Type_Sphere:
		{
			ret |= Serialize(_serializer_, m_data.sphere.radius, "Radius");
			break;
		}
		case Type_Box:
		{
			ret |= Serialize(_serializer_, m_data.box.halfExtents, "HalfExtents");
			break;
		}
		case Type_Capsule:
		{
			ret |= Serialize(_serializer_, m_data.capsule.radius, "Radius");
			ret |= Serialize(_serializer_, m_data.capsule.halfHeight, "HalfHeight");
			break;
		}
		case Type_Plane:
		{
			ret |= Serialize(_serializer_, m_data.plane.normal, "Normal");
			ret |= Serialize(_serializer_, m_data.plane.offset, "Offset");
			break;
		}
		case Type_ConvexMesh:
		case Type_TriangleMesh:
		{
			ret |= Serialize(_serializer_, m_dataPath, "DataPath");
			break;
		}
	};
	
	Serialize(_serializer_, m_name, "Name"); // optional
	setState(ret ? State_Unloaded : State_Error);

	return ret;
}

PhysicsGeometry::Id PhysicsGeometry::getHash() const
{
	Id ret = 0;
	ret = Hash<Id>(&m_type, sizeof(m_type), ret);
	ret = Hash<Id>(&m_data, sizeof(m_data), ret);
	ret = HashString<Id>(m_name.c_str(), ret);
	return ret;
}

// PRIVATE

PhysicsGeometry::PhysicsGeometry(uint64 _id, const char* _name)
	: Resource<PhysicsGeometry>(_id, _name)
{
}

PhysicsGeometry::~PhysicsGeometry()
{
	shutdownImpl();
}

bool PhysicsGeometry::initImpl()
{
	shutdownImpl();

	File cachedData;
	PathStr cachedPath;
	if (m_type == Type_ConvexMesh || m_type == Type_TriangleMesh)
	{
		FRM_ASSERT(!m_dataPath.isEmpty()); // \todo

		cachedPath.setf("_cache/%s.physx", FileSystem::GetFileName(m_dataPath.c_str()).c_str());
			
		if (FileSystem::Exists(cachedPath.c_str()))
		{
			DateTime sourceDate = FileSystem::GetTimeModified(m_dataPath.c_str());
			DateTime cachedDate = FileSystem::GetTimeModified(cachedPath.c_str());
			if (sourceDate <= cachedDate)
			{
				FRM_LOG("PhysicsGeometry: Loading cached data '%s'", cachedPath.c_str());
				if (!FileSystem::Read(cachedData, cachedPath.c_str()))
				{
					FRM_LOG_ERR("PhysicsGeometry: Error loading cached data '%s'", cachedPath.c_str());
				}
			}
		}
	}

	physx::PxGeometryHolder* geometryUnion = FRM_NEW(physx::PxGeometryHolder);
	switch (m_type)
	{
			break;
		case Type_Sphere:
			geometryUnion->sphere() = physx::PxSphereGeometry(m_data.sphere.radius);
			break;
		case Type_Box:
			geometryUnion->box() = physx::PxBoxGeometry(Vec3ToPx(m_data.box.halfExtents));
			break;
		case Type_Plane:
			geometryUnion->plane() = physx::PxPlaneGeometry(); // plane geometry requires a local pose on the shape, see Physics:InitImpl().
			break;
		case Type_Capsule:
			geometryUnion->capsule() = physx::PxCapsuleGeometry(m_data.capsule.radius, m_data.capsule.halfHeight);
			break;
		case Type_ConvexMesh:
		{
			if (cachedData.getDataSize() == 0)
			{
				MeshData* meshData = MeshData::Create(m_dataPath.c_str());
				physx::PxDefaultMemoryOutputStream pxOutput;
				if (!CookConvexMesh(meshData, pxOutput))
				{
					setState(State_Error);
					return false;
				}
				cachedData.setData((const char*)pxOutput.getData(), pxOutput.getSize()); // \todo avoid this copy?
				FileSystem::Write(cachedData, cachedPath.c_str());
			}

			physx::PxDefaultMemoryInputData pxInput((physx::PxU8*)cachedData.getData(), (physx::PxU32)cachedData.getDataSize());
			geometryUnion->convexMesh() = physx::PxConvexMeshGeometry(g_pxPhysics->createConvexMesh(pxInput)); // \todo release the convex mesh?

			break;
		}
		case Type_TriangleMesh:
		{
			if (cachedData.getDataSize() == 0)
			{
				MeshData* meshData = MeshData::Create(m_dataPath.c_str());
				physx::PxDefaultMemoryOutputStream pxOutput;
				if (!CookTriangleMesh(meshData, pxOutput))
				{
					setState(State_Error);
					return false;
				}
				cachedData.setData((const char*)pxOutput.getData(), pxOutput.getSize()); // \todo avoid this copy?
				FileSystem::Write(cachedData, cachedPath.c_str());
			}

			physx::PxDefaultMemoryInputData pxInput((physx::PxU8*)cachedData.getData(), (physx::PxU32)cachedData.getDataSize());
			geometryUnion->triangleMesh() = physx::PxTriangleMeshGeometry(g_pxPhysics->createTriangleMesh(pxInput)); // \todo release the tri mesh?

			break;
		}
		case Type_Heightfield:
		default:
			FRM_ASSERT(false);
			break;		
	};
	m_impl = geometryUnion;
	
	return true;
}

void PhysicsGeometry::shutdownImpl()
{
	if (m_impl)
	{
		FRM_DELETE((physx::PxGeometry*)m_impl);
		m_impl = nullptr;
	}
}
