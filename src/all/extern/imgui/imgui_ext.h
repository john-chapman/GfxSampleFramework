#pragma once
#include "imgui.h"

#define IMGUI_EXT_VERSION  "0.0"

/*	Overall design rationale:
	- Using namespaces to scope individual widgets rather than declaring structs/classes. This fits better with the immediate mode
	  paradigm (non-instantiable structs/classes feels like an abuse). Private static data is in the cpp file.
	- Deviated slightly from the ImGui enum style by scoping enums inside namespaces.
	- 'Immediate Mode' means that applications should never be forced to manage state, the API should be functions only.

	Widget design overview:
	- Begin()/End() pairs for complex widgets.
	- Optional sub-widgets (e.g. ruler bars for the virtual window) should be functions which the client code calls between the
	  Begin()/End pair.
	- Internally, widgets should be designed around user actions, e.g. OnLeftClick(), OnMouseDown(), etc. This makes the code 
	  easier to manage albeit at the cost of some redundancy.

	\todo
	- Color class. Store as ImU32, implicit conversion to/from ImVec4. Static helpers e.g. Invert(), Lighten(fraction), Darken(fraction).
	- Tabs API. Single function, creates a row of buttons + separator, returns the current state.
	- Table API. Begin(rows, cols, flags), NextCell(), NextRow(), End(). Track row height, column size for auto resize. Force row
	  height/column size. Accessors for the current cell size (if e.g. user wants to add a child frame).
*/

namespace ImGui {

// Return true if _p is inside the rectangle defined by _rectMin, _rectMax.
inline bool IsInside(const ImVec2& _p, const ImVec2& _rectMin, const ImVec2& _rectMax)
{
	return _p.x >= _rectMin.x && _p.x <= _rectMax.x && _p.y >= _rectMin.y && _p.y <= _rectMax.y;
}

inline ImU32 ColorInvertRGB(ImU32 _rgba)
{
	return (~_rgba & 0x00ffffff) | (_rgba & 0xff000000);
}

// VirtualWindow
// Child frame as a window onto a rectangular subregion of a virtual space. Useful for 1D or 2D visualization with pan/zoom functionality.
// \todo
// - Pan/zoom beyond the region rect when scrollbars are enabled, animate back to the scroll position when using a scroll bar. This requires
//   custom scroll bars, also fix flickering of vertical scrollbar on zoom.
// - Optionally constrain zoom/pan to region rect.
// - Cache reciprocal rect sizes for faster window <-> virtual conversion?
// - Smoothly fade minor grid lines in/out.
// - Draw ruler bars.
namespace VirtualWindow 
{

enum Flags_
{
	Flags_Square         = 1 << 0,  // Force square dimensions.
 	Flags_PanX           = 1 << 1,  // Enable pan (mouse middle + drag).
	Flags_PanY           = 1 << 2,  //             "
	Flags_ZoomX          = 1 << 3,  // Enable zoom (mouse wheel).
	Flags_ZoomY          = 1 << 4,  //             "
	Flags_ScrollBarX     = 1 << 5,  // Enable scroll bar.
	Flags_ScrollBarY     = 1 << 6,  //             "
	
	Flags_Pan            = Flags_PanX | Flags_PanY,
	Flags_Zoom           = Flags_ZoomX | Flags_ZoomY,
	Flags_PanZoom        = Flags_Pan | Flags_Zoom,
	Flags_ScrollBars     = Flags_ScrollBarX | Flags_ScrollBarY,
	
	Flags_Default        = Flags_PanZoom
};
typedef int Flags;

// Begin/end a virtual window.
bool   Begin(ImGuiID _id, const ImVec2& _size = ImVec2(-1, -1), Flags _flags = Flags_Default);
void   End(); // only call if VirtualWindowBegin() returns true

// Call prior to VirtualWindowBegin to set the virtual region rect.
void   SetNextRegion(const ImVec2& _rectMin, const ImVec2& _rectMax, ImGuiCond _cond = 0);
// Set the virtual region extents. Default is [-FLT_MAX,FLT_MAX].
void   SetNextRegionExtents(const ImVec2& _rectMin, const ImVec2& _rectMax, ImGuiCond _cond = 0);

// Set virtual region rect for the current virtual window (takes effect next frame);
void   SetRegion(const ImVec2& _rectMin, const ImVec2& _rectMax);

// Draw a grid with minimum spacing both window and virtual space, with grid lines aligned on multiples of _alignBase.
// Pass 0 to _windowSpacingMin to disable either dimension.
void   Grid(const ImVec2& _windowSpacingMin, const ImVec2& _virtualSpacingMin, const ImVec2& _alignBase = ImVec2(10, 10));

// Convert window <-> virtual space.
ImVec2 ToWindow(const ImVec2& _virtualPos);
float  ToWindowX(float _virtualPosX);
float  ToWindowY(float _virtualPosY);
ImVec2 ToVirtual(const ImVec2& _windowPos);
float  ToVirtualX(float _windowPosX);
float  ToVirtualY(float _windowPosY);

} // namespace VirtualWindow

} // namespace ImGui
