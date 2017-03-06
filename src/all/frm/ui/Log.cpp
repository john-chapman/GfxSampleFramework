#include <frm/ui/Log.h>

#include <cstring>

#include <imgui/imgui.h>

using namespace frm;
using namespace ui;
using namespace apt;

/*	Implementaiton is based on 2 ring buffers: a large(ish) text buffer and a smaller 'message' buffer which
	contains metadata for each message (color) and a ptr into the text buffer. Max number of messages is 
	therefore the message buffer size (but can be lower if you fill up the text buffer).

	- Text for a message must be contiguous in the buffer (hence wrap if it's not going to fit).
	- If writing new text overwrites text belonging to the message head, advance the head.
	- If the message buffer 'next write' ptr meets the head, advance the head.
*/

static const Log::Message kNullMessage = { "", 0, (ImU32)ImColor(0,0,0) };

// PUBLIC

Log::Log(int _maxMessageCount, int _maxMessageLength)
	: m_msgBuf(0)
	, m_msgBufHead(0)
	, m_msgBufNext(0)
	, m_msgBufCapacity(_maxMessageCount)
	, m_txtBuf(0)
	, m_txtBufNext(0)
	, m_txtBufCapacity(m_msgBufCapacity * _maxMessageLength)
	, m_lastLog(0)
	, m_lastDbg(0)
	, m_lastErr(0)
	, m_scrollToBottom(true)
{

	m_msgBuf     = new Message[m_msgBufCapacity];
	m_msgBufHead = m_msgBuf;
	m_msgBufNext = m_msgBuf;

	m_txtBuf     = new char[m_txtBufCapacity];
	m_txtBufNext = m_txtBuf;
}

Log::~Log()
{
	delete[] m_msgBuf;
	delete[] m_txtBuf;
}

const Log::Message* Log::addMessage(const char* _msg, LogType _type)
{
	const Message* ret;
	static ImU32 kColorErr = ImColor(1.0f, 0.2f, 0.0f);
	static ImU32 kColorDbg = ImColor(0.0f, 1.0f, 0.2f);
	static ImU32 kColorLog = ImColor(0.8f, 0.8f, 0.8f);

	switch (_type) {
		case LogType_Error:  ret = m_lastErr = addMessage(_msg, kColorErr); break;
		case LogType_Debug:  ret = m_lastDbg = addMessage(_msg, kColorDbg); break;
		case LogType_Log:  
		default:             ret = m_lastLog = addMessage(_msg, kColorLog); break;
	};

	return ret;
}

const Log::Message* Log::addMessage(const char* _msg, ImU32 _col)
{
	#define INC_WRAP(_ptr_, _start, _length) ++_ptr_; if (_ptr_ >= _start + _length) { _ptr_ = _start; }
	#define DEC_WRAP(_ptr_, _start, _length) --_ptr_; if (_ptr_ < _start) { _ptr_ = _start + _length - 1; }
	#define INC_MSG_HEAD() INC_WRAP(m_msgBufHead, m_msgBuf, m_msgBufCapacity)
	#define DEC_MSG_HEAD() DEC_WRAP(m_msgBufHead, m_msgBuf, m_msgBufCapacity)
	#define INC_MSG_NEXT() INC_WRAP(m_msgBufNext, m_msgBuf, m_msgBufCapacity)
	#define INC_TXT_NEXT() INC_WRAP(m_txtBufNext, m_txtBuf, m_txtBufCapacity)


	int len = (int)strlen(_msg) + 1; // +1 for null terminator
	APT_ASSERT(len < m_txtBufCapacity);
	len = APT_MIN(len, m_txtBufCapacity);
	const char* txtBufEnd = m_txtBuf + m_txtBufCapacity;
	if (m_txtBufNext + len >= txtBufEnd) {
	 // text must be contiguous in memory, so wrap if _msg won't fit in the remaining space
		m_txtBufNext = m_txtBuf;
	}
	memcpy(m_txtBufNext, _msg, len);

	Message* ret = m_msgBufNext;
	if (ret == m_lastErr) {
		m_lastErr = 0;
	} else if (ret == m_lastDbg) {
		m_lastDbg = 0;
	} else if (ret == m_lastLog) {
		m_lastLog = 0;
	}

	m_msgBufNext->m_txt = m_txtBufNext;
	m_msgBufNext->m_len = len;
	m_msgBufNext->m_col = _col;

 // advance the msg buffer
	const Message* msgBufEnd = m_msgBuf + m_msgBufCapacity;
	INC_MSG_NEXT();
	if (m_msgBufNext == m_msgBufHead) {
		INC_MSG_HEAD();
	}

 // advance msg buffer head if we overwrote the text
	//const char* txtEnd = m_txtBufNext + len;
	//const Message* msgCheck = m_msgBufHead + 1;
	//while (msgCheck != m_msgBufNext && m_msgBufHead->m_txt < txtEnd) {
	//	INC_WRAP(msgCheck, m_msgBuf, m_msgBufCapacity);
	//	INC_MSG_HEAD();
	//}
	
 // advance the txt buffer
	m_txtBufNext += len;
	INC_TXT_NEXT();

	m_scrollToBottom = true;
	return ret;

	#undef INC_TXT_NEXT
	#undef INC_MSG_NEXT
	#undef DEC_MSG_HEAD
	#undef INC_MSG_HEAD
	#undef DEC_WRAP
	#undef INC_WRAP
}

void Log::draw()
{
	const Message* msg = m_msgBufHead;
	const Message* msgEnd = m_msgBuf + m_msgBufCapacity;
	while (msg != m_msgBufNext) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImColor(msg->m_col));
		ImGui::TextWrapped(msg->m_txt);
		ImGui::PopStyleColor();
		++msg;
		if (msg >= msgEnd) {
			msg = m_msgBuf;
		}
	}
	if (m_scrollToBottom) {
		ImGui::SetScrollHere();
		m_scrollToBottom = false;
	}
}