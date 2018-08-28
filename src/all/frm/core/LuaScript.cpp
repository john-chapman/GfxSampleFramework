#include "LuaScript.h"

#include <frm/core/math.h>

#include <apt/log.h>
#include <apt/memory.h>
#include <apt/FileSystem.h>

#include <lua/lua.hpp>
#include <lua/lualib.h>
#include <lua/lauxlib.h>

	#undef APT_STRICT_ASSERT
	#define APT_STRICT_ASSERT(x) APT_ASSERT(x)

using namespace frm;
using namespace apt;

#define luaAssert(_call) \
	if ((m_err = (_call)) != 0) { \
		APT_LOG_ERR("Lua error: %s", lua_tostring(m_state, -1)); \
		lua_pop(m_state, 1); \
	}

extern "C" {

static int lua_print(lua_State* _L)
{
	if (lua_gettop(_L) > 0) {
		APT_LOG(luaL_optstring(_L, 1, "nil"));
	}
	return 0;
}

} // extern "C"

static LuaScript::ValueType GetValueType(int _luaType)
{
	switch (_luaType) {
		case LUA_TNIL:      return LuaScript::ValueType_Nil;
		case LUA_TTABLE:    return LuaScript::ValueType_Table;
		case LUA_TBOOLEAN:  return LuaScript::ValueType_Bool;
		case LUA_TNUMBER:   return LuaScript::ValueType_Number;
		case LUA_TSTRING:   return LuaScript::ValueType_String;
		case LUA_TFUNCTION: return LuaScript::ValueType_Function;
		default:            APT_ASSERT(false); break;
	};

	return LuaScript::ValueType_Count;
}

/*******************************************************************************

                                LuaScript

*******************************************************************************/

// PUBLIC

LuaScript* LuaScript::CreateAndExecute(const char* _path, Lib _libs)
{
	LuaScript* ret = Create(_path, _libs);
	if (!ret) {
		return nullptr;
	}
	if (ret->m_err == 0) {
		ret->execute();
	}
	return ret;
}

LuaScript* LuaScript::Create(const char* _path, Lib _libs)
{
	File f;
	if (!FileSystem::ReadIfExists(f, _path)) {
		return nullptr;
	}

	LuaScript* ret = APT_NEW(LuaScript(_libs));
	if (!ret->loadText(f.getData(), f.getDataSize(), f.getPath())) {
		goto LuaScript_Create_end;
	}

LuaScript_Create_end:
	if (ret->m_err != 0) {
		APT_DELETE(ret);
		return nullptr;
	}
	return ret;
}

void LuaScript::Destroy(LuaScript*& _script_)
{
	APT_DELETE(_script_);
	_script_ = 0;
}

bool LuaScript::find(const char* _name)
{
	if (m_currentTable) {
		popToCurrentTable();
		lua_pushstring(m_state, _name);
		if (lua_rawget(m_state, m_currentTable) == LUA_TNIL) {
			return false;
		}
		m_tableField[m_currentTable] = _name;
	} else {
		popAll();
		if (lua_getglobal(m_state, _name) == LUA_TNIL) {
			return false;
		}
	}
	return true;
}

bool LuaScript::next()
{
	if (!m_currentTable) {
		APT_LOG_ERR("LuaScript::next(): not in a table");
		return false;
	}
	popToCurrentTable();
	if (lua_rawgeti(m_state, -1, ++m_tableIndex[m_currentTable]) == LUA_TNIL) {
		lua_pop(m_state, 1);
		return false;
	}
	return true;
}

bool LuaScript::enterTable()
{
	if (lua_gettop(m_state) == 0) {
		APT_LOG_ERR("LuaScript::enterTable(): stack empty");
		return false;
	}
	if (lua_type(m_state, -1) != LUA_TTABLE) {
		APT_LOG_ERR("LuaScript::enterTable(): not a table");
		return false;
	}
	++m_currentTable;
	APT_ASSERT(m_currentTable != kMaxTableDepth);
	
	m_tableIndex[m_currentTable] = 0;
	m_tableField[m_currentTable].clear();
	m_tableLength[m_currentTable] = (int)lua_rawlen(m_state, -1);
	return true;
}

