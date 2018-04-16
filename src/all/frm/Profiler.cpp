#include <frm/Profiler.h>

#include <frm/gl.h>
#include <frm/GlContext.h>

#include <apt/math.h>
#include <apt/memory.h>
#include <apt/String.h>
#include <apt/StringHash.h>
#include <apt/Time.h>

#include <imgui/imgui.h>
#include <imgui/imgui_ext.h>
#include <EASTL/vector.h>
#include <EASTL/vector_map.h>

using namespace frm;
using namespace apt;

/******************************************************************************

                                Profiler

******************************************************************************/

#define Profiler_ALWAYS_GEN_QUERIES 0 // using a query pool seems to use a lot more memory (on Nvidia)
#define Profiler_DEBUG 0
#if Profiler_DEBUG
	#define Profiler_STRICT_ASSERT(e) APT_ASSERT(e)
#else
	#define Profiler_STRICT_ASSERT(e) APT_STRICT_ASSERT(e)
#endif

// \todo make these configurable
static const int kFrameCount                 = 16;  // must be at least 2 (can't visualize the current write frame)
static const int kMaxTotalMarkersPerFrame    = 1024;
static const int kValueHistoryCount          = 512;

namespace {

// Basic ring buffer, capacity must be a power of 2.
// We assume that the ring buffer is always full, hence only push_back() is supported and front() is 
// User at_relative() to access elements relative to front(), at_absolute() to access the internal container.
template <typename tType>
class RingBuffer
{
	uint32 m_capacity = 0;
	tType* m_data     = nullptr;
	tType* m_back     = nullptr;

public:
	RingBuffer(uint32 _capacity)
		: m_capacity(_capacity)
	{
		APT_ASSERT(APT_IS_POW2(_capacity));
		m_data = APT_NEW_ARRAY(tType, _capacity);
		m_back = m_data - 1; // first call to push_back() must write at element 0
	}

	RingBuffer(uint32 _capacity, const tType& _assign)
		: m_capacity(_capacity)
	{
		APT_ASSERT(APT_IS_POW2(_capacity));
		m_data = APT_NEW_ARRAY(tType, _capacity);
		assign(_assign);
		m_back = m_data - 1; // first call to push_back() must write at element 0
	}

	~RingBuffer()
	{
		APT_DELETE_ARRAY(m_data);
	}

	RingBuffer(RingBuffer<tType>&) = delete;
	RingBuffer<tType>& operator=(RingBuffer<tType>&) = delete;

	RingBuffer(RingBuffer&& _rhs_)
	{
		eastl::swap(m_capacity, _rhs_.m_capacity);
		eastl::swap(m_data,     _rhs_.m_data);
		eastl::swap(m_back,     _rhs_.m_back);
	}

	RingBuffer<tType>& operator=(RingBuffer<tType>&& _rhs_)
	{
		eastl::swap(m_capacity, _rhs_.m_capacity);
		eastl::swap(m_data,     _rhs_.m_data);
		eastl::swap(m_back,     _rhs_.m_back); 
		return *this;
	}

	void assign(const tType& _val)
	{
		for (uint32 i = 0; i < m_capacity; ++i) {
			m_data[i] = _val;
		}
		m_back = m_data;
	}

	bool empty() const
	{
		return m_back == m_data - 1;
	}

	uint32 capacity() const
	{
		return m_capacity;
	}

	tType* data()
	{
		return m_data;
	}

	tType& front()
	{
		auto ret = m_back + 1;
		if_unlikely (ret == m_data + m_capacity) {
			ret = m_data;
		}
		return *ret;
	}

	tType& back()
	{
		APT_STRICT_ASSERT(!empty()); // m_back is invalid in this case
		return *m_back;
	}

	void push_back(const tType& _val)
	{
		++m_back;
		if_unlikely (m_back == m_data + m_capacity) {
			m_back = m_data;
		}
		*m_back = _val;
	}

	tType& at_relative(uint32 _i)
	{
		auto fr = &front();
		uint32 off = (uint32)(fr - m_data);
		return m_data[APT_MOD_POW2(_i + off, m_capacity)];
	}

	tType& at_absolute(uint32 _i)
	{
		APT_STRICT_ASSERT(_i < m_capacity);
		return m_data[_i];
	}

	bool is_element(const tType* _ptr)
	{
		return _ptr >= m_data && _ptr < (m_data + m_capacity);
	}
};

// Common code for CPU,GPU
struct ProfilerData
{

// Frames, markers
	RingBuffer<Profiler::Frame>*      m_frames;
	RingBuffer<Profiler::Marker>*     m_markers;
	eastl::vector<Profiler::Marker*>  m_markerStack;
	eastl::vector<StringHash>         m_trackedMarkers;
	uint64                            m_avgFrameDuration;

 // Values
	struct ValueData
	{
		Profiler::Value   m_value;
		uint32            m_count; // samples within a single frame
		RingBuffer<float> m_history = RingBuffer<float>(kValueHistoryCount, 0.0f);
	};
	eastl::vector_map<StringHash, ValueData> m_values;
	eastl::vector<StringHash>                m_pinnedValues;

	
	ProfilerData(int _frameCount, int _maxTotalMarkersPerFrame)
	{
		m_frames  = APT_NEW(RingBuffer<Profiler::Frame>(_frameCount, Profiler::Frame()));
		m_markers = APT_NEW(RingBuffer<Profiler::Marker>(_frameCount * _maxTotalMarkersPerFrame, Profiler::Marker()));

		m_markerStack.reserve(8);
	}

	~ProfilerData()
	{
	 // some static systems (e.g. Log) may push markers during shutdown
		//APT_DELETE(m_frames);
		//APT_DELETE(m_markers);
	}

	uint32 getFrameIndex(const Profiler::Frame* _frame)
	{
		Profiler_STRICT_ASSERT(!_frame || m_frames->is_element(_frame));
		return (uint32)(_frame - m_frames->data());
	}

	uint32 getMarkerIndex(const Profiler::Marker* _marker)
	{
		Profiler_STRICT_ASSERT(!_marker || m_markers->is_element(_marker));
		return (uint32)(_marker - m_markers->data());
	}

