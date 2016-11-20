#include <frm/im3d.h>

#include <frm/def.h>
#include <frm/math.h>
#include <frm/geom.h>

#include <cstring>

	// \todo tmp debug stuff, remove
	#include <apt/log.h>
	#include <frm/Input.h>

#define Im3dAssert(_x) APT_ASSERT(_x)

using namespace Im3d;

const Im3d::Color Im3d::kColorBlack   = Im3d::Color(0.0f, 0.0f, 0.0f);
const Im3d::Color Im3d::kColorWhite   = Im3d::Color(1.0f, 1.0f, 1.0f);
const Im3d::Color Im3d::kColorRed     = Im3d::Color(1.0f, 0.0f, 0.0f);
const Im3d::Color Im3d::kColorGreen   = Im3d::Color(0.0f, 1.0f, 0.0f);
const Im3d::Color Im3d::kColorBlue    = Im3d::Color(0.0f, 0.0f, 1.0f);

static Context  kDefaultContext;
static Context* g_currentContext = &kDefaultContext; 

void Im3d::SetCurrentContext(Context& _ctx)
{
	g_currentContext = &_ctx;
}

Context& Im3d::GetCurrentContext()
{
	return *g_currentContext;
}

void  Im3d::BeginPoints()                { GetCurrentContext().begin(Context::kPoints); }
void  Im3d::BeginLines()                 { GetCurrentContext().begin(Context::kLines); }
void  Im3d::BeginLineStrip()             { GetCurrentContext().begin(Context::kLineStrip); }
void  Im3d::BeginLineLoop()              { GetCurrentContext().begin(Context::kLineLoop); }
void  Im3d::End()                        { GetCurrentContext().end(); }
void  Im3d::SetColor(Color _color)       { GetCurrentContext().setColor(_color); }
Color Im3d::GetColor()                   { return GetCurrentContext().getColor(); }
void  SetAlpha(float _alpha)             { GetCurrentContext().setAlpha(_alpha); }
float GetAlpha()                         { return GetCurrentContext().getAlpha(); }
void  Im3d::SetSize(float _width)        { GetCurrentContext().setSize(_width); }
float Im3d::GetSize()                    { return GetCurrentContext().getSize(); }
void  Im3d::PushMatrix()                 { GetCurrentContext().pushMatrix(); }
void  Im3d::PopMatrix()                  { GetCurrentContext().popMatrix(); }
void  Im3d::SetMatrix(const Mat4& _mat)  { GetCurrentContext().setMatrix(_mat); }
const Mat4& Im3d::GetMatrix()            { return GetCurrentContext().getMatrix(); }
void  Im3d::MulMatrix(const Mat4& _mat)  { GetCurrentContext().mulMatrix(_mat); }

void Im3d::Translate(float _x, float _y, float _z)
{
 // \todo just inject the translation directly into the matrix stack
	GetCurrentContext().mulMatrix(glm::translate(glm::mat4(1.0f), glm::vec3(_x, _y, _z)));
}

void  Im3d::Scale(float _x, float _y, float _z)
{
	GetCurrentContext().mulMatrix(glm::scale(glm::mat4(1.0f), glm::vec3(_x, _y, _z)));
}

void Im3d::Vertex(const Vec3& _position)
{
	Context& ctx = GetCurrentContext();
	Vec3 pos = Vec3(ctx.getMatrix() * Vec4(_position, 1.0f));
	ctx.vertex(Vec3(pos), ctx.getSize(), ctx.getColor());
}
void Im3d::Vertex(const Vec3& _position, float _width)
{
	Context& ctx = GetCurrentContext();
	Vec3 pos = Vec3(ctx.getMatrix() * Vec4(_position, 1.0f));
	ctx.vertex(pos, _width, ctx.getColor());
}
void Im3d::Vertex(const Vec3& _position, Color _color)
{
	Context& ctx = GetCurrentContext();
	Vec3 pos = Vec3(ctx.getMatrix() * Vec4(_position, 1.0f));
	ctx.vertex(pos, ctx.getSize(), _color);
}
void Im3d::Vertex(const Vec3& _position, float _size, Color _color)
{
	Context& ctx = GetCurrentContext();
	Vec3 pos = Vec3(ctx.getMatrix() * Vec4(_position, 1.0f));
	ctx.vertex(pos, _size, _color);
}

