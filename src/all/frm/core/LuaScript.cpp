#include "LuaScript.h"

#include <frm/core/math.h>

#include <apt/log.h>
#include <apt/memory.h>
#include <apt/FileSystem.h>
#include <apt/StringHash.h>
#include <apt/Time.h>

#include <lua/lua.hpp>
#include <lua/lualib.h>
#include <lua/lauxlib.h>

	#undef APT_STRICT_ASSERT
	#define APT_STRICT_ASSERT(x) APT_ASSERT(x)

using namespace frm;
using namespace apt;

#define luaAssert(_call) \
	if ((m_err = (_call)) != LUA_OK) { \
		APT_LOG_ERR("Lua error: %s", lua_tostring(m_state, -1)); \
		lua_pop(m_state, 1); \
	}

extern "C" 
{

int luaopen_FrmCore(lua_State* _L);

int lua_print(lua_State* _L);
int lua_include(lua_State* _L);

} // extern "C"

static void* lua_alloc(void* _ud, void* _ptr, size_t _osize, size_t _nsize)
{
	APT_UNUSED(_ud);
	APT_UNUSED(_osize);
	if (_nsize == 0) {
		APT_FREE(_ptr);
		return nullptr;
	}
	return APT_REALLOC(_ptr, _nsize);
}

static int lua_panic(lua_State* _L)
{
	APT_LOG_ERR("Lua panic: %s", lua_tostring(_L, -1));
	APT_ASSERT(false);
	return 0; // call abort()
}

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

	LuaScript* ret = APT_NEW(LuaScript(_path, _libs));
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
	if (m_currentTable != 1) {
		popToCurrentTable();
		lua_pushstring(m_state, _name);
		if (lua_rawget(m_state, m_currentTable) == LUA_TNIL) {
			return false;
		}
	} else {
		popAll();
		if (lua_getglobal(m_state, _name) == LUA_TNIL) {
			return false;
		}
	}
	m_tableField[m_currentTable] = _name;
	return true;
}

