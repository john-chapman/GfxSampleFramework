#include "LuaScript.h"

#include <apt/log.h>
#include <apt/memory.h>
#include <apt/FileSystem.h>

#include <lua/lua.hpp>
#include <lua/lualib.h>
#include <lua/lauxlib.h>

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
	const char* str = luaL_optstring(_L, 1, "nil");
	APT_LOG(str);
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
	if (m_needPop) { // don't reset m_needPop, we're about to push a new value
		lua_pop(m_state, 1);
	}
	int type = LUA_TNIL;
	if (m_currentTable != -1) {
	 // table search
		lua_pushstring(m_state, _name);
		type = lua_gettable(m_state, -2);
	}
	if (type == LUA_TNIL) {
	 // global search
		type = lua_getglobal(m_state, _name);
	}
	if (type == LUA_TNIL) {
		while (lua_type(m_state, 1) == LUA_TNIL) {
			lua_pop(m_state, 1);
		}
		return false;
	}
	m_needPop = true;
	return true;
}

bool LuaScript::next()
{
	if (m_currentTable == -1) {
		APT_LOG_ERR("LuaScript: next() called when not in a table");
		return false;
	}
	if (m_needPop) {
		lua_pop(m_state, 1);
	}
	int type = lua_geti(m_state, -1, m_tableIndex[m_currentTable]);
	if (type == LUA_TNIL) {
		lua_pop(m_state, 1);
		return false;
	}
	++m_tableIndex[m_currentTable];
	m_needPop = true;
	return true;
}

bool LuaScript::enterTable()
{
	if (m_needPop && lua_type(m_state, -1) == LUA_TTABLE) {
		++m_currentTable;
		APT_ASSERT(m_currentTable != kMaxTableDepth);

		lua_len(m_state, -1);
		m_tableLength[m_currentTable] = (int)lua_tonumber(m_state, -1);
		lua_pop(m_state, 1);
		
		m_tableIndex[m_currentTable] = 1; // lua indices begin at 1
		m_needPop = false; // table is popped by leaveTable()
		return true;
	}
	APT_ASSERT(false); // nothing on the stack, or not a table
	return false;
}

void LuaScript::leaveTable()
{
	APT_ASSERT(m_currentTable >= 0); // unmatched call to enterTable()
	
	if (m_needPop) {
		lua_pop(m_state, 1); // pop value
		m_needPop = false;
	}
	--m_currentTable;
	APT_ASSERT(lua_type(m_state, -1) == LUA_TTABLE); // sanity check, stack top should be a table
	lua_pop(m_state, 1); // pop table
}

LuaScript::ValueType LuaScript::getType() const
{
	if (!m_needPop) {
		APT_LOG_ERR("LuaScript: getType() called with no value selected");
		return ValueType_Nil;
	}
	return GetValueType(lua_type(m_state, -1));
}

template <>
bool LuaScript::getValue<bool>(int _i) const
{
	bool needPop = gotoIndex(_i);
	if (!lua_isboolean(m_state, -1)) {
		APT_LOG_ERR("LuaScript: getValue<bool>(%d) was not a boolean", _i);
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
			APT_LOG_ERR("LuaScript: getValue<%s>(%d) was not a number", apt::DataTypeString(_enum), _i); \
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
		APT_LOG_ERR("LuaScript: getValue<const char*>(%d) was not a string", _i);
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
	bool needPop = gotoIndex(_i);
	if (!lua_isboolean(m_state, -1)) {
		APT_LOG_ERR("LuaScript: setValue<bool>(%d) was not a boolean", _i);
	}
	lua_pushboolean(m_state, _value ? 1 : 0);
	//lua_rawset
	bool ret = lua_toboolean(m_state, -1) != 0;
	if (needPop) {
		lua_pop(m_state, 1);
	}
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

bool LuaScript::gotoIndex(int _i) const
{
	if (_i > 0) {
		if (m_currentTable == kInvalidIndex) {
			APT_LOG_ERR("LuaScript::gotoIndex(%d): not a table", _i);
			return false;
		}
		if (_i > m_tableLength[m_currentTable]) {
			APT_LOG_ERR("LuaScript::gotoIndex(%d): index out of bounds (table length = %d)", _i, m_tableLength[m_currentTable]);
			return false;
		}
		lua_geti(m_state, -1, _i);
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
	luaAssert(lua_pcall(m_state, 0, LUA_MULTRET, 0));
	return m_err == 0;
}
