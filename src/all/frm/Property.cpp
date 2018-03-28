#include <frm/Property.h>

#include <apt/memory.h>
#include <apt/FileSystem.h>
#include <apt/Json.h>

#include <EASTL/utility.h>
#include <imgui/imgui.h>

#include <new>


using namespace frm;
using namespace apt;

/******************************************************************************

                                Property

******************************************************************************/

// PUBLIC

int Property::GetTypeSize(Type _type)
{
	switch (_type) {
		case Type_Bool:    return (int)sizeof(bool);
		case Type_Int:     return (int)sizeof(int);
		case Type_Float:   return (int)sizeof(float);
		case Type_String:  return (int)sizeof(StringBase);
		default:           return -1;
	};
}

Property::Property(
	const char* _name,
	Type        _type,
	int         _count,
	const void* _default,
	const void* _min,
	const void* _max,
	void*       storage_,
	const char* _displayName,
	Edit*       _edit
	)
{
	APT_ASSERT(_name); // must provide a name
	m_name.set(_name);
	m_displayName.set(_displayName ? _displayName : _name);
	m_type = _type;
	m_pfEdit = _edit;
	m_count = (uint8)_count;
	int sizeBytes = GetTypeSize(_type) * _count;

	m_default = (char*)malloc_aligned(sizeBytes, 16);
	m_min     = (char*)malloc_aligned(sizeBytes, 16);
	m_max     = (char*)malloc_aligned(sizeBytes, 16);
	if (storage_) {
		m_data = (char*)storage_;
		m_ownsData = false;
	} else {
		m_data = (char*)malloc_aligned(sizeBytes, 16);
		m_ownsData = true;
	}

	if (_type == Type_String) {
	 // call StringBase ctor
		if (m_ownsData) {
			new((apt::String<0>*)m_data) apt::String<0>();
		}
		new((apt::String<0>*)m_default) apt::String<0>();
		((StringBase*)m_default)->set((const char*)_default);
	} else {
		memcpy(m_default, _default, sizeBytes);
	}

 // min/max aren't arrays
	if (_min) {
		memcpy(m_min, _min, GetTypeSize(_type));
	}
	if (_max) {
		memcpy(m_max, _max, GetTypeSize(_type));
	}

	setDefault();
}

Property::~Property()
{
	if (m_ownsData) {
		free_aligned(m_data);
	}
	free_aligned(m_default);
	free_aligned(m_min);
	free_aligned(m_max);
}

Property::Property(Property&& _rhs)
{
	memset(this, 0, sizeof(Property));
	m_type = Type_Count;
	swap(*this, _rhs);
}
Property& Property::operator=(Property&& _rhs)
{
	if (this != &_rhs) {
		swap(*this, _rhs);
	}
	return _rhs;
}
void frm::swap(Property& _a_, Property& _b_)
{
	using eastl::swap;
	swap(_a_.m_data,        _b_.m_data);
	swap(_a_.m_default,     _b_.m_default);
	swap(_a_.m_min,         _b_.m_min);
	swap(_a_.m_max,         _b_.m_max);
	swap(_a_.m_type,        _b_.m_type);
	swap(_a_.m_count,       _b_.m_count);
	swap(_a_.m_ownsData,    _b_.m_ownsData);
	swap(_a_.m_name,        _b_.m_name);
	swap(_a_.m_displayName, _b_.m_displayName);
	swap(_a_.m_pfEdit,      _b_.m_pfEdit);	
}

