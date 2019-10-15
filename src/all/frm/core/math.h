#pragma once

#include "frm.h"

#include <linalg/linalg.h>

namespace frm {
	using linalg::identity;

	typedef linalg::aliases::float2   vec2;
	typedef linalg::aliases::float3   vec3;
	typedef linalg::aliases::float4   vec4;
	typedef linalg::aliases::uint2    uvec2;
	typedef linalg::aliases::uint3    uvec3;
	typedef linalg::aliases::uint4    uvec4;
	typedef linalg::aliases::int2     ivec2;
	typedef linalg::aliases::int3     ivec3;
	typedef linalg::aliases::int4     ivec4;
	typedef linalg::aliases::bool2    bvec2;
	typedef linalg::aliases::bool3    bvec3;
	typedef linalg::aliases::bool4    bvec4;
	typedef linalg::aliases::float2x2 mat2;
	typedef linalg::aliases::float3x3 mat3;
	typedef linalg::aliases::float4x4 mat4;
	typedef linalg::aliases::float4   quat;
	
	namespace internal {
		template<> struct TypeTraits<vec2>      { typedef VecT Family; typedef float32 BaseType; enum { kCount = 2  }; };
		template<> struct TypeTraits<vec3>      { typedef VecT Family; typedef float32 BaseType; enum { kCount = 3  }; };
		template<> struct TypeTraits<vec4>      { typedef VecT Family; typedef float32 BaseType; enum { kCount = 4  }; };
		template<> struct TypeTraits<uvec2>     { typedef VecT Family; typedef uint32  BaseType; enum { kCount = 2  }; };
		template<> struct TypeTraits<uvec3>     { typedef VecT Family; typedef uint32  BaseType; enum { kCount = 3  }; };
		template<> struct TypeTraits<uvec4>     { typedef VecT Family; typedef uint32  BaseType; enum { kCount = 4  }; };
		template<> struct TypeTraits<ivec2>     { typedef VecT Family; typedef sint32  BaseType; enum { kCount = 2  }; };
		template<> struct TypeTraits<ivec3>     { typedef VecT Family; typedef sint32  BaseType; enum { kCount = 3  }; };
		template<> struct TypeTraits<ivec4>     { typedef VecT Family; typedef sint32  BaseType; enum { kCount = 4  }; };
		template<> struct TypeTraits<bvec2>     { typedef VecT Family; typedef bool    BaseType; enum { kCount = 2  }; };
		template<> struct TypeTraits<bvec3>     { typedef VecT Family; typedef bool    BaseType; enum { kCount = 3  }; };
		template<> struct TypeTraits<bvec4>     { typedef VecT Family; typedef bool    BaseType; enum { kCount = 4  }; };
		template<> struct TypeTraits<mat2>      { typedef MatT Family; typedef float32 BaseType; enum { kCount = 4  }; };
		template<> struct TypeTraits<mat3>      { typedef MatT Family; typedef float32 BaseType; enum { kCount = 9  }; };
		template<> struct TypeTraits<mat4>      { typedef MatT Family; typedef float32 BaseType; enum { kCount = 16 }; };
	}

	constexpr float kPi      = 3.14159265359f;
	constexpr float kTwoPi   = 2.0f * kPi;
	constexpr float kHalfPi  = 0.5f * kPi;
}

namespace frm {

	// Transformation helpers.
	mat4 TransformationMatrix(const vec3& _translation, const mat3& _rotationScale);
	mat4 TransformationMatrix(const vec3& _translation, const quat& _rotation, const vec3& _scale = vec3(1.0f));
	mat3 TransformationMatrix(const vec2& _translation, const mat2& _rotationScale);
	mat4 TranslationMatrix(const vec3& _translation);
	mat4 RotationMatrix(const vec3& _axis, float _radians);
	mat4 RotationMatrix(const quat& _q);
	quat RotationQuaternion(const vec3& _axis, float _radians);
	quat RotationQuaternion(const mat3& _rotation);
	mat4 ScaleMatrix(const vec3& _scale);

	// Extract translation/rotation/scale from _m.
	vec3 GetTranslation(const mat4& _m);
	vec2 GetTranslation(const mat3& _m);
	mat3 GetRotation(const mat4& _m);
	mat2 GetRotation(const mat3& _m);
	vec3 GetScale(const mat4& _m);