	Profiler::Marker& pushMarker(const char* _name)
	{
	 // \todo It would be nice to check if we pushed too many markers in a single frame, however we don't explicitly track the 
	 // count and checking for overlap in the ring buffer is complicated; the test below always fails for the first marker pushed
	 // during a frame because we set frame.m_markerBegin = m_markers->front() during nextFrame().
		//APT_ASSERT(&m_markers->front() != m_frames->back().m_markerBegin); // too many markers pushed this frame

		APT_ASSERT(m_markerStack.size() < APT_DATA_TYPE_MAX(decltype(Profiler::Marker::m_stackDepth)));
		Profiler::Marker newMarker;
		newMarker.m_name = _name;
		newMarker.m_stackDepth = (decltype(Profiler::Marker::m_stackDepth))m_markerStack.size();
		m_markers->push_back(newMarker);
		m_markerStack.push_back(&m_markers->back());
		return m_markers->back();
	}

	Profiler::Marker& popMarker(const char* _name)
	{
		auto ret = m_markerStack.back();
		m_markerStack.pop_back();
		APT_ASSERT_MSG(strcmp(ret->m_name, _name) == 0, "Unmatched marker push/pop '%s'/'%s'", ret->m_name, _name);
		return *ret;
	}

	auto findTrackedMarker(StringHash _nameHash)
	{
		return eastl::find(m_trackedMarkers.begin(), m_trackedMarkers.end(), _nameHash);
	}

	void trackMarker(StringHash _nameHash)
	{
		if (findTrackedMarker(_nameHash) == m_trackedMarkers.end()) {
			m_trackedMarkers.push_back(_nameHash);
		}

	 // pin tracked markers by default
		if (eastl::find(m_pinnedValues.begin(), m_pinnedValues.end(), _nameHash) == m_pinnedValues.end()) {
			m_pinnedValues.push_back(_nameHash);
		}
	}

	void untrackMarker(StringHash _nameHash)
	{
		m_trackedMarkers.erase(findTrackedMarker(_nameHash));

		auto valueData = m_values.find(_nameHash);
		if (valueData != m_values.end()) { // can happen if you track/untrack while paused
			m_values.erase(valueData);
		}
	}

	void value(const char* _name, float _value, const char* _format)
	{
		StringHash nameHash(_name);
		auto& data    = m_values[nameHash];
		auto& val     = data.m_value;
		auto& hist    = data.m_history;

		val.m_name    = _name;
		val.m_format  = _format;
		//val.m_max     = APT_MAX(val.m_max, _value);
		//val.m_min     = APT_MIN(val.m_min, _value);
		//val.m_avg     = (val.m_avg - val.m_avg / 1000.0f) + _value / 1000.0f; // approx moving average
		
		++data.m_count;
		hist.back()  += _value;
	}

	bool isValuePinned(StringHash _nameHash)
	{
		return eastl::find(m_pinnedValues.begin(), m_pinnedValues.end(), _nameHash) != m_pinnedValues.end();
	}

	void endFrame()
	{
		APT_ASSERT_MSG(m_markerStack.empty(), "Marker '%s' was not popped before frame end", m_markerStack.back()->m_name);

	 // average frame duration
		uint64 avg  = 0;
		for (uint32 i = 1; i < m_frames->capacity(); ++i) {
			auto& thisFrame = m_frames->at_relative(i);
			auto& prevFrame = m_frames->at_relative(i - 1);
			if_unlikely (thisFrame.m_id == 0 || prevFrame.m_id == 0) {
				break;
			}
			avg += thisFrame.m_startTime - prevFrame.m_startTime;
		}
		m_avgFrameDuration = avg / m_frames->capacity();

	 // reset values
		for (auto& it : m_values) {
			auto& data       = it.second;
			auto& value      = data.m_value;
			auto& history    = data.m_history;

			if (data.m_count == 0) { // no values were pushed (usually if the profiler was paused
				continue;
			}

			history.back()  /= (float)data.m_count;
			value.m_min      = FLT_MAX;
			value.m_max      = -FLT_MAX;
			value.m_avg      = 0.0f;
			for (uint32 i = 0; i < history.capacity(); ++i) {
				float v      = history.at_relative(i);
				value.m_min  = APT_MIN(value.m_min, v);
				value.m_max  = APT_MAX(value.m_max, v);
				value.m_avg += v;
			}
			value.m_avg /= (float)history.capacity();
			data.m_count = 0;
			data.m_history.push_back(0.0f);
		}

		if_likely (!m_frames->empty()) { // m_frames->back() is outside the buffer in this case
			m_frames->back().m_markerEnd = getMarkerIndex(&m_markers->front());
		}
	}

	Profiler::Frame& beginFrame()
	{
		Profiler::Frame nextFrame;
		nextFrame.m_id          = Profiler::GetFrameIndex();
		nextFrame.m_markerBegin = getMarkerIndex(&m_markers->front());
		nextFrame.m_startTime   = (uint64)Time::GetTimestamp().getRaw();
		m_frames->push_back(nextFrame);
		return m_frames->back();
	}

	void trackMarkers(Profiler::Frame& _frame)
	{
		if_unlikely (m_frames->empty() || _frame.m_id == 0) { // uninitialized frame
			return;
		}
		APT_STRICT_ASSERT(m_frames->is_element(&_frame));
		auto i = _frame.m_markerBegin;
		while (i != _frame.m_markerEnd) {
			auto& marker = m_markers->at_absolute(i);
			if (eastl::find(m_trackedMarkers.begin(), m_trackedMarkers.end(), StringHash(marker.m_name)) != m_trackedMarkers.end()) {
				value(marker.m_name, (float)Timestamp(marker.m_stopTime - marker.m_startTime).asMilliseconds(), Profiler::kFormatTimeMs);
			}
			i = APT_MOD_POW2(i + 1, m_markers->capacity());
		}
	}
};


ProfilerData  g_CpuData               = ProfilerData(kFrameCount, kMaxTotalMarkersPerFrame);
ProfilerData  g_GpuData               = ProfilerData(kFrameCount, kMaxTotalMarkersPerFrame);
uint64        g_GpuTimeOffset         = 0; // convert GPU -> CPU time; this value can be arbitrarily large as the clocks aren't necessarily relative to the same moment
auto          g_GpuFrameStartQueries  = eastl::vector<GLuint>(kFrameCount, (GLuint)0);
auto          g_GpuMarkerStartQueries = eastl::vector<GLuint>(kFrameCount * kMaxTotalMarkersPerFrame, (GLuint)0);
auto          g_GpuMarkerStopQueries  = eastl::vector<GLuint>(kFrameCount * kMaxTotalMarkersPerFrame, (GLuint)0);
uint32        g_GpuFrameGetBegin      = 0; // see NextFrame()
uint32        g_GpuMarkerGetBegin     = 0; //      "

uint64 GpuToSystemTicks(GLuint64 _gpuTime)
{
	return (uint64)_gpuTime * Time::GetSystemFrequency() / 1000000000ull; // nanoseconds -> system ticks
}
uint64 GpuToTimestamp(GLuint64 _gpuTime)
{
	return GpuToSystemTicks(_gpuTime) + g_GpuTimeOffset;
}

void SyncGpu()
{
	GLint64 gpuTime;
	glAssert(glGetInteger64v(GL_TIMESTAMP, &gpuTime));
	uint64 cpuTicks = Time::GetTimestamp().getRaw();
	uint64 gpuTicks = GpuToSystemTicks(gpuTime);
	APT_ASSERT(cpuTicks > gpuTicks);
	g_GpuTimeOffset = cpuTicks - gpuTicks; 
}

} // namespace


