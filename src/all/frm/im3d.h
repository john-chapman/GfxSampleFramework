#pragma once
#ifndef im3d_h
#define im3d_h

#include <frm/def.h>
#include <frm/math.h>

#include <imgui/imgui.h>

#include <vector>

namespace Im3d {

typedef frm::mat4   Mat4;
typedef frm::vec3   Vec3;
typedef frm::vec4   Vec4;
typedef frm::quat   Quat;
typedef frm::uint32 U32;

class Context;

struct Color
{
	U32 m_value;
	
	Color(): m_value(0) 
	{
	}
	
	Color(U32 _rgba): m_value(_rgba)
	{
	}

	Color(const Vec4& _rgba)
	{
		m_value  = (U32)(_rgba.a * 255.0f) << 24;
		m_value |= (U32)(_rgba.b * 255.0f) << 16;
		m_value |= (U32)(_rgba.g * 255.0f) << 8;
		m_value |= (U32)(_rgba.r * 255.0f);
	}

	Color(float _r, float _g, float _b, float _a = 1.0f)
	{
		m_value  = (U32)(_a * 255.0f) << 24;
		m_value |= (U32)(_b * 255.0f) << 16;
		m_value |= (U32)(_g * 255.0f) << 8;
		m_value |= (U32)(_r * 255.0f);
	}

	void scaleAlpha(float _alpha)
	{
		_alpha *= (float)(m_value >> 24) / 255.0f;
		U32 mask = ((1 << 8) - 1) << 24;
		m_value = (m_value & ~mask) | ((U32)(_alpha * 255.0f) << 24);
	}

	void set(int _i, float _val)
	{
		_i *= 8;
		U32 mask = ((1 << 8) - 1) << _i;
		m_value = (m_value & ~mask) | ((U32)(_val * 255.0f) << _i);
	}
	void setR(float _val) { set(0, _val); }
	void setG(float _val) { set(1, _val); }
	void setB(float _val) { set(2, _val); }
	void setA(float _val) { set(3, _val); }

	operator U32() const
	{
		return m_value;
	}
};
extern const Color kColorBlack;
extern const Color kColorWhite;
extern const Color kColorRed;
extern const Color kColorGreen;
extern const Color kColorBlue;

struct Vertex
{
	Vec4  m_positionSize; //< xyz = position, w = line/point size (in pixels)
	Color m_color;        //< rgba

	Vertex(Vec3 _position, float _width, Color _color)
		: m_positionSize(_position, _width)
		, m_color(_color)
	{
	}
};


/// Get/set current draw context.
Context& GetCurrentContext();
void     SetCurrentContext(Context& _ctx);

/// Begin primitive rendering. End() *must* be called before starting a new primitive.
void BeginPoints();
void BeginLines();
void BeginLineLoop();
void BeginLineStrip();

/// End primitive rendering.
void End();

/// Push a vertex using the current size and color.
void  Vertex(const Vec3& _position);
/// Push a vertex, override size.
void  Vertex(const Vec3& _position, float _size);
/// Push a vertex, override color.
void  Vertex(const Vec3& _position, Color _color);
/// Push a vertex, override size and color.
void  Vertex(const Vec3& _position, float _size, Color _color);

/// Current draw state (affects all subsequent vertices).
void  SetColor(Color _color);
Color GetColor();
void  SetAlpha(float _alpha);
float GetAlpha();
void  SetSize(float _size);
float GetSize();

/// Matrix stack (affects all subsequent vertices).
void  PushMatrix();
void  PopMatrix();
void  SetMatrix(const Mat4& _mat);
void  MulMatrix(const Mat4& _mat);
const Mat4& GetMatrix();
void  Translate(float _x, float _y, float _z);
void  Scale(float _x, float _y, float _z);

/// High order shapes.
/// \todo Filled* variants, at least for box/quad.
void DrawXyzAxes();
void DrawSphere(const Vec3& _origin, float _radius, int _detail = 24);
void DrawBox(const Vec3& _min, const Vec3& _max);
void DrawCylinder(const Vec3& _start, const Vec3& _end, float _radius, int _detail = 24);
void DrawCapsule(const Vec3& _start, const Vec3& _end, float _radius, int _detail = 12);

inline void Vertex(float _x, float _y, float _z)                    { Vertex(Vec3(_x, _y, _z)); }
inline void Vertex(float _x, float _y, float _z, Color _color)      { Vertex(Vec3(_x, _y, _z), _color); }
inline void Vertex(float _x, float _y, float _z, float _width)      { Vertex(Vec3(_x, _y, _z), _width); }
inline void SetColor(float _r, float _g, float _b, float _a = 1.0f) { SetColor(Color(_r, _g, _b, _a)); }


typedef U32 Id;
static const Id kInvalidId = ~0u;
Id MakeId(const char* _str);

/// Manipulate position/orientation/scale via a gizmo.
/// \return true if the gizmo is active.
bool Gizmo(const char* _id, Vec3* _position_, Quat* _orientation_, Vec3* _scale_);
bool PositionGizmo(const char* _id, Vec3* _position_);
bool OrientationGizmo(const char* _id, Quat* _orientation_);
bool ScaleGizmo(const char* _id, Vec3* _scale_);


////////////////////////////////////////////////////////////////////////////////
/// \class Context
/// Storage + helpers for primitive assembly. 
////////////////////////////////////////////////////////////////////////////////
class Context
{
public:
	enum Key
	{
		kMouseLeft,
		kMouseRight,
		kMouseMiddle,
		
