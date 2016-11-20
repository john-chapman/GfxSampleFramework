#include <frm/geom.h>

#include <frm/math.h>

using namespace frm;

/*******************************************************************************

                               AlignedBox

*******************************************************************************/

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


/*******************************************************************************

                                 Sphere

*******************************************************************************/

void Sphere::transform(const mat4& _mat)
{
 // extract max scale
	float sx = _mat[0][0] * _mat[0][0] + _mat[0][1] * _mat[0][1] + _mat[0][2] * _mat[0][2];
	float sy = _mat[1][0] * _mat[1][0] + _mat[1][1] * _mat[1][1] + _mat[1][2] * _mat[1][2];
	float sz = _mat[2][0] * _mat[2][0] + _mat[2][1] * _mat[2][1] + _mat[2][2] * _mat[2][2];
	float scale = glm::max(sx, glm::max(sy, sz));
	scale = sqrtf(scale);
	m_radius *= scale;
	m_origin *= scale;

 // translate after scaling
	m_origin = m_origin + vec3(column(_mat, 3));
}


/*******************************************************************************

                                   Ray

*******************************************************************************/

float Ray::distance2(const Line& _l, float& tnear_) const
{
	vec3 w = _l.m_start - m_origin;
	vec3 ldir = _l.m_end - _l.m_start;
	float a = length2(ldir);
	float b = dot(ldir, m_direction);
	float d = dot(ldir, w );
	float e = dot(m_direction, w);

 	float sn, sd, tn, td;
	float sc, tc;
	float denom = a - b * b;
 // if denom is zero, try finding closest point on segment1 to origin0
    if (abs(denom) < FLT_EPSILON) {
	 // clamp s_c to 0
		sd = td = 1.0f;
		sn = 0.0f;
		tn = e;
    } else {
	 // clamp s_c within [0,1]
		sd = td = denom;
		sn = b * e - d;
		tn = a * e - b * d;
  
		if (sn < 0.0f) {
		// clamp s_c to 0
		    sn = 0.0f;
		    tn = e;
		    td = 1.0f;
		} else if (sn > sd) {
		// clamp s_c to 1
		    sn = sd;
		    tn = e + b;
		    td = 1.0f;
		}
	}

    // clamp t_c within [0,+inf]
    // clamp t_c to 0
    if (tn < 0.0f) {
        tc = 0.0f;
        if (-d < 0.0f) {
		 // clamp s_c to 0
            sc = 0.0f;
		} else if (-d > a) {
		 // clamp s_c to 1
		    sc = 1.0f;
		} else {
		    sc = -d / a;
		}
    } else {
        tc = tn / td;
        sc = sn / sd;
    }
	tnear_ = tc;
    return length2(w + sc * ldir - tc * m_direction);
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
		if (m_planes[i].distance(_sphere.m_origin) < -_sphere.m_radius) {
			return false;
		}
	}
	return true;
}

bool Frustum::intersect(const AlignedBox& _box) const
{
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
			if (m_planes[i].distance(points[j]) < 0.0f) {
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

                            Intersection Tests

*******************************************************************************/

bool frm::Intersects(const Ray& _r, const AlignedBox& _b)
{
 // \todo cheaper version
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

bool frm::Intersects(const Ray& _r, const Sphere& _s)
{
	vec3 w = _s.m_origin - _r.m_origin;
	float w2 = length2(w);
	float toa = dot(w, _r.m_direction);
	float r2 = _s.m_radius * _s.m_radius;
	if (toa < 0.0f && w2 > r2) {
		return false;
	}
	return w2 - (toa * toa) <= r2;
}
bool frm::Intersect(const Ray& _r, const Sphere& _s, float& t0_, float& t1_)
{
	vec3 w = _s.m_origin - _r.m_origin; 
	float toa = dot(w, _r.m_direction); 
	if (toa < 0.0f) {
		return false;
	}
	float w2 = length2(w) - toa * toa; 
	float r2 = _s.m_radius * _s.m_radius;
	if (w2 > r2) {
		return false;
	}
	float tho = sqrtf(r2 - w2); 
	t0_ = apt::max(toa - tho, 0.0f);
	t1_ = toa + tho;
	 
	return true;
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
	APT_ASSERT(false);
	return false;
}
bool frm::Intersect(const Ray& _r, const Cylinder& _c, float& t0_, float& t1_)
{
	APT_ASSERT(false);
	return false;
}

bool frm::Intersects(const Ray& _r, const Capsule& _c)
{
	Line ln(_c.m_start, _c.m_end);
	float t0;
	return _r.distance2(ln, t0) < (_c.m_radius * _c.m_radius);
}
bool frm::Intersect(const Ray& _r, const Capsule& _c, float& t0_, float& t1_)
{
	APT_ASSERT(false);
	return false;
}
