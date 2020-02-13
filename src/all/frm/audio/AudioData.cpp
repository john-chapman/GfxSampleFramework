#include "AudioData.h"

#include <frm/core/log.h>
#include <frm/core/math.h>
#include <frm/core/memory.h>
#include <frm/core/FileSystem.h>
#include <frm/core/Time.h>

using namespace frm;

#define AUDIO_MALLOC(_size) FRM_MALLOC_ALIGNED(_size, 16) // conservative alignment e.g. for SIMD ops
#define AUDIO_FREE(_ptr)    FRM_FREE_ALIGNED(_ptr)

static inline double Lerp(double _a, double _b, double _x)
{
	return _a + (_b - _a) * _x;
}

static inline double FrameIndexToSeconds(uint _frameIndex, int _frameRateHz)
{
	return (double)_frameIndex / (double)_frameRateHz; 
}

static inline uint SecondsToFrameIndex(double _seconds, int _frameRateHz)
{
	return (uint)Floor(_seconds * (double)_frameRateHz);
}

// PUBLIC

AudioData* AudioData::Create(const char* _path)
{
	Id id = GetHashId(_path);
	AudioData* ret = Find(id);
	if (!ret) {
		ret = FRM_NEW(AudioData(id, _path));
		ret->m_path.set(_path);
	}
	Use(ret);
	return ret;
}

void AudioData::Destroy(AudioData*& _inst_)
{
	FRM_DELETE(_inst_);
}

void AudioData::FileModified(const char* _path)
{
	for (int i = 0, n = GetInstanceCount(); i < n; ++i) {
		auto audioData = GetInstance(i);
		if (audioData->m_path == _path) {
			audioData->reload();
		}
	}
}

void AudioData::resample(int _frameRateHz, DataType _dataType)
{
	if (!m_data) {
		return;
	}
	if (m_frameRateHz == _frameRateHz &&  m_dataType == _dataType) {
		return;
	}

	FRM_AUTOTIMER("Resampling '%s' (%dHz %s -> %dHz %s)", getName(), m_frameRateHz, DataTypeString(m_dataType), _frameRateHz, DataTypeString(_dataType));

	int     frameSizeBytes   = (int)DataTypeSizeBytes(_dataType) * m_channelCount;
	uint32  frameCount       = m_frameCount;
	uint32  sampleCount      = frameCount * m_channelCount;
	char*   newData          = nullptr;
	uint32  newDataSizeBytes = frameSizeBytes * frameCount;

	if (m_frameRateHz == _frameRateHz) {
	 // sample rate is the same, simple data type conversion
		newData = (char*)AUDIO_MALLOC(frameSizeBytes * frameCount);
		DataTypeConvert(m_dataType, _dataType, m_data, newData, sampleCount);
			
	} else {
	 // frame rate is different, resample (+ convert data type implicitly)
		int    srcFrameRateHz   = m_frameRateHz;
		int    dstFrameRateHz   = _frameRateHz;
		double ratio            = (float)dstFrameRateHz / srcFrameRateHz;
		       frameCount       = (uint32)Ceil(ratio * (double)m_frameCount);
		       newDataSizeBytes = frameCount * frameSizeBytes;
		       newData          = (char*)AUDIO_MALLOC(newDataSizeBytes);
		//memset(newData, 0, newDataSizeBytes);

		if (srcFrameRateHz > dstFrameRateHz) {
		 // downsample
			FRM_ASSERT(false);
		} else {
		 // upsample
			for (uint32 dstIndex = 0; dstIndex < frameCount; ++dstIndex) {
				double t = FrameIndexToSeconds(dstIndex, dstFrameRateHz);
				for (int channel = 0; channel < m_channelCount; ++channel) {
					double srcSample = sample(t, channel);
					DataTypeConvert(DataType_Float64, _dataType, &srcSample, newData + dstIndex * frameSizeBytes + channel * DataTypeSizeBytes(_dataType));
				}
			}
		}
	}

	AUDIO_FREE(m_data);
	m_data           = newData;
	m_dataType       = _dataType;
	m_frameCount     = frameCount;
	m_frameSizeBytes = frameSizeBytes;
	m_dataSizeBytes  = newDataSizeBytes;
	m_frameRateHz    = _frameRateHz;

}

void AudioData::SetDefaultFormat(int _frameRateHz, DataType _dataType)
{
	s_defaultFrameRateHz = _frameRateHz;
	s_defaultDataType = _dataType;

	FRM_LOG_DBG("AudioData: Set default format %dHz %s", _frameRateHz, DataTypeString(_dataType));
}