bool Property::edit()
{
	const int kStrBufLen = 1024;
	bool ret = false;
	if (m_pfEdit) {
		ret = m_pfEdit(*this);

	} else {
		switch (m_type) {
			case Type_Bool:
				switch (m_count) {
					case 1:
						ret |= ImGui::Checkbox((const char*)m_displayName, (bool*)m_data);
						break;
					default: {
						String displayName;
						for (int i = 0; i < (int)m_count; ++i) {
							displayName.setf("%s[%d]", (const char*)m_displayName, i); 
							ret |= ImGui::Checkbox((const char*)displayName, (bool*)m_data);
						}
						break;
					}	
				};
				break;
			case Type_Int:
				switch (m_count) {
					case 1:
						ret |= ImGui::SliderInt((const char*)m_displayName, (int*)m_data, *((int*)m_min), *((int*)m_max));
						break;
					case 2:
						ret |= ImGui::SliderInt2((const char*)m_displayName, (int*)m_data, *((int*)m_min), *((int*)m_max));
						break;
					case 3:
						ret |= ImGui::SliderInt3((const char*)m_displayName, (int*)m_data, *((int*)m_min), *((int*)m_max));
						break;
					case 4:
						ret |= ImGui::SliderInt4((const char*)m_displayName, (int*)m_data, *((int*)m_min), *((int*)m_max));
						break;
					default: {
						APT_ASSERT(false); // \todo arbitrary arrays, see bool
						break;
					}
				};
				break;
			case Type_Float:
				switch (m_count) {
					case 1:
						ret |= ImGui::SliderFloat((const char*)m_displayName, (float*)m_data, *((float*)m_min), *((float*)m_max));
						break;
					case 2:
						ret |= ImGui::SliderFloat2((const char*)m_displayName, (float*)m_data, *((float*)m_min), *((float*)m_max));
						break;
					case 3:
						ret |= ImGui::SliderFloat3((const char*)m_displayName, (float*)m_data, *((float*)m_min), *((float*)m_max));
						break;
					case 4:
						ret |= ImGui::SliderFloat4((const char*)m_displayName, (float*)m_data, *((float*)m_min), *((float*)m_max));
						break;
					default: {
						APT_ASSERT(false); // \todo arbitrary arrays, see bool
						break;
					}
				};
				break;
			case Type_String: {
				char buf[kStrBufLen];
				switch (m_count) {
					case 1: {
						StringBase& str = (StringBase&)*m_data;
						APT_ASSERT(str.getCapacity() < kStrBufLen);
						memcpy(buf, (const char*)str, str.getLength() + 1);
						if (ImGui::InputText((const char*)m_displayName, buf, kStrBufLen)) {
							str.set(buf);
							ret = true;
						}
						break;
					}
					default: {
						String displayName;
						for (int i = 0; i < (int)m_count; ++i) {
							displayName.setf("%s[%d]", (const char*)m_displayName, i); 
						 	StringBase& str = (StringBase&)*m_data;
							APT_ASSERT(str.getCapacity() < kStrBufLen);
							if (ImGui::InputText((const char*)displayName, buf, kStrBufLen)) {
								str.set(buf);
								ret = true;
							}
						}
						break;
					};
				};
				break;
			default:
				APT_ASSERT(false);
			}
		};
	}

	if (ImGui::GetIO().MouseClicked[1] && ImGui::IsItemHovered()) {
		setDefault();
	}
	return ret;
}

void Property::setDefault()
{
	if (getType() == Type_String) {
		((StringBase*)m_data)->set(((StringBase*)m_default)->c_str());
	} else {
		memcpy(m_data, m_default, GetTypeSize(getType()) * getCount());
	}
}

bool frm::Serialize(SerializerJson& _serializer_, Property& _prop_)
{
	bool ret = true;
	uint count = _prop_.m_count;
	if (count > 1) {
		if (_serializer_.beginArray(count, (const char*)_prop_.m_name)) {
			for (uint i = 0; i < count; ++i) {
				switch (_prop_.m_type) {
					case Property::Type_Bool:   ret &= Serialize(_serializer_, ((bool*)_prop_.m_data)[i]); break;
					case Property::Type_Int:    ret &= Serialize(_serializer_, ((int*)_prop_.m_data)[i]); break;
					case Property::Type_Float:  ret &= Serialize(_serializer_, ((float*)_prop_.m_data)[i]); break;
					case Property::Type_String: ret &= Serialize(_serializer_, ((StringBase*)_prop_.m_data)[i]); break;
					default:                    ret = false; APT_ASSERT(false);
				}
			}
			_prop_.m_count = (uint8)count;
			_serializer_.endArray();
		} else {
			ret = false;
		}
	} else {
		switch (_prop_.m_type) {
			case Property::Type_Bool:   ret &= Serialize(_serializer_, *((bool*)_prop_.m_data),       (const char*)_prop_.m_name); break;
			case Property::Type_Int:    ret &= Serialize(_serializer_, *((int*)_prop_.m_data),        (const char*)_prop_.m_name); break;
			case Property::Type_Float:  ret &= Serialize(_serializer_, *((float*)_prop_.m_data),      (const char*)_prop_.m_name); break;
			case Property::Type_String: ret &= Serialize(_serializer_, *((StringBase*)_prop_.m_data), (const char*)_prop_.m_name); break;
			default:                    ret = false; APT_ASSERT(false);
		}
	}
	return ret;
}


/******************************************************************************

                              PropertyGroup

******************************************************************************/

