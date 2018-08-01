#pragma once

#include "AudioData.h"

#include <frm/core/def.h>

#include <EASTL/vector.h>
#include <EASTL/vector_map.h>

#if !FRM_MODULE_AUDIO
	#error FRM_MODULE_AUDIO was not enabled
#endif

namespace frm {

struct AudioDevice
{
	const char*      m_name;
	int              m_maxInputChannels;
	int              m_maxOutputChannels;
	double           m_inputLatency;
	double           m_outputLatency;
	int              m_defaultFrameRate;
	const void*      m_impl;
};

struct AudioSource
{
	const AudioData* m_audioData   = nullptr;
	const char*      m_position    = nullptr;
	float            m_volume      = 1.0f;
	float            m_pan         = 0.0f; // -1,1 = left,right
	int              m_loopCount   = 1;
};
typedef uint32 AudioSourceId;
constexpr AudioSourceId AudioSourceId_Invalid = ~AudioSourceId(0);


////////////////////////////////////////////////////////////////////////////////
// Audio
////////////////////////////////////////////////////////////////////////////////
class Audio
{
public:

	static void          Init();
	static void          Shutdown();
	static void          Update();

	static AudioSourceId Play(AudioData* _audioData, float _volume = 1.0f, float _pan = 0.0f, int _loopCount = 1);

	static void          SetSourceVolume(AudioSourceId _id, float _pan);
	static void          SetSourcePan(AudioSourceId _id, float _pan);

	static void          Edit();
private:
	static Audio*              s_instance;

	eastl::vector<AudioDevice> m_devices;
	AudioDevice*               m_deviceOut    = nullptr;
	void*                      m_streamOut    = nullptr;

	typedef eastl::vector_map<AudioSourceId, AudioSource> AudioSourceMap;
	AudioSourceId              m_nextSourceId = 0;
	AudioSourceMap             m_sources;

	static int StreamCallbackOut(const void* _input, void* output_, unsigned long _frameCount, void* _timeInfo, unsigned long _statusFlags, void* _user);

	Audio();
	~Audio();
};

} // namespace frm