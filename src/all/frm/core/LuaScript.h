#pragma once

#include <frm/core/def.h>

struct lua_State;

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// LuaScript
// Traversal of a loaded script is a state machine:
//
//  LuaScript* script = LuaScript::Create("script.lua");
//  
//  if (script->find("Value")) {                                // find a global value
//     if (script->getType() == LuaScript::ValueType_Number) {  // optionally check it's the right type
//        int v = script->getValue<int>();                      // retrieve/store the value
//     }
//  }
//  
//  if (script->find("Table")) {
//     if (script->enterTable()) {
//        // inside a table, either call 'find' as above (for named values) or...
//        while (script->next()) {                                    // get the next value while one exists
//           if (script->getType() == LuaScript::ValueType_Number) {  // optionally check it's the right type
//              int v = script->getValue<int>();                      // retrieve/store the value
//           }
//        }
//        script->leaveTable(); // must leave the table before proceeding
//     }
//  }
//  
//  LuaScript::Destroy(script);
////////////////////////////////////////////////////////////////////////////////
class LuaScript
{
public:
	enum ValueType
	{
		ValueType_Nil,
		ValueType_Table,
		ValueType_Bool,
		ValueType_Number,
		ValueType_String,
		ValueType_Function,

		ValueType_Count
	};

	enum Lib
	{
	 // Lua standard
		Lib_LuaTable        = 1 << 0,
		Lib_LuaString       = 1 << 1,
		Lib_LuaUtf8         = 1 << 2,
		Lib_LuaMath         = 1 << 3,
		Lib_LuaIo           = 1 << 4,
		Lib_LuaOs           = 1 << 5,
		Lib_LuaPackage      = 1 << 6,
		Lib_LuaCoroutine    = 1 << 7,
		Lib_LuaDebug        = 1 << 8,

	 // framework libs
		Lib_FrmCore         = 1 << 9,
		Lib_FrmFileSystem   = 1 << 10,
		
		Lib_None            = 0,
		Lib_Defaults        = Lib_LuaTable | Lib_LuaString | Lib_LuaUtf8 | Lib_LuaMath | Lib_FrmCore | Lib_FrmFileSystem,
		Lib_LuaStandard     = Lib_LuaTable | Lib_LuaString | Lib_LuaUtf8 | Lib_LuaMath | Lib_LuaIo | Lib_LuaOs | Lib_LuaPackage | Lib_LuaCoroutine | Lib_LuaDebug, 
	};

	// Load/execute a script file. Return 0 if an error occurred.
	static LuaScript* CreateAndExecute(const char* _path, Lib _libs = Lib_Defaults);

	static LuaScript* Create(const char* _path, Lib _libs = Lib_Defaults);

	static void Destroy(LuaScript*& _script_);

 // Traversal

	// Go to a named value in the current table, or a global value if not in a table/not found. Return false if not found.
	bool      find(const char* _name);

	// Go to the next value in the current table. Return true if not the end of the table.
	bool      next();
	
	// Enter the current table (call immediately after find() or next()). Return false if the current value is not a table.
	bool      enterTable();
	// Leave the current table.
	void      leaveTable();

	// Reset the traversal state machine.
	void      reset();

 // Introspection

	// Get the type of the current value.
	ValueType getType() const;

	// Get the number of elements in the current table. Return -1 if not in a table.
	int       getTableLength() const { return m_tableLength[m_currentTable]; }

	// Get the current value. tType must match the type of the current value (i.e. getValue<int>() must be called only if the value type is ValueType_Number).
	// _i permits array access (when in an array). 1 <= _i < getArrayLength().
	// Note that the ptr returned by getValue<const char*> is only valid until the next call to find() or next().
	template <typename tType>
	tType     getValue(int _i = 0) const;

	// Get a named value. Equivalent to find(_name) followed by getValue(-1).
	template <typename tType>
	tType     getValue(const char* _name) { APT_VERIFY(find(_name)); return getValue<tType>(-1); }
	
	bool      execute();

private:
	static const int kMaxTableDepth = 8;
	static const int kInvalidIndex  = -1;
	
	lua_State* m_state                        = nullptr;
	int        m_err                          = 0;
	int        m_currentTable                 = kInvalidIndex;     // Subtable level (stack top), -1 if not in a table.
	int        m_tableIndex[kMaxTableDepth]   = { kInvalidIndex }; // Traversal index stack (per table).
	int        m_tableLength[kMaxTableDepth]  = { 0 };             // Stored table lengths.
	bool       m_needPop                      = false;             // Whether the next find() or next() call should pop the stack.

	LuaScript(Lib _libs);
	~LuaScript();

	bool loadLibs(Lib _libs);

	bool gotoIndex(int _i) const;

	bool loadText(const char* _buf, uint _bufSize, const char* _name);

}; // class LuaScript

} // namespace frm
