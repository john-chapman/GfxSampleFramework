#pragma once

#include <frm/core/frm.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// TextParser
// Common text parsing operations; advance a string ptr with character
// classification, check if a region of the string contains characters, keep
// a count of line endings, etc. Wraps cctype and cstring.
//
// Only line feed '\n' are counted as line endings; carriage return '\r' are 
// treated as whitespace only.
////////////////////////////////////////////////////////////////////////////////
class TextParser
{
public:
	TextParser(const char* _str);

	// Classification functions test the char at the current position.
	bool isNull() const { return *m_pos == 0; }
	bool isWhitespace() const;
	bool isAlpha() const;
	bool isNum() const;
	bool isAlphaNum() const;
	bool isLineEnd() const;

	// advance*() and skip*() functions return the character they stop on (0 if the function reached the end of the sequence).
	char advance(sint _n = 1) { m_pos += _n; return *m_pos;  }
	char advanceToNext(char _c); // advance to next occurence of _c
	char advanceToNext(const char* _list); // advance to next occurence of any char in _list
	char advanceToNextWhitespace();
	char advanceToNextWhitespaceOr(char _c);
	char advanceToNextWhitespaceOr(const char* _list);
	char advanceToNextAlpha();
	char advanceToNextNum();
	char advanceToNextAlphaNum();
	char advanceToNextNonAlphaNum();
	char skipLine();
	char skipWhitespace(); // include empty lines

	void reset(const char* _pos = nullptr) { m_pos = _pos ? _pos : m_start; }

	// Return the first occurence of any characters from _list in the region between _beg and the current position, or 0 if none.
	char containsAny(const char* _beg, const char* _list);

	// Return true if the region between _beg and the current position exactly matches _str.
	bool matches(const char *_beg, const char* _str);

	// Advance to the next occurrence of substring _str, return false if not found.
	bool find(const char* _str);

	// Return # occurences of '\n' up to and including _pos (or the current position if _pos is 0).
	int getLineCount(const char* _pos = nullptr) const;

	// Return # of chars between the start of the string and the current position.
	int getCharCount() const { return (int)(m_pos - m_start); }

	operator const char*() { return m_pos; }


	// readNext*() and compareNext*() functions operate on the next region of non-whitespace characters,
	// returning true if the conversion/comparison succeeded in which case the current position is the
	// first whitespace character immediately after the region. If the functions return false the current
	// position is unchanged.
	bool readNextBool(bool& out_);      // accepts 't', 'f', 'true', 'false', '1', '0'  
	bool readNextDouble(double& out_);  // strtod
	bool readNextInt(long int& out_);   // strtol
	bool compareNext(const char* _str);

private:
	const char* m_start;
	const char* m_pos;

};

} // namespace frm
