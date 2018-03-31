#pragma once

#include <frm/def.h>

#include <apt/log.h>
#include <apt/static_initializer.h>
#include <apt/String.h>
#include <apt/Time.h>

#include <EASTL/bonus/ring_buffer.h>

namespace frm {

class Log
{
public:
	typedef apt::String<64> Str;
	typedef apt::LogType    Type;
	typedef apt::Timestamp  Timestamp;

	struct Message
	{
		Str       m_str;
		Type      m_type;
		Timestamp m_time;
	};

	Log(int _maxMessageCount, const char* _output = "");
	~Log();
	

	void           setOutput(const char* _output);
	const Message* getLastMessage(Type _type) const;
	void           clearLastMessage(Type _type = apt::LogType_Count);
	void           addMessage(const char* _str, Type _type = apt::LogType_Count);
	int            getMessageCount() const;
	const Message* getMessage(int _i) const;
	void           flush();


private:
	Message* m_lastMessage[apt::LogType_Count] = {};

	struct Buffer;
	Buffer* m_buf;
};

} // namespace frm
