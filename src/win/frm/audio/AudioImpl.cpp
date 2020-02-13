#include <frm/audio/Audio.h>

#include <frm/core/log.h>
#include <frm/core/win.h>

#include <portaudio/portaudio.h>

#include <EASTL/vector.h>

using namespace frm;

namespace {

static HINSTANCE hPortAudioDll; // the DLL is loaded on demand when calling a Pa_ function, note that FreeLibrary() is never called

#define DLL_NAME   "extern/portaudio_x64"
#define DLL_HANDLE hPortAudioDll
#define DLL_API    __cdecl

template <typename T>
bool DllGetProc(HINSTANCE& _hDll_, const char* _dllName, T& _proc_, const char* _procName)
{
	if_unlikely (!_hDll_) {
		_hDll_ = LoadLibrary(_dllName);
		if (!_hDll_) {
			FRM_LOG_ERR("Failed to load DLL '%s'", _dllName);
			return false;
		}
	}
	if_unlikely (!_proc_) {
		_proc_ = (T)GetProcAddress(_hDll_, _procName);
		if (!_proc_) {
			FRM_LOG_ERR("Failed to load '%s'", _procName);
			return false;
		}
	}

	return true;
}

} // namespace

#define DLL_PROC(_returnType, _procName, ...) \
	typedef _returnType (DLL_API* ProcT)(## __VA_ARGS__); \
	static ProcT proc; \
	if (!DllGetProc(DLL_HANDLE, DLL_NAME, proc, #_procName)) { \
		return (_returnType)0; \
	} 

int Pa_GetVersion(void)
{
	DLL_PROC(int, Pa_GetVersion, void);
	return proc();	
}

const char* Pa_GetVersionText(void)
{
	DLL_PROC(const char*, Pa_GetVersionText, void);
	return proc();
}

const PaVersionInfo* Pa_GetVersionInfo()
{
	DLL_PROC(const PaVersionInfo*, Pa_GetVersionInfo, void);
	return proc();
}

const char* Pa_GetErrorText(PaError errorCode)
{
	DLL_PROC(const char*, Pa_GetErrorText, PaError);
	return proc(errorCode);
}

PaError Pa_Initialize(void)
{
	DLL_PROC(PaError, Pa_Initialize, void);
	return proc();
}

PaError Pa_Terminate(void)
{
	DLL_PROC(PaError, Pa_Terminate, void);
	return proc();
}

PaHostApiIndex Pa_GetHostApiCount(void)
{
	DLL_PROC(PaHostApiIndex, Pa_GetHostApiCount, void);
	return proc();
}

PaHostApiIndex Pa_GetDefaultHostApi(void)
{
	DLL_PROC(PaHostApiIndex, Pa_GetDefaultHostApi, void);
	return proc();
}

const PaHostApiInfo* Pa_GetHostApiInfo(PaHostApiIndex hostApi)
{
	DLL_PROC(const PaHostApiInfo*, Pa_GetHostApiInfo, PaHostApiIndex);
	return proc(hostApi);
}

PaHostApiIndex Pa_HostApiTypeIdToHostApiIndex(PaHostApiTypeId type)
{
	DLL_PROC(PaHostApiIndex, Pa_HostApiTypeIdToHostApiIndex, PaHostApiTypeId);
	return proc(type);
}

PaDeviceIndex Pa_HostApiDeviceIndexToDeviceIndex(PaHostApiIndex hostApi, int hostApiDeviceIndex)
{
	DLL_PROC(PaDeviceIndex, Pa_HostApiDeviceIndexToDeviceIndex, PaHostApiIndex, int);
	return proc(hostApi, hostApiDeviceIndex);
}

const PaHostErrorInfo* Pa_GetLastHostErrorInfo(void)
{
	DLL_PROC(const PaHostErrorInfo*, Pa_GetLastHostErrorInfo, void);
	return proc();
}

PaDeviceIndex Pa_GetDeviceCount(void)
{
	DLL_PROC(PaDeviceIndex, Pa_GetDeviceCount, void);
	return proc();
}

PaDeviceIndex Pa_GetDefaultInputDevice(void)
{
	DLL_PROC(PaDeviceIndex, Pa_GetDefaultInputDevice, void);
	return proc();
}

PaDeviceIndex Pa_GetDefaultOutputDevice(void)
{
	DLL_PROC(PaDeviceIndex, Pa_GetDefaultOutputDevice, void);
	return proc();
}

const PaDeviceInfo* Pa_GetDeviceInfo( PaDeviceIndex device )
{
	DLL_PROC(const PaDeviceInfo*, Pa_GetDeviceInfo, PaDeviceIndex);
	return proc(device);
}

PaError Pa_IsFormatSupported(const PaStreamParameters* inputParameters, const PaStreamParameters* outputParameters, double sampleRate)
{
	DLL_PROC(PaError, Pa_IsFormatSupported, const PaStreamParameters*, const PaStreamParameters*, double);
	return proc(inputParameters, outputParameters, sampleRate);
}

PaError Pa_OpenStream(PaStream** stream, const PaStreamParameters* inputParameters, const PaStreamParameters* outputParameters, double sampleRate, unsigned long framesPerBuffer, PaStreamFlags streamFlags, PaStreamCallback* streamCallback, void* userData)
{
	DLL_PROC(PaError, Pa_OpenStream, PaStream**, const PaStreamParameters*, const PaStreamParameters*, double, unsigned long, PaStreamFlags, PaStreamCallback*, void*);
	return proc(stream, inputParameters, outputParameters, sampleRate, framesPerBuffer, streamFlags, streamCallback, userData);
}

PaError Pa_CloseStream(PaStream* stream)
{
	DLL_PROC(PaError, Pa_CloseStream, PaStream*);
	return proc(stream);
}

PaError Pa_SetStreamFinishedCallback(PaStream* stream, PaStreamFinishedCallback* streamFinishedCallback)
{
	DLL_PROC(PaError, Pa_SetStreamFinishedCallback, PaStream*, PaStreamFinishedCallback*);
	return proc(stream, streamFinishedCallback); 
}

PaError Pa_StartStream(PaStream* stream)
{
	DLL_PROC(PaError, Pa_StartStream, PaStream*);
	return proc(stream);
}

PaError Pa_StoptStream(PaStream* stream)
{
	DLL_PROC(PaError, Pa_StopStream, PaStream*);
	return proc(stream);
}

PaError Pa_AbortStream(PaStream* stream)
{
	DLL_PROC(PaError, Pa_AbortStream, PaStream*);
	return proc(stream);
}

PaError Pa_IsStreamStopped(PaStream* stream)
{
	DLL_PROC(PaError, Pa_IsStreamStopped, PaStream*);
	return proc(stream);
}

PaError Pa_IsStreamActive(PaStream* stream)
{
	DLL_PROC(PaError, Pa_IsStreamActive, PaStream*);
	return proc(stream);
}

const PaStreamInfo* Pa_GetStreamInfo(PaStream* stream)
{
	DLL_PROC(const PaStreamInfo*, Pa_GetStreamInfo, PaStream*);
	return proc(stream);
}

PaTime Pa_GetStreamTime(PaStream* stream)
{
	DLL_PROC(PaTime, Pa_GetStreamTime, PaStream*);
	return proc(stream);
}

double Pa_GetStreamCpuLoad(PaStream* stream)
{
	DLL_PROC(PaTime, Pa_GetStreamCpuLoad, PaStream*);
	return proc(stream);
}

PaError Pa_GetSampleSize(PaSampleFormat format)
{
	DLL_PROC(PaError, Pa_GetSampleSize, PaSampleFormat);
	return proc(format);
}
