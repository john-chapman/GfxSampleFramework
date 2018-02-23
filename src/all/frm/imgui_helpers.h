#pragma once

#include <frm/def.h>
#include <frm/math.h>

#include <imgui/imgui.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// VirtualWindow
// Provide mapping between a rectangular subregion of a virtual space and a 
// window space rectangle. Useful for 1D or 2D visualization with zoom/pan
// functionality.
//
// The code uses suffix 'V' to denote units in virtual space, 'W' for units in 
// window space (pixels).
//
// Typical usage:
//    static VirtualWindow s_virtualWindow(initialSizeV, initialOriginV);
//
//    auto& io       = ImGui::GetIO();
//    auto& drawList = *ImGui::GetWindowDrawList();
//    vec2 zoom      = vec2(io.MouseWheel * -16.0f);
//    vec2 pan       = io.MouseDown[2] ? vec2(io.MouseDelta) : vec2(0.0f); 
//    s_virtualWindow.begin(zoom, pan);
//      // render to drawList here, use s_virtualWindow.virtualToWindow()
//    s_virtualWindow.end();
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

		Flags_Default = Flag_Square | Flag_GridX | Flag_GridY | Flag_GridOrigin,
	};
	enum Color
	{
		Color_Background,
		Color_Border,
		Color_Grid,
		Color_GridOrigin,

		Color_Count
	};

	VirtualWindow(const vec2& _sizeV, const vec2& _originV = vec2(0.0f));
	VirtualWindow(float _sizeV, float _originV = 0.0f): VirtualWindow(vec2(_sizeV), vec2(_originV)) {}
	~VirtualWindow();

	// Convert window space (pixels) to virtual space.
	vec2        windowToVirtual(const vec2& _posW) const                 { return apt::TransformPosition(m_windowToVirtual, _posW); }
	// Convert virtual space to window space (pixels).
	vec2        virtualToWindow(const vec2& _posV) const                 { return apt::Floor(apt::TransformPosition(m_virtualToWindow, _posV)); }
		
	// Begin rendering (push the ImGui clip rectangle), handle zoom (_deltaSizeV) and pan (_deltaOriginV).
	void        begin(const vec2& _deltaSizeV, const vec2& _deltaOriginV);
	// End rendering (pop the ImGui clip rectangle).
	void        end();
	
	// If the parent window is focused and the window region is hovered.
	bool        isActive() const                                         { return m_isActive; }

	// Window size (pixels), 0 to fill the available content region of the current ImGui window.
	void        setSizeW(float _width, float _height = 0.0f)             { m_requestedSizeW = vec2(_width, _height); }
	const vec2& getSizeW() const                                         { return m_sizeW; }

	// Virtual subregion is ± sizeV*0.5, centerd on originV.
	void        setSizeV(float _width, float _height = 0.0f)             { m_sizeV = vec2(_width, _height); }
	const vec2& getSizeV() const                                         { return m_sizeV; }
	void        setOriginV(float _x, float _y)                           { m_originV = vec2(_x, _y); }
	const vec2& getOriginV() const                                       { return m_originV; }

	// Orientation of the virtual subregion relative to the window, by default positive values in V move down/right in W.
	// E.g. use setOrientationV(vec2(0,-1), vec2(-1,0)) to flip both axes.
	void        setOrientationV(const vec2& _down, const vec2& _right)   { m_basisV = mat2(_right, _down); }
	void        setOrientationV(const vec2& _down)                       { setOrientationV(_down, vec2(-_down.y, _down.x)); } // _right is perpendicular to _down
	const mat2& getOrientationV() const                                  { return m_basisV; }

	// Grid lines subdivide the virtual subregion, aligned on multiples of gridSpacingBase with a minimum spacing in both V and W.
	void        setMinGridSpacingW(float _x, float _y = 0.0f)            { m_minGridSpacingW = vec2(_x, _y == 0.0f ? _x : _y); }
	void        setMinGridSpacingV(float _x, float _y = 0.0f)            { m_minGridSpacingV = vec2(_x, _y == 0.0f ? _x : _y); }
	void        setGridSpacingBase(float _x, float _y = 0.0f)            { m_gridSpacingBase = vec2(_x, _y == 0.0f ? _x : _y); }

	void        setFlags(int _flags)                                     { m_flags = _flags; }
	void        setFlag(Flag _flag, bool _value)                         { m_flags = _value ? (m_flags | _flag) : (m_flags & ~_flag); }
	bool        getFlag(Flag _flag) const                                { return (m_flags & _flag) != 0; }
	
	void        setColor(Color _color, ImU32 _value)                     { m_colors[_color] = _value; }
	ImU32       getColor(Color _color) const                             { return m_colors[_color]; }


	// Edit settings.
	void        edit();

private:
	vec2  m_requestedSizeW         = vec2(0.0f);      // Requested window size, 0 to fill the available content region.
	vec2  m_sizeW                  = vec2(-1.0f);     // Actual window size (m_maxW - m_minW).
	vec2  m_minW                   = vec2(-1.0f);
	vec2  m_maxW                   = vec2(-1.0f);

	vec2  m_sizeV                  = vec2(1.0f);
	vec2  m_originV                = vec2(0.0f);
	mat2  m_basisV                 = identity;
	vec2  m_minV                   = vec2(-0.5f);     // Min of window space rectangle in V.
	vec2  m_maxV                   = vec2(0.5f);      // Max of window space rectangle in V.

	mat3  m_virtualToWindow        = identity;        // Transform virtual -> window space.
	mat3  m_windowToVirtual        = identity;        // Transform window -> virtual space.

	vec2  m_minGridSpacingW        = vec2(16.0f);     // Minimum spacing between grid lines in W.
	vec2  m_minGridSpacingV        = vec2(1.0f);      // Minimum spacing between grid lines in V.
	vec2  m_gridSpacingBase        = vec2(10.0f);     // Major grid lines appear on multiples of m_gridSpacingBase.

	bool  m_isActive               = false;           // If the parent window is focused and the window region is hovered.
	int   m_flags                  = Flags_Default;
	ImU32 m_colors[Color_Count];



	void updateRegionW();
	void updateRegionV(const vec2& _zoom, const vec2& _pan);
	void updateTransforms();
	void drawGrid();
	void editColor(Color _enum, const char* _name);
};


} // namespace frm
