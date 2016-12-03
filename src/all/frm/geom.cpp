#include <frm/geom.h>

#include <frm/math.h>

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
	m_origin += vec3(column(_mat, 3)); 
	m_direction = mat3(_mat) * m_direction;
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
	m_origin += vec3(column(_mat, 3)); 
	m_direction = mat3(_mat) * m_direction;
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
	m_origin = m_origin + vec3(column(_mat, 3));
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
	m_normal = normalize(cross(u, v));
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

Frustum::Frustum(float _aspect, float _tanHalfFov, float _clipNear, float _clipFar)
{
	/*if (_isOrtho) {
		float right = tanHalfFov * _clipFar * _aspect;
		float up = tanHalfFov * _clipFar;
	 // near plane
		m_vertices[0] = vec3( right,  up, -_clipNear);
		m_vertices[1] = vec3(-right,  up, -_clipNear);
		m_vertices[2] = vec3(-right, -up, -_clipNear);
		m_vertices[3] = vec3( right, -up, -_clipNear);
	 // far plane
		m_vertices[4] = vec3( right,  up, -_clipFar);
		m_vertices[5] = vec3(-right,  up, -_clipFar);
		m_vertices[6] = vec3(-right, -up, -_clipFar);
		m_vertices[7] = vec3( right, -up, -_clipFar);
	} else {*/
	 	float ny = _tanHalfFov * _clipNear;
		float nx = _aspect * ny;
	 // near plane
		m_vertices[0] = vec3( nx,  ny, -_clipNear);
		m_vertices[1] = vec3(-nx,  ny, -_clipNear);
		m_vertices[2] = vec3(-nx, -ny, -_clipNear);
		m_vertices[3] = vec3( nx, -ny, -_clipNear);
	//	far plane
		float fy = _tanHalfFov * _clipFar;
		float fx = _aspect * fy;
		m_vertices[4] = vec3( fx,  fy, -_clipFar);
		m_vertices[5] = vec3(-fx,  fy, -_clipFar);
		m_vertices[6] = vec3(-fx, -fy, -_clipFar);
		m_vertices[7] = vec3( fx, -fy, -_clipFar);
	/*}*/

	initPlanes();
}

Frustum::Frustum(float _tanFovUp, float _tanFovDown, float _tanFovLeft, float _tanFovRight, float _clipNear, float _clipFar, bool _isOrtho)
{
	float yt =  _tanFovUp;
	float yb = -_tanFovDown;
	float xl = -_tanFovLeft;
	float xr =  _tanFovRight;

 // near plane
	float nt = yt;
	float nb = yb;
	float nl = xl;
	float nr = xr;
	if (!_isOrtho) {
		nt *= _clipNear;
		nb *= _clipNear;
		nl *= _clipNear;
		nr *= _clipNear;
	}
	m_vertices[0] = vec3(nl, nt, -_clipNear);
	m_vertices[1] = vec3(nr, nt, -_clipNear);
	m_vertices[2] = vec3(nr, nb, -_clipNear);
	m_vertices[3] = vec3(nl, nb, -_clipNear);

 // far plane
	float ft = yt;
	float fb = yb;
	float fl = xl;
	float fr = xr;
	if (!_isOrtho) {
		ft *= _clipFar;
		fb *= _clipFar;
		fl *= _clipFar;
		fr *= _clipFar;
	}
	m_vertices[4] = vec3(fl, ft, -_clipFar);
	m_vertices[5] = vec3(fr, ft, -_clipFar);
	m_vertices[6] = vec3(fr, fb, -_clipFar);
	m_vertices[7] = vec3(fl, fb, -_clipFar);

	initPlanes();
}