// PUBLIC

const char* Profiler::kFormatTimeMs;

void Profiler::NextFrame()
{
 // allocating GPU queries requires a GL context, do this once during the first call to NextFrame
	APT_ONCE {
		APT_ASSERT(GlContext::GetCurrent());
		
		#if !Profiler_ALWAYS_GEN_QUERIES
			glAssert(glGenQueries((GLsizei)g_GpuFrameStartQueries.capacity(),  g_GpuFrameStartQueries.data()));
			glAssert(glGenQueries((GLsizei)g_GpuMarkerStartQueries.capacity(), g_GpuMarkerStartQueries.data()));
			glAssert(glGenQueries((GLsizei)g_GpuMarkerStopQueries.capacity(),  g_GpuMarkerStopQueries.data()));
		#endif
	}

	SyncGpu(); // \todo timestamp query is slow?
	
 // retrieve available frame start queries, starting from the last unavailable frame
 // also find the limits of the marker query retrieval
	auto gpuMarkerGetEnd = g_GpuMarkerGetBegin;
	while (g_GpuFrameGetBegin != g_GpuData.getFrameIndex(&g_GpuData.m_frames->front())) {
		auto& frame  = g_GpuData.m_frames->at_absolute(g_GpuFrameGetBegin);
		auto& query  = g_GpuFrameStartQueries[g_GpuData.getFrameIndex(&frame)];
		GLint available = GL_FALSE;
		glAssert(glGetQueryObjectiv(query, GL_QUERY_RESULT_AVAILABLE, &available));
		if (!available) {
			break;
		}
		GLuint64 gpuTime;
		glAssert(glGetQueryObjectui64v(query, GL_QUERY_RESULT, &gpuTime));
		#if Profiler_ALWAYS_GEN_QUERIES
			glAssert(glDeleteQueries(1, &query));
		#endif
		frame.m_startTime = GpuToTimestamp(gpuTime);
		gpuMarkerGetEnd = frame.m_markerBegin; // markers *up to* the last available frame start are implicitly available
	
		g_GpuFrameGetBegin = APT_MOD_POW2(g_GpuFrameGetBegin + 1, g_GpuData.m_frames->capacity());
	}

 // retrieve available marker start/stop queries
	while (g_GpuMarkerGetBegin != gpuMarkerGetEnd) {
		auto& marker     = g_GpuData.m_markers->at_absolute(g_GpuMarkerGetBegin);
		auto  queryIndex = g_GpuData.getMarkerIndex(&marker);
		auto& queryStart = g_GpuMarkerStartQueries[queryIndex];
		auto& queryStop  = g_GpuMarkerStopQueries[queryIndex];

		#if Profiler_DEBUG
			GLint available = GL_FALSE;
			glAssert(glGetQueryObjectiv(queryStart, GL_QUERY_RESULT_AVAILABLE, &available));
			Profiler_STRICT_ASSERT(available);
			glAssert(glGetQueryObjectiv(queryStop, GL_QUERY_RESULT_AVAILABLE, &available));
			Profiler_STRICT_ASSERT(available);
		#endif
		GLuint64 gpuStartTime, gpuStopTime;
		glAssert(glGetQueryObjectui64v(queryStart, GL_QUERY_RESULT, &gpuStartTime));
		#if Profiler_ALWAYS_GEN_QUERIES
			glAssert(glDeleteQueries(1, &queryStart));
		#endif
		marker.m_startTime = GpuToTimestamp(gpuStartTime);
		glAssert(glGetQueryObjectui64v(queryStop, GL_QUERY_RESULT, &gpuStopTime));
		#if Profiler_ALWAYS_GEN_QUERIES
			glAssert(glDeleteQueries(1, &queryStop));
		#endif
		marker.m_stopTime = GpuToTimestamp(gpuStopTime);

		g_GpuMarkerGetBegin = APT_MOD_POW2(g_GpuMarkerGetBegin + 1, g_GpuData.m_markers->capacity());
	}

 // increment the frame index first so that new frame data will have the correct index
	++s_frameIndex;

	if (s_pause && s_setPause) {
		return;
	}

	g_CpuData.endFrame();
	g_CpuData.trackMarkers(g_CpuData.m_frames->back());
	g_CpuData.beginFrame();

	g_GpuData.endFrame();
	auto gpuAvailFrameIndex = APT_MOD_POW2(g_GpuMarkerGetBegin + g_GpuData.m_frames->capacity() - 2, g_GpuData.m_frames->capacity()); // markers are available 2 frames behind g_GpuMarkerGetBegin
	g_GpuData.trackMarkers(g_GpuData.m_frames->at_absolute(gpuAvailFrameIndex));
	auto& frame = g_GpuData.beginFrame();
	frame.m_startTime = 0;
	#if Profiler_ALWAYS_GEN_QUERIES
		glAssert(glGenQueries(1, &g_GpuFrameStartQueries[g_GpuData.getFrameIndex(&frame)]));
	#endif
	glAssert(glQueryCounter(g_GpuFrameStartQueries[g_GpuData.getFrameIndex(&frame)], GL_TIMESTAMP));

	CpuValue("#CPU", (float)Timestamp(g_CpuData.m_avgFrameDuration).asMilliseconds(), kFormatTimeMs);
	GpuValue("#GPU", (float)Timestamp(g_GpuData.m_avgFrameDuration).asMilliseconds(), kFormatTimeMs);

	s_pause = s_setPause;
}

