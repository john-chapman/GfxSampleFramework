#include "Audio.h"

#include <frm/core/Profiler.h>

#include <apt/log.h>
#include <apt/memory.h>

#include <EASTL/vector.h>
#include <imgui/imgui.h>
#include <portaudio/portaudio.h>

#include <new>

using namespace frm;
using namespace apt;

namespace {

enum Event
{
 // playback control
	Event_Play,
	Event_Stop,

 // source properties
	Event_SetSourceVolume,
	Event_SetSourcePan,
	Event_SetSourceLoopCount,

 // callback -> main thread
	Event_ReleaseAudioData, // the resource system isn't thread safe (and wouldn't be lock-free anyway), hence need to unuse on the main thread via a queue

	Event_Count
};			

static const char* GetEnumStr(Event _event)
{
	#define CASE_ENUM(_e) case _e: return #_e;
	switch (_event) {
		default: return "Unkown event";
		CASE_ENUM(Event_Play);
		CASE_ENUM(Event_Stop);
		CASE_ENUM(Event_SetSourceVolume);
		CASE_ENUM(Event_SetSourcePan);
		CASE_ENUM(Event_SetSourceLoopCount);
		CASE_ENUM(Event_ReleaseAudioData);
	};
	#undef CASE_ENUM
};

// Simple fixed-size event, hold up to a 16 bytes of data (e.g. source ID + vec3).
class AudioEvent: apt::aligned<AudioEvent, 8>
{
	char   m_data[12]    = {};
	uint32 m_type        = (uint32)Event_Count;
	uint32 m_sourceId    = AudioSourceId_Invalid;

public:
	AudioEvent()
	{
	}

	AudioEvent(Event _type, AudioSourceId _sourceId = AudioSourceId_Invalid)
		: m_type(_type)
		, m_sourceId(_sourceId)
	{
	}

	AudioSourceId sourceId() const
	{
		return (AudioSourceId)m_sourceId;
	}

	Event type() const
	{
		return (Event)m_type;
	}	

	template <typename tType>
	const tType& data() const 
	{ 
		APT_STATIC_ASSERT(sizeof(tType)  <= sizeof(m_data));
		APT_STATIC_ASSERT(alignof(tType) <= alignof(AudioEvent));
		return *((tType*)m_data);
	}
	
	template <typename tType>
	tType& data() 
	{ 
		APT_STATIC_ASSERT(sizeof(tType)  <= sizeof(m_data));
		APT_STATIC_ASSERT(alignof(tType) <= alignof(AudioEvent));
		return *((tType*)m_data);
	}
};

} // namespace

#define paCheckError(err) \
	do { \
		if (err != paNoError) { \
			APT_LOG_ERR("PortAudio error: %s\n", Pa_GetErrorText(err)); \
			APT_ASSERT(false); \
		} \
	} while (0)


#include <atomic>

// Lock-free SPSC ring buffer.
// Note that this relies on being able to know the current size to detect if reads/writes will underflow/overflow.
// See https://www.snellman.net/blog/archive/2016-12-13-ring-buffers/ for a discussion.
// \todo In some cases it may be desirable to read/write the buffer directly to avoid copying. This requires a more
//   low-level API to update the atomics when operations on the buffer are done.
template <typename tType, uint kAlignment>
class LockFreeRingBuffer_SPSC: private apt::non_copyable<LockFreeRingBuffer_SPSC<tType, kAlignment> >
{
	uint32               m_capacity = 0;
	tType*               m_data     = nullptr;
	std::atomic<uint32>  m_readAt;
	std::atomic<uint32>  m_writeAt;

	uint32 size(uint32 _ri, uint32 _wi) const
	{
		return _wi - _ri;
	}

public:
	LockFreeRingBuffer_SPSC(uint32 _capacity)
		: m_capacity(_capacity)
	{
		APT_ASSERT(APT_IS_POW2(_capacity));
		APT_ASSERT(_capacity < APT_DATA_TYPE_MAX(uint32) / 2);
		APT_ASSERT(std::atomic<size_t>().is_lock_free());
		std::atomic_init(&m_readAt,  0);
		std::atomic_init(&m_writeAt, 0);
		m_data = (tType*)APT_MALLOC_ALIGNED(sizeof(tType) * _capacity, kAlignment);
	}

	~LockFreeRingBuffer_SPSC()
	{
		APT_FREE_ALIGNED(m_data);
	}

