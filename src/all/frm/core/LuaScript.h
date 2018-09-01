#pragma once

#include <frm/core/def.h>

#include <apt/String.h>

struct lua_State;

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// LuaScript
// Traversal of a loaded script is a state machine:
//
//  LuaScript* script = LuaScript::CreateAndExecute("script.lua");
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
//
// A LuaScript may be executed multiple times; calling execute() resets the 
// traversal state.
//
// Use pushValue()/popValue() to pass args to/get retvals from a function:
//
//   if (script->find("add")) {              // function 'add' is on the top of the stack
//      script->pushValue(1);                // push first arg
//      script->pushValue(2);                // push second arg
//      APT_VERIFY(script->call() == 1);     // the function only returns 1 value
//      int onePlusTwo = script->popValue(); // pop the ret val
//   }
//
// \todo
// - Traversing tables with non-integer keys via next() isn't currently 
//   implemented. It's possible via lua_next - need to detect when not in an
//   array table (m_tableLength[m_currentTable] == 0). getTableLength() in this
//   case needs to traverse the whole table via lua_next() to find the length.
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

	enum Lib_
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
	typedef int Lib;

	// Load/execute a script file. Return 0 if an error occurred.
	static LuaScript* Create(const char* _path, Lib _libs = Lib_Defaults);
	static LuaScript* CreateAndExecute(const char* _path, Lib _libs = Lib_Defaults);

	static void Destroy(LuaScript*& _script_);

 // Traversal

	// Go to a named value in the current table, or a global value if not in a table. Return false if not found.
	bool      find(const char* _name);

	// Go to the next value in the current table. Return true if not the end of the table.
	// \todo Currently only works for arrays.
	bool      next();
	
	// Enter the current table (call immediately after find() or next()). Return false if the current value is not a table.
	bool      enterTable();
	// Leave the current table.
	void      leaveTable();

	// Reset the traversal state machine.
	void      reset() { popAll(); }

 // Introspection

	// Get the type of the current value.
	ValueType getType() const;

	// Get the number of elements in the current table. Return -1 if not in a table.
	int       getTableLength() const { return m_tableLength[m_currentTable]; }

	// Get the current value. tType must match the type of the current value (i.e. getValue<int>() must be called only if the value type is ValueType_Number).
	// _i permits array access (when in an array). 1 <= _i < getTableLength().
	// Note that the ptr returned by getValue<const char*> is only valid until the next call to find(), next() or set().
	template <typename tType>
	tType     getValue(int _i = 0) const;

	// Get a named value. Equivalent to find(_name) followed by getValue().
	template <typename tType>
	tType     getValue(const char* _name) { APT_VERIFY(find(_name)); return getValue<tType>(); }
	
 // Modification
	
	// Set the current value, or the _ith element of the current table if _i > 0.
	// If _i > current table length, push ValueType_Nil elements to force the correct size.
	template <typename tType>
	void      setValue(tType _value, int _i = 0);

	// Set a named value. If the value already exists this modifies the type and value.
	template <typename tType>
	void      setValue(tType _value, const char* _name);


 // Execution

	// Execute the script. This may be called multiple times, each time the traversal state is reset.
	bool      execute();

	// Call function. Use pushValue() to push function arguments on the stack (left -> right). Return the number of ret vals on the stack, or -1 if error.
	int       call();

	// Push/pop to/from the internal stack.
	template <typename tType>
	void      pushValue(tType _value);
	template <typename tType>
	tType     popValue();

 // Debug

	void      dbgPrintStack();

private:
	static const int kMaxTableDepth = 10;
	
	apt::PathStr     m_name;
	lua_State*       m_state                        = nullptr;
	int              m_err                          = 0;

	int              m_currentTable                 = 1;        // Stack index of the current table (start at 1 to account for the script chunk).
	int              m_tableLength[kMaxTableDepth]  = { 0 };    // Length per table.
	int              m_tableIndex[kMaxTableDepth]   = { 0 };    // Index of the current element per table.
	apt::String<16>  m_tableField[kMaxTableDepth];              // Name of the current field per table (0 is the current global).

	LuaScript(const char* _name, Lib _libs);
	~LuaScript();

	bool loadLibs(Lib _libs);
	bool loadText(const char* _buf, uint _bufSize, const char* _name);

	void popToCurrentTable();
	void popAll();
	void setValue(int _i);
	void setValue(const char* _name);
	bool gotoIndex(int _i) const;

}; // class LuaScript

} // namespace frm
