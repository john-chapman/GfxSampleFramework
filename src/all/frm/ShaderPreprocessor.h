#pragma once
#ifndef frm_ShaderPreprocessor_h
#define frm_ShaderPreprocessor_h

#include <frm/def.h>
#include <apt/String.h>
#include <EASTL/vector.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// ShaderPreprocessor
// Read source code from a file, process #includes.
// - Only `#include "filename"` supported (no `#include <filename>`). This means
//   that filenames are expected to be relative to the working directory of the
//   exe.
// - Duplicate include directives are only processed once within a single parse
//   (prevents recursive includes).
// - Does **not** track #ifdef/#endif; this may cause issues if an #include
//   directive is wrapped in #ifdef/#endif but then subsequently redeclared.
// - Inserts #line pragmas before/after each include to maintain the correctness
//   of compiler-generated error messages.
//
// Typical usage:
// - Call append() for the version string and any global defines.
// - Call process() for each filename to be appended to the source result.
// - Call getResult() when done, pass the string to the compiler.
////////////////////////////////////////////////////////////////////////////////
class ShaderPreprocessor
{
public:
	ShaderPreprocessor();

	// Open a file, process and append to the source result. Return false if an error occurred.
	bool process(const char* _fileName);

	// Directly append _str to the source result with an optional newline char.
	void append(const char* _str, bool _newLine = false);

	// Get the source result. Subsequent calls to append() or process() will invalidate the ptr returned 
	// by this method. Note that this may not be a valid null-terminated string if process() failed.
	const char* getResult() const { return (const char*)m_result.data(); }

private:
	eastl::vector<char>             m_result;    //< Source code result.
	eastl::vector<apt::String<64> > m_included;  //< Prevent recursive includes.
	
	// Process an include directive. _tp points to the first char after an #include directive. Extract 
	// filename, check that the file wasn't already included and call process().
	bool include(const char* _tp, uint _line, uint _file, const char* _fileName);

	void appendLinePragma(uint _line, uint _file);
	
	void appendLineComment(const char* _str);

}; // class ShaderPreprocessor

} // namespace frm

#endif // frm_ShaderPreprocessor_h