static bool EditColor(Property& _prop)
{
	bool ret = false;
	switch (_prop.getCount()) {
	case 3:
		ret = ImGui::ColorEdit3(_prop.getDisplayName(), (float*)_prop.getData());
		break;
	case 4:
		ret = ImGui::ColorEdit4(_prop.getDisplayName(), (float*)_prop.getData());
		break;
	default:
		APT_ASSERT(false);
		break;
	};
	return ret;
}

static bool EditPath(Property& _prop)
{
	bool ret = false;
	FileSystem::PathStr& pth = *((FileSystem::PathStr*)_prop.getData());
	if (ImGui::Button(_prop.getDisplayName())) {
		if (FileSystem::PlatformSelect(pth)) {
			FileSystem::MakeRelative(pth);
			ret = true;
		}
	}
	ImGui::SameLine();
	ImGui::Text(ICON_FA_FLOPPY_O "  \"%s\"", (const char*)pth);
	return ret;
}

// PUBLIC

PropertyGroup::PropertyGroup(const char* _name)
	: m_name(_name)
{
}

PropertyGroup::~PropertyGroup()
{
	for (auto& it : m_props) {
		delete it.second;
	}
	m_props.clear();
}

PropertyGroup::PropertyGroup(PropertyGroup&& _rhs)
{
	using eastl::swap;
	swap(m_name,  _rhs.m_name);
	swap(m_props, _rhs.m_props);
}
PropertyGroup& PropertyGroup::operator=(PropertyGroup&& _rhs)
{
	using eastl::swap;
	if (this != &_rhs) {
		swap(m_name,  _rhs.m_name);
		swap(m_props, _rhs.m_props);
	}
	return *this;
}
void frm::swap(PropertyGroup& _a_, PropertyGroup& _b_)
{
	using eastl::swap;
	swap(_a_.m_name,  _b_.m_name);
	swap(_a_.m_props, _b_.m_props);
}

bool* PropertyGroup::addBool(const char* _name, bool _default, bool* storage_, const char* _displayName)
{
	Property* ret = new Property(_name, Property::Type_Bool, 1, &_default, nullptr, nullptr, storage_, _displayName, nullptr);
	m_props[StringHash(_name)] = ret;
	return ret->asBool();
}
int* PropertyGroup::addInt(const char* _name, int _default, int _min, int _max, int* storage_, const char* _displayName)
{
	Property* ret = new Property(_name, Property::Type_Int, 1, &_default, &_min, &_max, storage_, _displayName, nullptr);
	m_props[StringHash(_name)] = ret;
	return ret->asInt();
}
ivec2* PropertyGroup::addInt2(const char* _name, const ivec2& _default, int _min, int _max, ivec2* storage_, const char* _displayName)
{
	Property* ret = new Property(_name, Property::Type_Int, 2, &_default.x, &_min, &_max, storage_, _displayName, nullptr);
	m_props[StringHash(_name)] = ret;
	return ret->asInt2();
}
ivec3* PropertyGroup::addInt3(const char* _name, const ivec3& _default, int _min, int _max, ivec3* storage_, const char* _displayName)
{
	Property* ret = new Property(_name, Property::Type_Int, 3, &_default.x, &_min, &_max, storage_, _displayName, nullptr);
	m_props[StringHash(_name)] = ret;
	return ret->asInt3();
}
ivec4* PropertyGroup::addInt4(const char* _name, const ivec4& _default, int _min, int _max, ivec4* storage_, const char* _displayName)
{
	Property* ret = new Property(_name, Property::Type_Int, 4, &_default.x, &_min, &_max, storage_, _displayName, nullptr);
	m_props[StringHash(_name)] = ret;
	return ret->asInt4();
}
float* PropertyGroup::addFloat(const char* _name, float _default, float _min, float _max, float* storage_, const char* _displayName)
{
	Property* ret = new Property(_name, Property::Type_Float, 1, &_default, &_min, &_max, storage_, _displayName, nullptr);
	m_props[StringHash(_name)] = ret;
	return ret->asFloat();
}
vec2* PropertyGroup::addFloat2(const char* _name, const vec2& _default, float _min, float _max, vec2* storage_, const char* _displayName)
{
	Property* ret = new Property(_name, Property::Type_Float, 2, &_default.x, &_min, &_max, storage_, _displayName, nullptr);
	m_props[StringHash(_name)] = ret;
	return ret->asFloat2();
}
vec3* PropertyGroup::addFloat3(const char* _name, const vec3& _default, float _min, float _max, vec3* storage_, const char* _displayName)
{
	Property* ret = new Property(_name, Property::Type_Float, 3, &_default.x, &_min, &_max, storage_, _displayName, nullptr);
	m_props[StringHash(_name)] = ret;
	return ret->asFloat3();
}
vec4* PropertyGroup::addFloat4(const char* _name, const vec4& _default, float _min, float _max, vec4* storage_, const char* _displayName)
{
	Property* ret = new Property(_name, Property::Type_Float, 4, &_default.x, &_min, &_max, storage_, _displayName, nullptr);
	m_props[StringHash(_name)] = ret;
	return ret->asFloat4();
}
vec3* PropertyGroup::addRgb(const char* _name, const vec3& _default, float _min, float _max, vec3* storage_, const char* _displayName)
{
	Property* ret = new Property(_name, Property::Type_Float, 3, &_default.x, &_min, &_max, storage_, _displayName, &EditColor);
	m_props[StringHash(_name)] = ret;
	return ret->asFloat3();
}
vec4* PropertyGroup::addRgba(const char* _name, const vec4& _default, float _min, float _max, vec4* storage_, const char* _displayName)
{
	Property* ret = new Property(_name, Property::Type_Float, 4, &_default.x, &_min, &_max, storage_, _displayName, &EditColor);
	m_props[StringHash(_name)] = ret;
	return ret->asFloat4();
}
StringBase* PropertyGroup::addString(const char* _name, const char* _default, StringBase* storage_, const char* _displayName)
{
	Property* ret = new Property(_name, Property::Type_String, 1, _default, nullptr, nullptr, storage_, _displayName, nullptr);
	m_props[StringHash(_name)] = ret;
	return ret->asString();
}
StringBase* PropertyGroup::addPath(const char* _name, const char* _default, StringBase* storage_, const char* _displayName)
{
	Property* ret = new Property(_name, Property::Type_String, 1, _default, nullptr, nullptr, storage_, _displayName, &EditPath);
	m_props[StringHash(_name)] = ret;
	return ret->asString();
}