	// Write up to _count elements from _src into the buffer. Return the actual number of elements written. If the return value is < _count, the buffer overflowed.
	uint32 write(const tType* _src, uint32 _count)
	{
		auto writeAt = m_writeAt.load();
		auto readAt  = m_readAt.load();
		_count = APT_MIN(_count, m_capacity - size(readAt, writeAt));

		auto wi = APT_MOD_POW2(writeAt, m_capacity);
		if_likely (wi + _count <= m_capacity) { // assume this is likely if we always write blocks which are integer factors of m_capacity
		 // no wrap, 1 memcpy
			memcpy(m_data + wi, _src, sizeof(tType) * _count);

		} else {
		 // wrap, 2 memcpy
			auto canWrite = m_capacity - wi;
			auto overflow = _count - canWrite;
			memcpy(m_data + wi, _src, sizeof(tType) * canWrite);
			memcpy(m_data, _src + canWrite, sizeof(tType) * overflow);
		}

		writeAt += _count;
		m_writeAt.store(writeAt);

		return _count;
	}

	// Read up to _count elements from the buffer into dst_. Return the actual number of elements read. If the return value is < _count, the buffer underflowed.
	uint32 read(tType* dst_, uint32 _count)
	{
		auto writeAt = m_writeAt.load();
		auto readAt  = m_readAt.load();
		_count = APT_MIN(_count,	size(readAt, writeAt));

		auto ri = APT_MOD_POW2(readAt, m_capacity);
		if_likely (ri + _count <= m_capacity) {
		 // no wrap, 1 memcpy
			memcpy(dst_, m_data + ri, sizeof(tType) * _count);

		} else {
		 // wrap, 2 memcpy
			auto canRead  = m_capacity - ri;
			auto undeflow = _count - canRead;
			memcpy(dst_, m_data + ri, sizeof(tType) * canRead);
			memcpy(dst_ + canRead, m_data, sizeof(tType) * undeflow);
		}

		readAt += _count;
		m_readAt.store(readAt);

		return _count;
	}

	uint32 size() const
	{
		auto ri = m_readAt.load();
		auto wi = m_writeAt.load();
		return size(ri, wi);
	} 
};

/*******************************************************************************

                                    Audio

*******************************************************************************/

typedef LockFreeRingBuffer_SPSC<AudioEvent, alignof(AudioEvent)> AudioEventQueue; 
static apt::storage<AudioEventQueue> s_callbackEventQueue;
static apt::storage<AudioEventQueue> s_mainThreadEventQueue;

static void Mix(AudioSource* _inst_, float* _output_, int _frameCount)
{
	const AudioData* audioData      = _inst_->m_audioData;
	const int        frameSizeBytes = audioData->getFrameSizeBytes();
	const int        channelCount   = audioData->getChannelCount();
	const char*      audioDataEnd   = audioData->getDataEnd();
	const float      volumeLeft     = APT_SATURATE(_inst_->m_volume * APT_SATURATE(1.0f - _inst_->m_pan));
	const float      volumeRight    = APT_SATURATE(_inst_->m_volume * APT_SATURATE(1.0f + _inst_->m_pan));
	
	if (audioData->getDataType() == apt::DataType_Float) {
		while (_frameCount > 0 && _inst_->m_loopCount > 0) {
			int framesToCopy = APT_MIN(_frameCount, (int)(audioDataEnd - _inst_->m_position) / frameSizeBytes);
			_frameCount -= framesToCopy;
			const float* beg = (float*)_inst_->m_position;
			const float* end = (float*)beg + framesToCopy;
			if (channelCount == 1) {
				while (beg != end) {
					*(_output_++) += beg[0] * volumeLeft;
					*(_output_++) += beg[0] * volumeRight;
					++beg;
				}
			} else if (channelCount == 2) {
				APT_ASSERT(false);
			} else {
				APT_ASSERT(false);
			}

			_inst_->m_position += frameSizeBytes * framesToCopy;
			if (_inst_->m_position >= audioDataEnd) {
				_inst_->m_position = audioData->getData();
				--_inst_->m_loopCount;
			}
		}
	} else {
	}
}

// PUBLIC

void Audio::Init()
{
	new(s_callbackEventQueue)   AudioEventQueue(256);
	new(s_mainThreadEventQueue) AudioEventQueue(256);
	
	s_instance = APT_NEW(Audio);	
}

void Audio::Shutdown()
{
	s_callbackEventQueue->~AudioEventQueue();
	s_mainThreadEventQueue->~AudioEventQueue();

	APT_DELETE(s_instance);
}

void Audio::Update()
{
	PROFILER_MARKER_CPU("#Audio::Update");

	AudioEvent eventQueue[256];
	auto eventCount = s_mainThreadEventQueue->read(eventQueue, APT_ARRAY_COUNT(eventQueue));
	for (uint32 i = 0; i < eventCount; ++i) {
		auto& e = eventQueue[i];
		switch (e.type()) {
			default:
				APT_ASSERT_MSG(false, "Audio: Invalid event type in main queue: %s", GetEnumStr(e.type()));
				break;
			case Event_ReleaseAudioData: {
				AudioData* audioData = e.data<AudioData*>();
				AudioData::Release(audioData);
				break;
			}
		};
	}
}

