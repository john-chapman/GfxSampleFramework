#include <frm/im3d.h>

#include <frm/def.h>
#include <frm/math.h>

#define Im3dAssert(_x) APT_ASSERT(_x)

using namespace Im3d;

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

void Im3d::DrawCapsule(const Vec3& _start, const Vec3& _end, float _radius, int _detail)
{
	Vec3 org = _start + (_end - _start) * 0.5f;
	Mat4 cmat = frm::LookAt(org, _end);
	float ln = glm::length(_end - _start) * 0.5f;
	int detail2 = _detail * 2; // force cap base detail to match ends

	PushMatrix();
	Translate(org.x, org.y, org.z);
	MulMatrix(cmat);

 // yz silhoette + cap bases
	BeginLineLoop();
		for (int i = 0; i <= detail2; ++i) {
			float rad = glm::two_pi<float>() * ((float)i / (float)detail2) - glm::half_pi<float>();
			Vec3 v = Vec3(0.0f, 0.0f, -ln) + Vec3(cosf(rad), sinf(rad), 0.0f);
			Im3d::Vertex(v);
		}
		for (int i = 0; i < detail2; ++i) {
			float rad = glm::pi<float>() * ((float)i / (float)_detail) + glm::pi<float>();
			Vec3 v = Vec3(0.0f, 0.0f, -ln) + Vec3(0.0f, cosf(rad), sinf(rad));
			Im3d::Vertex(v);
		}
		for (int i = 0; i < detail2; ++i) {
			float rad = glm::pi<float>() * ((float)i / (float)_detail);
			Vec3 v = Vec3(0.0f, 0.0f, ln) + Vec3(0.0f, cosf(rad), sinf(rad));
			Im3d::Vertex(v);
		}
		for (int i = 0; i <= detail2; ++i) {
			float rad = glm::two_pi<float>() * ((float)i / (float)detail2) - glm::half_pi<float>();
			Vec3 v = Vec3(0.0f, 0.0f, ln) + Vec3(cosf(rad), sinf(rad), 0.0f);
			Im3d::Vertex(v);
		}
	End();

 // xz silhoette
	BeginLineLoop();
		for (int i = 0; i < _detail; ++i) {
			float rad = glm::pi<float>() * ((float)i / (float)_detail) + glm::pi<float>();
			Vec3 v = Vec3(0.0f, 0.0f, -ln) + Vec3(cosf(rad), 0.0f, sinf(rad));
			Im3d::Vertex(v);
		}
		for (int i = 0; i < _detail; ++i) {
			float rad = glm::pi<float>() * ((float)i / (float)_detail);
			Vec3 v = Vec3(0.0f, 0.0f, ln) + Vec3(cosf(rad), 0.0f, sinf(rad));
			Im3d::Vertex(v);
		}
	End();

	PopMatrix();
}


/*******************************************************************************

                                   Context

*******************************************************************************/

// PUBLIC

Context::Context()
	: m_matStack(Mat4(1.0f))
	, m_colorStack(Color(1.0f, 1.0f, 1.0f))
	, m_sizeStack(1.0f)
	, m_mode(kNone)
	, m_firstVertThisPrim(0)
	, m_vertCountThisPrim(0)
{
}

Context::~Context()
{
}

void Context::begin(Mode _mode)
{
	Im3dAssert(m_mode == kNone);
	m_mode = _mode;
	m_vertCountThisPrim = 0;
	switch (m_mode) {
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
	Im3dAssert(m_mode != kNone);
	switch (m_mode) {
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

	m_mode = kNone;
}

void Context::vertex(const Vec3& _position, float _width, Color _color)
{
	Im3dAssert(m_mode != kNone);

	switch (m_mode) {
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
	Im3dAssert(m_mode == kNone);
	m_points.clear();
	m_lines.clear();
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
