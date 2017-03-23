#include <frm/geom.h>

#include <frm/math.h>

#define geom_debug
#ifdef geom_debug
	#include <imgui/imgui.h>
	#include <im3d/im3d.h>
#endif

using namespace frm;

static float GetMaxScale(const mat4& _mat)
{
	float sx = _mat[0][0] * _mat[0][0] + _mat[0][1] * _mat[0][1] + _mat[0][2] * _mat[0][2];
	float sy = _mat[1][0] * _mat[1][0] + _mat[1][1] * _mat[1][1] + _mat[1][2] * _mat[1][2];
	float sz = _mat[2][0] * _mat[2][0] + _mat[2][1] * _mat[2][1] + _mat[2][2] * _mat[2][2];
	float ret = apt::max(sx, apt::max(sy, sz));
	ret = sqrtf(ret);
	return ret;
}

/*******************************************************************************

                                  Line

*******************************************************************************/

Line::Line(const vec3& _origin, const vec3& _direction)
	: m_origin(_origin)
	, m_direction(_direction)
{
}

void Line::transform(const mat4& _mat)
{
	m_origin = vec3(_mat * vec4(m_origin, 1.0f));
	m_direction = vec3(_mat * vec4(m_direction, 0.0f));
}

/*******************************************************************************

                                   Ray

*******************************************************************************/

Ray::Ray(const vec3& _origin, const vec3& _direction)
	: m_origin(_origin)
	, m_direction(_direction)
{
}

void Ray::transform(const mat4& _mat)
{
	m_origin = vec3(_mat * vec4(m_origin, 1.0f));
	m_direction = vec3(_mat * vec4(m_direction, 0.0f));
}

/*******************************************************************************

                                LineSegment

*******************************************************************************/

LineSegment::LineSegment(const vec3& _start, const vec3& _end)
	: m_start(_start)
	, m_end(_end)
{
}

void LineSegment::transform(const mat4& _mat)
{
	m_start = vec3(_mat * vec4(m_start, 1.0f));
	m_end   = vec3(_mat * vec4(m_end, 1.0f));
}

/*******************************************************************************

                                 Sphere

*******************************************************************************/

Sphere::Sphere(const vec3& _origin, float _radius)
	: m_origin(_origin)
	, m_radius(_radius)
{
}

Sphere::Sphere(const AlignedBox& _box)
	: m_origin(_box.getOrigin())
	, m_radius(length(_box.m_max - _box.m_min) * 0.5f)
{
}

void Sphere::transform(const mat4& _mat)
{
	float maxScale = GetMaxScale(_mat);	
	m_radius *= maxScale;
	m_origin *= maxScale;

 // translate after scaling
	m_origin = vec3(_mat * vec4(m_origin, 1.0f));
}

/*******************************************************************************

                                  Plane

*******************************************************************************/

Plane::Plane(const vec3& _normal, float _offset)
	: m_normal(_normal)
	, m_offset(_offset)
{
}

Plane::Plane(const vec3& _normal, const vec3& _origin)
	: m_normal(_normal)
	, m_offset(dot(_normal, _origin))
{
}

Plane::Plane(float _a, float _b, float _c, float _d)
	: m_normal(_a, _b, _c)
	, m_offset(_d / length2(m_normal))
{
	m_normal = normalize(m_normal);
}

Plane::Plane(const vec3& _p0, const vec3& _p1, const vec3& _p2)
{
	vec3 u(_p1 - _p0);
	vec3 v(_p2 - _p0);
	m_normal = normalize(cross(v, u));
	m_offset = dot(m_normal, (_p0 + _p1 + _p2) / 3.0f);
}

void Plane::transform(const mat4& _mat)
{
	vec3 origin = vec3(_mat * vec4(getOrigin(), 1.0f));
	m_normal = vec3(_mat * vec4(m_normal, 0.0f));
	m_normal = normalize(m_normal);
	m_offset = dot(m_normal, origin);
}

vec3 Plane::getOrigin() const
{ 
	return m_normal * m_offset; 
}

/*******************************************************************************

                               AlignedBox

*******************************************************************************/

AlignedBox::AlignedBox(const vec3& _min, const vec3& _max)
	: m_min(_min)
	, m_max(_max)
{
}

AlignedBox::AlignedBox(const Sphere& _sphere)
{
	m_min = _sphere.m_origin - vec3(_sphere.m_radius);
	m_max = _sphere.m_origin + vec3(_sphere.m_radius);
}

