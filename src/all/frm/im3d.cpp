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
			Im3d::Vertex(v);
		}
		for (int i = 0; i <= _detail; ++i) {
			float rad = glm::two_pi<float>() * ((float)i / (float)_detail) - glm::half_pi<float>();
			Vec3 v = Vec3(0.0f, 0.0f, ln) + Vec3(cosf(rad), sinf(rad), 0.0f) * _radius;
			Im3d::Vertex(v);
		}
	End();

	BeginLines();
		Im3d::Vertex(-_radius, 0.0f, -ln);
		Im3d::Vertex(-_radius, 0.0f,  ln);
		Im3d::Vertex( _radius, 0.0f, -ln);
		Im3d::Vertex( _radius, 0.0f,  ln);
		Im3d::Vertex(0.0f, _radius, -ln);
		Im3d::Vertex(0.0f, _radius,  ln);
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
			Im3d::Vertex(v);
		}
		for (int i = 0; i < _detail; ++i) {
			float rad = glm::pi<float>() * ((float)i / (float)_detail) + glm::pi<float>();
			Vec3 v = Vec3(0.0f, 0.0f, -ln) + Vec3(0.0f, cosf(rad), sinf(rad)) * _radius;
			Im3d::Vertex(v);
		}
		for (int i = 0; i < _detail; ++i) {
			float rad = glm::pi<float>() * ((float)i / (float)_detail);
			Vec3 v = Vec3(0.0f, 0.0f, ln) + Vec3(0.0f, cosf(rad), sinf(rad)) * _radius;
			Im3d::Vertex(v);
		}
		for (int i = 0; i <= detail2; ++i) {
			float rad = glm::two_pi<float>() * ((float)i / (float)detail2) - glm::half_pi<float>();
			Vec3 v = Vec3(0.0f, 0.0f, ln) + Vec3(cosf(rad), sinf(rad), 0.0f) * _radius;
			Im3d::Vertex(v);
		}
	End();

 // xz silhoette
	BeginLineLoop();
		for (int i = 0; i < _detail; ++i) {
			float rad = glm::pi<float>() * ((float)i / (float)_detail) + glm::pi<float>();
			Vec3 v = Vec3(0.0f, 0.0f, -ln) + Vec3(cosf(rad), 0.0f, sinf(rad)) * _radius;
			Im3d::Vertex(v);
		}
		for (int i = 0; i < _detail; ++i) {
			float rad = glm::pi<float>() * ((float)i / (float)_detail);
			Vec3 v = Vec3(0.0f, 0.0f, ln) + Vec3(cosf(rad), 0.0f, sinf(rad)) * _radius;
			Im3d::Vertex(v);
		}
	End();

	PopMatrix();
}

Im3d::Id Im3d::MakeId(const char* _str)
{
	static const U32 kFnv1aBase32  = 0x811C9DC5u;
	static const U32 kFnv1aPrime32 = 0x01000193u;

	APT_ASSERT(_str);
	U32 ret = kFnv1aBase32;
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

bool Context::gizmo(Id _id, Vec3* _position_, Quat* _orientation_, Vec3* _scale_)
{
frm::Ray cursorRay(m_cursorRayOriginW, m_cursorRayDirectionW);
float tnear, tfar;
	 
 // \todo something more scientific - maintain an exact screen space size
	float d = glm::length(*_position_ - m_viewOriginW);
	float distanceScale = 1.0f / (2.0f * glm::atan(0.5f / d)) * 0.1f;

	if (_id == m_activeId) {
	} else if (isKeyDown(kMouseLeft)) {
		static const float kCapsuleRadius = 0.05f;
		frm::Capsule xcap(*_position_, *_position_ + Vec3(1.0f, 0.0f, 0.0f) * distanceScale, kCapsuleRadius);
		frm::Capsule ycap(*_position_, *_position_ + Vec3(0.0f, 1.0f, 0.0f) * distanceScale, kCapsuleRadius);
		frm::Capsule zcap(*_position_, *_position_ + Vec3(0.0f, 0.0f, 1.0f) * distanceScale, kCapsuleRadius);
		
		if (cursorRay.intersect(xcap, tnear, tfar) || cursorRay.intersect(ycap, tnear, tfar) || cursorRay.intersect(zcap, tnear, tfar)) {
			m_activeId = _id;
		} else {
			m_activeId = kInvalidId;
		}
	}

 // draw the gizmo
	pushAlpha();
	if (_id != m_activeId) {
		setAlpha(0.5f);
	}
	PushMatrix();
	// \todo probably don't want to scale the gizmo
		Mat4 wm = glm::scale(glm::translate(glm::mat4(1.0f), *_position_) * glm::mat4_cast(*_orientation_), /**_scale_ **/ Vec3(distanceScale));
		MulMatrix(wm);
		
		BeginLines();
			Vertex(Vec3(0.0f, 0.0f, 0.0f), 4.0f, kColorRed);
			Vertex(Vec3(1.0f, 0.0f, 0.0f), 4.0f, kColorRed);

			Vertex(Vec3(0.0f, 0.0f, 0.0f), 4.0f, kColorGreen);
			Vertex(Vec3(0.0f, 1.0f, 0.0f), 4.0f, kColorGreen);

			Vertex(Vec3(0.0f, 0.0f, 0.0f), 4.0f, kColorBlue);
			Vertex(Vec3(0.0f, 0.0f, 1.0f), 4.0f, kColorBlue);
		End();

		BeginPoints();
			Vertex(Vec3(0.0f), 10.0f, kColorWhite);
		End();
	PopMatrix();
	popAlpha();

	return true;
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