void Profiler::PushCpuMarker(const char* _name)
{
	if (!s_pause) {
		g_CpuData.pushMarker(_name).m_startTime = (uint64)Time::GetTimestamp().getRaw();
	}
}
void Profiler::PopCpuMarker(const char* _name)
{
	if (!s_pause) {
		g_CpuData.popMarker(_name).m_stopTime = (uint64)Time::GetTimestamp().getRaw();
	}
}

void Profiler::PushGpuMarker(const char* _name)
{
	if (!s_pause) {
		auto& marker = g_GpuData.pushMarker(_name);
		marker.m_issueTime = (uint64)Time::GetTimestamp().getRaw();
		#if Profiler_ALWAYS_GEN_QUERIES
			glAssert(glGenQueries(1, &g_GpuMarkerStartQueries[g_GpuData.getMarkerIndex(&marker)]));
		#endif
		glAssert(glQueryCounter(g_GpuMarkerStartQueries[g_GpuData.getMarkerIndex(&marker)], GL_TIMESTAMP));
	}
}

void Profiler::PopGpuMarker(const char* _name)
{
	if (!s_pause) {
		auto& marker = g_GpuData.popMarker(_name);
		#if Profiler_ALWAYS_GEN_QUERIES
			glAssert(glGenQueries(1, &g_GpuMarkerStopQueries[g_GpuData.getMarkerIndex(&marker)]));
		#endif
		glAssert(glQueryCounter(g_GpuMarkerStopQueries[g_GpuData.getMarkerIndex(&marker)], GL_TIMESTAMP));
	}
}

void Profiler::TrackCpuMarker(const char* _name)
{
	g_CpuData.trackMarker(StringHash(_name));
}

void Profiler::UntrackCpuMarker(const char* _name)
{
	g_CpuData.untrackMarker(StringHash(_name));
}

void Profiler::TrackGpuMarker(const char* _name)
{
	g_GpuData.trackMarker(StringHash(_name));
}

void Profiler::UntrackGpuMarker(const char* _name)
{
	g_GpuData.untrackMarker(StringHash(_name));
}

void Profiler::CpuValue(const char* _name, float _value, const char* _format)
{
	if (!s_pause) {
		g_CpuData.value(_name, _value, _format);
	}
}

void Profiler::GpuValue(const char* _name, float _value, const char* _format)
{
	if (!s_pause) {
		g_GpuData.value(_name, _value, _format);
	}
}

static ImU32  kBgColor;
static ImU32  kGpuColor;
static ImU32  kCpuColor;
static float  kFrameBarPadding;
static float  kFrameBarHeight;
static ImU32  kFrameBarColor;
static ImU32  kFrameBarTextColor;
static float  kMarkerPadding;
static float  kMarkerHeight;

static bool              g_MarkerWindowActive;
static ImGuiTextFilter   g_Filter;
static Profiler::Frame*  g_HighlightFrame;
static Profiler::Marker* g_HighlightMarker;
static Profiler::Marker* g_HighlightMarkerNext;
static Profiler::Frame*  g_SelectedFrame;
static Profiler::Marker* g_SelectedMarker;

enum ViewMode_
{
	ViewMode_Markers,
	ViewMode_Tree,
	ViewMode_Values,

	ViewMode_Count
};
typedef int ViewMode;
static ViewMode g_ViewMode = ViewMode_Markers;


static void InitStyle()
{
	kBgColor           = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.25f));
	kGpuColor          = 0xffff0800;
	kCpuColor          = 0xff1ce4ff;
	kFrameBarPadding   = 2.0f;
	kFrameBarHeight    = ImGui::GetFontSize() + kFrameBarPadding * 2.0f;
	kFrameBarColor     = ImGui::ColorConvertFloat4ToU32(ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
	kFrameBarTextColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
	kMarkerPadding     = 4.0f;
	kMarkerHeight      = ImGui::GetFontSize() + kMarkerPadding * 2.0f;
}


