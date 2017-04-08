#include <frm/AppProperty.h>

#include <frm/icon_fa.h>
#include <frm/Input.h>

#include <apt/memory.h>
#include <apt/ArgList.h>
#include <apt/FileSystem.h>
#include <apt/Ini.h>

#include <imgui/imgui.h>

#include <algorithm> // swap
#include <cstring>   // memcpy

using namespace frm;
using namespace apt;

/*******************************************************************************

                                   AppProperty

*******************************************************************************/

static uint kPropTypeSizes[] =
{
	sizeof(bool),
	sizeof(int),
	sizeof(float),
	sizeof(char*) // Type_String
};

// PUBLIC

AppProperty::AppProperty(AppProperty&& _rhs)
	: m_data(0)
{
	swap(*this, _rhs);
}
AppProperty& AppProperty::operator=(AppProperty&& _rhs)
{
	swap(*this, _rhs);
	return *this;
}
void frm::swap(AppProperty& _a, AppProperty& _b)
{
	using std::swap;
	swap(_a.m_name,        _b.m_name);
	swap(_a.m_displayName, _b.m_displayName);
	swap(_a.m_type,        _b.m_type);
	swap(_a.m_count,       _b.m_count);
	swap(_a.m_size,        _b.m_size);
	swap(_a.m_isHidden,    _b.m_isHidden);
	swap(_a.m_data,        _b.m_data);
	swap(_a.m_pfEdit,      _b.m_pfEdit);
}

AppProperty::~AppProperty()
{ 
	freeData(); 
}

