#include <frm/ShaderPreprocessor.h>

#include <apt/log.h>
#include <apt/File.h>
#include <apt/FileSystem.h>
#include <apt/String.h>
#include <apt/TextParser.h>

using namespace frm;
using namespace apt;

// PUBLIC 

ShaderPreprocessor::ShaderPreprocessor()
{
}

bool ShaderPreprocessor::process(const char* _fileName)
{
	File f;
	if (!FileSystem::Read(f, _fileName)) {
		return false;
	}

 // initial line pragma marks the start of this file
	uint lineCount = 0u, fileCount = m_included.size();
	appendLineComment(_fileName);
	appendLinePragma(lineCount, fileCount);

 // store the filename to prevent recursive includes
	m_included.push_back(String<64>(_fileName));

	int commentBlock = 0; // if !0 then we're inside a comment block
	bool commentLine = false;
	TextParser tp(f.getData());
	while (!tp.isNull()) {
		if (tp.isLineEnd()) {
			++lineCount;
			commentLine = false;

		} else if (*tp == '/') {
		 // potential comment block/line comment
			if (tp[1] == '/') { // comment line
				commentLine = true;

			} else if (tp[1] == '*') { // comment block
				++commentBlock;
			}

		} else if (*tp == '*') {
		 // potential comment block ending
			if (tp[1] == '/') {
				--commentBlock;
				if (commentBlock < 0) {
					APT_LOG_ERR("ShaderPreprocessor: Comment block error ('%s' line %d)", _fileName, lineCount);
					return false;
				}
			}

		} else if (*tp == '#' && commentBlock == 0 && !commentLine) {
		 // potential include directive
			if (strncmp(tp, "#include", sizeof("#include") - 1u) == 0)  {
			 // store counters/file name - just a convenience to avoid passing them around
				tp.advanceToNextWhitespace();
				tp.skipWhitespace();
				if (!include(tp, lineCount, fileCount, _fileName)) {
					return false;
				}
				tp.skipLine();
				++lineCount;

				continue; // don't advance tp or push to the result
			}
		
		}

		m_result.push_back(*tp);
		tp.advance();
	}

	m_result.push_back('\n');
	m_result.push_back('\0');
	return true;
}

void ShaderPreprocessor::append(const char* _str, bool _newLine)
{
	while (*_str) {
		m_result.push_back(*_str);
		++_str;
	}
	if (_newLine) {
		m_result.push_back('\n');
	}
}

// PRIVATE

bool ShaderPreprocessor::include(const char* _tp, uint _line, uint _file, const char* _fileName)
{
 // extract filename
	TextParser tp(_tp);
	if (*tp != '"') {
		APT_LOG_ERR("ShaderPreprocessor: Invalid #include ('%s' line %d)", _fileName, _line);
		return false;
	}
	tp.advance(); // skip '"'
	const char *beg = tp;
	if (tp.advanceToNext("\"\n") != '"') {
		APT_LOG_ERR("ShaderPreprocessor: Invalid #include ('%s' line %d)", _fileName, _line);
		return false;
	}
	String<64> fname;
	fname.set(beg, tp - beg);

 // check whether we already included this file
	for (auto it = m_included.begin(); it != m_included.end(); ++it) {
		if (*it == (const char*)fname) {
			return true; // it's not an error for an include to appear multiple times, we simply skip it
		}
	}
	
	bool ret = process(fname);
	m_result.pop_back(); // pop the terminating '\0' added by process()

 // resume line/file
	appendLinePragma(_line + 1u, _file); // +1 because this replaces the #include directive itself

	return ret;
}

void ShaderPreprocessor::appendLinePragma(uint _line, uint _file)
{
	String<sizeof("#line 9999 999\n\0")> line;
	line.setf("#line %d %d", _line + 1u, _file); // +1 because #line pragma is for the following line
	append(line, true);
}

void ShaderPreprocessor::appendLineComment(const char* _str)
{
	m_result.push_back('/');
	m_result.push_back('/');
	append(_str, true);
}