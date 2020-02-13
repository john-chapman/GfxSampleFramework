#pragma once

#include <frm/core/frm.h>
#include <frm/core/Resource.h>

namespace frm {

///////////////////////////////////////////////////////////////////////////////
// AudioData
// Raw audio data resource.
//
// Data consists of frames of interleaved samples. Use resample() to modify the
// sample rate and/or data type. resample() is called automatically on all
// instances during load if SetDefaultFormat() was called previously (see
// Audio::Init()).
//
// Note that reload() isn't supported due to the lack of thread safety.
//
// \todo Implement downsampling.
///////////////////////////////////////////////////////////////////////////////
class AudioData: public Resource<AudioData>
{
public:
	// Load from _path.
	static AudioData* Create(const char* _path);
	
	// Destroy _inst_.
	static void       Destroy(AudioData*& _inst_);

	// Reload _path.
	static void       FileModified(const char* _path);

	// All subsequently loaded AudioData resources will be resampled to match _frameRateHz and _dataType.
	static void       SetDefaultFormat(int _frameRateHz, DataType _dataType);
	
	bool              load();
	bool              reload()                     { FRM_ASSERT(false); return false; } // can't implement, not thread safe
	
	// Resample to match _sampleRateHz and _dataType.
	void              resample(int _frameRateHz, DataType _dataType);

	const char*       getPath() const              { return (const char*)m_path; }
	int               getFrameRateHz() const       { return m_frameRateHz;       }
	int               getChannelCount() const      { return m_channelCount;      }
	uint              getFrameCount() const        { return m_frameCount;        }
	int               getFrameSizeBytes() const    { return m_frameSizeBytes;    }
	uint              getDataSizeBytes() const     { return m_dataSizeBytes;     }
	DataType          getDataType() const          { return m_dataType;          }
	const char*       getData() const              { return m_data;              }
	const char*       getDataEnd() const           { return m_data + m_dataSizeBytes; }

	double            getDurationSeconds() const;

private:
	AudioData(uint64 _id, const char* _name);
	~AudioData();

	// Perform an interpolated sample at _time.
	double sample(double _time, int _channel) const;

	static int           s_defaultFrameRateHz;
	static DataType s_defaultDataType;

	PathStr  m_path;  // empty if not from a file
	int           m_frameRateHz        = -1;
	int           m_channelCount       = -1;
	uint32        m_frameCount         =  0;
	int           m_frameSizeBytes     =  0;
	uint32        m_dataSizeBytes      =  0;
	DataType m_dataType           = DataType_Invalid;
	char*         m_data               = nullptr;


	// File format implementations.
	static bool ReadWav(AudioData* _audioData, const char* _data, uint _dataSize);
};

} // namespace frm