void Im3d::DrawXyzAxes()
{
	BeginLines();
		SetColor(1.0f, 0.0f, 0.0f);
		Vertex(0.0f, 0.0f, 0.0f);
		Vertex(1.0f, 0.0f, 0.0f);

		SetColor(0.0f, 1.0f, 0.0f);
		Vertex(0.0f, 0.0f, 0.0f);
		Vertex(0.0f, 1.0f, 0.0f);

		SetColor(0.0f, 0.0f, 1.0f);
		Vertex(0.0f, 0.0f, 0.0f);
		Vertex(0.0f, 0.0f, -1.0f);
	End();
}

void Im3d::DrawSphere(const Vec3& _origin, float _radius, int _detail)
{
 // xy circle
	BeginLineLoop();
		for (int i = 0; i < _detail; ++i) {
			float rad = glm::two_pi<float>() * ((float)i / (float)_detail);
			Vertex(cosf(rad) * _radius + _origin.x, sinf(rad) * _radius + _origin.y, 0.0f + _origin.z);
		}
	End();

 // xz circle
	BeginLineLoop();
		for (int i = 0; i < _detail; ++i) {
			float rad = glm::two_pi<float>() * ((float)i / (float)_detail);
			Vertex(cosf(rad) * _radius + _origin.x, 0.0f + _origin.y, sinf(rad) * _radius + _origin.z);
		}
	End();

 // yz circle
	BeginLineLoop();
		for (int i = 0; i < _detail; ++i) {
			float rad = glm::two_pi<float>() * ((float)i / (float)_detail);
			Vertex(0.0f + _origin.x, cosf(rad) * _radius + _origin.y, sinf(rad) * _radius + _origin.z);
		}
	End();
}

void Im3d::DrawBox(const Vec3& _min, const Vec3& _max)
{
	BeginLines();
		Vertex(_min[0], _min[1], _min[2]); Vertex(_max[0], _min[1], _min[2]);
		Vertex(_min[0], _max[1], _min[2]); Vertex(_max[0], _max[1], _min[2]);
		Vertex(_min[0], _min[1], _max[2]); Vertex(_max[0], _min[1], _max[2]);
		Vertex(_min[0], _max[1], _max[2]); Vertex(_max[0], _max[1], _max[2]);

		Vertex(_min[0], _min[1], _min[2]); Vertex(_min[0], _max[1], _min[2]);
		Vertex(_max[0], _min[1], _min[2]); Vertex(_max[0], _max[1], _min[2]);
		Vertex(_min[0], _min[1], _max[2]); Vertex(_min[0], _max[1], _max[2]);
		Vertex(_max[0], _min[1], _max[2]); Vertex(_max[0], _max[1], _max[2]);

		Vertex(_min[0], _min[1], _min[2]); Vertex(_min[0], _min[1], _max[2]);
		Vertex(_max[0], _min[1], _min[2]); Vertex(_max[0], _min[1], _max[2]);
		Vertex(_min[0], _max[1], _min[2]); Vertex(_min[0], _max[1], _max[2]);
		Vertex(_max[0], _max[1], _min[2]); Vertex(_max[0], _max[1], _max[2]);
	End();
}