Property* PropertyGroup::find(StringHash _nameHash)
{
	auto ret = m_props.find(_nameHash);
	if (ret != m_props.end()) {
		return ret->second;
	}
	return nullptr;
}

bool PropertyGroup::edit(bool _showHidden)
{
	bool ret = false;
	for (auto& it : m_props) {
		Property& prop = *it.second;
		if (_showHidden || prop.getName()[0] != '#') {
			ret |= prop.edit();
		}
	}
	return ret;
}

bool frm::Serialize(SerializerJson& _serializer_, PropertyGroup& _propGroup_)
{
	if (_serializer_.beginObject((const char*)_propGroup_.m_name)) {
		bool ret = true;
		for (auto& it : _propGroup_.m_props) {
			ret &= Serialize(_serializer_, *it.second);
		}
		_serializer_.endObject();
		return ret;
	}
	return false;
}

/******************************************************************************

                              Properties

******************************************************************************/

// PUBLIC

Properties::~Properties()
{
	for (auto& it : m_groups) {
		delete it.second;
	}
	m_groups.clear();
}


PropertyGroup& Properties::addGroup(const char* _name)
{
	PropertyGroup* group = findGroup(_name);
	if (group) {
		return *group;
	}
	PropertyGroup* ret = new PropertyGroup(_name);
	m_groups[StringHash(_name)] = ret;
	return *ret;
}

Property* Properties::findProperty(StringHash _nameHash)
{
	for (auto& group : m_groups) {
		Property* prop = group.second->find(_nameHash);
		if (prop) {
			return prop;
		}
	}
	return nullptr;
}

PropertyGroup* Properties::findGroup(StringHash _nameHash)
{
	auto ret = m_groups.find(_nameHash);
	if (ret != m_groups.end()) {
		return ret->second;
	}
	return nullptr;	
}

bool Properties::edit(bool _showHidden)
{
	bool ret = false;
	for (auto& it: m_groups) {
		PropertyGroup& group = *it.second;
		if (ImGui::TreeNode(group.getName())) {
			ret |= group.edit(_showHidden);
			ImGui::TreePop();
		}
	}
	return ret;
}

bool frm::Serialize(SerializerJson& _serializer_, Properties& _props_)
{
	bool ret = true;
	for (auto& it : _props_.m_groups) {
		ret &= Serialize(_serializer_, *it.second);
	}
	return true;
}
