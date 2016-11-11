#pragma once
#ifndef frm_geom_h
#define frm_geom_h

#include <frm/def.h>
#include <frm/math.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
/// \class AlignedBox
////////////////////////////////////////////////////////////////////////////////
struct AlignedBox
{
	vec3 m_min, m_max;

	AlignedBox(const vec3& _min = vec3(-FLT_MAX), const vec3& _max = vec3(FLT_MAX))
		: m_min(_min)
		, m_max(_max)
	{
	}

	void transform(const mat4& _mat);
	
	vec3 getOrigin() const 
	{ 
		return m_min + (m_max - m_min) * 0.5f; 
	}

	/// \return Square of the distance of the closest point on the box to _p. 
	float distance2(const vec3& _p) const
	{ 
		float ret = 0.0f;
		for (int i = 0; i < 3; ++i) {
			float v = _p[i];
			float d = 0.0f;
			if (v < m_min[i]) {
				d = m_min[i] - v;
			}
			if (v > m_max[i]) {
				d = v - m_max[i];
			}
			ret += d * d;
		}
		return ret;
	}

	/// \return nearest point to _p which lies on the box.
	vec3 nearest(const vec3& _p) const
	{ 
		vec3 ret;
		for (int i = 0; i < 3; ++i) {
			float v = _p[i];
			if (v < m_min[i]) {
				v = m_min[i];
			}
			if (v > m_max[i]) {
				v = m_max[i];
			}
			ret[i] = v;
		}
		return ret;
	}

};


////////////////////////////////////////////////////////////////////////////////
/// \class Sphere
////////////////////////////////////////////////////////////////////////////////
struct Sphere
{
	vec3  m_origin;
	float m_radius;

	Sphere(const vec3& _origin = vec3(0.0f), float _radius = FLT_MAX)
		: m_origin(_origin)
		, m_radius(_radius)
	{
	}

	Sphere(const AlignedBox& _box)
		: m_origin(_box.getOrigin())
		, m_radius(length(_box.m_max - _box.m_min) * 0.5f)
	{
	};

	void transform(const mat4& _mat);
	
	bool intersect(const Sphere& _s) const
	{
		float rsum = m_radius + _s.m_radius;
		return length2(m_origin - _s.m_origin) < (rsum * rsum);
	}
};

////////////////////////////////////////////////////////////////////////////////
/// \class Plane
////////////////////////////////////////////////////////////////////////////////
struct Plane
{
	vec3  m_normal;
	float m_offset;

	Plane(const vec3& _normal = vec3(0.0f, 0.0f, 1.0f), float _offset = 0.0f)
		: m_normal(_normal)
		, m_offset(_offset)
	{
	}

	Plane(const vec3& _normal, const vec3& _origin)
		: m_normal(_normal)
		, m_offset(dot(_normal, _origin))
	{
	}

	/// Generalized plane equation.
	Plane(float _a, float _b, float _c, float _d)
		: m_normal(_a, _b, _c)
		, m_offset(_d / length2(m_normal))
	{
		m_normal = normalize(m_normal);
	}

	/// 3 coplanar points. The winding of the points determines the normal 
	/// orientation; the normal points points outward from a clockwise winding
	/// of the points:
	///    1
	///   / \
	///  / ----->N
	/// /     \
	/// 0------2
	Plane(const vec3& _p0, const vec3& _p1, const vec3& _p2)
	{
		vec3 u(_p1 - _p0);
		vec3 v(_p2 - _p0);
		m_normal = normalize(cross(u, v));
		m_offset = dot(m_normal, (_p0 + _p1 + _p2) / 3.0f);
	}

	void transform(const mat4& _mat)
	{
		vec3 origin = vec3(_mat * vec4(getOrigin(), 1.0f));
		m_normal = vec3(_mat * vec4(m_normal, 0.0f));
		m_normal = normalize(m_normal);
		m_offset = dot(m_normal, origin);
	}

	/// \return distance of _p from the plane, + if in front, - if behind.
	float distance(const vec3& _p) const
	{ 
		return dot(m_normal, _p) - m_offset; 
	}

	/// \return nearest point to _p which lies on the plane.
	vec3 nearest(const vec3& _p) const
	{ 
		return _p - m_normal * distance(_p); 
	}


	vec3 getOrigin() const
	{ 
		return m_normal * m_offset; 
	}
};


////////////////////////////////////////////////////////////////////////////////
/// \class Line
////////////////////////////////////////////////////////////////////////////////
struct Line
{
	vec3 m_start;
	vec3 m_end;

	Line(const vec3& _start, const vec3& _end)
		: m_start(_start)
		, m_end(_end)
	{
	}

	void transform(const mat4& _mat)
	{
		m_end = mat3(_mat) * (m_end - m_start);
		m_start += vec3(column(_mat, 3)); 
		m_end += m_start;
	}


	/// \return Square of the distance of the closes point on the line segment to _p. 
	float distance2(const vec3& _p)
	{
		vec3 w = _p - m_start;
		vec3 dir = m_end - m_start;
		float proj = dot(w, dir);
		if (proj < 0.0f) {
			return length2(w);
		}

		float ln2 = length2(dir);
		if (proj >= ln2) {
			return length2(w) - 2.0f * proj + ln2;
		} else {
			return length2(w) - proj * proj / ln2;
		}
	}

	/// \return Nearest point to _p which lies on the line, unconstrained by
	///   the endpoints.
	vec3 nearest(const vec3& _p) const
	{
		vec3 dir = m_end - m_start;
		return m_start + (dot(_p - m_start, dir) / length2(dir)) * dir;
	}