AlignedBox::AlignedBox(const Frustum& _frustum)
{
	m_min = vec3(FLT_MAX);
	m_max = vec3(-FLT_MAX);
	for (int i = 0; i < 8; ++i) {
		m_min = min(m_min, _frustum.m_vertices[i]);
		m_max = max(m_max, _frustum.m_vertices[i]);
	}
}

void AlignedBox::transform(const mat4& _mat)
{
	vec3 mx = vec3(column(_mat, 0));
	vec3 my = vec3(column(_mat, 1));
	vec3 mz = vec3(column(_mat, 2));
	vec3 mw = vec3(column(_mat, 3));

	vec3 xa = mx * m_min.x;
	vec3 xb = mx * m_max.x;
	vec3 ya = my * m_min.y;
	vec3 yb = my * m_max.y;
	vec3 za = mz * m_min.z;
	vec3 zb = mz * m_max.z;

	m_min = min(xa, xb) + min(ya, yb) + min(za, zb) + mw;
	m_max = max(xa, xb) + max(ya, yb) + max(za, zb) + mw;
}

vec3 AlignedBox::getOrigin() const 
{ 
	return 0.5f * (m_max + m_min);
}

void AlignedBox::getVertices(vec3* out_) const
{
	out_[0] = vec3(m_min.x, m_min.y, m_min.z);
	out_[1] = vec3(m_max.x, m_min.y, m_min.z);
	out_[2] = vec3(m_max.x, m_min.y, m_max.z);
	out_[3] = vec3(m_min.x, m_min.y, m_max.z);
	out_[4] = vec3(m_min.x, m_max.y, m_min.z);
	out_[5] = vec3(m_max.x, m_max.y, m_min.z);
	out_[6] = vec3(m_max.x, m_max.y, m_max.z);
	out_[7] = vec3(m_min.x, m_max.y, m_max.z);
}

/*******************************************************************************

                                 Cylinder

*******************************************************************************/

Cylinder::Cylinder(const vec3& _start, const vec3& _end, float _radius)
	: m_start(_start)
	, m_end(_end)
	, m_radius(_radius)
{
}

void Cylinder::transform(const mat4& _mat)
{
	m_radius *= GetMaxScale(_mat);
	m_start = vec3(_mat * vec4(m_start, 1.0f));
	m_end   = vec3(_mat * vec4(m_end, 1.0f));
}

vec3 Cylinder::getOrigin() const
{
	return 0.5f * (m_end + m_start);
}

/*******************************************************************************

                                 Capsule

*******************************************************************************/

Capsule::Capsule(const vec3& _start, const vec3& _end, float _radius)
	: m_start(_start)
	, m_end(_end)
	, m_radius(_radius)
{
}

void Capsule::transform(const mat4& _mat)
{
	m_radius *= GetMaxScale(_mat);
	m_start = vec3(_mat * vec4(m_start, 1.0f));
	m_end   = vec3(_mat * vec4(m_end, 1.0f));
}

vec3 Capsule::getOrigin() const
{
	return 0.5f * (m_end + m_start);
}

/*******************************************************************************

                                 Frustum

*******************************************************************************/

Frustum::Frustum(float _aspect, float _tanHalfFov, float _near, float _far)
{
	/*if (_isOrtho) {
		float right = tanHalfFov * _far * _aspect;
		float up = tanHalfFov * _far;
	 // near plane
		m_vertices[0] = vec3( right,  up, -_near);
		m_vertices[1] = vec3(-right,  up, -_near);
		m_vertices[2] = vec3(-right, -up, -_near);
		m_vertices[3] = vec3( right, -up, -_near);
	 // far plane
		m_vertices[4] = vec3( right,  up, -_far);
		m_vertices[5] = vec3(-right,  up, -_far);
		m_vertices[6] = vec3(-right, -up, -_far);
		m_vertices[7] = vec3( right, -up, -_far);
	} else {*/
	 	float ny = _tanHalfFov * _near;
		float nx = _aspect * ny;
	 // near plane
		m_vertices[0] = vec3( nx,  ny, -_near);
		m_vertices[1] = vec3(-nx,  ny, -_near);
		m_vertices[2] = vec3(-nx, -ny, -_near);
		m_vertices[3] = vec3( nx, -ny, -_near);
	//	far plane
		float fy = _tanHalfFov * _far;
		float fx = _aspect * fy;
		m_vertices[4] = vec3( fx,  fy, -_far);
		m_vertices[5] = vec3(-fx,  fy, -_far);
		m_vertices[6] = vec3(-fx, -fy, -_far);
		m_vertices[7] = vec3( fx, -fy, -_far);
	/*}*/

	initPlanes();
}

