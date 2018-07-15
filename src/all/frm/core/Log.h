#pragma once

#include <frm/def.h>

#include <apt/log.h>
#include <apt/String.h>
#include <apt/Time.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// Log
// Message buffer with optional output file. Messages are stored in a ringbuffer
// and flush to the output file on overflow.
////////////////////////////////////////////////////////////////////////////////
class Log
{
public:
	typedef apt::String<64> String;
	typedef apt::LogType    Type;
	typedef apt::Timestamp  Timestamp;

	struct Message
	{
		String    m_str;
		Type      m_type;
		Timestamp m_time;
	};

	// Allocate space for _bufferSize messages, optionally set the output file path.
	Log(int _bufferSize, const char* _outputPath = "");
	~Log();
	
	// Add a message to the buffer.
	void           addMessage(const char* _msg, Type _type = apt::LogType_Count);

	// Return the last message of _type, or the back of the buffer by default.
	const Message* getLastMessage(Type _type = apt::LogType_Count) const;
	// Clear the cached ptr for _type.
	void           clearLastMessage(Type _type = apt::LogType_Count);
	
	// Flush to the output file.
	void           flush();

	// Message access.
	int            getMessageCount() const;
	const Message* getMessage(int _i) const;

	// Set the output file.
	void           setOutput(const char* _path);
	

private:
	Message* m_lastMessage[apt::LogType_Count] = {};

	struct Buffer;
	Buffer* m_buf;
};

} // namespace frm
