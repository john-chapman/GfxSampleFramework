#pragma once
#ifndef frm_Spline_h
#define frm_Spline_h

#include <frm/def.h>
#include <frm/math.h>

#include <vector>

namespace frm {

/*	Notes:
	- Different requirements for 'path-based' animation and keyframe animation:
		- Path-based (e.g. camera motion) probably requires a position spline 
		  reparameterized by arc length to maintain a constant speed (you store a
		  second 'time spline').
		
		- Keyframe animation (e.g. for a skeletal animation clip) has explicit
		  timing associated with the data points. 
	- Skeletal animation: 
		- http://blog.demofox.org/2012/09/21/anatomy-of-a-skeletal-animation-system-part-1/
		- Not all animations have a track for all bones, this way you can 'add'
		  animations together.
*/

////////////////////////////////////////////////////////////////////////////////
/// \class SplinePath
/// Spline path with cubic interpolation.
/// \todo Avoid clamping indices everywhere by maintaining dummy positions at 
///   the start/end.
/// \todo Function pointer for interpolation.
////////////////////////////////////////////////////////////////////////////////
class SplinePath
{
public:

	vec3 evaluate(float _t) const;

	void append(const vec3& _position);

	/// Construct derived members (segment metadata, spline length).
	void build();

	void edit();

private:
	struct Segment
	{
		float m_beg;               //< Normalized segment start.
		float m_len;               //< Normalized segment length.
	};
	std::vector<vec3>    m_data;   //< Raw position data.
	std::vector<Segment> m_segs;   //< Segment metadata (m_data.size()-1 segs).
	float                m_length; //< Total length.

	
	/// Find segment containing _t.
	int findSegment(float _t) const;

	/// Interpolate between _p1 and _p2 by _t.
	static vec3 itpl(const vec3& _p0, const vec3& _p1, const vec3& _p2, const vec3& _p3, float _t);

	/// Recursively compute the arc length of a subsection of the curve between
	/// _p1 and _p2.
	float arclen(
		const vec3& _p0, const vec3& _p1, const vec3& _p2, const vec3& _p3,
		float _threshold = FLT_EPSILON,
		float _dbeg = 0.0f, float _dmid = 0.5f, float _dend = 1.0f,
		float _step = 0.25f
		);

	void getClampIndices(int _i, int& p0_, int& p1_, int& p2_, int& p3_) const;

}; // class Spline

} // namespace frm

#endif // frm_Spline_h
