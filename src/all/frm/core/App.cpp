#include "App.h"

#include <frm/core/frm.h>
#include <frm/core/log.h>
#include <frm/core/platform.h>
#include <frm/core/ArgList.h>
#include <frm/core/Input.h>
#include <frm/core/Profiler.h>
#ifdef FRM_PLATFORM_WIN
	#include <frm/core/win.h> // SetCurrentDirectory
#endif
#if FRM_MODULE_AUDIO
	#include <frm/audio/Audio.h>
#endif

#include <cstring>

using namespace frm;

// PUBLIC

bool App::init(const frm::ArgList& _args)
{
	#if FRM_MODULE_AUDIO
		Audio::Init();
	#endif

	dispatchCallbacks(Event_OnInit);

	return true;
}

void App::shutdown()
{
	dispatchCallbacks(Event_OnShutdown);

	#if FRM_MODULE_AUDIO
		Audio::Shutdown();
	#endif
}

bool App::update()
{
	Profiler::NextFrame();

	PROFILER_MARKER_CPU("#App::update");

	Input::PollAllDevices();
	Timestamp thisUpdate = Time::GetTimestamp();
	m_deltaTime = (thisUpdate - m_prevUpdate).asSeconds() * m_timeScale;
	m_prevUpdate = thisUpdate;
	
	#if FRM_MODULE_AUDIO
		Audio::Update();
	#endif
	
	dispatchCallbacks(Event_OnUpdate);

	return true;
}

void App::registerCallback(Event _event, Callback* _callback, void* _arg)
{
	CallbackList& callbackList = m_callbacks[(int)_event];
	CallbackListEntry callback = { _callback, _arg };
	FRM_ASSERT(eastl::find(callbackList.begin(), callbackList.end(), callback) == callbackList.end()); // double registration
	callbackList.push_back(callback);
}

void App::unregisterCallback(Event _event, Callback* _callback, void* _arg)
{
	CallbackList& callbackList = m_callbacks[(int)_event];
	CallbackListEntry callback = { _callback, _arg };
	auto it = eastl::find(callbackList.begin(), callbackList.end(), callback);
	FRM_ASSERT(it != callbackList.end()); // not found
	callbackList.erase_unsorted(it);
}

// PROTECTED

App::App()
	: m_timeScale(1.0)
	, m_deltaTime(0.0)
{
	m_prevUpdate = Time::GetTimestamp();

	#ifdef FRM_PLATFORM_WIN
	{
	 // force the current working directoy to the exe location
		TCHAR buf[MAX_PATH] = {};
		DWORD buflen;
		FRM_PLATFORM_VERIFY(buflen = GetModuleFileName(0, buf, MAX_PATH));
		char* pathend = strrchr(buf, (int)'\\');
		*(++pathend) = '\0';
		FRM_PLATFORM_VERIFY(SetCurrentDirectory(buf));
		FRM_LOG("Set current directory: '%s'", buf);
	}
	#endif
}

App::~App()
{
}

// PRIVATE

void App::dispatchCallbacks(Event _event)
{
	CallbackList& callbackList = m_callbacks[(int)_event];
	for (CallbackListEntry& callback : callbackList)
	{
		callback();
	}
}

