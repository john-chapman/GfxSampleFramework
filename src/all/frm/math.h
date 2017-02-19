#pragma once
#ifndef frm_math_h
#define frm_math_h

#include <apt/math.h>

namespace frm {

using apt::vec2;
using apt::vec3;
using apt::vec4;
using apt::uvec2;
using apt::uvec3;
using apt::uvec4;
using apt::ivec2;
using apt::ivec3;
using apt::ivec4;
using apt::mat2;
using apt::mat3;
using apt::mat4;
using apt::quat;

using apt::LCG;

using apt::pi;
using apt::two_pi;

/// \return Matrix with position = _from and forward vector = (_to - _from).
mat4 GetLookAtMatrix(const vec3& _from, const vec3& _to, const vec3& _up = vec3(0.0f, 1.0f, 0.0f));

vec3 GetTranslation(const mat4& _m);
mat3 GetRotation(const mat4& _m);
vec3 GetScale(const mat4& _m);
vec3 ToEulerXYZ(const mat3& _m);
mat3 FromEulerXYZ(const vec3& _euler);

inline bool fequal(float _a, float _b)             { return apt::abs(_a - _b) < FLT_EPSILON; }
inline bool fequal(const vec3& _a, const vec3& _b) { return apt::all(apt::lessThan(apt::abs(_a - _b), vec3(FLT_EPSILON))); }
inline bool fequal(const vec4& _a, const vec4& _b) { return apt::all(apt::lessThan(apt::abs(_a - _b), vec4(FLT_EPSILON))); }

} // namespace frm

#endif // frm_math_h