void LuaScript::leaveTable()
{
	if (!m_currentTable) {
		APT_LOG_ERR("LuaScript::leaveTable(): not in a table");
		return;
	}
	popToCurrentTable();
	--m_currentTable;
	APT_ASSERT(lua_type(m_state, -1) == LUA_TTABLE); // stack top should be a table
	lua_pop(m_state, 1); // pop table
}

LuaScript::ValueType LuaScript::getType() const
{
	return GetValueType(lua_type(m_state, -1));
}

template <>
bool LuaScript::getValue<bool>(int _i) const
{
	bool needPop = gotoIndex(_i);
	if (!lua_isboolean(m_state, -1)) {
		APT_LOG_ERR("LuaScript::getValue<bool>(%d): not a boolean", _i);
	}
	bool ret = lua_toboolean(m_state, -1) != 0;
	if (needPop) {
		lua_pop(m_state, 1);
	}
	return ret;
}
#define LuaScript_getValue_Number(_type, _enum) \
	template <> _type LuaScript::getValue<_type>(int _i) const { \
		bool needPop = gotoIndex(_i); \
		if (!lua_isnumber(m_state, -1)) \
			APT_LOG_ERR("LuaScript::getValue<%s>(%d): not a number", apt::DataTypeString(_enum), _i); \
		auto ret = (_type)lua_tonumber(m_state, -1); \
		if (needPop) \
			lua_pop(m_state, 1); \
		return ret; \
	}
APT_DataType_decl(LuaScript_getValue_Number)

template <>
const char* LuaScript::getValue<const char*>(int _i) const
{
	bool needPop = gotoIndex(_i);
	if (!lua_isstring(m_state, -1)) {
		APT_LOG_ERR("LuaScript::getValue<const char*>(%d): not a string", _i);
	}
	auto ret = lua_tostring(m_state, -1);
	if (needPop) {
		lua_pop(m_state, 1);
	}
	return ret;
}

template <>
void LuaScript::setValue<bool>(bool _value, int _i)
{
	if (!m_currentTable) {
		APT_LOG_ERR("LuaScript::setValue<bool>(%s, %d): not in a table", _value ? "true" : "false", _i);
		return;
	}
	if (_i == 0 && m_tableIndex[m_currentTable] == 0) {
		APT_LOG_ERR("LuaScript::setValue<bool>(%s, %d): stack empty", _value ? "true" : "false", _i);
		return;
	}
	lua_pushboolean(m_state, _value ? 1 : 0);
	_i = _i ? _i : m_tableIndex[m_currentTable];
	setValue(_i);
}

#define LuaScript_setValue_Number_i(_type, _enum) \
	template <> void LuaScript::setValue<_type>(_type _value, int _i) { \
		if (!m_currentTable) { \
			APT_LOG_ERR("LuaScript::setValue<%s>(%g, %d): not in a table", apt::DataTypeString(_enum), (lua_Number)_value, _i); \
			return; \
		} \
		if (_i == 0 && m_tableIndex[m_currentTable] == 0) { \
			APT_LOG_ERR("LuaScript::setValue<%s>(%g, %d): stack empty", apt::DataTypeString(_enum), (lua_Number)_value, _i); \
			return; \
		} \
		lua_pushnumber(m_state, (lua_Number)_value); \
		setValue(_i); \
	}
APT_DataType_decl(LuaScript_setValue_Number_i)

