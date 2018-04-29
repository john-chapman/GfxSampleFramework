#include "imgui_ext.h"

#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui_internal.h"

//#define IMGUI_EXT_STATE_ASSERT_MAX_ALLOC 1               // Assert if the state storage system uses more than IMGUI_EXT_STATE_MAX_ALLOC (allocations are never freed so this is a simple way to find out if something is going wrong).
#define IMGUI_EXT_STATE_MAX_ALLOC          (1024 * 1024)   // 1mb

using namespace ImGui;

/*******************************************************************************

                                    Misc

*******************************************************************************/

// Flag helpers.
static inline bool GetFlag(int _flags, int _flag)
{
	return ((_flags & _flag) != 0);
}
static inline void SetFlag(int& _flags, int _flag, bool _value)
{
	_flags = (_value ? (_flags | _flag) : (_flags & ~_flag));
}

// Resolve common _size argument where -1 means 'fill the available content region'.
static ImVec2 ResolveSize(const ImVec2& _size, float _aspect = -1.0f)
{
	ImVec2 rectMin = ImFloor(GetCursorPos());
	ImVec2 rectMax = ImFloor(GetContentRegionMax());
	ImVec2 ret     = rectMax - rectMin;
	if (_size.x > 0.0f) {
		ret.x = _size.x;
	}
	if (_size.y > 0.0f) {
		ret.y = _size.y;
	}
	if (_aspect > 0.0f) {
		ret.x = ret.y * _aspect;
	}
	return ret;
}

// Common logic for handling ImGuiCond. once_ should be init to false.
static bool ResolveCond(ImGuiCond& _cond_, bool& _once_)
{
	bool ret = false;
	if (_cond_) {
		if (GetFlag(_cond_, ImGuiCond_Always)) {
			ret = true;
		} else if (GetCurrentWindow()->Appearing && GetFlag(_cond_, ImGuiCond_Appearing)) {
			ret = true;
		} else {
			if (!_once_) {
				_once_ = true;
				ret = true;
			}
		}
	}
	_cond_ = 0;
	return ret;
}

/*******************************************************************************

                                 ImVectorMap

*******************************************************************************/
// Extend ImVector to a sorted associative container. Keys are unique.

template <typename K, typename V>
struct ImKeyValue
{
	K m_key;
	V m_value;
	
	ImKeyValue() {}
	ImKeyValue(K _key, const V& _value): m_key(_key), m_value(_value) {}

	static int Compare(const void* _lhs, const void* _rhs) // for qsort
	{
		auto lhs = (const ImKeyValue*)_lhs;
		auto rhs = (const ImKeyValue*)_rhs;

 	 // K might be unsigned (e.g. an ImU32) therefore can't just subtract
		if (lhs->m_key > rhs->m_key) {
			return +1;
		}
		if (lhs->m_key < rhs->m_key) {
			return -1;
		}
		return 0;
	}
};

template <typename K, typename V>
struct ImVectorMap: public ImVector<ImKeyValue<K, V> >
{
	iterator find(K _key)
	{ 
		auto ret = lower_bound(_key);
		if (ret != end() && ret->m_key == _key) {
			return ret;
		}
		return end();
	}

	const_iterator find(K _key) const
	{
		auto ret = lower_bound(_key);
		if (ret != end() && ret->m_key == _key) {
			return ret;
		}
		return end();
	}

	iterator insert(K _key, const V& _value)
	{
		iterator it = lower_bound(_key);
		IM_ASSERT(it == end() || it->m_key != _key); // use find_or_insert
		return ImVector<ImKeyValue<K, V> >::insert(it, ImKeyValue<K, V>(_key, _value));
	}

	iterator find_or_insert(K _key, const V& _value = V())
	{
		iterator it = find(K);
		if (it == end()) {
			it = insert(_key, _value);
		}
		return it;
	}

	void push_back(K _key, const V& _value)
	{
		ImVector<ImKeyValue<K, V> >::push_back(ImKeyValue<K, V>(_key, _value));
	}