	// Transform a position or direction by homogeneous matrix _m.
	inline vec3 TransformPosition(const mat4& _m, const vec3& _p)               { return mul(_m, vec4(_p, 1.0f)).xyz(); }
	inline vec2 TransformPosition(const mat3& _m, const vec2& _p)               { return mul(_m, vec3(_p, 1.0f)).xy();  }
	inline vec3 TransformDirection(const mat4& _m, const vec3& _d)              { return mul(_m, vec4(_d, 0.0f)).xyz(); }
	inline vec2 TransformDirection(const mat3& _m, const vec2& _d)              { return mul(_m, vec3(_d, 0.0f)).xy();  }

	// Get an orthonormal bases with X/Y/Z aligned with _axis.
	mat4 AlignX(const vec3& _axis, const vec3& _up = vec3(0.0f, 1.0f, 0.0f));
	mat4 AlignY(const vec3& _axis, const vec3& _up = vec3(0.0f, 1.0f, 0.0f));
	mat4 AlignZ(const vec3& _axis, const vec3& _up = vec3(0.0f, 1.0f, 0.0f));
	
	// Matrix with position = _from and +Z = (_to - _from).
	mat4 LookAt(const vec3& _from, const vec3& _to, const vec3& _up = vec3(0.0f, 1.0f, 0.0f));

	// Convert between radians and degrees.
	inline float Degrees(float _radians)                                        { return _radians * (180.0f / kPi); }
	inline float Radians(float _degrees)                                        { return _degrees * (kPi / 180.0f); }

	// Convert between mat3 and Euler angles (XYZ order).
	vec3 ToEulerXYZ(const mat3& _m);
	mat3 FromEulerXYZ(const vec3& _euler);

	// Convert between spherical and Cartesian coordinates.
	vec3 SphericalToCartesian(float _radius, float _azimuth, float _elevation);
	vec3 CartesianToSpherical(const vec3& _cartesian); // return value is vec3(_radius, _azimuth, _elevation)

	// Return the transpose of a matrix.
	mat4 Transpose(const mat4& _m);
	mat3 Transpose(const mat3& _m);
	mat2 Transpose(const mat2& _m);

	// Return the inverse of a matrix/quaternion.
	mat4 Inverse(const mat4& _m);
	mat3 Inverse(const mat3& _m);
	mat2 Inverse(const mat2& _m);
	quat Inverse(const quat& _q);   // can use Conjugate(_q) if _q is unit length
	quat Conjugate(const quat& _q); // equivalent to Inverse(_q) if _q is unit length

	// Return the inverse of an affine matrix.
	mat4 AffineInverse(const mat4& _m);
	mat3 AffineInverse(const mat3& _m);

	// Normalize a vector.
	template <typename tType>
	inline tType Normalize(const tType& _v)                                     { return linalg::normalize(_v); }

	// Return the length of a vector.
	template <typename tType>
	inline auto Length(const tType& _v)                                         { return linalg::length(_v); }

	// Return the square length of a vector.
	template <typename tType>
	inline auto Length2(const tType& _v)                                        { return linalg::length2(_v); }

	// Return the dot product of _a and _b.
	template <typename tType>
	inline auto Dot(const tType& _a, const tType& _b)                           { return linalg::dot(_a, _b); }
	
	// Return the cross product of _a and _b.
	template <typename tType>
	inline auto Cross(const tType& _a, const tType& _b)                         { return linalg::cross(_a, _b); }

	// Return the fractional part of _x (elementwise for vector/matrix types).
	template <typename tType>
	tType Fract(const tType& _x);

	// Return -1 if _x < 0, else 1 (elementwise for vector/matrix types).
	template <typename tType>
	tType Sign(const tType& _x);

	// Return the absolute value for _x (elementwise for vector/matrix types).
	template <typename tType>
	tType Abs(const tType& _x);

	// Return the largest integer value <= _x (elementwise for vector/matrix types).
	template <typename tType>
	tType Floor(const tType& _x);

	// Return the smallest integer value >= _x (elementwise for vector/matrix types).
	template <typename tType>
	tType Ceil(const tType& _x);

	// Return the remainder of _x / _y.
	template <typename tType>
	inline tType Mod(const tType& _x, const tType& _y)                           { return _x - _y * Floor(_x / _y); }

	// Return the nearest integer to _x, rounding away from zero in halfway cases (elementwise for vector/matrix types).
	template <typename tType>
	tType Round(const tType& _x);

	// Return the min of _a,_b (elementwise for vector/matrix types).
	template <typename tType>
	tType Min(const tType& _a, const tType& _b);
	#define FRM_MIN(_a, _b) frm::Min(_a, _b)

	// Return the max of _a,_b (elementwise for vector/matrix types).
	template <typename tType>
	tType Max(const tType& _a, const tType& _b);
	#define FRM_MAX(_a, _b) frm::Max(_a, _b)