Frustum::Frustum(float _up, float _down, float _right, float _left, float _near, float _far, bool _isOrtho)
{
 // near plane
	float nu = _up;
	float nd = _down;
	float nr = _right;
	float nl = _left;
	if (!_isOrtho) {
		nu *= _near;
		nd *= _near;
		nr *= _near;
		nl *= _near;
	}
	m_vertices[0] = vec3(nl, nu, -_near);
	m_vertices[1] = vec3(nr, nu, -_near);
	m_vertices[2] = vec3(nr, nd, -_near);
	m_vertices[3] = vec3(nl, nd, -_near);

 // far plane
	float fu = _up;
	float fd = _down;
	float fr = _right;
	float fl = _left;
	if (!_isOrtho) {
		fu *= _far;
		fd *= _far;
		fr *= _far;
		fl *= _far;
	}
	m_vertices[4] = vec3(fl, fu, -_far);
	m_vertices[5] = vec3(fr, fu, -_far);
	m_vertices[6] = vec3(fr, fd, -_far);
	m_vertices[7] = vec3(fl, fd, -_far);

	initPlanes();
}

Frustum::Frustum(const Frustum& _left, const Frustum& _right)
{
	m_vertices[0] = _left.m_vertices[0];
	m_vertices[3] = _left.m_vertices[3];
	m_vertices[4] = _left.m_vertices[4];
	m_vertices[7] = _left.m_vertices[7];
	m_vertices[1] = _right.m_vertices[1];
	m_vertices[2] = _right.m_vertices[2];
	m_vertices[5] = _right.m_vertices[5];
	m_vertices[6] = _right.m_vertices[6];
	initPlanes();
}

Frustum::Frustum(const Frustum& _base, float _nearOffset, float _farOffset)
{
	float d = length(_base.m_planes[Plane_Far].getOrigin() - _base.m_planes[Plane_Near].getOrigin());
	float n = _nearOffset / d;
	float f = _farOffset  / d;
	for (int i = 0; i < 4; ++i) {
		m_vertices[i]     = mix(_base.m_vertices[i], _base.m_vertices[i + 4], n);
		m_vertices[i + 4] = mix(_base.m_vertices[i], _base.m_vertices[i + 4], 1.0f + f);
	}
	initPlanes();
}

void Frustum::transform(const mat4& _mat)
{
	for (int i = 0; i < Plane_Count; ++i) {
		m_planes[i].transform(_mat);
	}
	for (int i = 0; i < 8; ++i) {
		m_vertices[i] = vec3(_mat * vec4(m_vertices[i], 1.0f));
	}
}

bool Frustum::inside(const Sphere& _sphere) const
{
	for (int i = 0; i < Plane_Count; ++i) {
		if (Distance(m_planes[i], _sphere.m_origin) < -_sphere.m_radius) {
			return false;
		}
	}
	return true;
}

bool Frustum::insideIgnoreNear(const Sphere& _sphere) const
{
	for (int i = 1; i < Plane_Count; ++i) {
		if (Distance(m_planes[i], _sphere.m_origin) < -_sphere.m_radius) {
			return false;
		}
	}
	return true;
}

bool Frustum::inside(const AlignedBox& _box) const
{
	 // todo TEST ALTERNATE METHOD AND TIME
 // build box points from extents
	vec3 points[] = {
		vec3(_box.m_min.x, _box.m_min.y, _box.m_min.z),
		vec3(_box.m_max.x, _box.m_min.y, _box.m_min.z),
		vec3(_box.m_max.x, _box.m_max.y, _box.m_min.z),
		vec3(_box.m_min.x, _box.m_max.y, _box.m_min.z),

		vec3(_box.m_min.x, _box.m_min.y, _box.m_max.z),
		vec3(_box.m_max.x, _box.m_min.y, _box.m_max.z),
		vec3(_box.m_max.x, _box.m_max.y, _box.m_max.z),
		vec3(_box.m_min.x, _box.m_max.y, _box.m_max.z)
	};

 // test points against frustum plains
	for (int i = 0; i < 6; ++i) {
		bool inside = false;
		for (int j = 0; j < 8; ++j) {
			if (Distance(m_planes[i], points[j]) > 0.0f) {
				inside = true;
				break;
			}
		}
		if (!inside) {
			return false;
		}
	}
	return true;
}