	vec3 getOrigin() const
	{
		return (m_end - m_start) * 0.5f;
	}
};

////////////////////////////////////////////////////////////////////////////////
/// \class Capsule
////////////////////////////////////////////////////////////////////////////////
struct Capsule
{
	vec3  m_start;
	vec3  m_end;
	float m_radius;

	Capsule(const vec3& _start, const vec3& _end, float _radius)
		: m_start(_start)
		, m_end(_end)
		, m_radius(_radius)
	{
	}

	void transform(const mat4& _mat)
	{
		Line l(m_start, m_end);
		l.transform(_mat);
		m_start = l.m_start;
		m_end = l.m_end;
	}

	vec3 getOrigin() const
	{
		return m_start + (m_end - m_start) * 0.5f;
	}
};

////////////////////////////////////////////////////////////////////////////////
/// \class Ray
////////////////////////////////////////////////////////////////////////////////
struct Ray
{
	vec3 m_origin;
	vec3 m_direction; //< Must be unit length.

	Ray(const vec3& _origin = vec3(0.0f), const vec3& _direction = vec3(0.0f, 0.0f, 1.0f))
		: m_origin(_origin)
		, m_direction(_direction)
	{
	}

	void transform(const mat4& _mat)
	{
		m_origin = m_origin + vec3(column(_mat, 3));
		m_direction = mat3(_mat) * m_direction;
	}

	/// \return square distance between the nearest points on the ray and line.
	///   tnear_ receives the distance along the ray to the nearest point.
	float distance2(const Line& _l, float& tnear_) const;

	/// \return point on the ray nearest to _p.
	vec3 nearest(const vec3& _p) const
	{
		return m_origin + dot(_p - m_origin, m_direction) * m_direction;
	}

	bool intersect(const Plane& _p, float& tnear_) const
	{
		tnear_ = dot(_p.m_normal, (_p.m_normal * _p.m_offset) - m_origin) / dot(_p.m_normal, m_direction);
		return tnear_ >= 0.0f;
	}

	bool intersect(const Capsule& _c, float& tnear_, float& tfar_) const
	{
		Line ln(_c.m_start, _c.m_end);
		if (distance2(ln, tnear_) < (_c.m_radius * _c.m_radius)) {

			return true;
		}
		return false;
	}

	bool intersect(const Sphere& _s, float& tnear_, float& tfar_) const
	{
		vec3 w = _s.m_origin - m_origin;
		float w2 = length2(w);
		float proj = dot(w, m_direction);
		float r2 = _s.m_radius * _s.m_radius;

		if (proj < 0.0f && w2 > r2) {
			return false;
		}

		return w2 - (proj * proj) <= r2;
	}

	float intersect(const AlignedBox& _b, float& tnear_, float& tfar_) const
	{
		vec3 omin = (_b.m_min - m_origin) / m_direction;
		vec3 omax = (_b.m_max - m_origin) / m_direction;
		vec3 tmax = apt::max(omax, omin);
		vec3 tmin = apt::min(omax, omin);
		tfar_ = apt::min(tmax.x, apt::min(tmax.y, tmax.z));
		tnear_ = apt::max(apt::max(tmin.x, 0.0f), apt::max(tmin.y, tmin.z));
		return tnear_;
	}
};

////////////////////////////////////////////////////////////////////////////////
/// \class Frustum
/// Frustum described by 6 planes/8 vertices. The vertex ordering is as follows:
///
///  4------------5
///  |\          /|
///  7-\--------/-6
///   \ 0------1 /
///    \|      |/
///     3------2
////////////////////////////////////////////////////////////////////////////////
struct Frustum
{
	enum Planes
	{
		kNear = 0,
		kFar,
		kTop,
		kRight,
		kBottom,
		kLeft

	};
	Plane m_planes[6];
	vec3  m_vertices[8];

	Frustum() {}

	/// Symmetrical perspective projection.
	Frustum(
		float _aspect,
		float _tanHalfFov,
		float _clipNear,
		float _clipFar
		);

	/// Asymmetrical perspective projection or orthographic projection.
	/// _tanFov* args are offsets in the orthographic case.
	Frustum(
		float _tanFovUp,
		float _tanFovDown,
		float _tanFovLeft,
		float _tanFovRight,
		float _clipNear,
		float _clipFar,
		bool  _isOrtho = false
		);

	/// Construct from the inverse of a matrix (e.g. a projection matrix).
	Frustum(const mat4& _invMat);

	/// Construct from a left/right eye frustum (combined frustum for VR).
	Frustum(const Frustum& _left, const Frustum& _right);

	void transform(const mat4& _mat);

	bool intersect(const Sphere& _sphere) const;
	bool intersect(const AlignedBox& _box) const;

private:
	void setVertices(const vec3* _vertices);
	void initPlanes();
};

inline bool Intersect(const Sphere& _s0, const Sphere& _s1)
{
	float rsum = _s0.m_radius + _s1.m_radius;
	return length2(_s0.m_origin - _s1.m_origin) < (rsum * rsum);
}

inline bool Intersect(const Sphere& _s, const Plane& _p, float& distance_)
{
	distance_ = _p.distance(_s.m_origin);
	return abs(distance_) < _s.m_radius;
}

inline bool Intersect(const Sphere& _s, const AlignedBox& _b)
{
	float d2 = _b.distance2(_s.m_origin);
	return d2 < (_s.m_radius * _s.m_radius);
}

} // namespace frm

#endif // frm_geom_h
