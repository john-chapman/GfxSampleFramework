#include "TextParser.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

using namespace frm;

TextParser::TextParser(const char* _str)
	: m_start(_str)
	, m_pos(_str)
{
}

bool TextParser::isWhitespace() const
{
	return isspace(*m_pos) != 0;
}
bool TextParser::isAlpha() const
{
	return isalpha(*m_pos) != 0;
}
bool TextParser::isNum() const
{
	return isdigit(*m_pos) != 0;
}
bool TextParser::isAlphaNum() const
{
	return isalnum(*m_pos) != 0;
}
bool TextParser::isLineEnd() const
{
	return *m_pos == '\n';
}

char TextParser::advanceToNext(char _c)
{
	while (!isNull() && *m_pos != _c) {
		++m_pos;
	}
	return *m_pos;
}

char TextParser::advanceToNext(const char* _list)
{
	while (!isNull()) {
		const char* c = _list;
		while (*c != 0) {
			if (*m_pos == *c) {
				return *c;
			}
			++c;
		}
		++m_pos;
	}
	return *m_pos;
}

char TextParser::advanceToNextWhitespace()
{
	while (!isNull() && !isWhitespace()) {
		++m_pos;
	}
	return *m_pos;
}

char TextParser::advanceToNextWhitespaceOr(char _c)
{
	while (!isNull() && !isWhitespace() && *m_pos != _c) {
		++m_pos;
	}
	return *m_pos;
}

char TextParser::advanceToNextWhitespaceOr(const char* _list)
{	
	while (!isNull() && !isWhitespace()) {
		const char* c = _list;
		while (*c != 0) {
			if (*m_pos == *c) {
				return *c;
			}
			++c;
		}
		++m_pos;
	}
	return *m_pos;
}

char TextParser::advanceToNextAlpha()
{
	while (!isNull() && !isAlpha()) {
		++m_pos;
	}
	return *m_pos;
}

char TextParser::advanceToNextNum()
{
	while (!isNull() && !isNum()) {
		++m_pos;
	}
	return *m_pos;
}

char TextParser::advanceToNextAlphaNum()
{
	while (!isNull() && !isAlphaNum()) {
		++m_pos;
	}
	return *m_pos;
}

char TextParser::advanceToNextNonAlphaNum()
{
	while (!isNull() && isAlphaNum()) {
		++m_pos;
	}
	return *m_pos;
}

char TextParser::skipLine() 
{
	if (advanceToNext('\n') == '\n') {
		advance();
	}
	return *m_pos;
}

char TextParser::skipWhitespace()
{
	while (!isNull() && isWhitespace()) {
		++m_pos;
	}
	return *m_pos;
}

char TextParser::containsAny(const char* _beg, const char* _list)
{
	while (*_beg != 0 && _beg != m_pos) {
		const char* c = _list;
		while (*c != 0) {
			if (*_beg == *c) {
				return *c;
			}
			++c;
		}
		++_beg;
	}
	return (char)0;
}

bool TextParser::matches(const char *_beg, const char* _str)
{
	return strncmp(_beg, _str, m_pos - _beg) == 0;
}

bool TextParser::find(const char* _str)
{
	const char* ret = strstr(m_pos, _str);
	if (!ret) {
		return false;
	}
	m_pos = ret;
	return true;
}

int TextParser::getLineCount(const char* _pos) const
{
	const char* c = _pos ? _pos : m_pos;
	int ret = 0;
	do {
		ret += *c == '\n' ? 1 : 0;
		--c;
	} while (c != m_start);
	return ret;
}

bool TextParser::readNextBool(bool& out_)
{
	skipWhitespace();
	if (*m_pos == 't' || *m_pos == 'T' || *m_pos == '1') {
		advanceToNextWhitespace();
		out_ = true;
		return true;
	}
	if (*m_pos == 'f' || *m_pos == 'F' || *m_pos == '0') {
		advanceToNextWhitespace();
		out_ = false;
		return true;
	}
	return false;
}

bool TextParser::readNextDouble(double& out_)
{
	skipWhitespace();
	if (isNum() || *m_pos == '+' || *m_pos == '-') {
		const char* beg = m_pos;
		advanceToNextWhitespace();
		out_ = strtod(beg, 0);
		return true;
	}
	return false;
}

bool TextParser::readNextInt(long int& out_)
{
	skipWhitespace();
	if (isNum() || *m_pos == '+' || *m_pos == '-') {
		const char* beg = m_pos;
		advanceToNextWhitespace();
		out_ = strtol(beg, 0, 0);
		return true;
	}
	return false;
}

bool TextParser::compareNext(const char* _str)
{
	skipWhitespace();
	const char* beg = m_pos;
	advanceToNextWhitespace();
	if (matches(beg, _str)) {
		return true;
	}
	m_pos = beg;
	return false;
}