#include <frm/Log.h>

#include <frm/Profiler.h>

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
	PathStr              m_output;

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

		PROFILER_MARKER_CPU("#Log::flush");

	 // concatenate message strings, prepended with the type
		static const char* kTypeStr[LogType_Count] =
		{
			"LOG",
			"ERR",
			"DBG",
		};
		apt::String<0> data;
		while (m_flushFrom != m_impl.end()) {
			auto& msg = *m_flushFrom;
			if (msg.m_type != LogType_Count) {
				data.appendf("[%s]  ", kTypeStr[msg.m_type]);
			}
			data.appendf("%s\n", (const char*)msg.m_str);
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

Log::Log(int _bufferSize, const char* _output)
{
	m_buf = new Log::Buffer(_bufferSize, _output);
}

Log::~Log()
{
	flush();
	delete m_buf;
}

void Log::addMessage(const char* _str, Type _type)
{
	Message msg;
	msg.m_str.set(_str);
	msg.m_type = _type;
	msg.m_time = Time::GetApplicationElapsed();
	auto ptr = m_buf->push_back(msg);
	
 // invalidate m_lastMessage ptrs
	for (auto& lastMsg : m_lastMessage) {
		if (ptr == lastMsg) {
			lastMsg = nullptr;
			break;
		}
	}

	if (_type != LogType_Count) {
		m_lastMessage[_type] = ptr;
	}
}

const Log::Message* Log::getLastMessage(Type _type) const
{
	if (_type == LogType_Count) {
		if (m_buf->m_impl.empty()) {
			return nullptr;
		}
		return &m_buf->m_impl.back();
	} else {
		return m_lastMessage[_type];
	}
}

void Log::clearLastMessage(Type _type)
{
	if (_type == LogType_Count) {
		for (auto& msg : m_lastMessage) {
			msg = nullptr;
		}
	} else {
		m_lastMessage[_type] = nullptr;
	}
}

void Log::flush()
{
	m_buf->flush();
}

int Log::getMessageCount() const
{
	return (int)m_buf->m_impl.size();
}
const Log::Message* Log::getMessage(int _i) const
{
	return &m_buf->m_impl[_i];
}

void Log::setOutput(const char* _output)
{
	m_buf->setOutput(_output);
}
