#pragma once
#ifndef frm_geom_h
#define frm_geom_h

#include <frm/def.h>
#include <frm/math.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
/// \class Line
/// Extends to infinity from m_origin in ±m_direciton.
////////////////////////////////////////////////////////////////////////////////
struct Line
{
	vec3 m_origin;
	vec3 m_direction; //< Must be unit length.

	Line() {}
	Line(const vec3& _origin, const vec3& _direction);

	void transform(const mat4& _mat);

}; // struct Line


////////////////////////////////////////////////////////////////////////////////
/// \class Ray
/// Extends to infinity from m_origin in +m_direction (i.e. a line bounded at 
/// the origin).
////////////////////////////////////////////////////////////////////////////////
struct Ray
{
	vec3 m_origin;
	vec3 m_direction; //< Must be unit length.

	Ray() {}
	Ray(const vec3& _origin, const vec3& _direction);
	
	void transform(const mat4& _mat);

}; // struct Ray


////////////////////////////////////////////////////////////////////////////////
/// \class LineSegment
////////////////////////////////////////////////////////////////////////////////
struct LineSegment
{
	vec3 m_start;
	vec3 m_end;

	LineSegment() {}
	LineSegment(const vec3& _start, const vec3& _end);

	void transform(const mat4& _mat);
	
}; // struct LineSegment


////////////////////////////////////////////////////////////////////////////////
/// \class Sphere
////////////////////////////////////////////////////////////////////////////////
struct Sphere
{
	vec3  m_origin;
	float m_radius;

	Sphere() {}
	Sphere(const vec3& _origin, float _radius);
	Sphere(const AlignedBox& _box);

	void transform(const mat4& _mat);
	
}; // struct Sphere


////////////////////////////////////////////////////////////////////////////////
/// \class Plane
////////////////////////////////////////////////////////////////////////////////
struct Plane
{
	vec3  m_normal;
	float m_offset;

	Plane() {}
	Plane(const vec3& _normal, float _offset);
	Plane(const vec3& _normal, const vec3& _origin);

	/// Generalized plane equation.
	Plane(float _a, float _b, float _c, float _d);

	/// 3 coplanar points. The winding of the points determines the normal 
	/// orientation; the normal points outward from a clockwise winding
	/// of the inputs:
	///    1
	///   / \
	///  / ----->N
	/// /     \
	/// 0------2
	Plane(const vec3& _p0, const vec3& _p1, const vec3& _p2);

	void transform(const mat4& _mat);
	vec3 getOrigin() const;

}; // struct Plane


////////////////////////////////////////////////////////////////////////////////
/// \class AlignedBox
////////////////////////////////////////////////////////////////////////////////
struct AlignedBox
{
	vec3 m_min, m_max;

	AlignedBox() {}
	AlignedBox(const vec3& _min, const vec3& _max);
	AlignedBox(const Sphere& _sphere);
	AlignedBox(const Frustum& _frustum);

	void transform(const mat4& _mat);	
	vec3 getOrigin() const;

	// Generate 8 vertices, write to out_.
	void getVertices(vec3* out_) const;

}; // struct AlignedBox


////////////////////////////////////////////////////////////////////////////////
/// \class Cylinder
////////////////////////////////////////////////////////////////////////////////
struct Cylinder
{
	vec3  m_start;
	vec3  m_end;
	float m_radius;

	Cylinder() {}
	Cylinder(const vec3& _start, const vec3& _end, float _radius);

	void transform(const mat4& _mat);
	vec3 getOrigin() const;

}; // struct Cylinder


////////////////////////////////////////////////////////////////////////////////
/// \class Capsule
////////////////////////////////////////////////////////////////////////////////
struct Capsule
{
	vec3  m_start;
	vec3  m_end;
	float m_radius;
	
	Capsule() {}
	Capsule(const vec3& _start, const vec3& _end, float _radius);

	void transform(const mat4& _mat);
	vec3 getOrigin() const;

}; // struct Capsule


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
	Frustum(
		float _up,          // tan(fov) if ortho
		float _down,        //        "
		float _left,        //        "
		float _right,       //        "
		float _clipNear,
		float _clipFar,
		bool  _isOrtho = false
		);

	/// Construct from the inverse of a matrix (e.g. a projection matrix).
	Frustum(const mat4& _invMat);

	/// Construct from a left/right eye frustum (combined frustum for VR).
	Frustum(const Frustum& _left, const Frustum& _right);

	/// Construct from _base, adjusting the near/far clipping planes.
	Frustum(const Frustum& _base, float _clipNear, float _clipFar);

	void transform(const mat4& _mat);

	bool inside(const Sphere& _sphere) const;
	bool inside(const AlignedBox& _box) const;
	
	bool insideIgnoreNear(const Sphere& _sphere) const;

