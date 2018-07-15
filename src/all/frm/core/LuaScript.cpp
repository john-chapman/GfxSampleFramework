#include <frm/LuaScript.h>

#include <apt/log.h>
#include <apt/FileSystem.h>

#include <lua/lua.hpp>

using namespace frm;
using namespace apt;

#define luaAssert(_call) \
	if ((m_err = (_call)) != 0) { \
		APT_LOG_ERR("Lua error: %s", lua_tostring(m_state, -1)); \
		lua_pop(m_state, 1); \
	}

extern "C" {

static int lua_print(lua_State* _lstate)
{
	const char* str = luaL_optstring(_lstate, 1, "nil");
	APT_LOG(str);
	return 0;
}

} // extern "C"

static LuaScript::ValueType GetValueType(int _luaType)
{
	switch (_luaType) {
		case LUA_TNIL:      return LuaScript::kNil;
		case LUA_TTABLE:    return LuaScript::kTable;
		case LUA_TBOOLEAN:  return LuaScript::kBool;
		case LUA_TNUMBER:   return LuaScript::kNumber;
		case LUA_TSTRING:   return LuaScript::kString;
		case LUA_TFUNCTION: return LuaScript::kFunction;
		default: APT_ASSERT(false); break;
	};

	return LuaScript::kValueTypeCount;
}

/*******************************************************************************

                                LuaScript

*******************************************************************************/

// PUBLIC

LuaScript* LuaScript::Create(const char* _path)
{
	File f;
	if (!FileSystem::ReadIfExists(f, _path)) {
		return 0;
	}

	LuaScript* ret = new LuaScript();
	if (!ret->loadText(f.getData(), f.getDataSize(), f.getPath())) {
		goto LuaScript_Create_end;
	}
	if (!ret->execute()) {
		goto LuaScript_Create_end;
	}

LuaScript_Create_end:
	if (ret->m_err != 0) {
		delete ret;
		return 0;
	}
	return ret;
}

void LuaScript::Destroy(LuaScript*& _script_)
{
	delete _script_;
	_script_ = 0;
}

bool LuaScript::find(const char* _name)
{
	if (m_needPop) { // don't reset m_needPop, we're about to push a new value
		lua_pop(m_state, 1);
	}
	int ret;
	if (m_currentTable == -1) {
	 // search globally
		ret = lua_getglobal(m_state, _name);
	} else {
	 // search the current table
		lua_pushstring(m_state, _name);
		ret = lua_gettable(m_state, -2);
	}
	if (ret == LUA_TNIL) {
		lua_pop(m_state, -1);
		return false;
	}
	m_needPop = true;
	return true;
}

bool LuaScript::next()
{
	APT_ASSERT(m_currentTable >= 0); // must be in a table
	if (m_currentTable == -1) {
		return false;
	}
	if (m_needPop) {
		lua_pop(m_state, 1);
	}
	int ret = lua_geti(m_state, -1, m_tableIndex[m_currentTable]);
	if (ret == LUA_TNIL) {
		lua_pop(m_state, 1);
		return false;
	}
	++m_tableIndex[m_currentTable];
	m_needPop = true;
	return true;
}

LuaScript::ValueType LuaScript::getType() const
{
	APT_ASSERT(m_needPop); // nothing on stack
	if (!m_needPop) {
		return kNil;
	}
	return GetValueType(lua_type(m_state, -1));
}

template <>
bool LuaScript::getValue<bool>() const
{
	APT_ASSERT_MSG(lua_type(m_state, -1) == LUA_TBOOLEAN, "LuaScript::getValue(): top was not a boolean");
	return lua_toboolean(m_state, -1) != 0;
}
template <>
int LuaScript::getValue<int>() const
{
	APT_ASSERT_MSG(lua_type(m_state, -1) == LUA_TNUMBER, "LuaScript::getValue(): top was not a number");
	return (int)lua_tonumber(m_state, -1);
}
template <>
unsigned int LuaScript::getValue<unsigned int>() const
{
	APT_ASSERT_MSG(lua_type(m_state, -1) == LUA_TNUMBER, "LuaScript::getValue(): top was not a number");
	return (unsigned int)lua_tonumber(m_state, -1);
}
template <>
float LuaScript::getValue<float>() const
{
	APT_ASSERT_MSG(lua_type(m_state, -1) == LUA_TNUMBER, "LuaScript::getValue(): top was not a number");
	return (float)lua_tonumber(m_state, -1);
}
template <>
double LuaScript::getValue<double>() const
{
	APT_ASSERT_MSG(lua_type(m_state, -1) == LUA_TNUMBER, "LuaScript::getValue(): top was not a number");
	return (double)lua_tonumber(m_state, -1);
}
template <>
const char* LuaScript::getValue<const char*>() const
{
	APT_ASSERT_MSG(lua_type(m_state, -1) == LUA_TSTRING, "LuaScript::getValue(): top was not a string");
	return lua_tostring(m_state, -1);
}

bool LuaScript::enterTable()
{
	if (m_needPop && lua_type(m_state, -1) == LUA_TTABLE) {
		++m_currentTable;
		APT_ASSERT(m_currentTable != kMaxTableDepth);

		lua_len(m_state, -1);
		m_tableLength[m_currentTable] = getValue<int>();
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

// PRIVATE

LuaScript::LuaScript()
	: m_state(0)
	, m_err(0)
	, m_currentTable(-1)
	, m_needPop(false)
{
	m_state = luaL_newstate();
	APT_ASSERT(m_state);
	luaL_openlibs(m_state);

 // register common functions
	lua_register(m_state, "print", lua_print);
}

LuaScript::~LuaScript()
{
	lua_close(m_state);
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
