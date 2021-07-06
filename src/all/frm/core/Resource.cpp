#include "Resource.h"

#include <frm/core/hash.h>
#include <frm/core/log.h>
#include <frm/core/String.h>
#include <frm/core/Time.h>

#include <EASTL/algorithm.h>

#include <cstdarg> // va_list

#include <imgui/imgui.h>

using namespace frm;

// PUBLIC

template <typename tDerived>
void Resource<tDerived>::Use(Derived* _inst_)
{
	if (_inst_)
	{
		++(_inst_->m_refs);
		if (_inst_->m_refs == 1 && _inst_->m_state != State_Loaded)
		{
			_inst_->m_state = State_Error;
			if (_inst_->load())
			{
				_inst_->m_state = State_Loaded;
			}
		}
	}
}

template <typename tDerived>
void Resource<tDerived>::Release(Derived*& _inst_)
{
	if (_inst_)
	{
		--(_inst_->m_refs);
		FRM_ASSERT(_inst_->m_refs >= 0);
		if (_inst_->m_refs == 0)
		{
			Derived::Destroy(_inst_);
		}
		_inst_ = nullptr;
	}
}

template <typename tDerived>
bool Resource<tDerived>::ReloadAll()
{
	bool ret = true;
	for (auto& inst : s_instances)
	{
		ret &= inst->reload();
	}
	return ret;
}

template <typename tDerived>
tDerived* Resource<tDerived>::Find(Id _id)
{
	for (auto it = s_instances.begin(); it != s_instances.end(); ++it)
	{
		if ((*it)->m_id == _id)
		{
			return *it;
		}
	}
	return nullptr;
}

template <typename tDerived>
tDerived* Resource<tDerived>::Find(const char* _name)
{
	for (auto it = s_instances.begin(); it != s_instances.end(); ++it)
	{
		if ((*it)->m_name == _name)
		{
			return *it;
		}
	}
	return nullptr;
}


// PROTECTED

template <typename tDerived>
typename Resource<tDerived>::Id Resource<tDerived>::GetUniqueId()
{
	Id ret = s_nextUniqueId++;
	FRM_ASSERT(!Find(ret));
	return ret;
}

template <typename tDerived>
typename Resource<tDerived>::Id Resource<tDerived>::GetHashId(const char* _str)
{
	return (Id)HashString<uint32>(_str) << 32;
}

template <typename tDerived>
Resource<tDerived>::Resource(const char* _name)
{
	init(GetHashId(_name), _name);
}

template <typename tDerived>
Resource<tDerived>::Resource(Id _id, const char* _name)
{
	init(_id, _name);
}

template <typename tDerived>
Resource<tDerived>::~Resource()
{
	FRM_ASSERT(m_refs == 0); // resource still in use
	auto it = eastl::find(s_instances.begin(), s_instances.end(), (Derived*)this);
	s_instances.back()->m_index = m_index;
	s_instances.erase_unsorted(it);
}

template <typename tDerived>
void Resource<tDerived>::setNamef(const char* _fmt, ...)
{	
	va_list args;
	va_start(args, _fmt);
	m_name.setfv(_fmt, args);
	va_end(args);
}


// PRIVATE

template <typename tDerived> uint32 Resource<tDerived>::s_nextUniqueId;
template <typename tDerived> typename Resource<tDerived>::InstanceList Resource<tDerived>::s_instances;

template <typename tDerived>
void Resource<tDerived>::init(Id _id, const char* _name)
{
	// At this point an id collision is an error; reusing existing resources must happen prior to calling the Resource ctor.
	FRM_ASSERT_MSG(Find(_id) == 0, "Resource '%s' already exists", _name);

	m_state = State_Unloaded;
	m_id = _id;
	m_name.set(_name);
	m_refs = 0;
	m_index = (int)s_instances.size();
	s_instances.push_back((Derived*)this);
}

template <typename tDerived>
Resource<tDerived>::InstanceList::~InstanceList()
{
	#if FRM_RESOURCE_WARN_UNRELEASED
		if (size() != 0)
		{
			String<256> list;
			for (auto inst : (*this))
			{
				list.appendf("\n\t'%s' -- %d refs", inst->getName(), inst->getRefCount());
			}
			list.append("\n");
			FRM_LOG_ERR("Warning: %d %s instances were not released:%s", (int)size(), tDerived::s_className, (const char*)list);
		}
	#endif
}

