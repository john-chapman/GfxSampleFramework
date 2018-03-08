#pragma once
#ifndef frm_ui_Log_h
#define frm_ui_Log_h

#include <frm/def.h>

#include <apt/log.h>

#include <imgui/imgui.h>

namespace frm { namespace ui {

////////////////////////////////////////////////////////////////////////////////
// Log
////////////////////////////////////////////////////////////////////////////////
class Log
{
public:
	
	struct Message
	{
		const char* m_txt;
		int         m_len;
		ImU32       m_col;

		operator const char*() { return m_txt; }
	};

	Log(int _maxMessageCount, int _maxMessageLength);
	~Log();
	
	const double   getElapsedTimeSinceLastMessage() const { return 0.0; }

	const Message* getLastLog() const { return m_lastLog; }
	const Message* getLastDbg() const { return m_lastDbg; }
	const Message* getLastErr() const { return m_lastErr; }

	void           clear();
	
	void		   clearLastLog() { m_lastLog = nullptr; }
	void		   clearLastDbg() { m_lastDbg = nullptr; }
	void		   clearLastErr() { m_lastErr = nullptr; }

	const Message* addMessage(const char* _msg, apt::LogType _type);
	const Message* addMessage(const char* _msg, ImU32 _col = ImColor(255, 255, 255));
	void           draw();
	void           update    (double _dt);
private:	
	void           createBuffers();
	void           deleteBuffers();

private:
	Message*       m_msgBuf;
	Message*       m_msgBufHead;     //< First message.
	Message*       m_msgBufNext;     //< Next place to write.
	int            m_msgBufCapacity; // size of m_msgBuf (#messages).

	char*          m_txtBuf;
	char*          m_txtBufNext;      //< Next place to write.
	int            m_txtBufCapacity;  //< Size of m_msgBuf (bytes).

	const Message* m_lastLog;
	const Message* m_lastDbg;
	const Message* m_lastErr;
	bool           m_scrollToBottom; 
	double         m_elapsedTimeSinceLastMessage;
}; // class Log

} } // namespace frm::ui

#endif // frm_ui_Log_h