	// Avoid the cost of sorting when inserting multiple items by calling push_back() for each item followed by a single call to sort().
	void sort()
	{
		if (size() > 1) {
			qsort(begin(), size(), sizeof(ImKeyValue<K, V>), ImKeyValue<K, V>::Compare);
		}
	}

private:
	iterator lower_bound(K _key)
	{
		iterator first = begin();
		iterator last  = end();
		auto count = last - first;
		while (count > 0) {
			auto step = count >> 1;
			iterator it = first + step;
			if (it->m_key < _key) {
				first = ++it;
				count -= step + 1;
			} else {
				count = step;
			}
		}
		return first;
	}
};


/*******************************************************************************

                                   StateMap

*******************************************************************************/

// StateMap
// Map IDs -> allocations. Designed for allocating blocks of per-widget state rather than individual variables.
// - Allocations are never freed.
// - Pointers to state objects are invalidated by any subsequent calls to Insert() or FindOrInsert().

namespace ImGui { namespace StateMap {

struct StateMapEntry
{
	ImU32 m_offset; // into g_StateStorage
	ImU32 m_size;   // size of the allocated object 
};
static ImVectorMap<ImGuiID, StateMapEntry> g_StateMap;
static ImVector<char>                      g_StateStorage;

// Return state associated with _id, or 0 if not found.
template <typename T>
static T* Find(ImGuiID _id)
{
	auto mapEntry = g_StateMap.find(_id);
	if (mapEntry == g_StateMap.end()) {
		return nullptr;
	}
	IM_ASSERT(sizeof(T) == mapEntry->m_value.m_size); // minimal type safety check
	return (T*)(g_StateStorage.Data + mapEntry->m_value.m_offset);
}

// Insert and return a new state.
template <typename T>
static T* Insert(ImGuiID _id, const T& _value = T())
{
	ImU32 align  = (ImU32)alignof(T) - 1;
	ImU32 size   = (ImU32)sizeof(T) + align;
	ImU32 offset = g_StateStorage.size();
	g_StateStorage.resize(offset + size, 0);
	offset = (offset + align) & (~align);
	IM_ASSERT(offset % (ImU32)alignof(T) == 0);

	StateMapEntry mapEntry = { offset, (ImU32)sizeof(T) };
	g_StateMap.insert(_id, mapEntry);
	auto ret = (T*)(g_StateStorage.Data + offset);
	*ret = _value;

	#ifdef IMGUI_EXT_STATE_ASSERT_MAX_ALLOC
		IM_ASSERT(g_StateStorage.size() < IMGUI_EXT_STATE_MAX_ALLOC); // allocations are never freed, are you making a lot of widgets?
	#endif
	return ret;
}

// Return either an existing state or a new one if _id not found.
template<typename T>
static T* FindOrInsert(ImGuiID _id, const T& _value = T())
{
	T* ret = Find<T>(_id);
	if (!ret) {
		ret = Insert<T>(_id, _value);
	}
	return ret;
}

} } // namespace ImGui::StateMap


/*******************************************************************************

                                 VirtualWindow

*******************************************************************************/

namespace ImGui { namespace VirtualWindow {

struct State
{
	ImGuiID m_id                   = 0;
	ImRect  m_rectV                = ImRect(ImVec2(-FLT_MAX, -FLT_MAX), ImVec2(FLT_MAX, FLT_MAX));
	ImRect  m_subrectV;
	ImRect  m_rectW;
	bool    m_setSubrectV          = false; // if SetRegion() was called, prevent scrollbars overriding the region extents

