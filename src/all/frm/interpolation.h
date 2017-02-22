#pragma once
#ifndef frm_interpolation_h
#define frm_interpolation_h

#include <frm/def.h>
#include <frm/math.h>

namespace frm {

// Linear interpolation.
template <typename T>
T lerp(const T& _p0, const T& _p1, float _delta);
float lerp(float _p0, float _p1, float _delta);

// Normalized linear interpolation.
template <typename T>
T nlerp(const T& _p0, const T& _p1, float _delta);
float nlerp(float _p0, float _p1, float _delta);

// Spherical linear interpolation.
template <typename T>
T slerp(const T& _p0, const T& _p1, float _delta);
float slerp(float _p0, float _p1, float _delta);

// Cosine interpolation. Decomposes to lerp() for vector types.
template <typename T>
T coserp(const T& _p0, const T& _p1, float _delta);
float coserp(float _p0, float _p1, float _delta);

// Smooth interpolation.
template <typename T>
T smooth(const T& _p0, const T& _p1, float _delta);
float smooth(float _p0, float _p1, float _delta);

// Lerp with more speed at the end of the curve.
template <typename T>
T accelerp(const T& _p0, const T& _p1, float _delta);
float accelerp(float _p0, float _p1, float _delta);

// Lerp with more speed at the start of the curve.
template <typename T>
T decelerp(const T& _p0, const T& _p1, float _delta);
float decelerp(float _p0, float _p1, float _delta);

// Cubic interpolation.
template <typename T>
T cuberp(const T& _p0, const T& _p1, const T& _p2, const T& _p3, float _delta);
float cuberp(float _p0, float _p1, float _p2, float _p3, float _delta);

// Hermite interpolation.
template <typename T>
T hermite(const T& _p0, const T& _p1, const T& _p2, const T& _p3, float _delta, float _tension = 0.0f, float bias = 0.0f);
float hermite(float _p0, float _p1, float _p2, float _p3, float _delta, float _tension = 0.0f, float bias = 0.0f);

template <typename T>
inline T lerp(const T& _p0, const T& _p1, float _delta) 
{ 
	return _p0 + (_p1 - _p0) * _delta; 
}
inline float lerp(float _p0, float _p1, float _delta) 
{ 
	return _p0 + (_p1 - _p0) * _delta; 
}

template <typename T>
inline T nlerp(const T& _p0, const T& _p1, float _delta) 
{ 
	return normalize(_p0 + (_p1 - _p0) * _delta);
}
inline float nlerp(float _p0, float _p1, float _delta) 
{ 
	return lerp(_p0, _p1, _delta);
}

template <typename T>
inline T slerp(const T& _p0, const T& _p1, float _delta) 
{ 
	float cosH = dot(p0, p1);
	if (abs(cosH) >= 1.0f) { // theta = 0, return p0
		return p0;
	}
		
	float H = acos(cosH);
	float sinH = sqrt(1.0f - cosH * cosH);

	float a, b;
	if (abs(sinH) < FLT_EPSILON) {
		a = b = 0.5f;
	} else {
		a = sin((1.0f - _delta) * H) / sinH;
		b = sin(delta * H) / sinH;
	}
	
	return (_p0 * a) + (_p1 * b);
}
inline float slerp(float _p0, float _p1, float _delta) 
{ 
	return lerp(_p0, _p1, _delta);
}

template <typename T>
inline T coserp(const T& _p0, const T& _p1, float _delta) 
{ 
	_delta = (1.0f - cos(_delta * pi<float>())) * 0.5f;
	return _p0 * (1.0f - _delta) + _p1 * _delta;
}
inline float coserp(float _p0, float _p1, float _delta) 
{ 
	_delta = (1.0f - cos(_delta * pi<float>())) * 0.5f;
	return _p0 * (1.0f - _delta) + _p1 * _delta;
}

template <typename T>
inline T smooth(const T& _p0, const T& _p1, float _delta) 
{ 
	return lerp(_p0, _p1, _delta * _delta * (3.0f - (2.0f * _delta)));
}
inline float smooth(float _p0, float _p1, float _delta) 
{ 
	return lerp(_p0, _p1, _delta * _delta * (3.0f - (2.0f * _delta)));
}

template <typename T>
inline T accelerp(const T& _p0, const T& _p1, float _delta) 
{ 
	return lerp(_p0, _p1, _delta * _delta);
}
inline float accelerp(float _p0, float _p1, float _delta) 
{ 
	return lerp(_p0, _p1, _delta * _delta);
}

template <typename T>
inline T decelerp(const T& _p0, const T& _p1, float _delta) 
{ 
	float rd = 1.0f - _delta;
	return lerp(_p0, _p1, 1.0f - rd * rd);
}
inline float decelerp(float _p0, float _p1, float _delta) 
{ 
	float rd = 1.0f - _delta;
	return lerp(_p0, _p1, 1.0f - rd * rd);
}

template <typename T>
inline T cuberp(const T& _p0, const T& _p1, const T& _p2, const T& _p3, float _delta) 
{ 
	T a0 = _p3 - _p2 - _p0 + _p1;
	T a1 = _p0 - _p1 - a0;
	T a2 = _p2 - _p0;
	T a3 = _p1;
	float d2 = _delta * _delta;	
	return a0 * _delta * d2 + a1 * d2 + a2 * _delta + a3;
}
inline float cuberp(float _p0, float _p1, float _p2, float _p3, float _delta) 
{ 
	float a0 = _p3 - _p2 - _p0 + _p1;
	float a1 = _p0 - _p1 - a0;
	float a2 = _p2 - _p0;
	float a3 = _p1;
	float d2 = _delta * _delta;	
	return a0 * _delta * d2 + a1 * d2 + a2 * _delta + a3;
}

template <typename T>
inline T hermite(const T& _p0, const T& _p1, const T& _p2, const T& _p3, float _delta, float _tension, float _bias) 
{ 
	float d2 = _delta * _delta;
	float d3 = d2 * _delta;

	float a0 = 2.0f * d3 - 3.0f * d2 + 1.0f;
	float a1 = d3 - 2.0f * d2 + _delta;
	float a2 = d3 - d2;
	float a3 = -2.0f * d3 + 3.0f * d2;

	T m0  = (_p1 - _p0) * (1.0f + _bias) * ( 1.0f - _tension) * 0.5f;
	  m0 += (_p2 - _p1) * (1.0f - _bias) * ( 1.0f - _tension) * 0.5f;
	T m1  = (_p2 - _p1) * (1.0f + _bias) * ( 1.0f - _tension) * 0.5f;
	  m1 += (_p3 - _p2) * (1.0f - _bias) * ( 1.0f - _tension) * 0.5f;

	return a0 * _p1 + a1 * m0 + a2 * m1 + a3 * _p2;
}
inline float hermite(float _p0, float _p1, float _p2, float _p3, float _delta, float _tension, float _bias) 
{
	float d2 = _delta * _delta;
	float d3 = d2 * _delta;

	float a0 = 2.0f * d3 - 3.0f * d2 + 1.0f;
	float a1 = d3 - 2.0f * d2 + _delta;
	float a2 = d3 - d2;
	float a3 = -2.0f * d3 + 3.0f * d2;

	float m0  = (_p1 - _p0) * (1.0f + _bias) * ( 1.0f - _tension) * 0.5f;
	      m0 += (_p2 - _p1) * (1.0f - _bias) * ( 1.0f - _tension) * 0.5f;
	float m1  = (_p2 - _p1) * (1.0f + _bias) * ( 1.0f - _tension) * 0.5f;
	      m1 += (_p3 - _p2) * (1.0f - _bias) * ( 1.0f - _tension) * 0.5f;

	return a0 * _p1 + a1 * m0 + a2 * m1 + a3 * _p2;
}

} // namespace frm

#endif // frm_interpolation_h
