#include <frm/im3d.h>

#include <frm/def.h>
#include <frm/math.h>
#include <frm/geom.h>

#include <cstring>

#define Im3dAssert(_x) APT_ASSERT(_x)

static float Remap(float _x, float _start, float _end) {
	return glm::clamp(_x * (1.0f / (_end - _start)) + (-_start / (_end - _start)), 0.0f, 1.0f);
}

using namespace Im3d;

const Im3d::Color Im3d::kColorBlack   = Im3d::Color(0.0f, 0.0f, 0.0f);
const Im3d::Color Im3d::kColorWhite   = Im3d::Color(1.0f, 1.0f, 1.0f);
const Im3d::Color Im3d::kColorRed     = Im3d::Color(1.0f, 0.0f, 0.0f);
const Im3d::Color Im3d::kColorGreen   = Im3d::Color(0.0f, 1.0f, 0.0f);
const Im3d::Color Im3d::kColorBlue    = Im3d::Color(0.0f, 0.0f, 1.0f);
const Im3d::Color Im3d::kColorCyan    = Im3d::Color(0.0f, 1.0f, 1.0f);
const Im3d::Color Im3d::kColorMagenta = Im3d::Color(1.0f, 0.0f, 1.0f);
const Im3d::Color Im3d::kColorYellow  = Im3d::Color(1.0f, 1.0f, 0.0f);

static const Im3d::Color kColorHighlight = Im3d::Color(1.0f, 1.0f, 0.3f);
static const Im3d::Color kColorInvisible = Im3d::Color(0.0f, 0.0f, 0.0f, 0.0f);

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
void  Im3d::BeginTriangles()             { GetCurrentContext().begin(Context::kTriangles); }
void  Im3d::BeginTriangleStrip()         { GetCurrentContext().begin(Context::kTriangleStrip); }
void  Im3d::End()                        { GetCurrentContext().end(); }

void  Im3d::PushDrawState()
{
	Context& ctx = GetCurrentContext();
	ctx.pushSize();
	ctx.pushColor();
	ctx.pushAlpha();
}
void  Im3d::PopDrawState()
{
	Context& ctx = GetCurrentContext();
	ctx.popAlpha();
	ctx.popColor();
	ctx.popSize();
}
void  Im3d::SetColor(Color _color)       { GetCurrentContext().setColor(_color); }
Color Im3d::GetColor()                   { return GetCurrentContext().getColor(); }
void  Im3d::SetAlpha(float _alpha)       { GetCurrentContext().setAlpha(_alpha); }
float Im3d::GetAlpha()                   { return GetCurrentContext().getAlpha(); }
void  Im3d::SetSize(float _size)         { GetCurrentContext().setSize(_size); }
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
void Im3d::Vertex(const Vec3& _position, float _size)
{
	Context& ctx = GetCurrentContext();
	Vec3 pos = Vec3(ctx.getMatrix() * Vec4(_position, 1.0f));
	ctx.vertex(pos, _size, ctx.getColor());
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
		Vertex(0.0f, 0.0f, 1.0f);
	End();
}

void Im3d::DrawQuad(const Vec3& _a, const Vec3& _b, const Vec3& _c, const Vec3& _d)
{
	BeginLineLoop();
		Vertex(_a);
		Vertex(_b);
		Vertex(_c);
		Vertex(_d);
	End();
}

void Im3d::DrawQuad(const Vec3& _origin, const Vec3& _normal, const Vec2& _scale)
{
 // \todo better way of doing this?
	PushMatrix();
		MulMatrix(frm::GetLookAtMatrix(_origin, _normal));
		Vec2 s = _scale * 0.5f;
		DrawQuad(
			Vec3(-s.x, -s.y, 0.0f),
			Vec3( s.x, -s.y, 0.0f),
			Vec3( s.x,  s.y, 0.0f),
			Vec3(-s.x,  s.y, 0.0f)
			);
	PopMatrix();
}

void Im3d::DrawQuadFilled(const Vec3& _a, const Vec3& _b, const Vec3& _c, const Vec3& _d)
{
	BeginTriangleStrip();
		Vertex(_a);
		Vertex(_b);
		Vertex(_c);
		Vertex(_d);
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
	Mat4 cmat = frm::GetLookAtMatrix(org, _end);
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
	Mat4 cmat = frm::GetLookAtMatrix(org, _end);
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
	Vec3 head = _start + (_end - _start) * (1.0f - _headLength);
	BeginLines();
		Vertex(_start);
		Vertex(head);
		Vertex(head, GetCurrentContext().getSize() * 2.0f);
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
	return GetCurrentContext().gizmo(id, _position_, _orientation_, _scale_) == Context::kActive;

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
	case kTriangles:
	case kTriangleStrip:
		m_firstVertThisPrim = (unsigned)m_triangles.size();
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
	case kTriangles:
		Im3dAssert(m_vertCountThisPrim % 3 == 0);
		break;
	case kTriangleStrip:
		Im3dAssert(m_vertCountThisPrim >= 3);
		break;
	default:
		break;
	};

	m_primMode = kNone;
}

