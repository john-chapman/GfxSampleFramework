#pragma once
#ifndef frm_Curve_h
#define frm_Curve_h

#include <frm/def.h>
#include <frm/math.h>

#include <EASTL/vector.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// Curve
// 2D curve. This is designed for edit/storage; runtime evaluation should use 
// a piecewise linear approximation which is cheaper to evaluate.
// 
// The curve representation is flat list of 'endpoints' (EP); each EP contains 
// 3 components: the 'value point' (VP) through which the curve will pass, plus 
// 2 'control points' (CP) which describe the in/out tangent of the curve at
// the VP.
//
// When sampling, CPs are constrained to lie within their containing segment. This 
// is necessary to ensure a 1:1 maping between the curve input and output (loops
// are prohibited).
//
// \todo Better tangent estimation on insert().
// \todo Allow decoupled CPs (to create cusps).
////////////////////////////////////////////////////////////////////////////////
class Curve
{
public:
	typedef int EndpointId;

	enum Component
	{
		Component_In,
		Component_Value,
		Component_Out,

		Component_Count
	};

	struct Endpoint
	{
		vec2 m_in;
		vec2 m_value;
		vec2 m_out;

		vec2& operator[](Component _component) { return (&m_in)[_component]; }
	};

	// Insert a new endpoint with the given value, return its index.
	EndpointId insert(const vec2& _value);

	// Move the specified component on endpoint by setting its value, return the new index.
	EndpointId move(EndpointId _endpoint, Component _component, const vec2& _value);

	// Erase the specified endpoint.
	void       erase(EndpointId _endpoint);

private:
	eastl::vector<Endpoint> m_endpoints;
	vec2 m_endpointMin, m_endpointMax; // endpoint bounding box, including CPs

}; // class Curve

} // namespace frm

#endif // frm_Curve_h