		kKeyCtrl,
		kKeyShift,
		kKeyAlt,

		kKeyT,                   //< Select translation gizmo.
		kKeyR,                   //< Select rotation gizmo.
		kKeyS,                   //< Select scale gizmo.

		kKeyCount
	};
	static const int kKeyMax = 256;

	Vec3  m_cursorRayOriginW;    //< World-space cursor ray origin.
	Vec3  m_cursorRayDirectionW; //< World-space cursor ray direction.
	bool  m_keyDown[kKeyMax];    //< Client-provided key data.
	int   m_keyMap[kKeyCount];   //< Map Keys enum to m_keyDown array.
	float m_deltaTime;
	Vec3  m_viewOriginW;
	float m_tanHalfFov;



	enum PrimitiveMode
	{
		kNone,
		kPoints,
		kLines,
		kLineStrip,
		kLineLoop
	};

	Context();
	~Context();

	/// Matrix stack.
	void  pushMatrix()                   { m_matStack.push(m_matStack.top()); }
	void  popMatrix()                    { m_matStack.pop(); }
	void  setMatrix(const Mat4& _mat)    { m_matStack.top() = _mat; }
	void  mulMatrix(const Mat4& _mat)    { m_matStack.top() = m_matStack.top() * _mat; }
	const Mat4& getMatrix() const        { return m_matStack.top(); }

	/// Color stack.
	void  pushColor()                    { m_colorStack.push(m_colorStack.top()); }
	void  popColor()                     { m_colorStack.pop(); }
	void  setColor(Color _color)         { m_colorStack.top() = _color; }
	Color getColor() const               { return m_colorStack.top(); }

	/// Alpha stack (multiplies vertex alpha).
	void  pushAlpha()                    { m_alphaStack.push(m_alphaStack.top()); }
	void  popAlpha()                     { m_alphaStack.pop(); }
	void  setAlpha(float _alpha)         { m_alphaStack.top() = _alpha; }
	float getAlpha() const               { return m_alphaStack.top(); }

	/// Size stack.
	void  pushSize()                     { m_sizeStack.push(m_sizeStack.top()); }
	void  popSize()                      { m_sizeStack.pop(); }
	void  setSize(float _size)           { m_sizeStack.top() = _size; }
	float getSize() const                { return m_sizeStack.top(); }

	/// Primitive begin/end.
	void begin(PrimitiveMode _mode);
	void end();

	/// Push a vertex (must occur between begin()/end() calls).
	void vertex(const Vec3& _position, float _size, Color _color);

	/// Call per-frame to reset draw list, etc.
	/// \note Set the i/o state before calling this.
	void reset();

	const void* getPointData() const     { return m_points.data(); }
	unsigned getPointCount() const       { return (unsigned)m_points.size(); }

	const void* getLineData() const      { return m_lines.data(); }
	unsigned getLineCount() const        { return (unsigned)m_lines.size(); }

	bool gizmo(Id _id, Vec3* _position_, Quat* _orientation_, Vec3* _scale_);

private:
	static const int kMaxMatStackDepth   = 8;
	static const int kMaxStateStackDepth = 32;

	template <typename tType, int kMaxDepth>
	struct Stack
	{
		tType m_values[kMaxDepth];
		int   m_top;

		Stack(const tType& _default);
		
		void         push(const tType& _val);
		void         pop();
		void         clear()                 { m_top = 0; }
		const tType& top() const             { return m_values[m_top]; }
		tType&       top()                   { return m_values[m_top]; }
	};
	Stack<Mat4,  kMaxMatStackDepth>   m_matStack;
	Stack<Color, kMaxStateStackDepth> m_colorStack;
	Stack<float, kMaxStateStackDepth> m_alphaStack;
	Stack<float, kMaxStateStackDepth> m_sizeStack;

	PrimitiveMode m_primMode;
	unsigned m_firstVertThisPrim;        //< Index of the first vertex pushed during this primitive
	unsigned m_vertCountThisPrim;        //< # calls to vertex() since the last call to begin()
	std::vector<struct Vertex> m_points;
	std::vector<struct Vertex> m_lines;

	bool m_keyDownCurr[kKeyMax];
	bool m_keyDownPrev[kKeyMax];
	Id m_activeId;

	bool isKeyDown(Key _key)     { return m_keyDownCurr[m_keyMap[_key]]; }
	bool wasKeyPressed(Key _key) { int k = m_keyMap[_key]; return m_keyDownPrev[k] && !m_keyDownCurr[k]; }
	
/* Approach for IM gizmos:
	- Set IO information at the start of each frame (cursor ray, mouse/keyboard states).
	- Each call to the Gizmo() function:
		- Generate a gizmo ID (must be the same for each call to the function, hence require string)
		- If it's active, process the state and update the outputs. You only ever need to store internal
		  state for the currently active gizmo!
		- If not active, make active if necessary (i.e. if no currently active gizmo and intersects the
		  gizmo and clicking or whatever).
		- Push draw primitives!
*/

};

} // namespace Im3d

#endif // im3d_h
