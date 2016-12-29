#pragma once
#ifndef frm_Spline_h
#define frm_Spline_h

#include <frm/def.h>
#include <frm/math.h>

#include <vector>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
/// \class SplinePath
/// Spline path with cubic interpolation.
/// \todo Avoid clamping indices everywhere by maintaining dummy positions at 
///   the start/end.
////////////////////////////////////////////////////////////////////////////////
class SplinePath
{
public:
	SplinePath();

	/// Evaluate the spline at _t (in [0,1]). _hint_ is useful in the common 
	/// case where evaluate() is called repeatedly with a monotonically increasing
	/// _t, it avoids performing a binary search on the spline data.
	vec3 evaluate(float _t, int* _hint_ = nullptr) const;

	/// Append a control point to the spline. This invalidates the internal derived
	/// data, so build() must be called again before using the spline.
	void append(const vec3& _position);

	/// Construct derived members (evaluation metadata, spline length).
	void build();

	void edit();
	bool serialize(apt::JsonSerializer& _serializer_);

	float getLength() const { return m_length; }

private:
	std::vector<vec3> m_raw;    //< Raw control points (for edit/serialize).
	std::vector<vec4> m_eval;   //< Subdivided spline (for evaluation). xyz = position, w = normalized segment start.
	float             m_length; //< Total spline length.

	void subdiv(int _segment, float _t0 = 0.0f, float _t1 = 1.0f, float _maxError = 1e-6f, int _limit = 5);
	
	void getClampIndices(int _i, int& i0_, int& i1_, int& i2_, int& i3_) const;

}; // class Spline

} // namespace frm

#endif // frm_Spline_h
