#include <frm/Property.h>

#include <frm/icon_fa.h>

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
	m_min = (char*)malloc_aligned(sizeBytes, 16);
	m_max = (char*)malloc_aligned(sizeBytes, 16);
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

		((StringBase*)m_data)->set((const char*)_default);
		((StringBase*)m_default)->set((const char*)_default);
	} else {
		memcpy(m_data, _default, sizeBytes);
		memcpy(m_default, _default, sizeBytes);
	}

 // min/max aren't arrays
	if (_min) {
		memcpy(m_min, _min, GetTypeSize(_type));
	}
	if (_max) {
		memcpy(m_max, _max, GetTypeSize(_type));
	}
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
					case 1:
						if (ImGui::InputText((const char*)m_displayName, buf, kStrBufLen)) {
							((StringBase*)m_data)->set(buf);
							ret = true;
						}
						break;
					default: {
						String displayName;
						for (int i = 0; i < (int)m_count; ++i) {
							displayName.setf("%s[%d]", (const char*)m_displayName, i); 
						 	if (ImGui::InputText((const char*)displayName, buf, kStrBufLen)) {
								((StringBase*)m_data)->set(buf);
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

	return ret;
}

bool Property::serialize(JsonSerializer& _serializer_)
{
	bool ret = true;
	if (m_count > 1) {
		if (_serializer_.beginArray(m_name)) {
			for (int i = 0; i < (int)m_count; ++i) {
				switch (m_type) {
					case Type_Bool:   ret &= _serializer_.value(*((bool*)m_data)); break;
					case Type_Int:    ret &= _serializer_.value(*((int*)m_data)); break;
					case Type_Float:  ret &= _serializer_.value(*((float*)m_data)); break;
					case Type_String: ret &= _serializer_.value(*((StringBase*)m_data)); break;
					default:          ret = false; APT_ASSERT(false);
				}
			}
			_serializer_.endArray();
		} else {
			ret = false;
		}
	} else {
		switch (m_type) {
			case Type_Bool:   ret &= _serializer_.value(m_name, *((bool*)m_data)); break;
			case Type_Int:    ret &= _serializer_.value(m_name, *((int*)m_data)); break;
			case Type_Float:  ret &= _serializer_.value(m_name, *((float*)m_data)); break;
			case Type_String: ret &= _serializer_.value(m_name, *((StringBase*)m_data)); break;
			default:          ret = false; APT_ASSERT(false);
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
	StringBase& pth = *((StringBase*)_prop.getData());
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
	return (bool*)ret->getData();
}
int* PropertyGroup::addInt(const char* _name, int _default, int _min, int _max, int* storage_, const char* _displayName)
{
	Property* ret = new Property(_name, Property::Type_Int, 1, &_default, &_min, &_max, storage_, _displayName, nullptr);
	m_props[StringHash(_name)] = ret;
	return (int*)ret->getData();
}
ivec2* PropertyGroup::addInt2(const char* _name, const ivec2& _default, int _min, int _max, ivec2* storage_, const char* _displayName)
{
	Property* ret = new Property(_name, Property::Type_Int, 2, &_default.x, &_min, &_max, storage_, _displayName, nullptr);
	m_props[StringHash(_name)] = ret;
	return (ivec2*)ret->getData();
}
ivec3* PropertyGroup::addInt3(const char* _name, const ivec3& _default, int _min, int _max, ivec3* storage_, const char* _displayName)
{
	Property* ret = new Property(_name, Property::Type_Int, 3, &_default.x, &_min, &_max, storage_, _displayName, nullptr);
	m_props[StringHash(_name)] = ret;
	return (ivec3*)ret->getData();
}
ivec4* PropertyGroup::addInt4(const char* _name, const ivec4& _default, int _min, int _max, ivec4* storage_, const char* _displayName)
{
	Property* ret = new Property(_name, Property::Type_Int, 4, &_default.x, &_min, &_max, storage_, _displayName, nullptr);
	m_props[StringHash(_name)] = ret;
	return (ivec4*)ret->getData();
}
float* PropertyGroup::addFloat(const char* _name, float _default, float _min, float _max, float* storage_, const char* _displayName)
{
	Property* ret = new Property(_name, Property::Type_Float, 1, &_default, &_min, &_max, storage_, _displayName, nullptr);
	m_props[StringHash(_name)] = ret;
	return (float*)ret->getData();
}
vec2* PropertyGroup::addFloat2(const char* _name, const vec2& _default, float _min, float _max, vec2* storage_, const char* _displayName)
{
	Property* ret = new Property(_name, Property::Type_Float, 2, &_default.x, &_min, &_max, storage_, _displayName, nullptr);
	m_props[StringHash(_name)] = ret;
	return (vec2*)ret->getData();
}
vec3* PropertyGroup::addFloat3(const char* _name, const vec3& _default, float _min, float _max, vec3* storage_, const char* _displayName)
{
	Property* ret = new Property(_name, Property::Type_Float, 3, &_default.x, &_min, &_max, storage_, _displayName, nullptr);
	m_props[StringHash(_name)] = ret;
	return (vec3*)ret->getData();
}
vec4* PropertyGroup::addFloat4(const char* _name, const vec4& _default, float _min, float _max, vec4* storage_, const char* _displayName)
{
	Property* ret = new Property(_name, Property::Type_Float, 4, &_default.x, &_min, &_max, storage_, _displayName, nullptr);
	m_props[StringHash(_name)] = ret;
	return (vec4*)ret->getData();
}
vec3* PropertyGroup::addRgb(const char* _name, const vec3& _default, float _min, float _max, vec3* storage_, const char* _displayName)
{
	Property* ret = new Property(_name, Property::Type_Float, 3, &_default.x, &_min, &_max, storage_, _displayName, &EditColor);
	m_props[StringHash(_name)] = ret;
	return (vec3*)ret->getData();
}
vec4* PropertyGroup::addRgba(const char* _name, const vec4& _default, float _min, float _max, vec4* storage_, const char* _displayName)
{
	Property* ret = new Property(_name, Property::Type_Float, 4, &_default.x, &_min, &_max, storage_, _displayName, &EditColor);
	m_props[StringHash(_name)] = ret;
	return (vec4*)ret->getData();
}
StringBase* PropertyGroup::addString(const char* _name, const char* _default, StringBase* storage_, const char* _displayName)
{
	Property* ret = new Property(_name, Property::Type_String, 1, _default, nullptr, nullptr, storage_, _displayName, nullptr);
	m_props[StringHash(_name)] = ret;
	return (StringBase*)ret->getData();
}
StringBase* PropertyGroup::addPath(const char* _name, const char* _default, StringBase* storage_, const char* _displayName)
{
	Property* ret = new Property(_name, Property::Type_String, 1, _default, nullptr, nullptr, storage_, _displayName, &EditPath);
	m_props[StringHash(_name)] = ret;
	return (StringBase*)ret->getData();
}

Property* PropertyGroup::find(apt::StringHash _nameHash)
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

bool PropertyGroup::serialize(JsonSerializer& _serializer_)
{
	if (_serializer_.beginObject(m_name)) {
		for (auto& it : m_props) {
			if (!it.second->serialize(_serializer_)) {
				return false;
			}
		}
		_serializer_.endObject();
		return true;
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

Property* Properties::findProperty(apt::StringHash _nameHash)
{
	for (auto& group : m_groups) {
		Property* prop = group.second->find(_nameHash);
		if (prop) {
			return prop;
		}
	}
	return nullptr;
}

PropertyGroup* Properties::findGroup(apt::StringHash _nameHash)
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

bool Properties::serialize(JsonSerializer& _serializer_)
{
	for (auto& it : m_groups) {
		if (!it.second->serialize(_serializer_)) {
			return false;
		}
	}
	return true;
}