// getValue/getDefault always defined together
#define DEFINE_getValue_getDefault(_type, _typeEnum, _count) \
	template <> _type& AppProperty::getValue<_type>() { \
		APT_ASSERT(_count <= m_count); \
		APT_ASSERT_MSG(getType() == _typeEnum, "AppProperty::getValue -- '%s' is not " # _type, getName()); \
		return *((_type*)m_data); \
	} \
	template <> _type& AppProperty::getDefault<_type>() { \
		APT_ASSERT(_count <= m_count); \
		APT_ASSERT_MSG(getType() == _typeEnum, "AppProperty::getDefault -- '%s' is not " # _type, getName()); \
		return *((_type*)(m_data + m_size)); \
	}
DEFINE_getValue_getDefault(bool,  Type_Bool,   1);
DEFINE_getValue_getDefault(int,   Type_Int,    1);
DEFINE_getValue_getDefault(float, Type_Float,  1);
DEFINE_getValue_getDefault(vec2,  Type_Float,  2);
DEFINE_getValue_getDefault(vec3,  Type_Float,  3);
DEFINE_getValue_getDefault(vec4,  Type_Float,  4);
DEFINE_getValue_getDefault(ivec2, Type_Int,    2);
DEFINE_getValue_getDefault(ivec3, Type_Int,    3);
DEFINE_getValue_getDefault(ivec4, Type_Int,    4);
DEFINE_getValue_getDefault(char*, Type_String, 1);
//DEFINE_getValue_getDefault(char*, kPath, 1);
#undef DEFINE_getValue_getDefault

// getMin/getMax always defined together
#define DEFINE_getMin_getMax(_type, _typeEnum, _count) \
	template <> _type& AppProperty::getMin<_type>() { \
		APT_ASSERT(_count <= m_count); \
		APT_ASSERT_MSG(getType() == _typeEnum, "AppProperty::getMin -- '%s' is not " # _type, getName()); \
		return *((_type*)(m_data + m_size * 2)); \
	} \
	template <> _type& AppProperty::getMax<_type>() { \
		APT_ASSERT(_count <= m_count); \
		APT_ASSERT_MSG(getType() == _typeEnum, "AppProperty::getMax -- '%s' is not " # _type, getName()); \
		return *((_type*)(m_data + m_size * 3)); \
	}
//DEFINE_getMin_getMax(bool, Type_Bool);
DEFINE_getMin_getMax(int,   Type_Int,   1);
DEFINE_getMin_getMax(float, Type_Float, 1);
DEFINE_getMin_getMax(vec2,  Type_Float, 2);
DEFINE_getMin_getMax(vec3,  Type_Float, 3);
DEFINE_getMin_getMax(vec4,  Type_Float, 4);
DEFINE_getMin_getMax(ivec2, Type_Int,   2);
DEFINE_getMin_getMax(ivec3, Type_Int,   3);
DEFINE_getMin_getMax(ivec4, Type_Int,   4);
//DEFINE_getMin_getMax(char*, Type_String);
//DEFINE_getMin_getMax(char*, kPath);
#undef DEFINE_getMin_getMax

void AppProperty::reset()
{
	memcpy(m_data, m_data + m_size, m_size);
}

bool AppProperty::edit()
{
	bool ret = false;
	if (m_pfEdit) {
		ret = m_pfEdit(*this);
	} else {
		switch (m_type) {
			case Type_Bool:
				switch (m_count) {
					case 1:
						ret |= ImGui::Checkbox((const char*)m_displayName, &getValue<bool>());
						break;
					default: {
						String displayName;
						for (int i = 0; i < (int)m_count; ++i) {
							displayName.setf("%s[%d]", (const char*)m_displayName, i); 
						 	ret |= ImGui::Checkbox((const char*)displayName, &getValue<bool>());
						}
						break;
					}
				};
				break;

			case Type_Int:
				switch (m_count) {
					case 1:
						ret |= ImGui::SliderInt((const char*)m_displayName, &getValue<int>(), getMin<int>(), getMax<int>());
						break;
					case 2:
						ret |= ImGui::SliderInt2((const char*)m_displayName, &getValue<int>(), getMin<int>(), getMax<int>());
						break;
					case 3:
						ret |= ImGui::SliderInt3((const char*)m_displayName, &getValue<int>(), getMin<int>(), getMax<int>());
						break;
					case 4:
						ret |= ImGui::SliderInt4((const char*)m_displayName, &getValue<int>(), getMin<int>(), getMax<int>());
						break;
					default: {
						APT_ASSERT(false); // \todo arbitrary arrays?
						break;
					}
				};
				break;

			case Type_Float:
				switch (m_count) {
					case 1:
						ret |= ImGui::SliderFloat((const char*)m_displayName, &getValue<float>(), getMin<float>(), getMax<float>());
						break;
					case 2:
						ret |= ImGui::SliderFloat2((const char*)m_displayName, &getValue<float>(), getMin<float>(), getMax<float>());
						break;
					case 3:
						ret |= ImGui::SliderFloat3((const char*)m_displayName, &getValue<float>(), getMin<float>(), getMax<float>());
						break;
					case 4:
						ret |= ImGui::SliderFloat4((const char*)m_displayName, &getValue<float>(), getMin<float>(), getMax<float>());
						break;
					default: {
						APT_ASSERT(false); // \todo arbitrary arrays?
						break;
					}
				};
				break;

			case Type_String:
				switch (m_count) {
					case 1:
						ret |= ImGui::InputText((const char*)m_displayName, getValue<char*>(), kMaxStringLength);
						break;
					default: {
						String displayName;
						for (int i = 0; i < (int)m_count; ++i) {
							displayName.setf("%s[%d]", (const char*)m_displayName, i); 
						 	ret |= ImGui::InputText((const char*)displayName, getValue<char*>(), kMaxStringLength);
						}
						break;
					};
				};
				break;

			default:
				APT_ASSERT(false);
		};
	}

 // right click to reset
	if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(1)) {
		reset();
	}
	return ret;
}

// PRIVATE

AppProperty::AppProperty(
	const char*       _name,
	const char*       _displayName,
	AppProperty::Type _type, 
	uint8             _count,
	bool              _isHidden,
	const void*       _value, 
	const void*       _default, 
	const void*       _min, 
	const void*       _max,
	Edit*             _pfEdit
	)
	: m_name(_name)
	, m_displayName(_displayName)
	, m_type(_type)
	, m_count(_count)
	, m_size((uint8)kPropTypeSizes[_type] * _count)
	, m_isHidden(_isHidden)
	, m_data(0)
	, m_pfEdit(_pfEdit)
{
	if (_type == Type_String) {
	 // alloc a single buffer for the value/default
		if (_default) {
			APT_ASSERT(strlen((const char*)_default) + 1 < kMaxStringLength);
		} else if (_value) {
			APT_ASSERT(strlen((const char*)_value) + 1 < kMaxStringLength);
		}
		char* val = new char[kMaxStringLength * 2];
		char* def = val + kMaxStringLength;
		if (_value) {
			strcpy(val, (const char*)_value);
		}
		if (_default) {
			strcpy(def, (const char*)_default);
		}
		allocData(&val, &def, 0, 0);

	} else {
		allocData(_value, _default, _min, _max);
	}
}

void AppProperty::allocData(
	const void* _value, 
	const void* _default, 
	const void* _min,
	const void* _max
	)
{
	APT_STATIC_ASSERT((sizeof(kPropTypeSizes) / sizeof(uint)) == AppProperty::Type_Count);

	m_data = (char*)malloc_aligned(m_size * (_min ? 4 : 2), 16); // conservative alignment
	if (_value) {
		memcpy(m_data, _value, m_size);
	}
	if (_default) {
		memcpy(m_data + m_size, _default, m_size);
	}
	if (_min) {
		memcpy(m_data + m_size * 2, _min, m_size);
	}
	if (_max) {
		memcpy(m_data + m_size * 3, _max, m_size);
	}
}
void AppProperty::freeData()
{
	if (getType() == Type_String) {
		delete[] getValue<char*>();
	}

	free_aligned(m_data);
}

/*******************************************************************************

                               AppPropertyGroup

*******************************************************************************/

static bool ColorEdit(AppProperty& _prop)
{
	bool ret = false;
	switch (_prop.getCount()) {
	case 3:
		ret = ImGui::ColorEdit3(_prop.getDisplayName(), &_prop.getValue<float>());
		break;
	case 4:
		ret = ImGui::ColorEdit4(_prop.getDisplayName(), &_prop.getValue<float>());
		break;
	default:
		APT_ASSERT(false);
		break;
	};
	return ret;
}

static bool PathEdit(AppProperty& _prop)
{
	bool ret = false;
	if (ImGui::Button(_prop.getDisplayName())) {
		FileSystem::PathStr tmp = _prop.getValue<char*>();
		if (FileSystem::PlatformSelect(tmp)) {
			FileSystem::MakeRelative(tmp);
			strncpy(_prop.getValue<char*>(), tmp, AppProperty::kMaxStringLength);
			ret = true;
		}
	}
	ImGui::SameLine();
	ImGui::Text(ICON_FA_FLOPPY_O "  \"%s\"", _prop.getValue<char*>());
	return ret;
}

// PUBLIC

AppPropertyGroup::AppPropertyGroup(AppPropertyGroup&& _rhs)
{
	swap(*this, _rhs);
}
AppPropertyGroup& AppPropertyGroup::operator=(AppPropertyGroup&& _rhs)
{
	swap(*this, _rhs);
	return *this;
}
void frm::swap(AppPropertyGroup& _a, AppPropertyGroup& _b)
{
	using std::swap;
	swap(_a.m_name, _b.m_name);
	swap(_a.m_propKeys, _b.m_propKeys);
	swap(_a.m_props, _b.m_props);
}

bool* AppPropertyGroup::addBool(const char* _name, const char* _displayName, bool _default, bool _isHidden)
{
	return &add(_name, _displayName, AppProperty::Type_Bool, 1, _isHidden, &_default, &_default, 0, 0)->getValue<bool>();
}
int* AppPropertyGroup::addInt(const char* _name, const char* _displayName, int _default, int _min, int _max, bool _isHidden)
{
	return &add(_name, _displayName, AppProperty::Type_Int, 1, _isHidden, &_default, &_default, &_min, &_max)->getValue<int>();
}
float* AppPropertyGroup::addFloat(const char* _name, const char* _displayName, float _default, float _min, float _max, bool _isHidden)
{
	return &add(_name, _displayName, AppProperty::Type_Float, 1, _isHidden, &_default, &_default, &_min, &_max)->getValue<float>();
}
vec2* AppPropertyGroup::addVec2(const char* _name, const char* _displayName, const vec2& _default, float _min, float _max, bool _isHidden)
{
	return &add(_name, _displayName, AppProperty::Type_Float, 2, _isHidden, &_default, &_default, &_min, &_max)->getValue<vec2>();
}
vec3* AppPropertyGroup::addVec3(const char* _name, const char* _displayName, const vec3& _default, float _min, float _max, bool _isHidden)
{
	return &add(_name, _displayName, AppProperty::Type_Float, 3, _isHidden, &_default, &_default, &_min, &_max)->getValue<vec3>();
}
vec4* AppPropertyGroup::addVec4(const char* _name, const char* _displayName, const vec4& _default, float _min, float _max, bool _isHidden)
{
	return &add(_name, _displayName, AppProperty::Type_Float, 4, _isHidden, &_default, &_default, &_min, &_max)->getValue<vec4>();
}
ivec2* AppPropertyGroup::addVec2i(const char* _name, const char* _displayName, const ivec2& _default, int _min, int _max, bool _isHidden)
{
	return &add(_name, _displayName, AppProperty::Type_Int, 2, _isHidden, &_default, &_default, &_min, &_max)->getValue<ivec2>();
}
ivec3* AppPropertyGroup::addVec3i(const char* _name, const char* _displayName, const ivec3& _default, int _min, int _max, bool _isHidden)
{
	return &add(_name, _displayName, AppProperty::Type_Int, 3, _isHidden, &_default, &_default, &_min, &_max)->getValue<ivec3>();
}
ivec4* AppPropertyGroup::addVec4i(const char* _name, const char* _displayName, const ivec4& _default, int _min, int _max, bool _isHidden)
{
	return &add(_name, _displayName, AppProperty::Type_Int, 4, _isHidden, &_default, &_default, &_min, &_max)->getValue<ivec4>();
}
vec3* AppPropertyGroup::addRgb(const char* _name, const char* _displayName, const vec3& _default, bool _isHidden)
{
	AppProperty* ret = add(_name, _displayName, AppProperty::Type_Float, 3, _isHidden, &_default, &_default, 0, 0);
	ret->m_pfEdit = &ColorEdit;
	return &ret->getValue<vec3>();
}
vec4* AppPropertyGroup::addRgba(const char* _name, const char* _displayName, const vec4& _default, bool _isHidden)
{
	AppProperty* ret = add(_name, _displayName, AppProperty::Type_Float, 4, _isHidden, &_default, &_default, 0, 0);
	ret->m_pfEdit = &ColorEdit;
	return &ret->getValue<vec4>();
}
char* AppPropertyGroup::addString(const char* _name, const char* _displayName, const char* _default, bool _isHidden)
{
	return add(_name, _displayName, AppProperty::Type_String, 1, _isHidden, _default, _default, 0, 0)->getValue<char*>();
}
char* AppPropertyGroup::addPath(const char* _name, const char* _displayName, const char* _default, bool _isHidden)
{
	AppProperty* ret = add(_name, _displayName, AppProperty::Type_String, 1, _isHidden, _default, _default, 0, 0);
	ret->m_pfEdit = &PathEdit;
	return ret->getValue<char*>();
}

AppProperty& AppPropertyGroup::operator[](const char* _name)
{
	AppProperty* ret = get(StringHash(_name));
	APT_ASSERT(ret);
	return *ret;
}
const AppProperty& AppPropertyGroup::operator[](const char* _name) const
{
	const AppProperty* ret = get(StringHash(_name));
	APT_ASSERT(ret);
	return *ret;
}

bool AppPropertyGroup::edit(bool _showHidden)
{
	bool ret = false;
	ImGui::PushID(this);
	if (ImGui::CollapsingHeader((const char*)m_name)) {
		for (auto p = m_props.begin(); p != m_props.end(); ++p) {
			if (_showHidden || !(*p).isHidden()) {
				ret |= (*p).edit();
			}
		}

		if (ImGui::Button("Reset Defaults")) {
			for (auto p = m_props.begin(); p != m_props.end(); ++p) {
				(*p).reset();
			}
		}
	}
	ImGui::PopID();
	return ret;
}

// PRIVATE

AppProperty* AppPropertyGroup::add(
	const char*       _name,
	const char*       _displayName,
	AppProperty::Type _type, 
	uint8             _count,
	bool              _isHidden,
	const void*       _value,
	const void*       _default,
	const void*       _min, 
	const void*       _max
	)
{
	StringHash nameHash(_name);
	APT_ASSERT_MSG(get(nameHash) == 0, "AppPropertyGroup: '%s' already exists in group '%s'", _name, m_name);
	m_propKeys.push_back(nameHash);
	m_props.push_back(AppProperty(_name, _displayName, _type, _count, _isHidden, _value, _default, _min, _max, 0));
	return &m_props.back();
}

AppProperty* AppPropertyGroup::get(StringHash _nameHash) const
{
	for (int i = 0; i < getSize(); ++i) {
		if (m_propKeys[i] == _nameHash) {
			return const_cast<AppProperty*>(&m_props[i]);
		}
	}
	return 0;
}

/*******************************************************************************

                                AppProperties

*******************************************************************************/

// PUBLIC

AppPropertyGroup& AppProperties::addGroup(const char* _name)
{
	StringHash nameHash(_name);
	APT_ASSERT_MSG(getGroup(nameHash) == 0, "AppProperties: group '%s' already exists", _name);
	m_groupKeys.push_back(nameHash);
	m_groups.push_back(AppPropertyGroup(_name));
	return m_groups.back();
}


AppPropertyGroup& AppProperties::operator[](const char* _name)
{
	AppPropertyGroup* ret = getGroup(StringHash(_name));
	APT_ASSERT(ret);
	return *ret;
}
const AppPropertyGroup& AppProperties::operator[](const char* _name) const
{
	const AppPropertyGroup* ret = getGroup(StringHash(_name));
	APT_ASSERT(ret);
	return *ret;
}

bool AppProperties::edit(const char* _title)
{
	bool ret = false;
	ImGui::Begin(_title);
		ImGui::Checkbox("Show Hidden", &m_editShowHidden);
		if (!m_iniPath.isEmpty()) {
			ImGui::SameLine();
			if (ImGui::Button("Save")) {
				save();
			}
			ImGui::SameLine();
			if (ImGui::Button("Load")) {
				load();
			}
		}
		for (int i = 0, n = getSize(); i < n; ++i) {
			ret |= m_groups[i].edit(m_editShowHidden);
		}
	ImGui::End();
	return ret;
}

void AppProperties::save() const
{
	Ini ini;
	appendToIni(ini);
	File f;
	APT_VERIFY(Ini::Write(ini, f));
	APT_VERIFY(FileSystem::Write(f, m_iniPath, FileSystem::RootType_Application));
}

void AppProperties::load()
{
	File f;
	if (FileSystem::ReadIfExists(f, m_iniPath, FileSystem::RootType_Application)) {
		Ini ini;
		APT_VERIFY(Ini::Read(ini, f));
		setValues(ini);
	}
}

void AppProperties::setValues(const ArgList& _argList)
{
	for (int i = 0, n = getSize(); i < n; ++i) {
		AppPropertyGroup& group = m_groups[i];
		for (int j = 0, m = group.getSize(); j < m; ++j) {
			AppProperty& prop = group[j];

			const Arg* arg = _argList.find(prop.getName());
			if (arg) {
				APT_ASSERT_MSG(prop.getCount() == arg->getValueCount(), "AppProperties: Count mismatch with arg list, '%s' has size %d, the argument has size %d", prop.getName(), prop.getSize(), arg->getValueCount());
				switch (prop.getType()) {
				case AppProperty::Type_Bool: {
					bool* v = &prop.getValue<bool>();
					for (int k = 0; k < prop.getCount(); ++k) {
						v[k] = arg->getValue(k).asBool();
					}
					break;
				}
				case AppProperty::Type_Int: {
					int* v = &prop.getValue<int>();
					for (int k = 0; k < prop.getCount(); ++k) {
						v[k] = (int)arg->getValue(k).asInt();
					}
					break;
				}
				case AppProperty::Type_Float: {
					float* v = &prop.getValue<float>();
					for (int k = 0; k < prop.getCount(); ++k) {
						v[k] = (float)arg->getValue(k).asDouble();
					}
					break;
				}
				default:
					APT_ASSERT(false); // unsupported type
					break;
				};
			}
		}
	}
}
	
void AppProperties::setValues(const Ini& _ini)
{
	Ini::Property p;
	for (int i = 0, n = getSize(); i < n; ++i) {
		AppPropertyGroup& group = m_groups[i];
		for (int j = 0, m = group.getSize(); j < m; ++j) {
			AppProperty& prop = group[j];

			p = _ini.getProperty(prop.getName(), group.getName());
			if (!p.isNull()) {
				APT_ASSERT_MSG(prop.getCount() == p.getCount(), "AppProperties: Count mismatch with INI, '%s' has size %d, the INI property has size %d", prop.getName(), prop.getSize(), p.getCount());
				switch (prop.getType()) {
				case AppProperty::Type_Bool: {
					bool* v = &prop.getValue<bool>();
					for (int k = 0; k < prop.getCount(); ++k) {
						v[k] = p.asBool(k);
					}
					break;
				}
				case AppProperty::Type_Int: {
					int* v = &prop.getValue<int>();
					for (int k = 0; k < prop.getCount(); ++k) {
						v[k] = (int)p.asInt(k);
					}
					break;
				}
				case AppProperty::Type_Float: {
					float* v = &prop.getValue<float>();
					for (int k = 0; k < prop.getCount(); ++k) {
						v[k] = (float)p.asDouble(k);
					}
					break;
				}
				case AppProperty::Type_String: {
					char* v = prop.getValue<char*>();
					strncpy(v, p.asString(), AppProperty::kMaxStringLength);
					break;
				}
				default:
					APT_ASSERT(false); // unsupported type
					break;
				};
			}
		}
	}
}

void AppProperties::appendToIni(apt::Ini& _ini_) const
{
	for (int i = 0; i < getSize(); ++i) {
		const AppPropertyGroup& group = m_groups[i];
		_ini_.pushSection(group.getName());
		for (int j = 0; j < group.getSize(); ++j) {
			AppProperty& prop = const_cast<AppProperty&>(group[j]);
			switch (prop.getType()) {
				case AppProperty::Type_Bool:   _ini_.pushValueArray<bool>(prop.getName(), &prop.getValue<bool>(), prop.getCount()); break;
				case AppProperty::Type_Int:    _ini_.pushValueArray<int>(prop.getName(), &prop.getValue<int>(), prop.getCount()); break;
				case AppProperty::Type_Float:  _ini_.pushValueArray<float>(prop.getName(), &prop.getValue<float>(), prop.getCount()); break;
				case AppProperty::Type_String: _ini_.pushValueArray(prop.getName(), (const char**)&prop.getValue<char*>(), 1); break;
				default: APT_ASSERT(false);
			};
		}
	}
}

// PRIVATE

AppPropertyGroup* AppProperties::getGroup(StringHash _nameHash) const
{
	for (int i = 0; i < getSize(); ++i) {
		if (m_groupKeys[i] == _nameHash) {
			return const_cast<AppPropertyGroup*>(&m_groups[i]);
		}
	}
	return 0;
}