void Frustum::setVertices(const vec3 _vertices[8])
{
	memcpy(m_vertices, _vertices, sizeof(m_vertices));
	initPlanes();
}

void Frustum::initPlanes()
{
	m_planes[Plane_Near]   = Plane(m_vertices[2], m_vertices[1], m_vertices[0]);
	m_planes[Plane_Far]    = Plane(m_vertices[4], m_vertices[5], m_vertices[6]);
	m_planes[Plane_Top]    = Plane(m_vertices[1], m_vertices[5], m_vertices[4]);
	m_planes[Plane_Right]  = Plane(m_vertices[6], m_vertices[5], m_vertices[1]);
	m_planes[Plane_Bottom] = Plane(m_vertices[3], m_vertices[7], m_vertices[6]);
	m_planes[Plane_Left]   = Plane(m_vertices[3], m_vertices[0], m_vertices[4]);
}


/*******************************************************************************
*******************************************************************************/

vec3 frm::Nearest(const Line& _line, const vec3& _point)
{
	vec3 p = _point - _line.m_origin;
	float q = dot(p, _line.m_direction);
	return _line.m_origin + _line.m_direction * q;
}
vec3 frm::Nearest(const Ray& _ray, const vec3& _point)
{
	vec3 p = _point - _ray.m_origin;
	float q = dot(p, _ray.m_direction);
	return _ray.m_origin + _ray.m_direction * apt::max(q, 0.0f);
}
vec3 frm::Nearest(const LineSegment& _segment, const vec3& _point)
{
	vec3 dir = _segment.m_end - _segment.m_start;
	vec3 p = _point - _segment.m_start;
	float q = dot(p, dir);
	if (q <= 0.0f) {
		return _segment.m_start;
	} else {
		float r = length2(dir);
		if (q >= r) {
			return _segment.m_end;
		} else {
			return _segment.m_start + dir * (q / r);
		}
	}
}
vec3 frm::Nearest(const Sphere& _sphere, const vec3& _point)
{
	return _sphere.m_origin + normalize(_sphere.m_origin - _point) * _sphere.m_radius;
}
vec3 frm::Nearest(const Plane& _plane, const vec3& _point)
{ 
	return _point - _plane.m_normal * Distance(_plane, _point); 
}
vec3 frm::Nearest(const AlignedBox& _box, const vec3& _point)
{
	vec3 ret;
	for (int i = 0; i < 3; ++i) {
		float v = _point[i];
		if (v < _box.m_min[i]) {
			v = _box.m_min[i];
		}
		if (v > _box.m_max[i]) {
			v = _box.m_max[i];
		}
		ret[i] = v;
	}
	return ret;
}
void frm::Nearest(const Line& _line0, const Line& _line1, float& t0_, float& t1_)
{
	vec3 p = _line0.m_origin - _line1.m_origin;
	float q = dot(_line0.m_direction, _line1.m_direction);
	float s = dot(_line1.m_direction, p);

	float d = 1.0f - q * q;
	if (d < FLT_EPSILON) { // lines are parallel
		t0_ = 0.0f;
		t1_ = s;
	} else {
		float r = dot(_line0.m_direction, p);
		t0_ = (q * s - r) / d;
		t1_ = (s - q * r) / d;
	}
}
void frm::Nearest(const Ray& _ray, const Line& _line, float& tr_, float& tl_)
{
	Nearest(Line(_ray.m_origin, _ray.m_direction), _line, tr_, tl_);
	tr_ = apt::max(tr_, 0.0f);
}
vec3 frm::Nearest(const Ray& _ray, const LineSegment& _segment, float& tr_)
{
	vec3 ldir = _segment.m_end - _segment.m_start;
	vec3 p = _segment.m_start - _ray.m_origin;
	float q = length2(ldir);
	float r = dot(ldir, _ray.m_direction);
	float s = dot(ldir, p);
	float t = dot(_ray.m_direction, p);

	float sn, sd, tn, td;
	float denom = q - r * r;
	if (denom < FLT_EPSILON) {
		sd = td = 1.0f;
		sn = 0.0f;
		tn = t;
	} else {
		sd = td = denom;
		sn = r * t - s;
		tn = q * t - r * s;
		if (sn < 0.0f) {
		    sn = 0.0f;
		    tn = t;
		    td = 1.0f;
		} else if (sn > sd) {
			sn = sd;
			tn = t + r;
			td = 1.0f;
		}
	}

	float ts;
	if (tn < 0.0f) {
		tr_ = 0.0f;
		if (r >= 0.0f) {
		    ts = 0.0f;
		} else if (s <= q) {
		    ts = 1.0f;
		} else {
		    ts = -s / q;
		}
	} else {
		tr_ = tn / td;
		ts = sn / sd;
	}
	return _segment.m_start + ts * ldir;
}
float frm::Distance2(const Line& _line, const vec3& _point)
{
	vec3 p = _point - _line.m_origin;
	float q = dot(p, _line.m_direction);
	return length2(p) - (q * q);
}
float frm::Distance2(const Ray& _ray, const vec3& _point)
{
	vec3 p = _point - _ray.m_origin;
	float q = dot(p, _ray.m_direction);
	if (q < 0.0f) {
		return length2(p);
	}
	return length2(p) - (q * q);
}
float frm::Distance2(const LineSegment& _segment, const vec3& _point)
{
	vec3 dir = _segment.m_end - _segment.m_start;
	vec3 p = _point - _segment.m_start;
	float q = dot(p, dir);
	if (q <= 0.0f) {
		return length2(p);
	} else {
		float r = length2(dir);
		if (q >= r) {
			return length2(_point - _segment.m_end);
		} else {
			return length2(p) - q * q / r;
		}
	}
}
float frm::Distance2(const Line& _line0, const Line& _line1)
{
	vec3 p = _line0.m_origin - _line1.m_origin;
	float q = dot(_line0.m_direction, _line1.m_direction);
	float s = dot(_line1.m_direction, p);

	float d = 1.0f - q * q;
	if (d < FLT_EPSILON) { // lines are parallel
		return length2(p - s * _line1.m_direction);
	} else {
		float r = dot(_line0.m_direction, p);		
		return length2(p + ((q * s - r) / d) * _line0.m_direction - ((s - q * r) / d) * _line1.m_direction);
	}
}
float frm::Distance2(const LineSegment& _segment0, const LineSegment& _segment1)
{
	vec3 dir0 = _segment0.m_end - _segment0.m_start;
	vec3 dir1 = _segment1.m_end - _segment1.m_start;
	vec3 p = _segment0.m_start - _segment1.m_start;
	float q = length2(dir0);
	float r = dot(dir0, dir1);
	float s = length2(dir1);
	float t = dot(dir0, p);
	float u = dot(dir1, p);

	float denom = q * s - r * r;
	float sn, sd, tn, td;

	if (denom < FLT_EPSILON) {
		sd = td = s;
		sn = 0.0f;
		tn = u;
	} else {
		sd = td = denom;
		sn = r * u - s * t;
		tn = q * u - r * t;
	
		if (sn < 0.0f) {
			sn = 0.0f;
			tn = u;
			td = s;
		} else if (sn > sd) {
			sn = sd;
			tn = u + r;
			td = s;
		}
	}
	
	float t0, t1;
	if (tn < 0.0f) {
		t1 = 0.0f;
		if (-t < 0.0f) {
			t0 = 0.0f;
		} else if (-t > q) {
			t0 = 1.0f;
		} else {
			t0 = -t / q;
		}
	} else if (tn > td) {
	    t1 = 1.0f;
	    if ((-t + r) < 0.0f) {
			t0 = 0.0f;
	    } else if ((-t + r) > q) {
			t0 = 1.0f;
	    } else {
			t0 = (-t + r) / q;
	    }
	} else {
		t0 = sn / sd;
		t1 = tn / td;
	}
	
	vec3 wc = p + t0 * dir0 - t1 * dir1;
	return length2(wc);
}
float frm::Distance2(const Ray& _ray, const LineSegment& _segment)
{
	float tr;
	vec3 p = Nearest(_ray, _segment, tr);
	return length2(_ray.m_origin + tr * _ray.m_direction - p);
}
float frm::Distance(const Plane& _plane, const vec3& _point)
{ 
	return dot(_plane.m_normal, _point) - _plane.m_offset; 
}
float frm::Distance2(const AlignedBox& _box, const vec3& _point)
{
	float ret = 0.0f;
	for (int i = 0; i < 3; ++i) {
		float v = _point[i];
		float d = 0.0f;
		if (v < _box.m_min[i]) {
			d = _box.m_min[i] - v;
		}
		if (v > _box.m_max[i]) {
			d = v - _box.m_max[i];
		}
		ret += d * d;
	}
	return ret;
}