 // once_ args to ResolveCond
	bool    m_setNextRegion        = false;
	bool    m_setNextRegionExtents = false;
};

static ImGuiCond g_SetNextRegionCond;
static ImRect    g_NextRegion;
static ImGuiCond g_SetNextRegionExtentsCond;
static ImRect    g_NextRegionExtents;
static State     g_CurrentState;

ImVec2 ToVirtual(const ImVec2& _windowPos, const State* _state)
{
	const ImRect& rectV = _state->m_subrectV;
	const ImRect& rectW = _state->m_rectW;
	ImVec2 ret = (_windowPos - rectW.Min) / rectW.GetSize();
	ret        = rectV.Min + ret * rectV.GetSize();
	return ret;
}

ImVec2 ToVirtualScale(const ImVec2& _windowScale, const State* _state)
{
	const ImRect& rectV = _state->m_subrectV;
	const ImRect& rectW = _state->m_rectW;
	ImVec2 ret = _windowScale / rectW.GetSize();
	return ret * rectV.GetSize();
}

ImVec2 ToWindow(const ImVec2& _virtualPos, const State* _state)
{
	const ImRect& rectV = _state->m_subrectV;
	const ImRect& rectW = _state->m_rectW;
	ImVec2 ret = (_virtualPos - rectV.Min) / rectV.GetSize();
	ret        = rectW.Min + ret * rectW.GetSize();
	return ImFloor(ret);
}

ImVec2 ToWindowScale(const ImVec2& _virtualScale, const State* _state)
{
	const ImRect& rectV = _state->m_subrectV;
	const ImRect& rectW = _state->m_rectW;
	ImVec2 ret = _virtualScale / rectV.GetSize();
	return ret * rectW.GetSize();
}

State* GetCurrentState()
{
	ImGuiID id = g_CurrentState.m_id;
	IM_ASSERT(id);    // not inside a VirtualWindow
	State* state = StateMap::Find<State>(id);
	IM_ASSERT(state); // id was invalid?
	return state;
}

bool Begin(ImGuiID _id, const ImVec2& _size, Flags _flags)
{
	ImGuiWindow* window = GetCurrentWindow();
	if (window->SkipItems) {
		return false;
	}

 // resolve SetNextVirtualWindowRegion/SetNextVirtualRegionExtents
	State* state     = StateMap::FindOrInsert<State>(_id);
	state->m_id      = _id;
	ImRect& rectW    = state->m_rectW;
	ImRect& rectV    = state->m_rectV;
	ImRect& subrectV = state->m_subrectV;
	if (ResolveCond(g_SetNextRegionExtentsCond, state->m_setNextRegionExtents)) {
		rectV = g_NextRegionExtents;
	}
	if (ResolveCond(g_SetNextRegionCond, state->m_setNextRegion)) {
		subrectV = g_NextRegion;
		state->m_setSubrectV = true;
	}
	
 // init and begin child frame
	bool scrollBarX = GetFlag(_flags, Flags_ScrollBarX);
	scrollBarX &= subrectV.GetSize().x < rectV.GetSize().x;
	bool scrollBarY = GetFlag(_flags, Flags_ScrollBarY);
	scrollBarY &= subrectV.GetSize().y < rectV.GetSize().y;
	SetNextWindowContentSize(rectV.GetSize() / subrectV.GetSize() * rectW.GetSize()); // convert to pixels, incorporate zoom
	ImVec2 sizeW = ResolveSize(_size, GetFlag(_flags, Flags_Square) ? 1.0f : -1.0f);
	ImGuiWindowFlags flags = 0
		//| ImGuiWindowFlags_NoScrollWithMouse // don't set this, avoid passing mouse wheel state up to the parent window (which interferes with zoom)
		| (scrollBarX ? ImGuiWindowFlags_AlwaysHorizontalScrollbar : 0)
		| (scrollBarY ? ImGuiWindowFlags_AlwaysVerticalScrollbar : ImGuiWindowFlags_NoScrollbar)
		;
	BeginChildFrame(_id, sizeW, flags);
	rectW.Min = GetItemRectMin();
	rectW.Max = rectW.Min + sizeW;//GetItemRectMax(); // \todo GetItemRectMax seems not to work for child frames?
	const float aspectW = sizeW.x / sizeW.y;

 // set focus on mouse wheel down/scroll (pan/zoom immediately without focusing the window first)
	auto& io = GetIO();
	bool hovered = IsWindowHovered();
	if (hovered) {
		if (GetFlag(_flags, Flags_Pan) && io.MouseDown[2] && !IsMouseDragging(2)) {
			SetWindowFocus();
		}
		if (GetFlag(_flags, Flags_Zoom) && io.MouseWheel && !IsMouseDragging(2)) {
			SetWindowFocus();
		}
	}
	bool focused = IsWindowFocused();
	
 // zoom/pan
	if (focused || state->m_setSubrectV) {
		bool setScroll      = state->m_setSubrectV; // see if (setScroll) below

		ImVec2 deltaSizeW   = ImVec2(io.MouseWheel, io.MouseWheel) * -16.0f;
		deltaSizeW.x        = GetFlag(_flags, Flags_ZoomX) ? deltaSizeW.x   : 0.0f;
		deltaSizeW.y        = GetFlag(_flags, Flags_ZoomY) ? deltaSizeW.y   : 0.0f;

		ImVec2 deltaOriginW = io.MouseDown[2] ? ImMin(io.MouseDelta, sizeW) : ImVec2(0.0f, 0.0f); // clamp mouse delta, in some cases it can be very large e.g. re-focusing the window on a second screen
		deltaOriginW.x      = GetFlag(_flags, Flags_PanX)  ? deltaOriginW.x : 0.0f;
		deltaOriginW.y      = GetFlag(_flags, Flags_PanY)  ? deltaOriginW.y : 0.0f;

		ImVec2 zoom = deltaSizeW / sizeW;
		if (hovered && (abs(zoom.x) > 0.0f || abs(zoom.y) > 0.0f)) {
			ImVec2 anchorW = io.MousePos;
			zoom = zoom * subrectV.GetSize(); // keep zoom rate proportional to current region size
			zoom.x *= aspectW;
			ImVec2 before = ToVirtual(anchorW, state);
			if ((subrectV.Max.x - subrectV.Min.x) > 1e-7f) {
				subrectV.Min.x -= zoom.x;
				subrectV.Max.x += zoom.x;
			}
			if ((subrectV.Max.y - subrectV.Min.y) > 1e-7f) {
				subrectV.Min.y -= zoom.y;
				subrectV.Max.y += zoom.y;
			}
			ImVec2 after  = ToVirtual(anchorW, state);

			ImVec2 offset = (before - after);
			subrectV.Min += offset;
			subrectV.Max += offset;

			setScroll = true;
		}

		ImVec2 pan = deltaOriginW / sizeW;
		if (abs(pan.x) > 0.0f || abs(pan.y) > 0.0f) {
			ImVec2 offset = pan * subrectV.GetSize();
			subrectV.Min -= offset;
			subrectV.Max -= offset;
			ImGui::CaptureMouseFromApp();

			setScroll = true;
		}
		
		if (scrollBarX || scrollBarY) {
			if (setScroll) {
			 // need to set scrollbars on pan/zoom
				ImVec2 scroll = (subrectV.Min - rectV.Min) / subrectV.GetSize() * rectW.GetSize();
				SetScrollX(scroll.x);
				SetScrollY(scroll.y); // \todo this flickers because we're not setting NoScrollWithMouse on the child window
			} else {
			 // else use scrollbars to pan
				ImVec2 scroll = GetCurrentWindow()->Scroll;
				scroll = scroll / (rectV.GetSize() / subrectV.GetSize() * rectW.GetSize()) * rectV.GetSize();
				ImVec2 subrectSize = subrectV.GetSize();
				if (scrollBarX) {
					subrectV.Min.x = state->m_rectV.Min.x + scroll.x;
					subrectV.Max.x = subrectV.Min.x + subrectSize.x;
				}
				if (scrollBarY) {
					subrectV.Min.y = state->m_rectV.Min.y + scroll.y;
					subrectV.Max.y = subrectV.Min.y + subrectSize.y;
				}
			}
		}
	}
	state->m_setSubrectV = false;

 // copy current state (ptr may be invalidated)
	g_CurrentState = *state;

	return true;
}

void End()
{
	IM_ASSERT(VirtualWindow::g_CurrentState.m_id);
	VirtualWindow::g_CurrentState.m_id = 0;
	EndChildFrame();
}

void SetNextRegion(const ImVec2& _rectMin, const ImVec2& _rectMax, ImGuiCond _cond)
{
	g_SetNextRegionCond = _cond;
	g_NextRegion = ImRect(_rectMin, _rectMax);
}

void SetNextRegionExtents(const ImVec2& _rectMin, const ImVec2& _rectMax, ImGuiCond _cond)
{
	g_SetNextRegionExtentsCond = _cond;
	g_NextRegionExtents = ImRect(_rectMin, _rectMax);
}

void SetRegion(const ImVec2& _rectMin, const ImVec2& _rectMax)
{
	auto state = GetCurrentState();
	state->m_subrectV = ImRect(_rectMin, _rectMax);
	state->m_setSubrectV = true;
}

void SetRegionExtents(const ImVec2& _rectMin, const ImVec2& _rectMax)
{
	auto state = GetCurrentState();
	state->m_rectV.Min = _rectMin;
	state->m_rectV.Max = _rectMax;
}

void Grid(const ImVec2& _windowSpacingMin, const ImVec2& _virtualSpacingMin, const ImVec2& _alignBase)
{
	const ImRect& rectV = g_CurrentState.m_subrectV;
	const ImRect& rectW = g_CurrentState.m_rectW;
	const ImVec2  sizeV = rectV.GetSize();
	const ImVec2  sizeW = rectW.GetSize();

	auto& drawList = *GetWindowDrawList();

	if (_windowSpacingMin.x > 0.0f) {
		float spacingV = _virtualSpacingMin.x;
		float spacingW = (spacingV / sizeV.x) * sizeW.x;
		while (spacingW < _windowSpacingMin.x) {
			spacingV *= _alignBase.x;
			spacingW *= _alignBase.x;
		}
		float i = ImFloor(rectV.Min.x / spacingV) * spacingV;
		for (; i <= rectV.Max.x; i += spacingV) {
			drawList.AddLine(
				ToWindow(ImVec2(i, rectV.Min.y), &g_CurrentState),
				ToWindow(ImVec2(i, rectV.Max.y), &g_CurrentState),
				GetColorU32(ImGuiCol_Border, 0.5f)
				);
		}
	}
	if (_windowSpacingMin.y > 0.0f) {
		float spacingV = _virtualSpacingMin.y;
		float spacingW = (spacingV / sizeV.y) * sizeW.y;
		while (spacingW < _windowSpacingMin.y) {
			spacingV *= _alignBase.y;
			spacingW *= _alignBase.y;
		}
		float i = ImFloor(rectV.Min.y/ spacingV) * spacingV;
		for (; i <= rectV.Max.y; i += spacingV) {
			drawList.AddLine(
				ToWindow(ImVec2(rectV.Min.x, i), &g_CurrentState),
				ToWindow(ImVec2(rectV.Max.x, i), &g_CurrentState),
				GetColorU32(ImGuiCol_Border, 0.5f)
				);
		}
	}
}

ImVec2 ToVirtual(const ImVec2& _windowPos)
{
	return ToVirtual(_windowPos, &VirtualWindow::g_CurrentState);
}
float ToVirtualX(float _windowPosX) 
{
	return ToVirtual(ImVec2(_windowPosX, 0.0f), &VirtualWindow::g_CurrentState).x;
}
float ToVirtualY(float _windowPosY) 
{
	return ToVirtual(ImVec2(0.0f, _windowPosY), &VirtualWindow::g_CurrentState).y;
}
ImVec2 ToWindow(const ImVec2& _virtualPos)
{
	return ToWindow(_virtualPos, &VirtualWindow::g_CurrentState);
}
float ToWindowX(float _virtualPosX)
{
	return ToWindow(ImVec2(_virtualPosX, 0.0f), &VirtualWindow::g_CurrentState).x;
}
float ToWindowY(float _virtualPosY)
{
	return ToWindow(ImVec2(0.0f, _virtualPosY), &VirtualWindow::g_CurrentState).y;
}

ImVec2 ToWindowScale(const ImVec2& _virtualScale)
{
	return ToWindowScale(_virtualScale, &VirtualWindow::g_CurrentState);
}
float ToWindowScaleX(float _virtualScaleX)
{
	return ToWindowScale(ImVec2(_virtualScaleX, 0.0f), &VirtualWindow::g_CurrentState).x;
}
float ToWindowScaleY(float _virtualScaleY)
{
	return ToWindowScale(ImVec2(0.0f, _virtualScaleY), &VirtualWindow::g_CurrentState).y;
}

ImVec2 ToVirtualScale(const ImVec2& _windowScale)
{
	return ToWindowScale(_windowScale, &VirtualWindow::g_CurrentState);
}
float ToVirtualScaleX(float _windowScaleX)
{
	return ToVirtualScale(ImVec2(_windowScaleX, 0.0f), &VirtualWindow::g_CurrentState).x;
}
float ToVirtualScaleY(float _windowScaleY)
{
	return ToVirtualScale(ImVec2(0.0f, _windowScaleY), &VirtualWindow::g_CurrentState).y;
}

} } // namespace ImGui::VirtualWindow