AudioSourceId Audio::Play(AudioData* _audioData, float _volume, float _pan, int _loopCount)
{
	APT_ASSERT(_audioData->getChannelCount() <= 2); // only support mono or stereo

	AudioData::Use(_audioData);

	auto id = s_instance->m_nextSourceId++;
	if_unlikely (id == AudioSourceId_Invalid) {
		id = s_instance->m_nextSourceId++;
	}

	AudioEvent events[4];
	events[0] = AudioEvent(Event_Play, id);
	events[0].data<AudioData*>() = _audioData;
	events[1] = AudioEvent(Event_SetSourceVolume, id);
	events[1].data<float>() = _volume;
	events[2] = AudioEvent(Event_SetSourcePan, id);
	events[2].data<float>() = _pan;
	events[3] = AudioEvent(Event_SetSourceLoopCount, id);
	events[3].data<int>() = _loopCount;
	APT_VERIFY(s_callbackEventQueue->write(events, APT_ARRAY_COUNT(events)) == APT_ARRAY_COUNT(events));

	return id;
}


void Audio::SetSourceVolume(AudioSourceId _id, float _volume)
{
	APT_STRICT_ASSERT(_id != AudioSourceId_Invalid);
	AudioEvent e(Event_SetSourceVolume, _id);
	e.data<float>() = _volume;
	APT_VERIFY(s_callbackEventQueue->write(&e, 1) == 1);
}

void Audio::SetSourcePan(AudioSourceId _id, float _pan)
{
	APT_STRICT_ASSERT(_id != AudioSourceId_Invalid);
	AudioEvent e(Event_SetSourcePan, _id);
	e.data<float>() = _pan;
	APT_VERIFY(s_callbackEventQueue->write(&e, 1) == 1);
}

