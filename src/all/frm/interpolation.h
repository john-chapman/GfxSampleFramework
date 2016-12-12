#pragma once
#ifndef frm_interpolation_h
#define frm_interpolation_h

#include <frm/def.h>
#include <frm/math.h>

namespace frm {

/// Linear interpolation.
template <typename T>
T lerp(const T& _p0, const T& _p1, float _delta);
float lerp(float _p0, float _p1, float _delta);

/// Normalized linear interpolation.
template <typename T>
T nlerp(const T& _p0, const T& _p1, float _delta);
float nlerp(float _p0, float _p1, float _delta);

/// Spherical linear interpolation.
template <typename T>
T slerp(const T& _p0, const T& _p1, float _delta);
float slerp(float _p0, float _p1, float _delta);

/// Cosine interpolation.
template <typename T>
T coserp(const T& _p0, const T& _p1, float _delta);
float coserp(float _p0, float _p1, float _delta);

/// Cubic interpolation. Decomposes to lerp() for vector types hence only float is provided.
float cuberp(float _p0, float _p1, float _p2, float _p3, float _delta);

/// Smooth interpolation.
template <typename T>
T smooth(const T& _p0, const T& _p1, float _delta);
float smooth(float _p0, float _p1, float _delta);

/// Lerp with more speed at the end of the curve.
template <typename T>
T accelerp(const T& _p0, const T& _p1, float _delta);
float accelerp(float _p0, float _p1, float _delta);

/// Lerp with more speed at the start of the curve.
template <typename T>
T decelerp(const T& _p0, const T& _p1, float _delta);
float decelerp(float _p0, float _p1, float _delta);




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

inline float cuberp(float _p0, float _p1, float _p2, float _p3, float _delta) 
{ 
	float d2 = _delta * _delta;	
	float a0 = _p3 - _p2 - _p0 + _p1;
	float a1 = _p0 - _p1 - a0;
	float a2 = _p2 - _p0;
	float a3 = _p1;
	return a0 * _delta * d2 + a1 * d2 + a2 * _delta + a3;
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

} // namespace frm

#endif // frm_interpolation_h
