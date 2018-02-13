#pragma once

#include <frm/def.h>
#include <frm/math.h>

#include <imgui/imgui.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// VirtualWindow
// Provide mapping between a rectangular region of a unitless virtual space and 
// a window space rectangle. Useful for 1D or 2D visualization with zoom/pan
// functionality.
//
// Typical usage:
//    static ImGui::VirtualWindow s_virtualWindow;
//    APT_ONCE s_virtualWindow.init(Flags_GridX | Flags_GridY);
//    auto& io  = ImGui::GetIO();
//    vec2 zoom = vec2(io.MouseWheel * -16.0f);
//    vec2 pan  = io.MouseDown[2] ? vec2(io.MouseDelta) : vec2(0.0f); 
//    s_virtualWindow.begin(zoom, pan);
//      // render to the draw list here, use s_virtualWindow.virtualToWindow()
//    s_virtualWindow.end();
//
// Todo:
// - Need to handle dragging to pan when mouse is outside of the window.
// - Fade in/out fine grid lines during zoom.
////////////////////////////////////////////////////////////////////////////////
struct VirtualWindow
{
	enum Flags
	{
		Flags_Square = 1 << 0,    // Force size to be square.
		Flags_GridX  = 1 << 1,    // Draw grid lines in X.
		Flags_GridY  = 1 << 2,    // Draw grid lines in Y.

		Flags_Default = Flags_Square | Flags_GridX | Flags_GridY
	};

	vec2 m_size                   = vec2(0.0f);      // Requested size, 0 to fill the available content region.

	vec2 m_minW                   = vec2(-1.0f);     // Window space region min.
	vec2 m_maxW                   = vec2(-1.0f);     // Window space region max.
	vec2 m_sizeW                  = vec2(-1.0f);     // Window space size (m_maxW - m_minW).

	vec2 m_minV                   = vec2(-1.5f);     // Virtual space region min.
	vec2 m_maxV                   = vec2( 1.5f);     // Virtual space region max.
	vec2 m_sizeV                  = vec2( 3.0f);     // Virtual space size (m_maxV - m_minV).
	
	vec2 m_minGridSpacingV        = vec2(0.1f);      // Minimum spacing of grid lines.
	vec2 m_minGridSpacingW        = vec2(16.0f);     // Minimum spacing of grid lines.
	vec2 m_gridSpacingBase        = vec2(10.0f);     // Major grid lines appear

	bool m_isActive               = false;           // If the parent window is focussed and the window region is hovered.

	int  m_flags                  = Flags_Default;   // Can override via begin().

	ImU32 m_colorBackground       = IM_COL32_BLACK;
	ImU32 m_colorBorder           = IM_COL32_WHITE;
	ImU32 m_colorGrid             = IM_COL32_WHITE;

	bool checkFlag(Flags _flag) const      { return (m_flags & _flag) != 0; }
	void setFlag(Flags _flag, bool _value) { m_flags = _value ? (m_flags | _flag) : (m_flags & ~_flag); }


	// Convert window space (pixels) to virtual space.
	vec2 windowToVirtual(const vec2& _posW);

	// Convert virtual space to window space (pixels).
	vec2 virtualToWindow(const vec2& _posV);
	
	// Call once to init internal state.
	void init(int _flags = Flags_Default);

	// Handle zoom/pan to update the virtual region, begin rendering (push the ImGui clip rectangle).
	void begin(const vec2& _zoom, const vec2& _pan, int _flags = Flags_Default);

	// Finish rendering (pops the ImGui clip rectangle).
	void end();

	// Modify settings.
	void edit();
};


} // namespace frm