private:
	void setVertices(const vec3* _vertices);
	void initPlanes();

}; // struct Frustum


/// Find the nearest point on a primitive to _point.
vec3 Nearest(const Line& _line, const vec3& _point); 
vec3 Nearest(const Ray& _ray, const vec3& _point);
vec3 Nearest(const LineSegment& _segment, const vec3& _point);
vec3 Nearest(const Sphere& _sphere, const vec3& _point);
vec3 Nearest(const Plane& _plane, const vec3& _point);
vec3 Nearest(const AlignedBox& _box, const vec3& _point);
/// Find point t0_ along _line0 nearest to _line1, and point t1_ along _line1 nearest to _line0.
void Nearest(const Line& _line0, const Line& _line1, float& t0_, float& t1_);
/// Find point tr_ along _ray nearest to _line, and point tl_ along _line nearest to _ray.
void Nearest(const Ray& _ray, const Line& _line, float& tr_, float& tl_);
/// Find point tr_ along _ray nearest to _segment, return point on segment nearest to _ray.
vec3 Nearest(const Ray& _ray, const LineSegment& _segment, float& tr_);

/// Find the square distance/distance between two primitives.
float        Distance2(const Line& _line, const vec3& _point);
inline float Distance (const Line& _line, const vec3& _point)                        { return apt::sqrt(Distance2(_line, _point)); }
float        Distance2(const Ray& _ray, const vec3& _point);
inline float Distance (const Ray& _ray, const vec3& _point)                          { return apt::sqrt(Distance2(_ray, _point)); }
float        Distance2(const LineSegment& _segment, const vec3& _point);
inline float Distance (const LineSegment& _segment, const vec3& _point)              { return apt::sqrt(Distance2(_segment, _point)); }
float        Distance2(const Line& _line0, const Line& _line1);
inline float Distance (const Line& _line0, const Line& _line1)                       { return apt::sqrt(Distance2(_line0, _line1)); }
float        Distance2(const LineSegment& _segment0, const LineSegment& _segment1);
inline float Distance (const LineSegment& _segment0, const LineSegment& _segment1)   { return apt::sqrt(Distance2(_segment0, _segment1)); }
float        Distance2(const Ray& _ray, const LineSegment& _segment);
inline float Distance (const Ray& _ray, const LineSegment& _segment)                 { return apt::sqrt(Distance2(_ray, _segment)); }
float        Distance (const Plane& _plane, const vec3& _point);
float        Distance2(const AlignedBox& _box, const vec3& _point);
inline float Distance (const AlignedBox& _box, const vec3& _point)                   { return apt::sqrt(Distance2(_box, _point)); }

/// Ray-primitive intersection.
/// t0_/t1_ return the first/second intersections. If the ray origin is inside the primitive, t0_ will be 0.
/// Intersects() may be cheaper than Intersect() hence use these functions if t0_/t1_ aren't required.
bool Intersects(const Ray& _r, const Sphere& _s);
bool Intersect (const Ray& _r, const Sphere& _s, float& t0_, float& t1_);
bool Intersects(const Ray& _r, const AlignedBox& _b);
bool Intersect (const Ray& _r, const AlignedBox& _b, float& t0_, float& t1_);
bool Intersects(const Ray& _r, const Plane& _p);
bool Intersect (const Ray& _r, const Plane& _p, float& t0_);
bool Intersects(const Ray& _r, const Cylinder& _c);
bool Intersect (const Ray& _r, const Cylinder& _c, float& t0_, float& t1_);
bool Intersects(const Ray& _r, const Capsule& _c);
bool Intersect (const Ray& _r, const Capsule& _c, float& t0_, float& t1_);

/// Primitive-primitive intersection.
bool Intersects(const Sphere& _sphere0, const Sphere& _sphere1);
bool Intersects(const Sphere& _sphere, const Plane& _plane);
bool Intersects(const Sphere& _sphere,  const AlignedBox& _box);
bool Intersects(const AlignedBox& _box0, const AlignedBox& _box1);
bool Intersects(const AlignedBox& _box, const Plane& _plane);


} // namespace frm

#endif // frm_geom_h