static void DrawDataMarkers(ProfilerData& _data, ImU32 _color, float _begY, float _endY)
{
	ImGui::PushID(&_data);

	auto& drawList   = *ImGui::GetWindowDrawList();
	auto& io         = ImGui::GetIO();

	auto rangeStart  = g_CpuData.m_frames->front().m_startTime; // always draw relative to the first CPU frame
	vec2 windowBeg   = ImGui::GetWindowPos();
	vec2 windowEnd   = vec2(ImGui::GetWindowPos()) + vec2(ImGui::GetWindowSize());

	_begY += kFrameBarHeight + 1.0f;
	auto textColor   = ImGui::ColorInvertRGB(_color);

 // draw markers
	for (uint32 i = 0; i < _data.m_frames->capacity() - 1; ++i) {
		auto& thisFrame = _data.m_frames->at_relative(i);
		auto& nextFrame = _data.m_frames->at_relative(i + 1);
		if_unlikely (thisFrame.m_id == 0 || nextFrame.m_id == 0) { // first execute, frame uninitialized
			break;
		}
		if_unlikely (thisFrame.m_startTime == 0 || nextFrame.m_startTime == 0) { // GPU frame unavailable
			continue;
		}
		
		Timestamp frameDuration = Timestamp(nextFrame.m_startTime - thisFrame.m_startTime);
		float     frameBeg      = ImGui::VirtualWindow::ToWindowX((float)Timestamp(thisFrame.m_startTime - rangeStart).asMilliseconds());
		float     frameEnd      = ImGui::VirtualWindow::ToWindowX((float)Timestamp(nextFrame.m_startTime - rangeStart).asMilliseconds());
		if (frameEnd < windowBeg.x) {
			continue;
		}
		if (frameBeg > windowEnd.x) {
			break;
		}

		for (auto j = thisFrame.m_markerBegin; j != thisFrame.m_markerEnd; j = APT_MOD_POW2(j + 1, _data.m_markers->capacity())) {
			auto& marker    = _data.m_markers->at_absolute(j);
			float markerBeg = ImGui::VirtualWindow::ToWindowX((float)Timestamp(marker.m_startTime - rangeStart).asMilliseconds());
			float markerEnd = ImGui::VirtualWindow::ToWindowX((float)Timestamp(marker.m_stopTime  - rangeStart).asMilliseconds());
			if (markerEnd < windowBeg.x) {
				continue;
			}
			if (markerBeg > windowEnd.x) {
				break;
			}
			
			markerBeg         = APT_MAX(markerBeg, windowBeg.x);        // clamp at window edge = keep label in view
			markerEnd         = APT_MIN(markerEnd, windowEnd.x) - 1.0f; //                   "
			float markerWidth = markerEnd - markerBeg;
			float markerY     = _begY + (kMarkerHeight + 1.0f) * (float)marker.m_stackDepth;

		 // apply filter
			auto  nameLen     = strlen(marker.m_name);
			bool  passFilter  = g_Filter.PassFilter(marker.m_name, marker.m_name + nameLen);
			if (g_HighlightMarker && !g_Filter.IsActive()) {
				passFilter    = strncmp(marker.m_name, g_HighlightMarker->m_name, nameLen) == 0;
			}

			float alpha       = passFilter ? 1.0f : 0.5f;

		 // cull markers < 3 pixels wide unless they pass the filter
			if (markerWidth < 3.0f) {
				if (g_Filter.IsActive() && passFilter) {
					markerWidth = 1.0f;
					markerEnd   = markerBeg + markerWidth;
				} else {
					continue;
				}
			}
		
		 // marker rectangle
			drawList.AddRectFilled(ImVec2(markerBeg, markerY), ImVec2(markerEnd, markerY + kMarkerHeight), IM_COLOR_ALPHA(_color, alpha));
			//drawList.AddRect(ImVec2(markerBeg, markerY), ImVec2(markerEnd, markerY + kMarkerHeight), IM_COLOR_ALPHA(_markerColor, 0.75f));

		 // name label
			float nameWidth   = ImGui::CalcTextSize(marker.m_name, marker.m_name + nameLen).x;
			if (nameWidth < markerWidth) {
				float nameBeg = markerBeg + markerWidth * 0.5f - nameWidth * 0.5f;
				drawList.AddText(ImVec2(nameBeg, markerY + kMarkerPadding), IM_COLOR_ALPHA(textColor, alpha), marker.m_name, marker.m_name + nameLen);
			}

		 // tooltip/marker selection
			if (g_MarkerWindowActive && ImGui::IsInside(io.MousePos, ImVec2(markerBeg, markerY), ImVec2(markerEnd, markerY + kMarkerHeight))) {
				g_HighlightMarkerNext    = &marker;
				Timestamp markerDuration = Timestamp(marker.m_stopTime - marker.m_startTime);
				double    markerPercent  = markerDuration.asMilliseconds() / frameDuration.asMilliseconds() * 100.0;
				Timestamp markerLatency  = Timestamp(marker.m_startTime - marker.m_issueTime);
				ImGui::BeginTooltip();
					ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(_color), marker.m_name);
					ImGui::Text("Duration: %s (%.3f%%)", markerDuration.asString(), markerPercent);
					if (marker.m_issueTime) {
						ImGui::Text("Latency:  %s", markerLatency.asString());
					}
				ImGui::End();

				if (io.MouseClicked[0]) {
					g_SelectedFrame  = &thisFrame;
					g_SelectedMarker = &marker;
				}
				if (Profiler::GetPause() && io.MouseClicked[1]) {
					ImGui::OpenPopup("MarkerPopup");
					g_SelectedMarker = &marker;
				}
				if (io.MouseDoubleClicked[0]) {
					ImGui::VirtualWindow::SetRegion(ImVec2((float)Timestamp(marker.m_startTime - rangeStart).asMilliseconds(), 0), ImVec2((float)Timestamp(marker.m_stopTime - rangeStart).asMilliseconds(), 200));
				}
				
			}
		}

	 // determine whether to highlight the current frame; we do this here as we want both the GPU and CPU
	 // frame bars to highlight simultaneously
		if (g_MarkerWindowActive && ImGui::IsInside(io.MousePos, ImVec2(frameBeg, _begY - kFrameBarHeight), ImVec2(frameEnd, _endY))) {
			g_HighlightFrame = &thisFrame;
		}
	}

	if (ImGui::BeginPopup("MarkerPopup")) {
		APT_ASSERT(g_SelectedMarker);
		StringHash nameHash(g_SelectedMarker->m_name);
		if (_data.findTrackedMarker(nameHash) == _data.m_trackedMarkers.end()) {
			if (ImGui::MenuItem("Track")) {
				_data.trackMarker(nameHash);
			}
		} else {
			if (ImGui::MenuItem("Untrack")) {
				_data.untrackMarker(nameHash);
			}
		}
		ImGui::EndPopup();
	}

	ImGui::PopID();
}

static void DrawDataFrames(ProfilerData& _data, ImU32 _color, float _begY, float _endY)
{
	auto& drawList  = *ImGui::GetWindowDrawList();
	auto& io        = ImGui::GetIO();

	auto rangeStart = g_CpuData.m_frames->front().m_startTime; // always draw relative to the first CPU frame
	vec2 windowBeg  = ImGui::GetWindowPos();
	vec2 windowEnd  = vec2(ImGui::GetWindowPos()) + vec2(ImGui::GetWindowSize());

 // draw frame bar
	drawList.AddRectFilled(ImVec2(windowBeg.x, _begY), ImVec2(windowEnd.x, _begY + kFrameBarHeight), kFrameBarColor);

 // draw frame borders, highlight
	for (uint32 i = 0; i < _data.m_frames->capacity() - 1; ++i) {
		auto& thisFrame = _data.m_frames->at_relative(i);
		auto& nextFrame = _data.m_frames->at_relative(i + 1);
		if_unlikely (thisFrame.m_id == 0 || nextFrame.m_id == 0) { // first execute, frame uninitialized
			break;
		}
		if_unlikely (thisFrame.m_startTime == 0 || nextFrame.m_startTime == 0) { // GPU frame unavailable
			continue;
		}
		Timestamp frameDuration = Timestamp(nextFrame.m_startTime - thisFrame.m_startTime);
		float     frameBeg      = ImGui::VirtualWindow::ToWindowX((float)Timestamp(thisFrame.m_startTime - rangeStart).asMilliseconds());
		float     frameEnd      = ImGui::VirtualWindow::ToWindowX((float)Timestamp(nextFrame.m_startTime - rangeStart).asMilliseconds());
		if (frameEnd < windowBeg.x) {
			continue;
		}
		if (frameBeg > windowEnd.x) {
			break;
		}

		bool highlight = g_HighlightFrame && g_HighlightFrame->m_id == thisFrame.m_id;

	 // border
		auto borderColor = kFrameBarColor;
		if (highlight) {
			borderColor = _color;
		}
		drawList.AddLine(ImVec2(frameBeg, _begY), ImVec2(frameBeg, _endY), borderColor);
	
	 // highlight
		frameBeg = APT_MAX(frameBeg, windowBeg.x);
		frameEnd = APT_MIN(frameEnd, windowEnd.x);
		if (highlight) {
			drawList.AddRectFilled(ImVec2(frameBeg, _begY), ImVec2(frameEnd - 1.0f, _begY + kFrameBarHeight), _color);
			drawList.AddLine(ImVec2(frameEnd - 1.0f, _begY), ImVec2(frameEnd - 1.0f, _endY), _color); // extra border at frame end
		}

	 // id label
		String<16> frameLabel("%07llu", thisFrame.m_id);
		if (ImGui::CalcTextSize(frameLabel.c_str(), frameLabel.c_str() + frameLabel.getLength()).x < frameEnd - frameBeg) {
			ImU32 textColor = highlight ? ImGui::ColorInvertRGB(_color) : kFrameBarTextColor;
			drawList.AddText(ImVec2(frameBeg + 2.0f, _begY + kFrameBarPadding), textColor, frameLabel.begin(), frameLabel.end()); 
		}
	}
}