template <typename tDerived>
bool Resource<tDerived>::Select(Derived*& _resource_, const char* _buttonLabel, std::initializer_list<const char*> _fileExtensions)
{
	bool ret = false;

	ImGui::PushID(s_className);
	ImGui::PushID("EditSelect");

	if (ImGui::Button(_buttonLabel))
	{
		ImGui::OpenPopup("SelectPopup");
	}
	
	if (ImGui::BeginPopup("SelectPopup"))
	{
		static ImGuiTextFilter filter;
		filter.Draw("Filter");

		if (!filter.IsActive())
		{
			if (ImGui::Selectable("Load.."))
			{
				PathStr newPath;
				if (FileSystem::PlatformSelect(newPath, _fileExtensions))
				{
					newPath = FileSystem::MakeRelative(newPath.c_str());
					if (!_resource_ || newPath != _resource_->getPath())
					{
						Derived* newResource = Derived::Create(newPath.c_str());
						if (CheckResource(newResource))
						{
							Derived::Release(_resource_);
							_resource_ = newResource;						
							ret = true;

							ImGui::CloseCurrentPopup();
						}
						else
						{
							Release(newResource);
						}
					}	
				}
			}
			ImGui::Separator();
					
			for (int resIndex = 0; resIndex < GetInstanceCount(); ++resIndex)
			{
				Derived* selectResource = GetInstance(resIndex);
				
				if (selectResource == _resource_)
				{
					continue;
				}
				
				if (*selectResource->getPath() != '\0' && filter.PassFilter(selectResource->getName()))
				{
					if (ImGui::Selectable(selectResource->getName()))
					{
						Release(_resource_);
						Use(selectResource);
						_resource_ = selectResource;
						ret = true;

						ImGui::CloseCurrentPopup();
					}
				}
			}
		}

		ImGui::EndPopup();
	}
	
	ImGui::PopID();
	ImGui::PopID();

	return ret;
}

namespace {

template <typename tType>
void ResourceView(bool _showHidden)
{
	const int instanceCount = Resource<tType>::GetInstanceCount();

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	
	constexpr ImGuiTreeNodeFlags treeNodeFlags = 0
		| ImGuiTreeNodeFlags_OpenOnArrow 
		| ImGuiTreeNodeFlags_SpanFullWidth
		;
	if (ImGui::TreeNodeEx(String<32>("%s (%d)###%s", Resource<tType>::GetClassName(), instanceCount, Resource<tType>::GetClassName()).c_str(), treeNodeFlags))
	{
		for (int i = 0; i < instanceCount; ++i)
		{
			const Resource<tType>* instance = Resource<tType>::GetInstance(i);
			const char* instanceName = instance->getName();
			if (!_showHidden && *instanceName == '#')
			{
				continue;
			}

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text(instanceName);
			ImGui::TableNextColumn();
			ImGui::Text("%d", (int)instance->getRefCount());
		}

		ImGui::TreePop();
	}
}

} // namespace

void frm::ShowResourceViewer(bool* _open_)
{
	static bool showHidden = false;

	ImGui::SetNextWindowPos(ImVec2(0.0f, ImGui::GetFrameHeightWithSpacing()), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x / 2, ImGui::GetIO().DisplaySize.y / 2), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Resource Viewer", _open_))
	{
		ImGui::End();
		return; // window collapsed, early-out
	}

	constexpr ImGuiTableFlags tableFlags = 0
		| ImGuiTableFlags_ScrollY
		| ImGuiTableFlags_BordersV 
		| ImGuiTableFlags_BordersOuterH
		| ImGuiTableFlags_RowBg 
		| ImGuiTableFlags_SizingStretchSame
		| ImGuiTableFlags_Resizable
		;
	constexpr ImGuiTableColumnFlags columnFlags = 0
		| ImGuiTableColumnFlags_NoReorder
		| ImGuiTableColumnFlags_NoHide
		;

	ImGui::Checkbox("Show Hidden", &showHidden);

	if (ImGui::BeginTable("ResourceViewer", 2, tableFlags))
	{
		ImGui::TableSetupColumn("Resource", columnFlags);
		ImGui::TableSetupColumn("# Refereces", columnFlags);
		ImGui::TableHeadersRow();

		#define RESOURCE_VIEW(_name) \
			ResourceView<_name>(showHidden)

		RESOURCE_VIEW(BasicMaterial);
		RESOURCE_VIEW(DrawMesh);
		RESOURCE_VIEW(SkeletonAnimation);
		RESOURCE_VIEW(Shader);
		RESOURCE_VIEW(SplinePath);
		RESOURCE_VIEW(Texture);

		#if FRM_MODULE_AUDIO
			RESOURCE_VIEW(AudioData);
		#endif

		#if FRM_MODULE_PHYSICS
			RESOURCE_VIEW(PhysicsMaterial);
			RESOURCE_VIEW(PhysicsGeometry);
		#endif

		#undef RESOURCE_VIEW
		ImGui::EndTable();
	}
	
	ImGui::End();
}

#define DECL_RESOURCE(_name) \
	template class Resource<_name>; \
	const char* Resource<_name>::s_className = #_name;

#include <frm/core/BasicRenderer/BasicMaterial.h>
DECL_RESOURCE(BasicMaterial);
#include <frm/core/DrawMesh.h>
DECL_RESOURCE(DrawMesh);
#include <frm/core/SkeletonAnimation.h>
DECL_RESOURCE(SkeletonAnimation);
#include <frm/core/Shader.h>
DECL_RESOURCE(Shader);
#include <frm/core/SplinePath.h>
DECL_RESOURCE(SplinePath);
#include <frm/core/Texture.h>
DECL_RESOURCE(Texture);

#if FRM_MODULE_AUDIO
	#include <frm/audio/AudioData.h>
	DECL_RESOURCE(AudioData);
#endif

#if FRM_MODULE_PHYSICS
	#include <frm/physics/PhysicsMaterial.h>
	DECL_RESOURCE(PhysicsMaterial);
	#include <frm/physics/PhysicsGeometry.h>
	DECL_RESOURCE(PhysicsGeometry);
#endif