void Im3d::DrawCylinder(const Vec3& _start, const Vec3& _end, float _radius, int _detail)
{
	Vec3 org = _start + (_end - _start) * 0.5f;
	Mat4 cmat = frm::LookAt(org, _end);
	float ln = glm::length(_end - _start) * 0.5f;

	PushMatrix();
	MulMatrix(cmat);

	BeginLineLoop();
		for (int i = 0; i <= _detail; ++i) {
			float rad = glm::two_pi<float>() * ((float)i / (float)_detail) - glm::half_pi<float>();
			Vec3 v = Vec3(0.0f, 0.0f, -ln) + Vec3(cosf(rad), sinf(rad), 0.0f) * _radius;
			Vertex(v);
		}
		for (int i = 0; i <= _detail; ++i) {
			float rad = glm::two_pi<float>() * ((float)i / (float)_detail) - glm::half_pi<float>();
			Vec3 v = Vec3(0.0f, 0.0f, ln) + Vec3(cosf(rad), sinf(rad), 0.0f) * _radius;
			Vertex(v);
		}
	End();

	BeginLines();
		Vertex(-_radius, 0.0f, -ln);
		Vertex(-_radius, 0.0f,  ln);
		Vertex( _radius, 0.0f, -ln);
		Vertex( _radius, 0.0f,  ln);
		Vertex(0.0f, _radius, -ln);
		Vertex(0.0f, _radius,  ln);
	End();

	PopMatrix();
}

void Im3d::DrawCapsule(const Vec3& _start, const Vec3& _end, float _radius, int _detail)
{
	Vec3 org = _start + (_end - _start) * 0.5f;
	Mat4 cmat = frm::LookAt(org, _end);
	float ln = glm::length(_end - _start) * 0.5f;
	int detail2 = _detail * 2; // force cap base detail to match ends

	PushMatrix();
	MulMatrix(cmat);

 // yz silhoette + cap bases
	BeginLineLoop();
		for (int i = 0; i <= detail2; ++i) {
			float rad = glm::two_pi<float>() * ((float)i / (float)detail2) - glm::half_pi<float>();
			Vec3 v = Vec3(0.0f, 0.0f, -ln) + Vec3(cosf(rad), sinf(rad), 0.0f) * _radius;
			Vertex(v);
		}
		for (int i = 0; i < _detail; ++i) {
			float rad = glm::pi<float>() * ((float)i / (float)_detail) + glm::pi<float>();
			Vec3 v = Vec3(0.0f, 0.0f, -ln) + Vec3(0.0f, cosf(rad), sinf(rad)) * _radius;
			Vertex(v);
		}
		for (int i = 0; i < _detail; ++i) {
			float rad = glm::pi<float>() * ((float)i / (float)_detail);
			Vec3 v = Vec3(0.0f, 0.0f, ln) + Vec3(0.0f, cosf(rad), sinf(rad)) * _radius;
			Vertex(v);
		}
		for (int i = 0; i <= detail2; ++i) {
			float rad = glm::two_pi<float>() * ((float)i / (float)detail2) - glm::half_pi<float>();
			Vec3 v = Vec3(0.0f, 0.0f, ln) + Vec3(cosf(rad), sinf(rad), 0.0f) * _radius;
			Vertex(v);
		}
	End();

 // xz silhoette
	BeginLineLoop();
		for (int i = 0; i < _detail; ++i) {
			float rad = glm::pi<float>() * ((float)i / (float)_detail) + glm::pi<float>();
			Vec3 v = Vec3(0.0f, 0.0f, -ln) + Vec3(cosf(rad), 0.0f, sinf(rad)) * _radius;
			Vertex(v);
		}
		for (int i = 0; i < _detail; ++i) {
			float rad = glm::pi<float>() * ((float)i / (float)_detail);
			Vec3 v = Vec3(0.0f, 0.0f, ln) + Vec3(cosf(rad), 0.0f, sinf(rad)) * _radius;
			Vertex(v);
		}
	End();

	PopMatrix();
}

void Im3d::DrawArrow(const Vec3& _start, const Vec3& _end, float _headLength)
{
	Vec3 head = _start + (_end - _start) * _headLength;
	Vec3 headStart = _end - head;
	BeginLines();
		Vertex(_start);
		Vertex(headStart);

		Vertex(headStart, GetCurrentContext().getSize() * 2.0f);
		Vertex(_end, 2.0f); // can't be 0 as the AA fades to 0
	End();
}