Frustum::Frustum(const mat4& _invMat)
{
 // transform an NDC box by the inverse matrix
	static const vec4 lv[8] = {
		vec4( 1.0f,  1.0f, -1.0f,  1.0f),
		vec4(-1.0f,  1.0f, -1.0f,  1.0f),
		vec4(-1.0f, -1.0f, -1.0f,  1.0f),
		vec4( 1.0f, -1.0f, -1.0f,  1.0f),

		vec4( 1.0f,  1.0f,  1.0f,  1.0f),
		vec4(-1.0f,  1.0f,  1.0f,  1.0f),
		vec4(-1.0f, -1.0f,  1.0f,  1.0f),
		vec4( 1.0f, -1.0f,  1.0f,  1.0f)
	};
	vec3 lvt[8];
	for (int i = 0; i < 8; ++i) {
		vec4 v = _invMat * lv[i];
		v /= v.w;
		lvt[i] = vec3(v);
	}
	setVertices(lvt);
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

void Frustum::transform(const mat4& _mat)
{
	for (int i = 0; i < 6; ++i) {
		m_planes[i].transform(_mat);
	}
	for (int i = 0; i < 8; ++i) {
		m_vertices[i] = vec3(_mat * vec4(m_vertices[i], 1.0f));
	}
}

bool Frustum::intersect(const Sphere& _sphere) const
{
	for (int i = 0; i < 6; ++i) {
		if (!Intersects(_sphere, m_planes[i])) {
			return false;
		}
	}
	return true;
}

bool Frustum::intersect(const AlignedBox& _box) const
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
			if (Distance(m_planes[i], points[j]) < 0.0f) {
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

void Frustum::setVertices(const vec3* _vertices)
{
	for (int i = 0; i < 8; ++i) {
		m_vertices[i] = _vertices[i];
	}
	initPlanes();
}

void Frustum::initPlanes()
{
	m_planes[kNear]   = Plane(m_vertices[2], m_vertices[1], m_vertices[0]);
	m_planes[kFar]    = Plane(m_vertices[4], m_vertices[5], m_vertices[6]);
	m_planes[kTop]    = Plane(m_vertices[1], m_vertices[5], m_vertices[4]);
	m_planes[kRight]  = Plane(m_vertices[6], m_vertices[5], m_vertices[1]);
	m_planes[kBottom] = Plane(m_vertices[3], m_vertices[7], m_vertices[6]);
	m_planes[kLeft]   = Plane(m_vertices[3], m_vertices[0], m_vertices[4]);
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

bool frm::Intersects(const Ray& _r, const Sphere& _s)
{
	vec3 p = _s.m_origin - _r.m_origin;
	float p2 = length2(p);
	float q = dot(p, _r.m_direction);
	float r2 = _s.m_radius * _s.m_radius;
	if (q < 0.0f && p2 > r2) {
		return false;
	}
	return p2 - (q * q) <= r2;
}
bool frm::Intersect(const Ray& _r, const Sphere& _s, float& t0_, float& t1_)
{
	vec3 p = _s.m_origin - _r.m_origin; 
	float q = dot(p, _r.m_direction); 
	if (q < 0.0f) {
		return false;
	}
	float p2 = length2(p) - q * q; 
	float r2 = _s.m_radius * _s.m_radius;
	if (p2 > r2) {
		return false;
	}
	float s = sqrt(r2 - p2); 
	t0_ = apt::max(q - s, 0.0f);
	t1_ = q + s;
	 
	return true;
}
bool frm::Intersects(const Ray& _r, const AlignedBox& _b)
{
 // \todo cheaper version?
	float t0, t1;
	return Intersect(_r, _b, t0, t1);
}
bool frm::Intersect(const Ray& _r, const AlignedBox& _b, float& t0_, float& t1_)
{
	vec3 omin = (_b.m_min - _r.m_origin) / _r.m_direction;
	vec3 omax = (_b.m_max - _r.m_origin) / _r.m_direction;
	vec3 tmax = apt::max(omax, omin);
	vec3 tmin = apt::min(omax, omin);
	t1_ = apt::min(tmax.x, apt::min(tmax.y, tmax.z));
	t0_ = apt::max(apt::max(tmin.x, 0.0f), apt::max(tmin.y, tmin.z));
	return t1_ > t0_;
}

bool frm::Intersects(const Ray& _r, const Plane& _p)
{
	float x = dot(_p.m_normal, _r.m_direction);
	return x <= 0.0f;
}
bool frm::Intersect(const Ray& _r, const Plane& _p, float& t0_)
{
	t0_ = dot(_p.m_normal, (_p.m_normal * _p.m_offset) - _r.m_origin) / dot(_p.m_normal, _r.m_direction);
	return t0_ >= 0.0f;
}

bool frm::Intersects(const Ray& _r, const Cylinder& _c)
{
	APT_ASSERT(false); // \todo
	return false;
}
bool frm::Intersect(const Ray& _r, const Cylinder& _c, float& t0_, float& t1_)
{
	APT_ASSERT(false); // \todo
	return false;
}

bool frm::Intersects(const Ray& _r, const Capsule& _c)
{
	return Distance2(_r, LineSegment(_c.m_start, _c.m_end)) < _c.m_radius * _c.m_radius;
}
bool frm::Intersect(const Ray& _r, const Capsule& _c, float& t0_, float& t1_)
{
	APT_ASSERT(false);
	return false;
}


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

	float d = dot(_plane.m_normal, dmin) + _plane.m_offset;
	if (d > 0.0f) {
		return false;
	}
	
	d = dot(_plane.m_normal, dmax) + _plane.m_offset;
	if (d >= 0.0f) {
		return true;
	}

	return false;
}