inline static void OrderByMagnitude(float& _t0_, float& _t1_)
{
	float a = _t0_;
	float b = _t1_;
	if (fabs(a) < fabs(b)) {
		_t0_ = a;
		_t1_ = b;
	} else {
		_t1_ = a;
		_t0_ = b;
	}
}

inline static bool SolveQuadratic(float _a, float _b, float _c, float& x0_, float &x1_) 
{
	float d = _b * _b - 4.0f * _a * _c; 
    if (d <= 0.0f) {
		return false;
	}
	d = sqrtf(d);
	float q = 0.5f * (_b + copysignf(1.0f, _b) * d);
	x0_ = _c / q;
	x1_ = q / _a;
	return true; 
} 


// Line-primitive intersection

bool frm::Intersects(const Line& _line, const Sphere& _sphere)
{
	vec3 p = _sphere.m_origin - _line.m_origin;
	float b = 2.0f * dot(_line.m_direction, p);
	float c = length2(p) - (_sphere.m_radius * _sphere.m_radius);
	float d = b * b - 4.0f * c;
	return d > 0.0f;
}
bool frm::Intersect(const Line& _line, const Sphere& _sphere, float& t0_, float& t1_)
{
	vec3 p = _sphere.m_origin - _line.m_origin;
	float a = 1.0f;//length2(_r.m_direction);
	float b = 2.0f * dot(_line.m_direction, p);
	float c = length2(p) - (_sphere.m_radius * _sphere.m_radius);
	return SolveQuadratic(a, b, c, t0_, t1_);
}
bool frm::Intersects(const Line& _line, const Plane& _plane)
{
	return dot(_line.m_direction, _plane.m_normal) != 0.0f;
}
bool frm::Intersect(const Line& _line, const Plane& _plane, float& t0_)
{
	float x = dot(_plane.m_normal, _line.m_direction);
	t0_ = dot(_plane.m_normal, (_plane.m_normal * _plane.m_offset) - _line.m_origin) / x;
	return x != 0.0f;
}
bool frm::Intersects(const Line& _line, const AlignedBox& _box)
{
 // \todo cheaper version?
	float t0, t1;
	return Intersect(_line, _box, t0, t1);
}
bool frm::Intersect(const Line& _line, const AlignedBox& _box, float& t0_, float& t1_)
{
	vec3 omin = (_box.m_min - _line.m_origin) / _line.m_direction;
	vec3 omax = (_box.m_max - _line.m_origin) / _line.m_direction;
	vec3 tmax = apt::max(omax, omin);
	vec3 tmin = apt::min(omax, omin);
	t1_ = apt::min(tmax.x, apt::min(tmax.y, tmax.z));
	t0_ = apt::max(tmin.x, apt::max(tmin.y, tmin.z));
	if (t1_ < t0_) {
		return false;
	}
	OrderByMagnitude(t0_, t1_);
	return true;
}
bool frm::Intersects(const Line& _line, const Capsule& _capsule)
{
	return false;
}
bool frm::Intersect(const Line& _line, const Capsule& _capsule, float& t0_, float& t1_)
{
	vec3 cdir = normalize(_capsule.m_end - _capsule.m_start);
	vec3 cc = (_capsule.m_start + _capsule.m_end) * 0.5f;
	vec3 oc = _line.m_origin - cc;
	float r2 = _capsule.m_radius * _capsule.m_radius;
	float ch = length(_capsule.m_end - _capsule.m_start) * 0.5f;
	float card = dot(cdir, _line.m_direction);
	float caoc = dot(cdir,oc);

	float a = 1.0f - card*card;
	float b = dot( oc, _line.m_direction) - caoc * card;
	float c = length2(oc) - caoc * caoc - r2;
	float d = b * b - a * c;
	if (d <= 0.0f) {
		return false;
	}
	d = sqrtf(d);

    float t = (-b - d) / a;
	float y = caoc + t * card;

    if (fabs(y) < ch) {
	 // t0 intersects the body
		t0_ = t;
		t1_ = (-b + d) / a; // second solution for the infinite cylinder

	 // t1 may intersect a cap
		oc = _line.m_origin - (cc + copysignf(1.0f, y) * cdir * ch);
		b = dot(_line.m_direction, oc);
		c = length2(oc) - r2;
		d = b * b - c;
		if (d > 0.0f) {
			d = sqrtf(d);
			t1_ = -b + d;
			//t1_ = apt::min(t1_, -b + d);
		}

		return true;
	}

	oc = _line.m_origin - (cc + copysignf(1.0f, y) * cdir * ch);
	b = dot(_line.m_direction, oc);
	c = length2(oc) - r2;
	d = b * b - c;
	if (d > 0.0f) {
	 // t0 intersects a cap
		d = sqrtf(d);
		t0_ = -b - d;

	 // t1 may intersect the body
		t1_ = apt::max(t1_, -b + d);
		return true;
    }
	return false;

	/*vec3 cdir = _capsule.m_end - _capsule.m_start;
	vec3 p = _capsule.m_start - _line.m_origin;
	vec3 q = cross(p, cdir);
	vec3 r = cross(_line.m_direction, cdir);
	float a = length2(r);
	float b = 2.0f * dot(r, q);
	float c = length2(q) - (_capsule.m_radius * _capsule.m_radius * length2(cdir));
	if (SolveQuadratic(a, b, c, t0_, t1_)) { // intersects the infinite cylinder
	 // clamp t at endpoint spheres
		return true;//t0_ != t1_;
	}
	return false;*/
}
bool frm::Intersects(const Line& _line, const Cylinder& _cylinder)
{
 // \todo cheaper version?
	float t0, t1;
	return Intersect(_line, _cylinder, t0, t1);
}
bool frm::Intersect(const Line& _line, const Cylinder& _cylinder, float& t0_, float& t1_)
{
	vec3 cdir = _cylinder.m_end - _cylinder.m_start;
	vec3 p = _cylinder.m_start - _line.m_origin;
	vec3 q = cross(p, cdir);
	vec3 r = cross(_line.m_direction, cdir);
	float a = length2(r);
	float b = 2.0f * dot(r, q);
	float c = length2(q) - (_cylinder.m_radius * _cylinder.m_radius * length2(cdir));
	if (SolveQuadratic(a, b, c, t0_, t1_)) { // intersects the infinite cylinder
	 // clamp t at endpoint planes
		vec3 nrm = normalize(cdir);
		float t2, t3;
		Intersect(_line, Plane(nrm, _cylinder.m_end), t2);
		Intersect(_line, Plane(-nrm, _cylinder.m_start), t3);
		if (t3 < t2) {
			t0_ = apt::clamp(t0_, t3, t2);
			t1_ = apt::clamp(t1_, t3, t2);
		} else {
			t0_ = apt::clamp(t0_, t2, t3);
			t1_ = apt::clamp(t1_, t2, t3);
		}
		OrderByMagnitude(t0_, t1_);
		
		return t0_ != t1_;
	}
	return false;
}