void Audio::Edit()
{
	auto SelectDevice = [&](AudioDevice* _current, int _minInputChannels, int _minOutputChannels) -> AudioDevice*
		{
			auto ret = _current;
			for (auto& device : s_instance->m_devices) {
				if (device.m_maxInputChannels < _minInputChannels) {
					continue;
				}
				if (device.m_maxOutputChannels < _minOutputChannels) {
					continue;
				}
				bool isSelected = _current == &device;
				if (ImGui::Selectable(device.m_name, isSelected)) {
					ret = &device;
				}
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			return ret;
		};

	
	if (ImGui::BeginCombo("Output", s_instance->m_deviceOut->m_name)) {
		s_instance->m_deviceOut = SelectDevice(s_instance->m_deviceOut, 0, 1); 
		ImGui::EndCombo();
	}

	int playbackInstanceCount = (int)s_instance->m_sources.size();
	ImGui::Text("Playback Instances: %d", playbackInstanceCount);
	ImGui::Text("Cpu Load: %f", (float)Pa_GetStreamCpuLoad(s_instance->m_streamOut));
	
	/*ImGui::Spacing();
	ImGui::Text(s_currentDevice->m_name);
	ImGui::Spacing();
	ImGui::Text("   Default sample rate: %d", s_currentDevice->m_defaultFrameRate);
	if (s_currentDevice->m_maxOutputChannels > 0) {
		ImGui::Text("   Max output channels: %d", s_currentDevice->m_maxOutputChannels);
		ImGui::Text("   Output latency:      %f", (float)s_currentDevice->m_outputLatency);
	}
	if (s_currentDevice->m_maxInputChannels > 0) {
		ImGui::Text("   Max input channels:  %d", s_currentDevice->m_maxInputChannels);
		ImGui::Text("   Input latency:       %f", (float)s_currentDevice->m_inputLatency);
	}*/
}

// PRIVATE

Audio* Audio::s_instance;

Audio::Audio()
{
	paCheckError(Pa_Initialize());
	APT_LOG(Pa_GetVersionText());

	int deviceCount = Pa_GetDeviceCount();
	for (int i = 0; i < deviceCount; ++i) {
		const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
		AudioDevice device;
		device.m_name              = deviceInfo->name;
		device.m_maxInputChannels  = (int)deviceInfo->maxInputChannels;
		device.m_maxOutputChannels = (int)deviceInfo->maxOutputChannels;
		device.m_inputLatency      = deviceInfo->defaultLowInputLatency;
		device.m_outputLatency     = deviceInfo->defaultLowOutputLatency;
		device.m_defaultFrameRate  = (int)deviceInfo->defaultSampleRate;
		device.m_impl              = deviceInfo;

		m_devices.push_back(device);
	}

	m_deviceOut = &m_devices[Pa_GetDefaultOutputDevice()];

	PaStreamParameters paramsOut = {};
	paramsOut.channelCount = 2;
	paramsOut.device = Pa_GetDefaultOutputDevice();
	paramsOut.sampleFormat = paFloat32;
	paramsOut.suggestedLatency = m_deviceOut->m_outputLatency;
	paCheckError(Pa_OpenStream(&m_streamOut, nullptr, &paramsOut, m_deviceOut->m_defaultFrameRate, 256, paNoFlag, (PaStreamCallback*)&StreamCallbackOut, this));
	paCheckError(Pa_StartStream(m_streamOut));

	AudioData::SetDefaultFormat((int)m_deviceOut->m_defaultFrameRate, DataType_Float32);
}

Audio::~Audio()
{
	paCheckError(Pa_AbortStream(m_streamOut));
	paCheckError(Pa_Terminate());

	m_deviceOut = nullptr;
	m_devices.clear();
}

int Audio::StreamCallbackOut(const void* _input, void* output_, unsigned long _frameCount, void* _timeInfo, unsigned long _statusFlags, void* _user)
{
	Audio* ctx = (Audio*)_user;
	auto& sourceList = ctx->m_sources;

 // audio events
	AudioEvent eventQueue[256];
	auto eventCount = s_callbackEventQueue->read(eventQueue, APT_ARRAY_COUNT(eventQueue));
	for (uint32 i = 0; i < eventCount; ++i) {
		auto& e = eventQueue[i];
		switch (e.type()) {
			default:
				APT_ASSERT_MSG(false, "Audio: Invalid event type in callback queue: %s", GetEnumStr(e.type()));
				break;

		 // playback control
			case Event_Play: {
				APT_ASSERT(e.data<AudioData*>() != nullptr);
				APT_ASSERT(e.sourceId() != AudioSourceId_Invalid);
				auto& source = sourceList[e.sourceId()];
				source.m_audioData = e.data<AudioData*>();
				source.m_position = source.m_audioData->getData();
				break;
			}
			case Event_Stop: {
				APT_ASSERT(e.sourceId() != AudioSourceId_Invalid);
				auto it = sourceList.find(e.sourceId());
				if (it != sourceList.end()) {
					it->second.m_loopCount = 0;
				}
				break;
			}

		 // source properties
			case Event_SetSourceVolume: {
				APT_ASSERT(e.sourceId() != AudioSourceId_Invalid);
				auto it = sourceList.find(e.sourceId());
				if (it != sourceList.end()) {
					it->second.m_volume = e.data<float>();
				}
				break;
			}
			case Event_SetSourcePan: {
				APT_ASSERT(e.sourceId() != AudioSourceId_Invalid);
				auto it = sourceList.find(e.sourceId());
				if (it != sourceList.end()) {
					it->second.m_pan = e.data<float>();
				}
				break;
			}
			case Event_SetSourceLoopCount: {
				APT_ASSERT(e.sourceId() != AudioSourceId_Invalid);
				auto it = sourceList.find(e.sourceId());
				if (it != sourceList.end()) {
					it->second.m_loopCount = e.data<int>();
				}
				break;
			}

		};
	}

 // mix sources
 // \todo static members here don't support multiple streams, need to make those external
	constexpr unsigned long kLocalBufferSize = 1024;
	APT_ASSERT((_frameCount * 2) < kLocalBufferSize);
	static float s_mixBuffer[kLocalBufferSize];
	static eastl::vector<AudioSourceId> s_sourceDeleteList;

	memset(s_mixBuffer, 0, sizeof(float) * kLocalBufferSize);
	for (auto& it : sourceList) {
		auto& source = it.second;
		Mix(&source, s_mixBuffer, (int)_frameCount);
		if (source.m_loopCount <= 0) {
			s_sourceDeleteList.push_back(it.first); // \todo push the iterator directly, avoid call to find() below
		}
	}
	memcpy(output_, s_mixBuffer, _frameCount * sizeof(float) * 2);

 // clear dead sources
	for (auto id : s_sourceDeleteList) {
		auto it = sourceList.find(id);
		APT_ASSERT(it != sourceList.end());

	 // \todo write the release events in a single step?
		AudioEvent e(Event_ReleaseAudioData);
		e.data<const AudioData*>() = it->second.m_audioData;
		s_mainThreadEventQueue->write(&e, 1);		

		sourceList.erase(it);
	}
	s_sourceDeleteList.clear();

	return paContinue;
}