template <>
void LuaScript::setValue<const char*>(const char* _value, int _i)
{
	if (!m_currentTable) {
		APT_LOG_ERR("LuaScript::setValue<const char*>(%s, %d): not in a table", _value, _i);
		return;
	}
	if (_i == 0 && m_tableIndex[m_currentTable] == 0) {
		APT_LOG_ERR("LuaScript::setValue<const char*>(%s, %d): stack empty", _value, _i);
		return;
	}
	lua_pushstring(m_state, _value);
	setValue(_i);
}

template <>
void LuaScript::setValue<bool>(bool _value, const char* _name)
{
	if (!m_currentTable) {
		APT_LOG_ERR("LuaScript::setValue<bool>(%s, %s): not in a table", _value ? "true" : "false", _name);
		return;
	}
	lua_pushboolean(m_state, _value ? 1 : 0);
	setValue(_name);
}

#define LuaScript_setValue_Number_name(_type, _enum) \
	template <> void LuaScript::setValue<_type>(_type _value, const char* _name) { \
		if (!m_currentTable) { \
			APT_LOG_ERR("LuaScript::setValue<%s>(%g, %s): not in a table", apt::DataTypeString(_enum), (lua_Number)_value, _name); \
			return; \
		} \
		lua_pushnumber(m_state, (lua_Number)_value); \
		setValue(_name); \
	}
APT_DataType_decl(LuaScript_setValue_Number_name)

template <>
void LuaScript::setValue<const char*>(const char* _value, const char* _name)
{
	if (!m_currentTable) {
		APT_LOG_ERR("LuaScript::setValue<const char*>(%s, %s): not in a table", _value, _name);
		return;
	}
	lua_pushstring(m_state, _value);
	setValue(_name);
}

// PRIVATE

LuaScript::LuaScript(Lib _libs)
{
	m_state = luaL_newstate();
	APT_ASSERT(m_state);
	loadLibs(_libs);
}

LuaScript::~LuaScript()
{
	lua_close(m_state);
}

bool LuaScript::loadLibs(Lib _libs)
{
	bool ret = true;

 // Lua standard

	luaL_requiref(m_state, "_G", luaopen_base, 1);
	lua_pop(m_state, 1);

	if ((_libs & Lib_LuaTable) != 0) {
		luaL_requiref(m_state, LUA_TABLIBNAME, luaopen_table, 1);
		lua_pop(m_state, 1);
	}
	if ((_libs & Lib_LuaString) != 0) {
		luaL_requiref(m_state, LUA_STRLIBNAME, luaopen_string, 1);
		lua_pop(m_state, 1);
	}
	if ((_libs & Lib_LuaUtf8) != 0) {
		luaL_requiref(m_state, LUA_UTF8LIBNAME, luaopen_utf8, 1);
		lua_pop(m_state, 1);
	}
	if ((_libs & Lib_LuaMath) != 0) {
		luaL_requiref(m_state, LUA_MATHLIBNAME, luaopen_math, 1);
		lua_pop(m_state, 1);
	}
	if ((_libs & Lib_LuaOs) != 0) {
		luaL_requiref(m_state, LUA_OSLIBNAME, luaopen_os, 1);
		lua_pop(m_state, 1);
	}
	if ((_libs & Lib_LuaPackage) != 0) {
		luaL_requiref(m_state, LUA_LOADLIBNAME, luaopen_package, 1);
		lua_pop(m_state, 1);
	}
	if ((_libs & Lib_LuaCoroutine) != 0) {
		luaL_requiref(m_state, LUA_COLIBNAME, luaopen_coroutine, 1);
		lua_pop(m_state, 1);
	}
	if ((_libs & Lib_LuaDebug) != 0) {
		luaL_requiref(m_state, LUA_DBLIBNAME, luaopen_debug, 1);
		lua_pop(m_state, 1);
	}

 // framework

	if ((_libs & Lib_FrmCore) != 0) {
		APT_ASSERT(false); // \todo
	}
	if ((_libs & Lib_FrmFileSystem) != 0) {
		APT_ASSERT(false); // \todo
	}


 // register common functions
	lua_register(m_state, "print", lua_print);

	return ret;
}

