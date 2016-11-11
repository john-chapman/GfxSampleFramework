#pragma once
#ifndef frm_LuaScript_h
#define frm_LuaScript_h

#include <frm/def.h>

struct lua_State;

namespace frm {

////////////////////////////////////////////////////////////////////////////////
/// \class LuaScript
/// Traversal of a loaded script is a state machine:
/// \code
///  LuaScript* script = LuaScript::Create("scripts/script.lua");
///  
///  if (script->find("Value")) {                         // find a global value
///     if (script->getType() == LuaScript::kNumber) {    // check it's the right type
///        int v = script->getValue<int>();               // retrieve/store the value
///     }
///  }
///  
///  if (script->find("Table")) {
///     if (script->enterTable()) {
///        // inside a table, either call 'find' as above (for named values) or...
///        while (script->next()) {                                // get the next value while one exists
///           if (script->getType() == LuaScript::kNumber) {       // check it's the right type
///              int v = script->getValue<int>();                  // retrieve/store the value
///           }
///        }
///        script->leaveTable(); // must leave the table before proceeding
///     }
///  }
///  
///  LuaScript::Destroy(script);
/// \endcode
////////////////////////////////////////////////////////////////////////////////
class LuaScript
{
public:
	enum ValueType
	{
		kNil,
		kTable,
		kBool,
		kNumber,
		kString,
		kFunction,

		kValueTypeCount
	};

	/// Load/execute a script file.
	/// \return 0 if an error occurred.
	static LuaScript* Create(const char* _path);

	static void Destroy(LuaScript*& _script_);


	/// Find a named value (globally or in the current table).
	/// \return true if the value is found, in which case getValue() may be called
	///   (and any const char* returned by getValue() is invalid).
	bool find(const char* _name);

	/// Get the next value in the current table.
	/// \return true if not the end of the table, in which case getValue() may be 
	///   called (and any const char* returned by getValue() is invalid).
	bool next();
	
	/// Get the type of the current value.
	ValueType getType() const;

	/// Get the current value. tType is expected to match the type of the current 
	/// value exactly (i.e. getValue<int>() must be called only if the value type
	/// is kNumber). 
	/// \note Ptr returned by getValue<const char*> is only valid until the next
	///    call to find() or next().
	template <typename tType>
	tType getValue() const;

	/// Enter the current table (call immediately after find() or next()).
	/// \return false if the current value is not a table.
	bool enterTable();

	/// Leave the current table. 
	void leaveTable();
	
	/// Get the length of the current table.
	int getTableLength() const { return m_tableLength[m_currentTable]; }

private:

	lua_State* m_state;
	int        m_err;

	static const int kMaxTableDepth = 8;
	int        m_currentTable;                //< Subtable level (stack top), -1 if not in a table.
	int        m_tableIndex[kMaxTableDepth];  //< Traversal index stack (per table).
	int        m_tableLength[kMaxTableDepth]; //< Stored table lengths.
	bool       m_needPop;                     //< Whether the next find() or next() call should pop the stack.

	LuaScript();
	~LuaScript();

	bool loadText(const char* _buf, uint _bufSize, const char* _name);
	bool execute();

}; // class LuaScript

} // namespace frm

#endif // frm_LuaScript_h