	// Return _x clamped in [_min,_max] (elementwise for vector/matrix types).
	template <typename tType>
	inline tType Clamp(const tType& _x, const tType& _min, const tType& _max)   { return Max(Min(_x, _max), _min); }
	#define FRM_CLAMP(_x, _min, _max) frm::Clamp(_x, _min, _max)

	// Return _x clamped in [0,1] (elementwise for vector/matrix types).
	template <typename tType>
	inline tType Saturate(const tType& _x)                                      { return Clamp(_x, tType(FRM_TRAITS_BASE_TYPE(tType)(0)), tType(FRM_TRAITS_BASE_TYPE(tType)(1))); }
	#define FRM_SATURATE(_x) frm::Saturate(_x)

	// Return whether _x is a power of 2.
	template <typename tType>
	inline bool IsPow2(const tType& _x)                                         { return (_x & (_x - 1)) == 0; }
	#define FRM_IS_POW2(_x) frm::IsPow2(_x)

	// Return _x % _y where _y is a power of 2.
	template <typename tType>
	inline tType ModPow2(const tType& _x, const tType& _y)                      { return _x & (_y - 1); }
	#define FRM_MOD_POW2(_x, _y) frm::ModPow2(_x, _y)

	namespace internal {
		template <typename tType>
		inline tType Fract(const tType& _x, FloatT)                             { return _x - std::floor(_x); }
		template <typename tType>
		inline tType Fract(const tType& _x, CompositeT)                         { return linalg::fract(_x); }
	}
	template <typename tType>
	inline tType Fract(const tType& _x)                                         { return internal::Fract(_x, FRM_TRAITS_FAMILY(tType)); }

	namespace internal {
		template <typename tType>
		inline tType Sign(const tType& _x, FloatT)                              { return std::copysign(tType(1), _x); }
		template <typename tType>
		inline tType Sign(const tType& _x, CompositeT)                          { return linalg::copysign(tType(1), _x); }
	}
	template <typename tType>
	inline tType Sign(const tType& _x)                                          { return internal::Sign(_x, FRM_TRAITS_FAMILY(tType)); }

	namespace internal {
		template <typename tType>
		inline tType Abs(const tType& _x, ScalarT)                              { return std::abs(_x); }
		template <typename tType>
		inline tType Abs(const tType& _x, CompositeT)                           { return linalg::abs(_x); }
	}
	template <typename tType>
	inline tType Abs(const tType& _x)                                           { return internal::Abs(_x, FRM_TRAITS_FAMILY(tType)); }

	namespace internal {
		template <typename tType>
		inline tType Floor(const tType& _x, ScalarT)                            { return std::floor(_x); }
		template <typename tType>
		inline tType Floor(const tType& _x, CompositeT)                         { return linalg::floor(_x); }
	}
	template <typename tType>
	inline tType Floor(const tType& _x)                                         { return internal::Floor(_x, FRM_TRAITS_FAMILY(tType)); }

	namespace internal {
		template <typename tType>
		inline tType Ceil(const tType& _x, ScalarT)                             { return std::ceil(_x); }
		template <typename tType>
		inline tType Ceil(const tType& _x, CompositeT)                          { return linalg::ceil(_x); }
	}
	template <typename tType>
	inline tType Ceil(const tType& _x)                                          { return internal::Ceil(_x, FRM_TRAITS_FAMILY(tType)); }

	namespace internal {
		template <typename tType>
		inline tType Round(const tType& _x, ScalarT)                            { return std::round(_x); }
		template <typename tType>
		inline tType Round(const tType& _x, CompositeT)                         { return linalg::round(_x); }
	}
	template <typename tType>
	inline tType Round(const tType& _x)                                         { return internal::Round(_x, FRM_TRAITS_FAMILY(tType)); }

	namespace internal {
		template <typename tType>
		inline tType Min(const tType& _a, const tType& _b, ScalarT)             { return std::min(_a, _b); }
		template <typename tType>
		inline tType Min(const tType& _a, const tType& _b, CompositeT)          { return linalg::min(_a, _b); }
	}
	template <typename tType>
	inline tType Min(const tType& _a, const tType& _b)                          { return internal::Min(_a, _b, FRM_TRAITS_FAMILY(tType)); }

	namespace internal {
		template <typename tType>
		inline tType Max(const tType& _a, const tType& _b, ScalarT)             { return std::max(_a, _b); }
		template <typename tType>
		inline tType Max(const tType& _a, const tType& _b, CompositeT)          { return linalg::max(_a, _b); }
	}
	template <typename tType>
	inline tType Max(const tType& _a, const tType& _b)                          { return internal::Max(_a, _b, FRM_TRAITS_FAMILY(tType)); }

} // namespace frm