bool LuaScript::next()
{
	if (m_currentTable == 1) {
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
	if (m_currentTable == 1) {
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

namespace {
 // need to handle int/float types seperately to avoid precision issues e.g. with string hashes
	template <typename tType>
	tType getValue(lua_State* _L, apt::internal::IntT)
	{
		return (tType)lua_tointeger(_L, -1);	
	}
	template <typename tType>
	tType getValue(lua_State* _L, apt::internal::FloatT)
	{
		return (tType)lua_tonumber(_L, -1);	
	}
}
#define LuaScript_getValue_Number(_type, _enum) \
	template <> _type LuaScript::getValue<_type>(int _i) const { \
		bool needPop = gotoIndex(_i); \
		if (!lua_isnumber(m_state, -1)) \
			APT_LOG_ERR("LuaScript::getValue<%s>(%d): not a number", apt::DataTypeString(_enum), _i); \
		auto ret = ::getValue<_type>(m_state, APT_TRAITS_FAMILY(_type)); \
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
	if (m_currentTable == 1) {
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

namespace {
	template <typename tType>
	void setValue(lua_State* _L, tType _value, apt::internal::IntT)
	{
		lua_pushinteger(_L, (lua_Integer)_value);	
	}
	template <typename tType>
	void setValue(lua_State* _L, tType _value, apt::internal::FloatT)
	{
		lua_pushnumber(_L, (lua_Number)_value);
	}
}
#define LuaScript_setValue_Number_i(_type, _enum) \
	template <> void LuaScript::setValue<_type>(_type _value, int _i) { \
		if (m_currentTable == 1) { \
			APT_LOG_ERR("LuaScript::setValue<%s>(%g, %d): not in a table", apt::DataTypeString(_enum), (lua_Number)_value, _i); \
			return; \
		} \
		if (_i == 0 && m_tableIndex[m_currentTable] == 0) { \
			APT_LOG_ERR("LuaScript::setValue<%s>(%g, %d): stack empty", apt::DataTypeString(_enum), (lua_Number)_value, _i); \
			return; \
		} \
		::setValue<_type>(m_state, _value, APT_TRAITS_FAMILY(_type)); \
		setValue(_i); \
	}
APT_DataType_decl(LuaScript_setValue_Number_i)

template <>
void LuaScript::setValue<const char*>(const char* _value, int _i)
{
	if (m_currentTable == 1) {
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
	lua_pushboolean(m_state, _value ? 1 : 0);
	setValue(_name);
}

#define LuaScript_setValue_Number_name(_type, _enum) \
	template <> void LuaScript::setValue<_type>(_type _value, const char* _name) { \
		lua_pushnumber(m_state, (lua_Number)_value); \
		setValue(_name); \
	}
APT_DataType_decl(LuaScript_setValue_Number_name)

template <>
void LuaScript::setValue<const char*>(const char* _value, const char* _name)
{
	lua_pushstring(m_state, _value);
	setValue(_name);
}


bool LuaScript::execute()
{
	APT_AUTOTIMER_DBG("LuaScript::execute() %s", (const char*)m_name);

 // script chunk is kept on the bottom of the stack so that we can call it multiple times
	popAll();
	lua_pushvalue(m_state, -1);
	luaAssert(lua_pcall(m_state, 0, LUA_MULTRET, 0));
	return m_err == 0;
}

int LuaScript::call()
{
	APT_AUTOTIMER_DBG("LuaScript::call() %s", (const char*)m_name);

	int top = lua_gettop(m_state);
	int nargs = top;
	if (m_currentTable != 1) {
	 // arg count is everything on the stack within the current table
		nargs = APT_MAX(0, nargs - m_currentTable - 1); // -1 = account for the LUA_TFUNCTION
	} else {
	 // arg count is everything on the stack
		nargs = APT_MAX(0, nargs - 2); // -2 = account for the LUA_TFUNCTION and the script chunk
	}

	luaAssert(lua_pcall(m_state, nargs, LUA_MULTRET, 0));
	
 // ret count is the difference between the new top and the position of the LUA_TFUNCTION before the call to lua_pcall
	int nrets = lua_gettop(m_state) - (top - (nargs + 1));
	if (m_err) {
		lua_pop(m_state, nrets);
		return -1;
	}
	return nrets;
}


template <>
void LuaScript::pushValue<bool>(bool _value)
{
	lua_pushboolean(m_state, _value);
}

#define LuaScript_pushValue_Number(_type, _enum) \
	template <> void LuaScript::pushValue<_type>(_type _value) { \
		lua_pushnumber(m_state, (lua_Number)_value); \
	}
APT_DataType_decl(LuaScript_pushValue_Number)

template <>
void LuaScript::pushValue<const char*>(const char* _value)
{
	lua_pushstring(m_state, _value);
}

template <>
bool LuaScript::popValue<bool>()
{
	if (!lua_isboolean(m_state, -1)) {
		APT_LOG_ERR("LuaScript::popValue<bool>): not a boolean");
	}
	bool ret = lua_toboolean(m_state, -1) != 0;
	lua_pop(m_state, 1);
	return ret;
}
#define LuaScript_popValue_Number(_type, _enum) \
	template <> _type LuaScript::popValue<_type>() { \
		if (!lua_isnumber(m_state, -1)) \
			APT_LOG_ERR("LuaScript::popValue<%s>(): not a number", apt::DataTypeString(_enum)); \
		auto ret = (_type)lua_tonumber(m_state, -1); \
		lua_pop(m_state, 1); \
		return ret; \
	}
APT_DataType_decl(LuaScript_popValue_Number)

template <>
const char* LuaScript::popValue<const char*>()
{
	if (!lua_isstring(m_state, -1)) {
		APT_LOG_ERR("LuaScript::popValue<const char*>(): not a string");
	}
	auto ret = lua_tostring(m_state, -1);
	lua_pop(m_state, 1);
	return ret;
}

void LuaScript::dbgPrintStack()
{
	int top = lua_gettop(m_state);
	String<128> msg("\n===");
	if (m_currentTable != 1) {
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



// PRIVATE

LuaScript::LuaScript(const char* _name, Lib _libs)
{
	APT_AUTOTIMER_DBG("LuaScript %s", (const char*)m_name);
	m_name = _name;
	m_state = lua_newstate(lua_alloc, nullptr);
	APT_ASSERT(m_state);
	lua_atpanic(m_state, lua_panic);
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
		luaL_requiref(m_state, "FrmCore", luaopen_FrmCore, 1);
		lua_pop(m_state, 1);
	}
	if ((_libs & Lib_FrmFileSystem) != 0) {
		APT_ASSERT(false); // \todo
	}


 // common functions
	lua_register(m_state, "print",   lua_print);
	lua_register(m_state, "include", lua_include);

	return ret;
}

bool LuaScript::loadText(const char* _buf, uint _bufSize, const char* _name)
{
	luaAssert(luaL_loadbufferx(m_state, _buf, _bufSize, _name, "t"));
	return m_err == 0;
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
	APT_STRICT_ASSERT(m_currentTable != 1);
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
	if (m_currentTable != 1) {
		lua_setfield(m_state, m_currentTable, _name);
		if (m_tableField[m_currentTable] == _name) {
			popToCurrentTable();
			lua_pushstring(m_state, _name);
			APT_VERIFY(lua_rawget(m_state, m_currentTable) != LUA_TNIL);
		}

	} else {
		lua_setglobal(m_state, _name);
		if (m_tableField[m_currentTable] == _name) {
			popAll();
			APT_VERIFY(lua_getglobal(m_state, _name) != LUA_TNIL);
		}

	}

}

bool LuaScript::gotoIndex(int _i) const
{
	if (_i > 0) {
		if (m_currentTable == 1) {
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



/*******************************************************************************

                                Frm Libs

*******************************************************************************/

extern "C" 
{

static int FrmCore_Log(lua_State* _L)
{
	if (lua_gettop(_L) > 0) {
		APT_LOG(luaL_optstring(_L, 1, "nil"));
	}
	return 0;
}
static int FrmCore_LogDbg(lua_State* _L)
{
	if (lua_gettop(_L) > 0) {
		APT_LOG_DBG(luaL_optstring(_L, 1, "nil"));
	}
	return 0;
}
static int FrmCore_LogErr(lua_State* _L)
{
	if (lua_gettop(_L) > 0) {
		APT_LOG_ERR(luaL_optstring(_L, 1, "nil"));
	}
	return 0;
}

static int FrmCore_StringHash(lua_State* _L)
{
	const char* str = luaL_optstring(_L, 1, "");
	StringHash ret(str);
	lua_pushinteger(_L, (lua_Integer)ret.getHash());
	return 1;
}

static int luaopen_FrmCore(lua_State* _L)
{
	static const struct luaL_Reg FrmCore[] = {
		{ "Log",        FrmCore_Log        },
		{ "LogDbg",     FrmCore_LogDbg     },
		{ "LogErr",     FrmCore_LogErr     },
		{ "StringHash", FrmCore_StringHash },
		{ NULL,         NULL               }
	};
	luaL_newlib(_L, FrmCore);
	return 1;
}

static int lua_print(lua_State* _L)
{
	if (lua_gettop(_L) > 0) {
		APT_LOG(luaL_optstring(_L, 1, "nil"));
	}
	return 0;
}

static int lua_include(lua_State* _L)
{
	const char* path = luaL_optstring(_L, 1, "");
	
	File f;
	if (!FileSystem::Read(f, path)) {
		return 1;
	}

	if (luaL_loadbuffer(_L, f.getData(), f.getDataSize(), path) != LUA_OK) {
		return 1;
	}
	
	if (lua_pcall(_L, 0, LUA_MULTRET, 0) != LUA_OK) {
		APT_LOG_ERR("Lua error: %s", lua_tostring(_L, -1));
		lua_pop(_L, 1);
		return 1;
	}

	return 0;
}

} // extern "C"