bool AudioData::load()
{
	if (m_path.isEmpty()) {
		return true;
	}

	FRM_AUTOTIMER("AudioData::load(%s)", (const char*)m_path);

	File f;
	if (!FileSystem::Read(f, (const char*)m_path)) {
		return false;
	}
	
	bool ret = false;
	if (FileSystem::CompareExtension("wav", (const char*)m_path)) {
		ret = ReadWav(this, f.getData(), f.getDataSize()); 
	} else {
		FRM_LOG_ERR("AudioData: Unknown extension '%s'", (const char*)m_path);
	}

 // resample if the default framerate and data type were set
	if (s_defaultDataType != DataType_Invalid && s_defaultFrameRateHz > 0) {
		resample(s_defaultFrameRateHz, s_defaultDataType);
	}

	return ret;
}

double AudioData::getDurationSeconds() const 
{
	return (double)m_frameCount / (double)m_frameRateHz; 
}


// PRIVATE

int      AudioData::s_defaultFrameRateHz = -1;
DataType AudioData::s_defaultDataType   = DataType_Invalid;

AudioData::AudioData(uint64 _id, const char* _name)
	: Resource(_id, _name)
{	
}

AudioData::~AudioData()
{
	AUDIO_FREE(m_data);
}

double AudioData::sample(double _time, int _channel) const
{
 // get frame indices for the relevant segment
	uint i = SecondsToFrameIndex(_time, m_frameRateHz);
	uint j = i + 1;
	j = j >= m_frameCount ? i : j; // clamp to frame count to avoid sampling outside the range

 // convert frame indices -> sample indices
	auto sampleSizeBytes = DataTypeSizeBytes(m_dataType);
	i = i * m_frameSizeBytes + _channel * sampleSizeBytes;
	j = j * m_frameSizeBytes + _channel * sampleSizeBytes;
	
 // fetch samples, convert to double
	double a, b;
	DataTypeConvert(m_dataType, DataType_Float64, &m_data[i], &a);
	DataTypeConvert(m_dataType, DataType_Float64, &m_data[j], &b);

	double x = Fract(_time * m_frameRateHz);
	return Lerp(a, b, x);
}

#define DR_WAV_IMPLEMENTATION
#define DR_WAV_NO_STDIO
#define DR_WAV_NO_CONVERSION_API
#include <dr_wav.h>

bool AudioData::ReadWav(AudioData* _audioData, const char* _data, uint _dataSize)
{
	bool ret = true;

	drwav wav;
	ret &= drwav_init_memory(&wav, _data, (size_t)_dataSize) != 0;
	if (!ret) {
		FRM_LOG_ERR("AudioData: drwav_init_memory failed");
		goto AudioData_ReadWav_end;
	}
	if (wav.totalSampleCount >= FRM_DATA_TYPE_MAX(int)) {
		FRM_LOG_ERR("AudioData: wav data too large (%llu bytes)", wav.totalSampleCount);
		goto AudioData_ReadWav_end;
	}

 // read the wav data
	int frameRateHz  = (int)wav.sampleRate;
	int channelCount = (int)wav.channels;
	int frameCount   = (int)wav.totalSampleCount;
	DataType dataType  = DataType_Invalid;
	switch (wav.bitsPerSample) {
		case 8:  dataType = DataType_Uint8N;  break;
		case 16: dataType = DataType_Sint16N; break;
		case 32: dataType = DataType_Float32; break; // \todo DataType_Uint32N in this case?
		default: FRM_LOG_ERR("AudioData: unsupported data type (%d bits per sample)", wav.bitsPerSample); ret = false; goto AudioData_ReadWav_end;
	};
	int frameSizeBytes = (int)DataTypeSizeBytes(dataType) * channelCount;
	FRM_ASSERT(frameSizeBytes == wav.bytesPerSample);
	char* data = (char*)AUDIO_MALLOC(frameSizeBytes * frameCount);	
	ret &= drwav_read(&wav, frameCount, data) == frameCount;
	if (!ret) {
		FRM_LOG_ERR("AudioData: drwav_read failed");
		goto AudioData_ReadWav_end;
	}
	
	if (_audioData->m_data) {
		AUDIO_FREE(_audioData->m_data);
	}
	_audioData->m_data            = data;
	_audioData->m_frameRateHz     = frameRateHz;
	_audioData->m_channelCount    = channelCount;
	_audioData->m_frameCount      = frameCount / channelCount;
	_audioData->m_frameSizeBytes  = frameSizeBytes;
	_audioData->m_dataSizeBytes   = frameSizeBytes * frameCount;
	_audioData->m_dataType        = dataType;
	_audioData->m_data            = data;

AudioData_ReadWav_end:
	if (!ret) {
		AUDIO_FREE(_audioData->m_data);
		_audioData->m_data = nullptr;
		_audioData->m_frameCount = 0;
	}
	
	return ret;
}