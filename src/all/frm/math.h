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

mat4 LookAt(const vec3& _from, const vec3& _to, const vec3& _up = vec3(0.0f, 1.0f, 0.0f));

inline vec3 GetTranslation(const mat4& _mat) { return vec3(apt::column(_mat, 3)); }

}

#endif // frm_math_h