static void DrawDataTree(ProfilerData& _data, ImU32 _color)
{
	ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);
	ImGui::PushStyleColor(ImGuiCol_Text, _color);

	for (uint32 i = 0; i < _data.m_frames->capacity() - 1; ++i) {
		auto& thisFrame = _data.m_frames->at_relative(i);
		auto& nextFrame = _data.m_frames->at_relative(i + 1);
		if_unlikely (thisFrame.m_id == 0 || nextFrame.m_id == 0) { // first execute, frame uninitialized
			break;
		}
		if_unlikely (thisFrame.m_startTime == 0 || nextFrame.m_startTime == 0) { // GPU frame unavailable
			continue;
		}

		Timestamp frameDuration = Timestamp(nextFrame.m_startTime - thisFrame.m_startTime);

		if (g_SelectedFrame == &thisFrame) {
			ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Always);
		} else if (g_SelectedFrame) {
			ImGui::SetNextTreeNodeOpen(false, ImGuiCond_Always);
		}
		String<64> frameInfo;
		frameInfo.setf("%07llu -- %s###%llu%d", thisFrame.m_id, frameDuration.asString(), &_data, i);
		if (ImGui::TreeNode((const char*)frameInfo)) {
			auto markerBegin = thisFrame.m_markerBegin;
			auto markerEnd   = thisFrame.m_markerEnd;
			ImGui::Columns(3);
			while (markerBegin != markerEnd) {
				auto& marker = _data.m_markers->at_absolute(markerBegin);
				Timestamp markerDuration = Timestamp(marker.m_stopTime - marker.m_startTime);
				double    markerPercent  = markerDuration.asMilliseconds() / frameDuration.asMilliseconds() * 100.0;

				ImGui::PushStyleColor(ImGuiCol_Text, textColor);
				ImGui::Text("%*s%s", marker.m_stackDepth * 4, "", marker.m_name);
				ImGui::NextColumn();
				ImGui::Text("%s", markerDuration.asString());
				ImGui::NextColumn();
				ImGui::Text("%.3f%%", markerPercent);
				ImGui::NextColumn();
				ImGui::PopStyleColor(1);
				
				markerBegin = APT_MOD_POW2(markerBegin + 1, _data.m_markers->capacity());
			}
			ImGui::Columns(1);
			ImGui::TreePop();
		}
	}

	ImGui::PopStyleColor(1);
}

