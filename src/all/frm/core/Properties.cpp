#include "Properties.h"

#include <frm/core/log.h>
#include <frm/core/types.h>
#include <frm/core/FileSystem.h>
#include <frm/core/Json.h>
#include <frm/core/Serializer.h>
#include <frm/core/String.h>
#include <frm/core/StringHash.h>

#include <imgui/imgui.h>

namespace frm {

/*******************************************************************************

                                 Properties

*******************************************************************************/

static Properties::Type JsonTypeToPropertiesType(Json::ValueType _type)
{	
	switch (_type)
	{
		default:                      FRM_ASSERT(false); return Json::ValueType_Null;
		case Json::ValueType_Bool:    return Properties::Type_Bool;
		case Json::ValueType_Number:  return Properties::Type_Float; // number types are stored internally as doubles and converted to int/float as appropriate.
		case Json::ValueType_String:  return Properties::Type_String;
	};
}

static Json::ValueType PropertiesTypeToJsonType(Properties::Type _type)
{
	switch (_type)
	{
		default:                      FRM_ASSERT(false); return Json::ValueType_Null;
		case Properties::Type_Bool:   return Json::ValueType_Bool;
		case Properties::Type_Float:
		case Properties::Type_Int:    return Json::ValueType_Number;
		case Properties::Type_String: return Json::ValueType_String;
	};
}

// PUBLIC

const char* Properties::GetTypeStr(Type _type)
{
	switch (_type)
	{
		default:          return "Unknown Type";
		case Type_Bool:   return "Bool";
		case Type_Int:    return "Int";
		case Type_Float:  return "Float";
		case Type_String: return "String";
	};
}

int Properties::GetTypeSizeBytes(Type _type)
{
	switch (_type)
	{
		default:          return 0;
		case Type_Bool:   return (int)sizeof(bool);
		case Type_Int:
		case Type_Float:  return (int)sizeof(double);
		case Type_String: return (int)sizeof(String<32>);
	};
}

#define Properties_GetTypeCount(_T, _retType, _retCount) \
	template <> Properties::Type Properties::GetType<_T>()  { return _retType; } \
	template <> Properties::Type Properties::GetCount<_T>() { return _retCount; }

Properties_GetTypeCount(bool,  Properties::Type_Bool,   1);
Properties_GetTypeCount(bvec2, Properties::Type_Bool,   2);
Properties_GetTypeCount(bvec3, Properties::Type_Bool,   3);
Properties_GetTypeCount(bvec4, Properties::Type_Bool,   4);

Properties_GetTypeCount(int,   Properties::Type_Int,    1);
Properties_GetTypeCount(ivec2, Properties::Type_Int,    2);
Properties_GetTypeCount(ivec3, Properties::Type_Int,    3);
Properties_GetTypeCount(ivec4, Properties::Type_Int,    4);

Properties_GetTypeCount(float, Properties::Type_Float,  1);
Properties_GetTypeCount(vec2,  Properties::Type_Float,  2);
Properties_GetTypeCount(vec3,  Properties::Type_Float,  3);
Properties_GetTypeCount(vec4,  Properties::Type_Float,  4);

Properties_GetTypeCount(StringBase, Properties::Type_String, 1);


bool Properties::DefaultEditFunc(Property& _prop)
{
	constexpr int kStrBufLen = 512;
	void* data = _prop.getExternalStorage();
	FRM_ASSERT(data);

	bool ret = false;
	switch (_prop.m_type)
	{
		case Type_Bool:
			switch (_prop.m_count)
			{
				case 1:
					ret |= ImGui::Checkbox((const char*)_prop.m_displayName, (bool*)data);
					break;
				default:
				{
					String<64> displayName;
					for (int i = 0; i < _prop.m_count; ++i)
					{
						displayName.setf("%s[%d]", (const char*)_prop.m_displayName, i); 
						ret |= ImGui::Checkbox((const char*)displayName, (bool*)data);
					}
					break;
				}	
			};
			break;
		case Type_Int:
			switch (_prop.m_count)
			{
				case 1:
					ret |= ImGui::SliderInt((const char*)_prop.m_displayName, (int*)data, *((int*)_prop.m_min), *((int*)_prop.m_max));
					break;
				case 2:
					ret |= ImGui::SliderInt2((const char*)_prop.m_displayName, (int*)data, *((int*)_prop.m_min), *((int*)_prop.m_max));
					break;
				case 3:
					ret |= ImGui::SliderInt3((const char*)_prop.m_displayName, (int*)data, *((int*)_prop.m_min), *((int*)_prop.m_max));
					break;
				case 4:
					ret |= ImGui::SliderInt4((const char*)_prop.m_displayName, (int*)data, *((int*)_prop.m_min), *((int*)_prop.m_max));
					break;
				default:
					FRM_ASSERT(false); // \todo arbitrary arrays, see bool
					break;
			};
			break;
		case Type_Float:
			switch (_prop.m_count)
			{
				case 1:
					ret |= ImGui::SliderFloat((const char*)_prop.m_displayName, (float*)data, *((float*)_prop.m_min), *((float*)_prop.m_max));
					break;
				case 2:
					ret |= ImGui::SliderFloat2((const char*)_prop.m_displayName, (float*)data, *((float*)_prop.m_min), *((float*)_prop.m_max));
					break;
				case 3:
					ret |= ImGui::SliderFloat3((const char*)_prop.m_displayName, (float*)data, *((float*)_prop.m_min), *((float*)_prop.m_max));
					break;
				case 4:
					ret |= ImGui::SliderFloat4((const char*)_prop.m_displayName, (float*)data, *((float*)_prop.m_min), *((float*)_prop.m_max));
					break;
				default:
					FRM_ASSERT(false); // \todo arbitrary arrays, see bool
					break;
			};
			break;
		case Type_String:
		{
			char buf[kStrBufLen];
			switch (_prop.m_count) {
				case 1:
				{
					StringBase& str = *(StringBase*)data;
					FRM_ASSERT(str.getCapacity() < kStrBufLen);
					memcpy(buf, (const char*)str, str.getLength() + 1);
					if (ImGui::InputText((const char*)_prop.m_displayName, buf, kStrBufLen, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
					{
						str.set(buf);
						ret = true;
					}
					break;
				}
				default:
				{
					String<64> displayName;
					for (int i = 0; i < _prop.m_count; ++i)
					{
						displayName.setf("%s[%d]", (const char*)_prop.m_displayName, i); 
					 	StringBase& str = *(StringBase*)data;
						FRM_ASSERT(str.getCapacity() < kStrBufLen);
						if (ImGui::InputText((const char*)displayName, buf, kStrBufLen, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
						{
							str.set(buf);
							ret = true;
						}
					}
					break;
				};
			};
			break;
		default:
			FRM_ASSERT(false);
			break;
		}
	};
	return ret;
}

bool Properties::ColorEditFunc(Property& _prop_)
{
	FRM_ASSERT(_prop_.getType() == Type_Float);

	float* data = (float*)_prop_.getExternalStorage();
	FRM_ASSERT(data);

	bool ret = false;
	if (_prop_.getCount() == 3)
	{
		ret = ImGui::ColorEdit3(_prop_.getDisplayName(), data);
	}
	else
	{
		ret = ImGui::ColorEdit4(_prop_.getDisplayName(), data);
	}
	return ret;
}

bool Properties::PathEditFunc(Property& _prop_)
{
	bool ret = false;
	void* data = _prop_.getExternalStorage();
	FRM_ASSERT(data);
	PathStr& pth = *((PathStr*)data);

	if (ImGui::Button(String<32>(ICON_FA_FOLDER " %s", _prop_.getDisplayName()).c_str()))
	{
		if (FileSystem::PlatformSelect(pth))
		{
			pth = FileSystem::MakeRelative((const char*)pth);
			ret = true;
		}
	}
	ImGui::SameLine();
	ImGui::Text("\"%s\"", (const char*)pth);
	return ret;
}

void Properties::DefaultDisplayFunc(const Property& _prop)
{
	ImGui::Text("%s: ", (const char*)_prop.m_displayName);
	void* data = _prop.getExternalStorage();
	FRM_ASSERT(data);

	switch (_prop.m_type)
	{
		case Type_Bool:
			for (int i = 0; i < _prop.m_count; ++i)
			{
				ImGui::SameLine();
				ImGui::Text("%d ", (int)((bool*)data)[i]);
			}
			break;
		case Type_Int:
			for (int i = 0; i < _prop.m_count; ++i)
			{
				ImGui::SameLine();
				ImGui::Text("%d ", ((int*)data)[i]);
			}
			break;
		case Type_Float:
			for (int i = 0; i < _prop.m_count; ++i)
			{
				ImGui::SameLine();
				ImGui::Text("%+07.3f ", ((float*)data)[i]);
			}
			break;
		case Type_String:
			for (int i = 0; i < _prop.m_count; ++i)
			{
				ImGui::Text("%s", ((StringBase*)data)[i].c_str());
			}
			break;
		default:
			FRM_ASSERT(false);
			break;
	};
}

void Properties::ColorDisplayFunc(const Property& _prop)
{
	FRM_ASSERT(_prop.getType() == Type_Float);

	ImGui::AlignTextToFramePadding();
	ImGui::Text("%s: ", _prop.getDisplayName());
	ImGui::SameLine();
	const float* data = (float*)_prop.getExternalStorage();
	FRM_ASSERT(data);
	ImGui::ColorButton(_prop.getDisplayName(), ImVec4(data[0], data[1], data[2], data[3]));
}

Properties* Properties::GetDefault()
{
	static Properties s_default("_Default");
	return &s_default;
}

Properties* Properties::GetCurrent()
{
	if (s_groupStack.empty())
	{
		return GetDefault();
	}
	
	return s_groupStack.back();
}

Property* Properties::AddColor(const char* _name, const vec3& _default, vec3* _storage, const char* _displayName)
{
	Property* ret = Add(_name, _default, _storage, _displayName);
	ret->setEditFunc(ColorEditFunc); 
	ret->setDisplayFunc(ColorDisplayFunc);
	return ret;
}

Property* Properties::AddColor(const char* _name, const vec4& _default, vec4* _storage, const char* _displayName)
{
	Property* ret = Add(_name, _default, _storage, _displayName);
	ret->setEditFunc(ColorEditFunc);
	ret->setDisplayFunc(ColorDisplayFunc);
	return ret;
}

Property* Properties::AddPath(const char* _name, const PathStr& _default, PathStr* _storage, const char* _displayName)
{
	Property* ret = Add(_name, (const StringBase&)_default, (StringBase*)_storage, _displayName);
	ret->setEditFunc(PathEditFunc);
	return ret;
}

Property* Properties::Find(const char* _propName, const char* _groupName)
{
	if (!_groupName)
	{
		return GetCurrent()->find(_propName);
	}

	Properties* group = nullptr;
	for (Properties* it : s_groupStack)
	{
		group = it->findGroup(_groupName);
	}

	if (!group)
	{
		group = GetDefault()->findGroup(_groupName);
	}

	if (!group)
	{
		group = GetDefault();
	}

	return group->find(_propName);
}

Properties* Properties::PushGroup(const char* _groupName)
{
	FRM_STRICT_ASSERT(_groupName);
	const StringHash groupHash = GetStringHash(_groupName);

	Properties* parentGroup = GetCurrent();
	Properties*& ret = parentGroup->m_subGroups[groupHash];
	if (!ret)
	{
		ret = FRM_NEW(Properties(_groupName));
	}
	s_groupStack.push_back(ret);
	return ret;
}

void Properties::PopGroup(int _count)
{
	FRM_ASSERT(!s_groupStack.empty());
	FRM_ASSERT(s_groupStack.size() >= _count);
	if (!s_groupStack.empty())
	{
		while (_count > 0)
		{
			s_groupStack.pop_back();
			--_count;
		}
	}
}

void Properties::InvalidateStorage(const char* _propName, const char* _groupName)
{
	Property* prop = Find(_propName, _groupName); // searches current group if !_groupName
	if (prop) 
	{
		prop->setExternalStorage(nullptr);
	}
}

void Properties::InvalidateGroup(const char* _groupName)
{
	Properties* group = nullptr;
	for (Properties* it : s_groupStack)
	{
		group = it->findGroup(_groupName);
	}

	if (!group)
	{
		group = GetDefault()->findGroup(_groupName);
	}

	if (!group)
	{
		group = GetDefault();
	}
	
	for (auto it : group->m_properties)
	{
		it.second->setExternalStorage(nullptr);
	}
}

Properties* Properties::Create(const char* _groupName)
{
	return FRM_NEW(Properties(_groupName));
}

void Properties::Destroy(Properties*& _properties_)
{
	FRM_DELETE(_properties_);
	_properties_ = nullptr;
}


bool Serialize(SerializerJson& _serializer_, Properties& _group_)
{
	bool ret = true;

	if (_serializer_.getMode() == SerializerJson::Mode_Read)
	{
		Json* json = _serializer_.getJson();
		while (json->next())
		{
			const char* name = json->getName();
			Json::ValueType jsonType = json->getType();
			if (jsonType == Json::ValueType_Object)
			{
			 // enter and serialize subgroup
				if (json->enterObject())
				{
					Properties* subGroup = Properties::PushGroup(name);
					ret &= Serialize(_serializer_, *subGroup);
					json->leaveObject();
					Properties::PopGroup();
				}
			}
			else
			{
			 // serialize property
				int count = 1;
				if (jsonType == Json::ValueType_Array)
				{
					json->enterArray();
					count = json->getArrayLength();
					json->next(); // go to first element in the array
					jsonType = json->getType();
				}

				Property::Type type = JsonTypeToPropertiesType(jsonType);
				Property* prop = _group_.findOrAdd(name, type, count);
				FRM_ASSERT(prop);

				if (prop->m_setFromCode)
				{
				 // property was set from code, check that the type and count are the same else do nothing (type/count was changed in code)
					if ((prop->m_type != type && prop->m_type != Properties::Type_Int) || prop->m_count != count)
					{
						FRM_LOG("Properties: '%s' (%s[%d]) type/count changed (%s[%d]), ignoring.", name, Properties::GetTypeStr(type), count, Properties::GetTypeStr(prop->m_type), prop->m_count);
						prop = nullptr;
					}
				}

				if (prop)
				{
					const int offset = count > 1 ? 0 : -1; // Json::getValue() takes -1 for none-arrays, add this to i below for correct behavior
					for (int i = 0; i < count; ++i)
					{
						switch (type)
						{
							default:                      FRM_ASSERT(false); break;
							case Properties::Type_Bool:   ((bool*)prop->m_storageInternal)[i] = json->getValue<bool>(i + offset); break;
							case Properties::Type_Int: 
							case Properties::Type_Float:  ((double*)prop->m_storageInternal)[i] = json->getValue<double>(i + offset); break;
							case Properties::Type_String: ((String<32>*)prop->m_storageInternal)[i].set(json->getValue<const char*>(i + offset)); break;
						};
					}

					if (prop->m_storageExternal)
					{
						prop->copy(prop->m_storageExternal, prop->m_storageInternal);
					}
				}

				if (count > 1)
				{
					json->leaveArray();
				}
			}
		}
	}
	else
	{
		Json* json = _serializer_.getJson();
		
	 // properties
		for (auto& it : _group_.m_properties)
		{
			Property* prop = it.second;

			if (!prop->m_setFromCode)
			{
				continue;
			}

			if (prop->isDefault())
			{
				continue;
			}

			if (prop->m_storageExternal)
			{
			 // update internal storage before we write
				prop->copy(prop->m_storageInternal, prop->m_storageExternal);
			}

			if (prop->getCount() > 1)
			{
				json->beginArray(prop->getName());
				for (int i = 0; i < prop->getCount(); ++i)
				{
					switch (prop->getType())
					{
						default:                      FRM_ASSERT(false); break;
						case Properties::Type_Bool:   json->pushValue<bool>(((bool*)prop->getInternalStorage())[i]); break;
						case Properties::Type_Int:    json->pushValue<int>((int)((double*)prop->getInternalStorage())[i]); break;
						case Properties::Type_Float:  json->pushValue<double>(((double*)prop->getInternalStorage())[i]); break;
						case Properties::Type_String: json->pushValue<const char*>(((StringBase*)prop->getInternalStorage())[i].c_str()); break;
					};
				}			
				json->endArray();
			}
			else
			{
				switch (prop->getType())
				{
					default:                      FRM_ASSERT(false); break;
					case Properties::Type_Bool:   json->setValue<bool>(*((bool*)prop->getInternalStorage()), prop->getName()); break;
					case Properties::Type_Int:    json->setValue<int>((int)*((double*)prop->getInternalStorage()), prop->getName()); break;
					case Properties::Type_Float:  json->setValue<double>(*((double*)prop->getInternalStorage()), prop->getName()); break;
					case Properties::Type_String: json->setValue<const char*>(((StringBase*)prop->getInternalStorage())->c_str(), prop->getName()); break;
				};
			}
		}

	 // subgroups
		for (auto& it : _group_.m_subGroups)
		{
			Properties* subGroup = it.second;
	
			// \todo eliminate redundant groups

			json->beginObject(subGroup->m_name.c_str());
			ret &= Serialize(_serializer_, *subGroup);
			json->leaveObject();
		}
	}

	return ret;
}

bool Properties::edit(const char* _filter)
{
	ImGui::PushID(this);

	bool ret = false;

	ImGuiTextFilter filter(_filter ? _filter : "");

	for (auto& it : m_properties)
	{
		Property* prop = it.second;
		if (prop->m_setFromCode && filter.PassFilter(prop->m_name.begin(), prop->m_name.end()))
		{
			ret |= prop->edit();
		}
	}

	ImGui::Spacing();
	
	for (auto& it : m_subGroups)
	{
		Properties* group = it.second;
		if (!group->m_properties.empty())
		{
			if (ImGui::TreeNode(group->m_name.c_str()))
			{
				ret |= group->edit(_filter);
				ImGui::TreePop();
			}
		}
	}

	ImGui::PopID();

	return ret;
}

void Properties::display(const char* _filter)
{
	ImGui::PushID(this);
	ImGui::PushID("display"); // required to make ID different to edit()

	ImGuiTextFilter filter(_filter ? _filter : "");

	for (auto& it : m_properties)
	{
		Property* prop = it.second;
		if (prop->m_setFromCode && filter.PassFilter(prop->m_name.begin(), prop->m_name.end()))
		{
			prop->display();
		}
	}

	ImGui::Spacing();
	
	for (auto& it : m_subGroups)
	{
		Properties* group = it.second;
		if (!group->m_properties.empty())
		{
			if (ImGui::TreeNode(group->m_name.c_str()))
			{
				group->display(_filter);
				ImGui::TreePop();
			}
		}
	}

	ImGui::PopID();
	ImGui::PopID();
}

// PRIVATE

eastl::vector<Properties*> Properties::s_groupStack;

StringHash Properties::GetStringHash(const char* _str)
{
	String<32> upperCase = _str;
	upperCase.toUpperCase();
	return StringHash(upperCase.c_str());
}

Properties::Properties(const char* _name)
{
	m_name = _name;
}

Properties::~Properties()
{
	while (!m_subGroups.empty())
	{
		FRM_DELETE(m_subGroups.back().second);
		m_subGroups.pop_back();
	}
	m_subGroups.clear();

	for (auto& prop : m_properties)
	{
		FRM_DELETE(prop.second);
	}
	m_properties.clear();
}

Property* Properties::findOrAdd(const char* _name, Type _type, int _count)
{
	Property*& ret = m_properties[GetStringHash(_name)];
	if (!ret)
	{
		ret = FRM_NEW(Property);
		ret->init(_name, _type, _count, nullptr);
	}
	return ret;
}

Property* Properties::add(const char* _name, Type _type, int _count, const void* _default, const void* _min, const void* _max, void* _storage, const char* _displayName)
{
	Property* ret = FRM_NEW(Property);
	ret->init(_name, _displayName, _type, _count, _storage, _default, _min, _max);

	Property*& existing = m_properties[GetStringHash(_name)];
	if (existing)
	{
		ret->m_storageInternal = existing->m_storageInternal;
		ret->copy(ret->m_storageExternal, ret->m_storageInternal); // force a copy internal -> external
		existing->m_storageInternal = nullptr;
		FRM_DELETE(existing);
	}
	existing = ret;

	return ret;
}

Property* Properties::find(const char* _propName)
{
	FRM_STRICT_ASSERT(_propName);

	const StringHash propHash  = GetStringHash(_propName);

	auto it = m_properties.find(propHash);
	if (it != m_properties.end())
	{
		return it->second;
	}

	return nullptr;
}

Properties* Properties::findGroup(const char* _groupName)
{
	FRM_STRICT_ASSERT(_groupName);

	const StringHash groupHash = GetStringHash(_groupName);
	if (groupHash == GetStringHash(m_name.c_str()))
	{
		return this;
	}

	for (auto& it : m_subGroups)
	{
		Properties* ret = it.second->findGroup(_groupName);
		if (ret)
		{
			return ret;
		}
	}

	return nullptr;
}

/******************************************************************************

                                Property

******************************************************************************/

// PUBLIC

void Property::reset()
{
	copy(m_storageInternal, m_default);
	if (m_storageExternal)
	{
		copy(m_storageExternal, m_default);
	}
}

void Property::setDefault(void* _default)
{
	copy(m_default, _default);
}

bool Property::isDefault() const
{
	return compare(m_default, getStorage());
}

void Property::setMin(void* _min)
{
	copy(m_min, _min);
}

void Property::setMax(void* _max)
{
	copy(m_max, _max);
}

int Property::getSizeBytes() const
{
	return Properties::GetTypeSizeBytes(m_type) * m_count;
}

void Property::setExternalStorage(void* _storage_)
{
	if (m_storageExternal == _storage_)
	{
		return;
	}

	if (_storage_)
	{
	 // setting external storage, copy current value from internal storage
		m_storageExternal = (char*)_storage_;
		copy(m_storageExternal, m_storageInternal);
	}
	else
	{
	 // invalidating external storage, copy current to internal storage
		copy(m_storageInternal, m_storageExternal);
		m_storageExternal = nullptr;
	}
}

bool Property::edit()
{
	FRM_ASSERT(m_editFunc);
	
	ImGui::PushID(this);
	bool ret = m_editFunc(*this);
	ImGui::PopID();
	return ret;
}

void Property::display()
{
	FRM_ASSERT(m_displayFunc);
	
	ImGui::PushID(this);
	ImGui::PushID("display"); // required to make ID different to edit()
	m_displayFunc(*this);
	ImGui::PopID();
	ImGui::PopID();
}

// PRIVATE

Property::~Property()
{
	if (m_storageExternal)
	{
		FRM_LOG_ERR("Properties: '%s' external storage was not invalidated.", m_name.c_str());
	}
	shutdown();
}

void Property::init(
	const char* _name, 
	Type        _type, 
	int         _count,
	void*       _value
	)
{
	FRM_ASSERT(!m_setFromCode);

	shutdown();
	
	m_name            = _name;
	m_displayName     = _name;
	m_type            = _type;
	m_count           = _count;
	
	alloc();

	if (_value)
	{
		copy(m_storageInternal, _value);
	}
}

void Property::init(
	const char* _name,
	const char* _displayName,
	Type        _type,
	int         _count,
	void*       _storageExternal,
	const void* _default,
	const void* _min,
	const void* _max
	)
{
	FRM_ASSERT(m_name.isEmpty());

	shutdown();
	
	m_name            = _name;
	m_displayName     = _displayName ? _displayName : _name;
	m_type            = _type;
	m_count           = _count;
	m_storageExternal = _storageExternal;
	
	alloc();
	copy(m_storageInternal, _default);
	copy(m_default, _default);
	if (_min)
	{
		copy(m_min, _min);
	}
	if (_max)
	{
		copy(m_max, _max);
	}
	if (_storageExternal)
	{
		copy(m_storageInternal, m_storageExternal);
	}
	else
	{
		m_storageExternal = FRM_MALLOC(getSizeBytes());
		if (m_type == Properties::Type_String)
		{
			Construct((String<32>*)m_storageExternal, (String<32>*)m_storageExternal + m_count);
		}
		copy(m_storageExternal, m_default);
		m_ownsStorage = true;
	}

	m_setFromCode = true;
}

void Property::alloc()
{
	m_storageInternal = FRM_MALLOC(getSizeBytes());
	m_default         = FRM_MALLOC(getSizeBytes());

	if (m_type != Properties::Type_Bool && m_type != Properties::Type_String)
	{
		m_min = FRM_MALLOC(getSizeBytes());
		m_max = FRM_MALLOC(getSizeBytes());
	}

	if (m_type == Properties::Type_String)
	{
		Construct((String<32>*)m_default,         (String<32>*)m_default + m_count);
		Construct((String<32>*)m_storageInternal, (String<32>*)m_storageInternal + m_count);
	}
}

void Property::shutdown()
{
	if (m_type == Properties::Type_String)
	{
		if (m_default)
		{
			Destruct((String<32>*)m_default,         (String<32>*)m_default + m_count);
		}

		if (m_storageInternal)
		{
			Destruct((String<32>*)m_storageInternal, (String<32>*)m_storageInternal + m_count);
		}
	}

	if (m_ownsStorage)
	{
		FRM_FREE(m_storageExternal);
	}

	FRM_FREE(m_storageInternal);
	FRM_FREE(m_default);
	FRM_FREE(m_min);
	FRM_FREE(m_max);
}

void Property::copy(void* dst_, const void* _src)
{
	FRM_ASSERT(dst_ && _src);

	switch (m_type)
	{
		default: 
			FRM_ASSERT(false);
			break;
		case Properties::Type_Bool:
			memcpy(dst_, _src, getSizeBytes());
			break;
		case Properties::Type_Int:
		case Properties::Type_Float:
			if (dst_ == m_storageInternal)
			{
				DataTypeConvert((m_type == Properties::Type_Int) ? DataType_Sint32 : DataType_Float32, DataType_Float64, _src, dst_, m_count);
			}
			else if (_src == m_storageInternal)
			{
				DataTypeConvert(DataType_Float64, (m_type == Properties::Type_Int) ? DataType_Sint32 : DataType_Float32, _src, dst_, m_count);
			}
			else
			{
				memcpy(dst_, _src, getSizeBytes());
			}
			break;
		case Properties::Type_String:
			for (int i = 0; i < m_count; ++i)
			{
				((StringBase*)dst_)[i].set(((const StringBase*)_src)[i].c_str());
			}
			break;
	};
}

bool Property::compare(const void* _a, const void* _b) const
{
	FRM_ASSERT(_a && _b);

	switch (m_type)
	{
		default:
		case Properties::Type_Bool:
		case Properties::Type_String:
			return memcmp(_a, _b, getSizeBytes()) == 0;
		case Properties::Type_Int:
		case Properties::Type_Float:
		{
			for (int i = 0; i < m_count; ++i)
			{
				double da, db;
				if (_a == m_storageInternal)
				{
					da = ((double*)_a)[i];
				}
				else
				{
					if (m_type == Properties::Type_Int)
					{
						da = (double)((sint32*)_a)[i];
					}
					else
					{
						da = (double)((float32*)_a)[i];
					}
				}

				if (_b == m_storageInternal)
				{
					db = ((double*)_b)[i];
				}
				else
				{
					if (m_type == Properties::Type_Int)
					{
						db = (double)((sint32*)_b)[i];
					}
					else
					{
						db = (double)((float32*)_b)[i];
					}
				}

				if (da != db)
				{
					return false;
				}
			}
		}
	};

	return true;
}

} // namespace frm