void Context::vertex(const Vec3& _position, float _size, Color _color)
{
	Im3dAssert(m_primMode != kNone);

	_color.scaleAlpha(getAlpha());
	switch (m_primMode) {
	case kPoints:
		m_points.push_back(struct Vertex(_position, _size, _color));
		break;
	case kLines:
		m_lines.push_back(struct Vertex(_position, _size, _color));
		break;
	case kLineStrip:
	case kLineLoop:
		if (m_vertCountThisPrim >= 2) {
			m_lines.push_back(m_lines.back());
			++m_vertCountThisPrim;
		}
		m_lines.push_back(struct Vertex(_position, _size, _color));
		break;
	case kTriangles:
		m_triangles.push_back(struct Vertex(_position, _size, _color));
		break;
	case kTriangleStrip:
		if (m_vertCountThisPrim >= 3) {
			m_triangles.push_back(*(m_triangles.end() - 2));
			m_triangles.push_back(*(m_triangles.end() - 2));
			m_vertCountThisPrim += 2;
		}
		m_triangles.push_back(struct Vertex(_position, _size, _color));
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
	m_triangles.clear();

 // copy keydown array internally so that we can make a delta to detect key presses
	memcpy(m_keyDownPrev, m_keyDownCurr, sizeof(m_keyDown)); // \todo avoid this copy, use an index
	memcpy(m_keyDownCurr, m_keyDown,     sizeof(m_keyDown));
}


float Context::pixelsToWorldSize(const Vec3& _position, float _pixels)
{
	float d = glm::length(_position - m_viewOriginW);
	return m_tanHalfFov * 2.0f * d * (_pixels / m_displaySize.y);
}

Context::UiState Context::gizmo(Id _id, Vec3* _position_, Quat* _orientation_, Vec3* _scale_, float _screenSize)
{
	frm::Ray cursorRay(m_cursorRayOriginW, m_cursorRayDirectionW);
	
	int ret = kCold;
	pushId();
		setId(_id);

		Im3d::Color colX = kColorRed;
		Im3d::Color colY = kColorGreen;
		Im3d::Color colZ = kColorBlue;

		float screenScale = pixelsToWorldSize(*_position_, 64.0f);
		const float kPlaneSize = 0.2f * screenScale;
		const float kPlaneOffset = kPlaneSize + 0.02f * screenScale;
		
		{ // XZ plane
			int xzState = kCold;
			Vec3 planeNormal = Vec3(0.0f, 1.0f, 0.0f);
			Vec3 planeOrigin = *_position_ + Vec3(kPlaneOffset, 0.0f, kPlaneOffset);
			float alignedAlpha = glm::abs(glm::dot(planeNormal, glm::normalize(m_viewOriginW - planeOrigin)));
			alignedAlpha = Remap(alignedAlpha, 0.2f, 0.3f);

			Id xzId = MakeId("xzplane");
			if (m_activeId == xzId || alignedAlpha > 0.0f) {
				xzState = planarGizmoW(xzId, _position_, planeNormal, planeOrigin, kPlaneSize);
				if (xzState != kCold) {
					colX = colZ = kColorHighlight;
					ret |= xzState;
				}

				PushDrawState();
					setColor(kColorHighlight);
					setAlpha(xzState == kCold ? 0.2f : 1.0f);
					DrawQuadFilled(
						planeOrigin + Vec3(-1.0f,  0.0f, -1.0f) * kPlaneSize,
						planeOrigin + Vec3( 1.0f,  0.0f, -1.0f) * kPlaneSize,
						planeOrigin + Vec3(-1.0f,  0.0f,  1.0f) * kPlaneSize,
						planeOrigin + Vec3( 1.0f,  0.0f,  1.0f) * kPlaneSize
						);
				PopDrawState();
			}
		}
		{ // XY plane
			int xyState = kCold;
			Vec3 planeNormal = Vec3(0.0f, 0.0f, 1.0f);
			Vec3 planeOrigin = *_position_ + Vec3(kPlaneOffset, kPlaneOffset, 0.0f);
			float alignedAlpha = glm::abs(glm::dot(planeNormal, glm::normalize(m_viewOriginW - planeOrigin)));
			alignedAlpha = Remap(alignedAlpha, 0.2f, 0.3f);

			Id xyId = MakeId("xyplane");
			if (m_activeId == xyId || alignedAlpha > 0.0f) {
				xyState = planarGizmoW(xyId, _position_, planeNormal, planeOrigin, kPlaneSize);
				if (xyState != kCold) {
					colX = colY = kColorHighlight;
					ret |= xyState;
				}

				PushDrawState();
					setColor(kColorHighlight);
					setAlpha(xyState == kCold ? 0.2f : 1.0f);
					DrawQuadFilled(
						planeOrigin + Vec3(-1.0f, -1.0f,  0.0f) * kPlaneSize,
						planeOrigin + Vec3( 1.0f, -1.0f,  0.0f) * kPlaneSize,
						planeOrigin + Vec3(-1.0f,  1.0f,  0.0f) * kPlaneSize,
						planeOrigin + Vec3( 1.0f,  1.0f,  0.0f) * kPlaneSize
						);
				PopDrawState();
			}
		}
		{ // ZY plane
			int zyState = kCold;
			Vec3 planeNormal = Vec3(1.0f, 0.0f, 0.0f);
			Vec3 planeOrigin = *_position_ + Vec3(0.0f, kPlaneOffset, kPlaneOffset);
			float alignedAlpha = glm::abs(glm::dot(planeNormal, glm::normalize(m_viewOriginW - planeOrigin)));
			alignedAlpha = Remap(alignedAlpha, 0.2f, 0.3f);

			Id zyId = MakeId("zyplane");
			if (m_activeId == zyId || alignedAlpha > 0.0f) {
				zyState = planarGizmoW(zyId, _position_, planeNormal, planeOrigin, kPlaneSize);
				if (zyState != kCold) {
					colZ = colY = kColorHighlight;
					ret |= zyState;
				}

				PushDrawState();
					setColor(kColorHighlight);
					setAlpha(zyState == kCold ? 0.2f : 1.0f);
					DrawQuadFilled(
						planeOrigin + Vec3(0.0f, -1.0f, -1.0f) * kPlaneSize,
						planeOrigin + Vec3(0.0f,  1.0f, -1.0f) * kPlaneSize,
						planeOrigin + Vec3(0.0f, -1.0f,  1.0f) * kPlaneSize,
						planeOrigin + Vec3(0.0f,  1.0f,  1.0f) * kPlaneSize
						);
				PopDrawState();
			}
		}
		
		ret |= axisGizmoW(MakeId("xaxis"), _position_, Vec3(1.0f, 0.0f, 0.0f), colX, 64.0f);
		ret |= axisGizmoW(MakeId("yaxis"), _position_, Vec3(0.0f, 1.0f, 0.0f), colY, 64.0f);
		ret |= axisGizmoW(MakeId("zaxis"), _position_, Vec3(0.0f, 0.0f, 1.0f), colZ, 64.0f);
		
	popId();
	return (UiState)ret;
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


Context::UiState Context::axisGizmoW(
	Id           _id,
	Vec3*        _position_, 
	const Vec3&  _axis,
	const Color& _color, 
	float        _sizePixels
	)
{
	float screenScale = pixelsToWorldSize(*_position_, _sizePixels);
	const frm::Ray cursorRay(m_cursorRayOriginW, m_cursorRayDirectionW);
	const frm::Line ln(*_position_, _axis);
	const frm::Capsule cp(
		*_position_ + _axis * screenScale * 0.2f, // leave a small space at the origin
		*_position_ + _axis * screenScale, 
		0.05f * screenScale
		);

	PushDrawState(); // push color, alpha, size
	setColor(_color);
	
	UiState ret = kCold;
	if (_id == m_activeId) {
		if (isKeyDown(kMouseLeft)) {
		 // active, move _position_
			float tr, tl;
			frm::Nearest(cursorRay, ln, tr, tl);
			*_position_ = *_position_ + _axis * tl - m_translationOffset;
		 // draw the axis
			setSize(1.0f);
			setAlpha(0.5f);
			BeginLines();
				Vertex(*_position_ - _axis * 9999.0f);
				Vertex(*_position_ + _axis * 9999.0f);
			End();
			ret = kActive;
		} else {
		 // deactivate
			m_hotId = m_activeId = kInvalidId;
		}
		setColor(kColorHighlight);

	} else if (_id == m_hotId) {
		if (m_activeId == kInvalidId && frm::Intersects(cursorRay, cp)) {
			ret = kHot;
			if (isKeyDown(kMouseLeft)) {
			 // activate, store offset
				m_activeId = _id;
				float tr, tl;
				frm::Nearest(cursorRay, ln, tr, tl);
				m_translationOffset = _axis * tl;
			}
		} else {
			m_hotId = kInvalidId;
		}
		setColor(kColorHighlight);
		
	} else {
	 // intersect, make hot
		if (m_activeId == kInvalidId && frm::Intersects(cursorRay, cp)) {
			m_hotId = _id;
		}
		setColor(_color);
	}
	
	float alignedAlpha = 1.0f - glm::abs(glm::dot(_axis, glm::normalize(m_viewOriginW - *_position_)));
	alignedAlpha = Remap(alignedAlpha, 0.1f, 0.2f);
	setAlpha(alignedAlpha);
	setSize(4.0f);
	DrawArrow(cp.m_start, cp.m_end, 0.2f);	
	
	PopDrawState();

	return ret;
}

Context::UiState Context::planarGizmoW(
	Id           _id,
	Vec3*        _position_,
	const Vec3&  _planeNormal,
	const Vec3&  _planeOrigin,
	float        _sizeW
	)
{
	const frm::Ray cursorRay(m_cursorRayOriginW, m_cursorRayDirectionW);
	const frm::Plane plane(_planeNormal, _planeOrigin);
	float t0;
	bool intersects = frm::Intersect(cursorRay, plane, t0);
	
	UiState ret = kCold;
	if (_id == m_activeId) {
		if (isKeyDown(kMouseLeft)) {
		 // active, move _position_
			if (intersects) {
				*_position_ = cursorRay.m_origin + cursorRay.m_direction * t0 - m_translationOffset;
				ret = kActive;

			 // \todo draw some additional info here?
			}
		} else {
		 // deactivate
			m_hotId = m_activeId = kInvalidId;
		}

	} else if (_id == m_hotId) {
		Vec3 p = cursorRay.m_origin + cursorRay.m_direction * t0;
		Vec3 delta = glm::abs(p - _planeOrigin);
		if (intersects && glm::all(glm::lessThan(delta, Vec3(_sizeW)))) {
			ret = kHot;
			if (m_activeId == kInvalidId) {
				if (isKeyDown(kMouseLeft)) {
				 // activate, store offset
					Vec3 p = cursorRay.m_origin + cursorRay.m_direction * t0;
					m_activeId = _id;
					m_translationOffset = p - *_position_;
				}
			}
		} else {
			m_hotId = kInvalidId;
		}
		
	} else {
	 // intersect, make hot
		if (m_activeId == kInvalidId && intersects) {
			Vec3 p = cursorRay.m_origin + cursorRay.m_direction * t0;
			Vec3 delta = glm::abs(p - _planeOrigin);
			if (glm::all(glm::lessThan(delta, Vec3(_sizeW)))) {
				m_hotId = _id;
			}
		}
	}
	
	return ret;
}

Context::UiState Context::handle(
	Id            _id,
	const Vec3&   _position,
	const Color&  _color,
	float         _size
	)
{
	const frm::Ray cursorRay(m_cursorRayOriginW, m_cursorRayDirectionW);
 // \note the hit sphere won't match the point drawn if the fov is hight (the projection of a sphere is an ellipse)
	float srad = pixelsToWorldSize(_position, _size) * 0.5f;
	frm::Sphere s(_position, srad);

	UiState ret = kCold;
	pushAlpha();
	pushColor();
	setColor(_color);
	float alpha = 1.0f;
	if (_id == m_activeId) {
		if (isKeyDown(kMouseLeft)) {
		 // active
			ret = kActive;
		} else {
		 // deactivate
			m_hotId = m_activeId = kInvalidId;
		}
	} else if (_id == m_hotId) {
		if (m_activeId == kInvalidId && frm::Intersects(cursorRay, s)) {
			ret = kHot;
			if (isKeyDown(kMouseLeft)) {
			 // activate
				m_activeId = _id;
			}
		} else {
			m_hotId = kInvalidId;
		}

	} else {
		alpha = 0.5f;
	 // intersect, make hot
		if (m_activeId == kInvalidId && frm::Intersects(cursorRay, s)) {
			m_hotId = _id;
		}
	}

	setAlpha(alpha);
	setSize(_size);
	BeginPoints();
		Vertex(_position);
	End();

	popColor();
	popAlpha();

	return ret;
}

void Context::movePlanar(
	Vec3*        _position_, 
	const Vec3&  _constraint, 
	const Vec3&  _planeNormal, 
	float        _planeOffset
	)	
{
	const frm::Ray cursorRay(m_cursorRayOriginW, m_cursorRayDirectionW);
	const frm::Plane pl(_planeNormal, _planeOffset);
	float t;
	if (frm::Intersect(cursorRay, pl, t)) {
		Vec3 displacement = cursorRay.m_origin + cursorRay.m_direction * t + m_translationOffset;
		displacement = (displacement - *_position_) * _constraint;
		*_position_ += displacement;
	}
}