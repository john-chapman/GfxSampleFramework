#pragma once

#include <frm/def.h>
#include <frm/math.h>

#include <imgui/imgui.h>

namespace frm {

// Point-circle test.
inline bool IsInside(const vec2& _point, const vec2& _origin, float _radius)
{
	return apt::Length2(_point - _origin) < (_radius * _radius);
}
// Point-rectangle test.
inline bool IsInside(const vec2& _point, const vec2& _min, const vec2& _max)
{
	return _point.x > _min.x && _point.x < _max.x && _point.y > _min.y && _point.y < _max.y;
}


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
class VirtualWindow
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
	float       windowToVirtual(float _posWX) const                      { return windowToVirtual(vec2(_posWX, 0.0f)).x; }
	// Convert virtual space to window space (pixels).
	vec2        virtualToWindow(const vec2& _posV) const                 { return apt::Floor(apt::TransformPosition(m_virtualToWindow, _posV)); }
	float       virtualToWindow(float _posVX) const                      { return virtualToWindow(vec2(_posVX, 0.0f)).x; }
		
	// Begin rendering (push the ImGui clip rectangle), handle zoom (_deltaSizeW) and pan (_deltaOriginW).
	// _anchorW is a window space position constrained to remain fixed during zoom, by default this is the mouse position.
	void        begin(const vec2& _deltaSizeW, const vec2& _deltaOriginW, const vec2& _anchorW = vec2(-1.0f));
	// End rendering (pop the ImGui clip rectangle).
	void        end();
	
	// If the parent window is focused and the window region is hovered.
	bool        isActive() const                                         { return m_isActive; }

	// Window size (pixels), 0 to fill the available content region of the current ImGui window.
	void        setSizeW(float _width, float _height = -1.0f)            { m_requestedSizeW = vec2(_width, _height); }
	const vec2& getSizeW() const                                         { return m_sizeW; }

	// Window min/max.
	const vec2& getMinW() const                                          { return m_minW; }
	const vec2& getMaxW() const                                          { return m_maxW; }

	// Virtual subregion is ± sizeV*0.5, centerd on originV.
	void        setSizeV(float _width, float _height = -1.0f)            { m_sizeV = vec2(_width, _height); }
	const vec2& getSizeV() const                                         { return m_sizeV; }
	void        setOriginV(float _x, float _y = 0.0f)                    { m_originV = vec2(_x, _y); }
	const vec2& getOriginV() const                                       { return m_originV; }
	void        setRegionV(const vec2& _min, const vec2& _max)           { m_sizeV = _max - _min; m_originV = _min + m_sizeV * 0.5f; }

	// Virtual subregion min/max.
	const vec2  getMinV() const                                          { return m_minV; }
	const vec2  getMaxV() const                                          { return m_maxV; }
	
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
	vec2  m_requestedSizeW         = vec2(-1.0f);     // Requested window size, -1 to fill the available content region.
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
	void updateRegionV(const vec2& _deltaSizeW, const vec2& _deltaOriginW, const vec2& _anchorW);
	void updateTransforms();
	void drawGrid();
	void editColor(Color _enum, const char* _name);
};


////////////////////////////////////////////////////////////////////////////////
// GradientEditor
////////////////////////////////////////////////////////////////////////////////
class GradientEditor
{
public:
	enum Flag
	{
		Flag_ZoomPan     = 1 << 0,    // Enable zoom/pan (via mouse wheel).
		Flag_Alpha       = 1 << 1,    // Enable alpha.
		Flag_Sampler     = 1 << 2,    // Display the sample point at _t (arg to drawEdit).

		Flags_Default    = Flag_Alpha | Flag_Sampler
	};

	int           m_flags             = Flags_Default;
	int           m_selectedKeyRGB    = -1;//Curve::kInvalidIndex;
	int           m_selectedKeyA      = -1;//Curve::kInvalidIndex;
	int           m_dragKey           = -1;//Curve::kInvalidIndex;
	int           m_dragComponent     = -1;
	float         m_dragOffset        = 0.0f;
	bool          m_keyBarRGBHovered;
	Curve*        m_curves[4]         = {}; // RGBA
	VirtualWindow m_virtualWindow;
	
	static GradientEditor* s_current; // tracke which GradientEditor has focus when >1 are visible

	GradientEditor(int _flags = Flags_Default);
	~GradientEditor() {}

	void        setCurves(CurveGradient& _curveGradient);
	void        setFlags(int _flags)                                     { m_flags = _flags; }
	void        setFlag(Flag _flag, bool _value)                         { m_flags = _value ? (m_flags | _flag) : (m_flags & ~_flag); }
	bool        getFlag(Flag _flag) const                                { return (m_flags & _flag) != 0; }
	
	bool        drawEdit(const vec2& _sizePixels = vec2(-1.0f, 64.0f), float _t = -1.0f, int _flags = 0);

	void        reset();

	// Edit settings.
	void        edit();



	void        drawGradient();
	void        drawKeysRGB();

	bool        editKeysRGB();

	// Move the currently selected key to _newX, return the new index.
	int         moveSelectedKeyRGB(float _newX, int _keyIndex, int _keyComponent);

	// Convert between window <-> gradient space.
	float       windowToGradient(float _x)                              { return m_virtualWindow.windowToVirtual(_x); }
	float       gradientToWindow(float _x)                              { return m_virtualWindow.virtualToWindow(_x); }

};

} // namespace frm
