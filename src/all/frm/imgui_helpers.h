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
// The code uses suffix 'V' to denote units in virtual space, 'W' for units in 
// window space (pixels).
//
// Typical usage:
//    static VirtualWindow s_virtualWindow;
//    APT_ONCE {
//       s_virtualWindow.m_sizeV = vec2(8.0f);
//    }
//
//    auto& io       = ImGui::GetIO();
//    auto& drawList = *ImGui::GetWindowDrawList();
//    vec2 zoom      = vec2(io.MouseWheel * -16.0f);
//    vec2 pan       = io.MouseDown[2] ? vec2(io.MouseDelta) : vec2(0.0f); 
//    s_virtualWindow.begin(zoom, pan);
//      // render to drawList here, use s_virtualWindow.virtualToWindow()
//    s_virtualWindow.end();
//
// m_basisV controls the orientation of V relative to W, e.g. to flip the axes or
// orient along a particular direction.
//
// Todo
// - Need to handle dragging to pan when mouse is outside of the window.
// - Fade in/out fine grid lines during zoom.
////////////////////////////////////////////////////////////////////////////////
struct VirtualWindow
{
public:
	enum Flag
	{
		Flag_Square      = 1 << 0,    // Force size to be square.
		Flag_GridX       = 1 << 1,    // Draw grid lines in X.
		Flag_GridY       = 1 << 2,    // Draw grid lines in Y.
		Flag_GridOrigin  = 1 << 3,    // Draw grid lines at origin.

		Flag_Default = Flag_Square | Flag_GridX | Flag_GridY | Flag_GridOrigin,
	};
	enum Color
	{
		Color_Background,
		Color_Border,
		Color_Grid,
		Color_GridOrigin,

		Color_Count
	};

	vec2  m_requestedSizeW         = vec2(0.0f);      // Requested window size, 0 to fill the available content region.

	vec2  m_sizeW                  = vec2(-1.0f);     // Actual window size (m_maxW - m_minW).
	vec2  m_minW                   = vec2(-1.0f);     // Window min.
	vec2  m_maxW                   = vec2(-1.0f);     // Window max.

	mat2  m_basisV                 = identity;        // Virtual space basis (orientation).
	vec2  m_originV                = vec2(0.0f);      // Virtual space origin.
	vec2  m_sizeV                  = vec2(1.0f);      // Virtual space size. Note that this is potentially smaller than (m_maxV - m_minV).
	vec2  m_minV                   = vec2(-0.5f);     // Min of window space rectangle in V.
	vec2  m_maxV                   = vec2(0.5f);      // Max of window space rectangle in V.

	mat3  m_virtualToWindow        = identity;        // Transform virtual -> window space.
	mat3  m_windowToVirtual        = identity;        // Transform window -> virtual space.

	vec2  m_minGridSpacingV        = vec2(1.0f);      // Minimum spacing between grid lines in V.
	vec2  m_minGridSpacingW        = vec2(16.0f);     // Minimum spacing between grid lines in W.
	vec2  m_gridSpacingBase        = vec2(10.0f);     // Major grid lines appear on multiples of m_gridSpacingBase.

	bool  m_isActive               = false;           // If the parent window is focused and the window region is hovered.
	int   m_flags                  = Flag_Default;
	ImU32 m_colors[Color_Count];



	VirtualWindow();
	~VirtualWindow();
	
	// Convert window space (pixels) to virtual space.
	vec2    windowToVirtual(const vec2& _posW) const        { return apt::TransformPosition(m_windowToVirtual, _posW); }
	// Convert virtual space to window space (pixels).
	vec2    virtualToWindow(const vec2& _posV) const        { return apt::Floor(apt::TransformPosition(m_virtualToWindow, _posV)); }
	
	// Handle zoom/pan to update the virtual region, begin rendering (push the ImGui clip rectangle).
	void    begin(const vec2& _zoom, const vec2& _pan);

	// Finish rendering (pops the ImGui clip rectangle).
	void    end();

	// Edit settings.
	void    edit();
	

	void    setFlag(Flag _flag, bool _value)                { m_flags = _value ? (m_flags | _flag) : (m_flags & ~_flag); }
	bool    getFlag(Flag _flag) const                       { return (m_flags & _flag) != 0; }
	
private:
	void updateRegionW();
	void updateRegionV(const vec2& _zoom, const vec2& _pan);
	void updateTransforms();
	void drawGrid();
	void editColor(Color _enum, const char* _name);
};


} // namespace frm