// Ray-primitive intersection

bool frm::Intersects(const Ray& _ray, const Sphere& _sphere)
{
	vec3  p  = _sphere.m_origin - _ray.m_origin;
	float p2 = length2(p);
	float q  = dot(p, _ray.m_direction);
	float r2 = _sphere.m_radius * _sphere.m_radius;
	if (q < 0.0f && p2 > r2) {
		return false;
	}
	return p2 - (q * q) <= r2;
}
bool frm::Intersect(const Ray& _ray, const Sphere& _sphere, float& t0_, float& t1_)
{
	if (!frm::Intersect(Line(_ray.m_origin, _ray.m_direction), _sphere, t0_, t1_)) {
		return false;
	}
	if (t0_ < 0.0f && t1_ < 0.0f) { // sphere behind ray origin
		return false;
	} else if (t0_ < 0.0f || t1_ < 0.0f) { // ray origin inside sphere
		float t = apt::max(t0_, t1_);
		t0_ = t1_ = t;
	}
	return true;
}
bool frm::Intersects(const Ray& _ray, const Plane& _plane)
{
	float t0 = dot(_plane.m_normal, (_plane.m_normal * _plane.m_offset) - _ray.m_origin) / dot(_plane.m_normal, _ray.m_direction);
	return t0 >= 0.0f;
}
bool frm::Intersect(const Ray& _ray, const Plane& _plane, float& t0_)
{
	t0_ = dot(_plane.m_normal, (_plane.m_normal * _plane.m_offset) - _ray.m_origin) / dot(_plane.m_normal, _ray.m_direction);
	return t0_ >= 0.0f;
}
bool frm::Intersects(const Ray& _ray, const AlignedBox& _box)
{
 // \todo cheaper version?
	float t0, t1;
	return Intersect(_ray, _box, t0, t1);
}
bool frm::Intersect(const Ray& _ray, const AlignedBox& _box, float& t0_, float& t1_)
{
	vec3 omin = (_box.m_min - _ray.m_origin) / _ray.m_direction;
	vec3 omax = (_box.m_max - _ray.m_origin) / _ray.m_direction;
	vec3 tmax = apt::max(omax, omin);
	vec3 tmin = apt::min(omax, omin);
	t1_ = apt::min(tmax.x, apt::min(tmax.y, tmax.z));
	t0_ = apt::max(tmin.x, apt::max(tmin.y, tmin.z));
	if (t0_ >= t1_) {
		return false;
	}
	if (t0_ < 0.0f && t1_ < 0.0f) { // box behind ray origin
		return false;
	} else if (t0_ < 0.0f || t1_ < 0.0f) { // ray origin inside box
		float t = apt::max(t0_, t1_);
		t0_ = t1_ = t;
	}
	return true;
}
bool frm::Intersects(const Ray& _ray, const Capsule& _capsule)
{
	float c2 = _capsule.m_radius * _capsule.m_radius;
	return Distance2(_ray, LineSegment(_capsule.m_start, _capsule.m_end)) < c2;
}
bool frm::Intersect(const Ray& _ray, const Capsule& _capsule, float& t0_, float& t1_)
{
	float tr;
	vec3 ps = Nearest(_ray, LineSegment(_capsule.m_start, _capsule.m_end), tr);
	
t0_ = tr;
t1_ = length(ps - _capsule.m_start);
	return true;
}
bool frm::Intersects(const Ray& _ray, const Cylinder& _cylinder)
{
 // \todo cheaper version?
	float t0, t1;
	return Intersect(_ray, _cylinder, t0, t1);
}
bool frm::Intersect(const Ray& _ray, const Cylinder& _cylinder, float& t0_, float& t1_)
{
	if (!frm::Intersect(Line(_ray.m_origin, _ray.m_direction), _cylinder, t0_, t1_)) {
		return false;
	}
	if (t0_ < 0.0f && t1_ < 0.0f) { // sphere behind ray origin
		return false;
	} else if (t0_ < 0.0f || t1_ < 0.0f) { // ray origin inside sphere
		float t = apt::max(t0_, t1_);
		t0_ = t1_ = t;
	}
	return true;
}