Im3d::Id Im3d::MakeId(const char* _str)
{
	static const U32 kFnv1aPrime32 = 0x01000193u;

	APT_ASSERT(_str);
	U32 ret = (U32)GetCurrentContext().getId(); // i.e. top of Id stack
	while (*_str) {
		ret ^= (U32)*_str++;
		ret *= kFnv1aPrime32;
	}
	return (Id)ret;
}

bool Im3d::Gizmo(const char* _id, Vec3* _position_, Quat* _orientation_, Vec3* _scale_)
{
	Id id = MakeId(_id);
	return GetCurrentContext().gizmo(id, _position_, _orientation_, _scale_);

}

/*******************************************************************************

                                   Context

*******************************************************************************/

// PUBLIC

Context::Context()
	: m_matStack(Mat4(1.0f))
	, m_colorStack(Color(1.0f, 1.0f, 1.0f))
	, m_alphaStack(1.0f)
	, m_sizeStack(1.0f)
	, m_primMode(kNone)
	, m_firstVertThisPrim(0)
	, m_vertCountThisPrim(0)
	, m_idStack(0x811C9DC5u) // fnv1 32 bit hash base
	, m_hotId(kInvalidId)
	, m_activeId(kInvalidId)
{
}

Context::~Context()
{
}

void Context::begin(PrimitiveMode _mode)
{
	Im3dAssert(m_primMode == kNone);
	m_primMode = _mode;
	m_vertCountThisPrim = 0;
	switch (m_primMode) {
	case kPoints:
		m_firstVertThisPrim = (unsigned)m_points.size();
		break;
	case kLines:
	case kLineStrip:
	case kLineLoop:
		m_firstVertThisPrim = (unsigned)m_lines.size();
		break;
	default:
		break;
	};
}

void Context::end()
{
	Im3dAssert(m_primMode != kNone);
	switch (m_primMode) {
	case kPoints:
		break;

	case kLines:
		Im3dAssert(m_vertCountThisPrim % 2 == 0);
		break;
	case kLineStrip:
		Im3dAssert(m_vertCountThisPrim > 1);
		break;
	case kLineLoop:
		Im3dAssert(m_vertCountThisPrim > 1);
		m_lines.push_back(m_lines.back());
		m_lines.push_back(m_lines[m_firstVertThisPrim]);
		break;
	
	default:
		break;
	};

	m_primMode = kNone;
}

void Context::vertex(const Vec3& _position, float _width, Color _color)
{
	Im3dAssert(m_primMode != kNone);

	_color.scaleAlpha(getAlpha());
	switch (m_primMode) {
	case kPoints:
		m_points.push_back(struct Vertex(_position, _width, _color));
		break;
	case kLines:
		m_lines.push_back(struct Vertex(_position, _width, _color));
		break;
	case kLineStrip:
	case kLineLoop:
		if (m_vertCountThisPrim >= 2) {
			m_lines.push_back(m_lines.back());
			++m_vertCountThisPrim;
		}
		m_lines.push_back(struct Vertex(_position, _width, _color));
		break;
	default:
		break;
	};
	++m_vertCountThisPrim;
}

void Context::reset()
{
	Im3dAssert(m_primMode == kNone);
	m_points.clear();
	m_lines.clear();

 // copy keydown array internally so that we can make a delta to detect key presses
	memcpy(m_keyDownPrev, m_keyDownCurr, sizeof(m_keyDown)); // \todo avoid this copy, use an index
	memcpy(m_keyDownCurr, m_keyDown,     sizeof(m_keyDown));
}