void LuaScript::popToCurrentTable()
{
	int n = lua_gettop(m_state) - m_currentTable;
	APT_STRICT_ASSERT(n >= 0);
	lua_pop(m_state, n);
}

void LuaScript::popAll()
{
	int n = lua_gettop(m_state);
	n = APT_MAX(0, n - 1); // we keep the script chunk on the bottom of the stack, see execute()
	lua_pop(m_state, n);
}

void LuaScript::setValue(int _i)
{
	APT_STRICT_ASSERT(m_currentTable);
	_i = _i ? _i : m_tableIndex[m_currentTable];
	m_tableLength[m_currentTable] = APT_MAX(_i, m_tableLength[m_currentTable]);
	lua_seti(m_state, m_currentTable, _i);
	if (_i == 0 || _i == m_tableIndex[m_currentTable]) {
		popToCurrentTable();
		gotoIndex(m_tableIndex[m_currentTable]);
	}
}

void LuaScript::setValue(const char* _name)
{
	APT_STRICT_ASSERT(m_currentTable);
	lua_setfield(m_state, m_currentTable, _name);
	if (m_tableField[m_currentTable] == _name) {
		popToCurrentTable();
		lua_pushstring(m_state, _name);
		APT_VERIFY(lua_rawget(m_state, m_currentTable) != LUA_TNIL);
	}
}

bool LuaScript::gotoIndex(int _i) const
{
	if (_i > 0) {
		if (!m_currentTable) {
			APT_LOG_ERR("LuaScript::gotoIndex(%d): not in a table", _i);
			return false;
		}
		if (_i > m_tableLength[m_currentTable]) {
			APT_LOG_ERR("LuaScript::gotoIndex(%d): index out of bounds (table length = %d)", _i, m_tableLength[m_currentTable]);
			return false;
		}
		lua_rawgeti(m_state, m_currentTable, _i);
		return true;
	}
	return false;
}

bool LuaScript::loadText(const char* _buf, uint _bufSize, const char* _name)
{
	luaAssert(luaL_loadbufferx(m_state, _buf, _bufSize, _name, "t"));
	return m_err == 0;
}

bool LuaScript::execute()
{
 // script chunk is kept on the bottom of the stack so that we can call it multiple times
	popAll();
	lua_pushvalue(m_state, -1);
	luaAssert(lua_pcall(m_state, 0, LUA_MULTRET, 0));
	return m_err == 0;
}

void LuaScript::dbgPrintStack()
{
	int top = lua_gettop(m_state);
	String<128> msg("\n===");
	if (m_currentTable) {
		msg.appendf(" current table = %d, index = %d, length = %d", m_currentTable, m_tableIndex[m_currentTable], m_tableLength[m_currentTable]);
	}
	for (int i = 1; i <= top; ++i) {
		msg.appendf("\n%d: ", i);
		int type = lua_type(m_state, i);
		switch (type) {
			case LUA_TSTRING:
				msg.appendf("LUA_TSTRING '%s'", lua_tostring(m_state, i));
				break;
			case LUA_TBOOLEAN:
				msg.appendf("LUA_TBOOLEAN '%d'", (int)lua_toboolean(m_state, i));
				break;
			case LUA_TNUMBER:
				msg.appendf("LUA_TNUMBER '%g'", lua_tonumber(m_state, i));
				break;
			case LUA_TTABLE:
				msg.append("LUA_TTABLE");
				break;
			case LUA_TFUNCTION:
				msg.append("LUA_TFUNCTION");
				break;
			case LUA_TNIL:
				msg.append("LUA_TNIL");
				break;
			case LUA_TNONE:
				msg.append("LUA_TNONE");
				break;
			default:
				msg.appendf("? %s", lua_typename(m_state, i));
				break;
		};
	}
	APT_LOG_DBG(msg.c_str());
}