static void DrawValueData(ProfilerData& _data, ProfilerData::ValueData& _valueData, ImU32 _color, const ImVec2& _size = ImVec2(-1, -1), bool _enableTooltip = true)
{
	ImGui::PushID(&_valueData);

	vec2 size = _size;
	if (size.x <= 0) {
		size.x = ImGui::GetContentRegionAvail().x;
	}
	if (size.y <= 0) {
		size.y = ImGui::GetContentRegionAvail().y;
	}

	vec2 beg = vec2(ImGui::GetWindowPos()) + vec2(ImGui::GetCursorPos()) - vec2(ImGui::GetScrollX(), ImGui::GetScrollY());
	vec2 end = beg + size;
	auto& drawList = *ImGui::GetWindowDrawList();

	ImGui::InvisibleButton("##PreventDrag", size);
	drawList.AddRectFilled(beg, end, IM_COLOR_ALPHA(kBgColor, 0.75f));
	drawList.AddRect(beg, end, ImGui::GetColorU32(ImGuiCol_Border));
	ImGui::PushClipRect(beg + vec2(1.0f), end - vec2(1.0f), true);

	auto& value    = _valueData.m_value;
	auto& history  = _valueData.m_history;
	
 // plot graph relative to the average = keep average in the vertical center
	#define ValueToWindow(idx) vec2(beg.x + (float)idx / (float)(n - 1) * size.x, beg.y + size.y * 0.5f + (value.m_avg - history.at_relative(idx)) / range * size.y * 0.5f)
	float range = APT_MAX(1.0f, value.m_max - value.m_min);
	uint32 n    = history.capacity() - 1;
	vec2 prev   = ValueToWindow(0);
	for (uint32 i = 1; i < n; ++i ) {
		vec2 curr = ValueToWindow(i);
		//drawList.AddLine(Floor(prev), Floor(curr), _color);
		drawList.AddLine(prev, curr, _color);
		prev = curr;
	}


	auto MakeValueLabel = 
		[](const char* _format, float _value, StringBase& out_)
		{
			if (_format == Profiler::kFormatTimeMs) {
				_format = "%1.2fms";
				if (_value >= 1000.0f) {
					_value /= 1000.0f;
					_format = "%1.3fs";
				} else if (_value < 0.1f) {
					_value *= 1000.0f;
					_format = "%1.0fus";
				}
			}
			out_.setf(_format, _value);
		};

	if (_enableTooltip && ImGui::IsItemHovered()) {
		uint32 i = (uint32)((ImGui::GetMousePos().x - beg.x) / size.x * (float)history.capacity());
		drawList.AddCircleFilled(ValueToWindow(i), 2.0f, _color);
		float v  = history.at_relative(i);
		String<64> labelTooltip;
		MakeValueLabel(value.m_format, v, labelTooltip);
		ImGui::BeginTooltip();
			ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(_color), labelTooltip.c_str());
		ImGui::EndTooltip();
	}
	#undef ValueToWindow

	if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(1)) {
		ImGui::OpenPopup("ValuePopup");
	}

	if (ImGui::BeginPopup("ValuePopup")) {
		StringHash nameHash(value.m_name);
		auto it = eastl::find(_data.m_pinnedValues.begin(), _data.m_pinnedValues.end(), nameHash);
		if (it != _data.m_pinnedValues.end()) {
			if (ImGui::MenuItem("Unpin")) {
				_data.m_pinnedValues.erase(it);
			}
		} else {
			if (ImGui::MenuItem("Pin")) {
				_data.m_pinnedValues.push_back(nameHash);
			}
		}
		ImGui::EndPopup();
	}

	String<64> labelName, labelMin, labelMax, labelAvg;
	labelName.set(value.m_name);
	MakeValueLabel(value.m_format, value.m_min, labelMin);
	MakeValueLabel(value.m_format, value.m_max, labelMax);
	MakeValueLabel(value.m_format, value.m_avg, labelAvg);
	vec2 labelNameSize  = ImGui::CalcTextSize(labelName.begin(), labelName.end());
	vec2 labelMinSize   = ImGui::CalcTextSize(labelMin.begin(),  labelMin.end());
	vec2 labelMaxSize   = ImGui::CalcTextSize(labelMax.begin(),  labelMax.end());
	vec2 labelAvgSize   = ImGui::CalcTextSize(labelAvg.begin(),  labelAvg.end());

	float maxWidth        = APT_MAX(labelMinSize.x, APT_MAX(labelMaxSize.x, labelAvgSize.x));
	auto  kLabelBgColor   = IM_COLOR_ALPHA(IM_COL32_BLACK, 0.75f);
	auto  kMinMaxAvgColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);

	vec2 padding = ImGui::GetStyle().FramePadding;
	beg += padding;
	end -= padding;

	auto AddTextRect = 
		[&](const vec2& _beg, const vec2& _size, const char* _textBeg, const char* _textEnd, ImU32 _textCol, ImU32 _bgCol)
		{
			drawList.AddRectFilled(_beg - vec2(2.0f), _beg + _size + vec2(2.0f), _bgCol);
			drawList.AddText(_beg, _textCol, _textBeg, _textEnd);
		};	
	AddTextRect(beg, labelNameSize, labelName.begin(), labelName.end(), _color, kLabelBgColor);
	if (size.x > (labelNameSize.x + maxWidth + padding.x * 2.0f) && size.y > (labelNameSize.y * 3.0f + padding.y * 2.0f)) {
	 	AddTextRect(vec2(end.x - labelMinSize.x, end.y - labelMinSize.y), labelMinSize, labelMin.begin(), labelMin.end(), kMinMaxAvgColor, kLabelBgColor);
		AddTextRect(vec2(end.x - labelMaxSize.x, beg.y), labelMaxSize, labelMax.begin(), labelMax.end(), kMinMaxAvgColor, kLabelBgColor);
		AddTextRect(vec2(end.x - labelAvgSize.x, beg.y + size.y * 0.5f - labelAvgSize.y * 0.5f - padding.y), labelAvgSize, labelAvg.begin(), labelAvg.end(), kMinMaxAvgColor, kLabelBgColor);
	}

	ImGui::PopClipRect();
	ImGui::PopID();
}

void Profiler::DrawUi()
{
	APT_ONCE InitStyle();

	ImGui::Begin("Profiler", nullptr, 0
		| ImGuiWindowFlags_MenuBar
		);

	if (s_frameIndex <= kFrameCount) {
		ImGui::End();
		return;
	}
	
	bool fit = false;
	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("View")) {
			if (ImGui::MenuItem("Markers")) g_ViewMode = ViewMode_Markers;
			if (ImGui::MenuItem("Values"))  g_ViewMode = ViewMode_Values;
			if (ImGui::MenuItem("Tree"))    g_ViewMode = ViewMode_Tree;
			
			ImGui::EndMenu();
		}

		ImGui::SameLine();
		g_Filter.Draw("Filter", 160.0f);

		ImGui::SameLine();
		if (ImGui::SmallButton(s_pause ? ICON_FA_PLAY " Resume" : ICON_FA_PAUSE " Pause")) {
			SetPause(!s_pause);
		}
		ImGui::SameLine();
		if (g_ViewMode == ViewMode_Markers && ImGui::SmallButton(ICON_FA_ARROWS_H " Fit")) {
			fit = true;
		}


		ImGui::EndMenuBar();
	}

	auto& firstFrame  = g_CpuData.m_frames->front();
	auto& lastFrame   = g_CpuData.m_frames->back();
	auto  rangeStart  = g_CpuData.m_frames->front().m_startTime;
	float timeRange   = (float)Timestamp(g_CpuData.m_avgFrameDuration).asMilliseconds() * kFrameCount;
	//float timeRange  = (float)Timestamp(frameEnd.m_startTime - frameBeg.m_startTime).asMilliseconds();
	
	g_MarkerWindowActive  = ImGui::IsWindowFocused();
	g_HighlightFrame      = nullptr;
	g_HighlightMarker     = g_HighlightMarkerNext;
	g_HighlightMarkerNext = nullptr;
	if (!s_pause) {
		g_SelectedFrame   = nullptr;
		g_SelectedMarker  = nullptr;
	}