bool Context::gizmo(Id _id, Vec3* _position_, Quat* _orientation_, Vec3* _scale_, float _screenSize)
{
	frm::Ray cursorRay(m_cursorRayOriginW, m_cursorRayDirectionW);
	 
 // maintain screen size 
	float d = glm::length(*_position_ - m_viewOriginW);
	float screenScale = 2.0f / (2.0f * glm::atan(0.5f / d)) * m_tanHalfFov * _screenSize;
	screenScale /= m_displaySize.y;

	
	PushMatrix();
	// \todo probably don't want to scale the gizmo
		Mat4 wm = glm::scale(glm::translate(glm::mat4(1.0f), *_position_) * glm::mat4_cast(*_orientation_), /**_scale_ **/ Vec3(screenScale));
		MulMatrix(wm);
	

		SetSize(4.0f);
		bool ret = false;
		pushId();
			setId(_id);
			ret = axisGizmoW(MakeId("xaxis"), _position_, Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), _position_->y, kColorRed,   screenScale);
			ret = axisGizmoW(MakeId("yaxis"), _position_, Vec3(0.0f, 1.0f, 0.0f), m_cursorRayDirectionW,  length(*_position_ - m_cursorRayOriginW), kColorGreen, screenScale);
			ret = axisGizmoW(MakeId("zaxis"), _position_, Vec3(0.0f, 0.0f, 1.0f), Vec3(0.0f, 1.0f, 0.0f), _position_->y, kColorBlue,  screenScale);
		popId();

	PopMatrix();

	return ret;
}

// PRIVATE

template <typename tType, int kMaxDepth>
Context::Stack<tType, kMaxDepth>::Stack(const tType& _default)
	: m_top(0)
{
	m_values[0] = _default;
}

template <typename tType, int kMaxDepth>
void Context::Stack<tType, kMaxDepth>::push(const tType& _val)
{
	Im3dAssert(m_top < kMaxDepth);
	m_values[++m_top] = _val;
}

template <typename tType, int kMaxDepth>
void Context::Stack<tType, kMaxDepth>::pop()
{
	Im3dAssert(m_top != 0);
	--m_top;
}

template struct Context::Stack<Mat4,  Context::kMaxMatStackDepth>;
template struct Context::Stack<Color, Context::kMaxStateStackDepth>;
template struct Context::Stack<float, Context::kMaxStateStackDepth>;


bool Context::axisGizmoW(
	Id           _id,
	Vec3*        _position_, 
	const Vec3&  _axis, 
	const Vec3&  _planeNormal,
	float        _planeOffset,
	const Color& _color, 
	float        _screenScale
	)
{
	frm::Capsule cp(*_position_, *_position_ + _axis * _screenScale, 0.05f * _screenScale);
	frm::Plane pl(_planeNormal, _planeOffset);
	frm::Ray cursorRay(m_cursorRayOriginW, m_cursorRayDirectionW);

	bool ret = false;
	pushAlpha();
	setAlpha(0.5f);
	setColor(_color);
	if (_id == m_activeId) {
		setAlpha(1.0f);
		if (isKeyDown(kMouseLeft)) {
		 // active, move _position_
			float t;
			if (frm::Intersect(cursorRay, pl, t)) {
				Vec3 displacement = cursorRay.m_origin + cursorRay.m_direction * t + m_translationOffset;
				displacement = (displacement - *_position_) * _axis; // constrain movement to axis
				*_position_ += displacement;
			}
			
		 // draw the axis
			pushSize();
				setSize(1.0f);
				setAlpha(0.5f);
				BeginLines();
					Vertex(-_axis * 9999.0f);
					Vertex( _axis * 9999.0f);
				End();
				setAlpha(1.0f);
			popSize();

			ret = true;
		} else {
		 // deactivate
			m_hotId = m_activeId = kInvalidId;
		}

	} else if (_id == m_hotId) {
		setAlpha(1.0f);
		if (m_activeId == kInvalidId && frm::Intersects(cursorRay, cp)) {
			if (isKeyDown(kMouseLeft)) {
			 // activate, store offset
				m_activeId = _id;
				float t;
				APT_VERIFY(frm::Intersect(cursorRay, pl, t)); // should always intersect in this case
				m_translationOffset = *_position_ - (cursorRay.m_origin + cursorRay.m_direction * t);
			}
		} else {
			m_hotId = kInvalidId;
		}
		
	} else {
	 // intersect, make hot
		if (m_activeId == kInvalidId && frm::Intersects(cursorRay, cp)) {
			m_hotId = _id;
		}
	}
	DrawArrow(Vec3(0.0f, 0.0f, 0.0f), _axis, 0.2f);
	
	popAlpha();

	return ret;
}