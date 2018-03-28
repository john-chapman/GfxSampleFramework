#include <frm/Log.h>

#include <apt/FileSystem.h>
#include <apt/Time.h>

#include <EASTL/bonus/ring_buffer.h>

using namespace apt;
using namespace frm;

struct Log::Buffer
{
	typedef eastl::ring_buffer<Log::Message> Impl;
	Impl                 m_impl;
	Impl::const_iterator m_flushFrom;
	FileSystem::PathStr  m_output;

	Log::Buffer(int _bufSize, const char* _output)
		: m_impl(_bufSize)
		, m_flushFrom(m_impl.end())
	{
		setOutput(_output);
	}

	void setOutput(const char* _output) 
	{
		m_output = _output;
		if (*_output != '\0') {
		 // clear the log file
			File f;
			FileSystem::Write(f, _output);
		}
	}

	Message* push_back(const Message& _msg)
	{
	 // if we're about to overwrite the msg at m_flushFrom, need to flush
		if (m_impl.end() + 1 == m_flushFrom) {
			flush();
		}
		m_impl.push_back(_msg);
		return &m_impl.back();
	}

	void flush()
	{
		if (m_output.isEmpty()) {
			return;
		}

	 // concatenate message strings, prepended with the type
		static const char* kTypeStr[apt::LogType_Count] =
		{
			"LOG",
			"ERR",
			"DBG",
		};
		String<0> data;
		while (m_flushFrom != m_impl.end()) {
			auto& msg = *m_flushFrom;
			data.appendf("[%s]  %s\n", kTypeStr[msg.m_type], (const char*)msg.m_str);
			++m_flushFrom;
		}

	 // append to output
		File f;
		FileSystem::ReadIfExists(f, (const char*)m_output);
		f.appendData((const char*)data, data.getLength());
		FileSystem::Write(f, (const char*)m_output);
	}
};

// PUBLIC

Log::Log(int _maxMessageCount, const char* _output)
{
	m_buf = new Log::Buffer(_maxMessageCount, _output);
}

Log::~Log()
{
	flush();
	delete m_buf;
}

void Log::setOutput(const char* _output)
{
	m_buf->setOutput(_output);
}

const Log::Message* Log::getLastMessage(Type _type)
{
	return m_lastMessage[_type];
}
	
void Log::addMessage(const char* _str, Type _type)
{
	Message msg;
	msg.m_str.set(_str);
	msg.m_type = _type;
	msg.m_time = Time::GetApplicationElapsed();
	m_lastMessage[_type] = m_buf->push_back(msg);
}

void Log::flush()
{
	m_buf->flush();
}