// Primitive-primitive intersection

bool frm::Intersects(const Sphere& _sphere0, const Sphere& _sphere1)
{
	float rsum = _sphere0.m_radius + _sphere1.m_radius;
	return length2(_sphere0.m_origin  - _sphere1.m_origin) < (rsum * rsum);
}

bool frm::Intersects(const Sphere& _sphere, const Plane& _plane)
{
	return abs(Distance(_plane, _sphere.m_origin)) < _sphere.m_radius;
}

bool frm::Intersects(const Sphere& _sphere, const AlignedBox& _box)
{	
	float d2 = Distance2(_box, _sphere.m_origin);
	return d2 < (_sphere.m_radius * _sphere.m_radius);
}

bool frm::Intersects(const AlignedBox& _box0, const AlignedBox& _box1)
{
	for (int i = 0; i < 3; ++i) {
		if (_box0.m_min[i] > _box1.m_max[i] || _box1.m_min[i] > _box0.m_max[i]) {
			return false;
		}
	}
	return true;
}

bool frm::Intersects(const AlignedBox& _box, const Plane& _plane)
{
	vec3 dmax, dmin;
	for (int i = 0; i < 3; ++i) {
		if (_plane.m_normal[i] >= 0.0f) {
			dmin[i] = _box.m_min[i];
			dmax[i] = _box.m_max[i];
		} else {
			dmin[i] = _box.m_max[i];
			dmax[i] = _box.m_min[i];
		}
	}

	float d = dot(_plane.m_normal, dmin) - _plane.m_offset;
	if (d > 0.0f) {
		return false;
	}	
	d = dot(_plane.m_normal, dmax) - _plane.m_offset;
	if (d >= 0.0f) {
		return true;
	}

	return false;
}