if (g_ViewMode == ViewMode_Markers) {
	float cursorX = ImGui::GetCursorPosX();
	ImGui::SetCursorPosX(cursorX + 64.0f); // space for the CPU/GPU avg frame duration labels
	float gpuBegY, cpuBegY;

	ImGui::PushStyleColor(ImGuiCol_FrameBg, kBgColor);
	float oldScrollBarSize = ImGui::GetStyle().ScrollbarSize;
	ImGui::GetStyle().ScrollbarSize = 10.0f;

	ImGui::VirtualWindow::SetNextRegionExtents(ImVec2(timeRange * -0.1f, 200), ImVec2(timeRange * 1.1f, 200), ImGuiCond_Always);
	if (fit) {
		float pad = (float)Timestamp(lastFrame.m_startTime - firstFrame.m_startTime).asMilliseconds() * 0.05f;
		float beg = (float)Timestamp(firstFrame.m_startTime - rangeStart).asMilliseconds() - pad;
		float end = (float)Timestamp(lastFrame.m_startTime  - rangeStart).asMilliseconds() + pad;
		ImGui::VirtualWindow::SetNextRegion(ImVec2(beg, 0), ImVec2(end, 200), ImGuiCond_Always);
	} else {
		ImGui::VirtualWindow::SetNextRegion(ImVec2(0.0f, 0), ImVec2(100.0f, 200), ImGuiCond_Once);
	}
	if (ImGui::VirtualWindow::Begin(ImGui::GetID("ProfilerMarkers"), ImVec2(-1, -1), 0
		| ImGui::VirtualWindow::Flags_PanX
		| ImGui::VirtualWindow::Flags_ZoomX
		| ImGui::VirtualWindow::Flags_ScrollBarX
		)) {
		
		g_MarkerWindowActive |= ImGui::IsWindowFocused(); // virtual window is separate from its parent

		//ImGui::VirtualWindow::Grid(ImVec2(8, 0), ImVec2(0.01f, 0.0f)); // 10us intervals

		vec2 windowBeg = ImGui::GetWindowPos();
		vec2 windowEnd = vec2(ImGui::GetWindowPos()) + vec2(ImGui::GetContentRegionAvail());
		float rangeY   = (windowEnd.y - windowBeg.y) * 0.5f;
		gpuBegY        = windowBeg.y;
		cpuBegY        = windowBeg.y + rangeY + 1.0f;
		cpuBegY        = APT_MAX(cpuBegY, gpuBegY + kFrameBarHeight + 1.0f + kMarkerHeight + 1.0f);

		DrawDataMarkers(g_GpuData, kGpuColor, gpuBegY, cpuBegY);
		DrawDataMarkers(g_CpuData, kCpuColor, cpuBegY, windowEnd.y);

		DrawDataFrames(g_GpuData, kGpuColor, gpuBegY, cpuBegY);
		DrawDataFrames(g_CpuData, kCpuColor, cpuBegY, windowEnd.y);

	 // if highlighted GPU marker, draw the issue time on the CPU timeline
		if (g_HighlightMarker && g_HighlightMarker->m_issueTime != 0) {
			auto rangeStart = g_CpuData.m_frames->front().m_startTime;
			float issueBeg  = ImGui::VirtualWindow::ToWindowX((float)Timestamp(g_HighlightMarker->m_issueTime - rangeStart).asMilliseconds());
			      issueBeg -= ImGui::CalcTextSize(ICON_FA_MAP_MARKER, ICON_FA_MAP_MARKER + 1).x * 0.5f;
			ImGui::GetWindowDrawList()->AddText(ImVec2(issueBeg, cpuBegY + kFrameBarHeight - ImGui::GetFontSize()), kGpuColor, ICON_FA_MAP_MARKER);
		}


		ImGui::VirtualWindow::End();
	}

	auto& drawList = *ImGui::GetWindowDrawList();
	cursorX += ImGui::GetWindowPos().x;
	String<32> label;
	label.setf("GPU\n%s", Timestamp(g_GpuData.m_avgFrameDuration).asString());
	drawList.AddText(ImVec2(cursorX, gpuBegY + 2.0f), kGpuColor, label.begin(), label.end());
	label.setf("CPU\n%s", Timestamp(g_CpuData.m_avgFrameDuration).asString());
	drawList.AddText(ImVec2(cursorX, cpuBegY + 2.0f), kCpuColor, label.begin(), label.end());

	ImGui::GetStyle().ScrollbarSize = oldScrollBarSize;
	ImGui::PopStyleColor(1);

} else if (g_ViewMode == ViewMode_Tree) {
	ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNode("GPU")) {
		DrawDataTree(g_GpuData, kGpuColor);
		ImGui::TreePop();
	}
	
	ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNode("CPU")) {
		DrawDataTree(g_CpuData, kCpuColor);
		ImGui::TreePop();
	}

} else if (g_ViewMode == ViewMode_Values) {

	ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNode("GPU")) {
		for (auto& valueData : g_GpuData.m_values) {
			if (!g_Filter.IsActive() || g_Filter.PassFilter(valueData.second.m_value.m_name)) {
				DrawValueData(g_GpuData, valueData.second, kGpuColor, ImVec2(-1, 80), g_MarkerWindowActive);
			}
		}

		ImGui::TreePop();
	}

	ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNode("CPU")) {
		for (auto& valueData : g_CpuData.m_values) {
			if (!g_Filter.IsActive() || g_Filter.PassFilter(valueData.second.m_value.m_name)) {
				DrawValueData(g_CpuData, valueData.second, kCpuColor, ImVec2(-1, 80), g_MarkerWindowActive);
			}
		}

		ImGui::TreePop();
	}
}
	ImGui::End();
}

void Profiler::DrawPinnedValues()
{
	ImVec2 size = ImVec2(160, 80);

	ImVec2 padding = ImGui::GetStyle().ItemSpacing;
	ImVec2 cursor = ImVec2(padding.x, ImGui::GetWindowSize().y - size.y - padding.y);

	for (auto& valueData : g_GpuData.m_values) {
		if (g_GpuData.isValuePinned(StringHash(valueData.second.m_value.m_name))) {
			ImGui::SetCursorPos(cursor);
			DrawValueData(g_GpuData, valueData.second, kGpuColor, size, true);
			cursor.x += size.x + padding.x;
		}
	}
	for (auto& valueData : g_CpuData.m_values) {
		if (g_CpuData.isValuePinned(StringHash(valueData.second.m_value.m_name))) {
			ImGui::SetCursorPos(cursor);
			DrawValueData(g_CpuData, valueData.second, kCpuColor, size, true);
			cursor.x += size.x + padding.x;
		}
	}

}

// PRIVATE

uint64 Profiler::s_frameIndex;
bool   Profiler::s_pause;
bool   Profiler::s_setPause;

void Profiler::SetPause(bool _pause)
{
 // we only set s_pause at the end of NextFrame() to ensure we have a complete frame of data before pausing
	s_setPause = _pause;
}