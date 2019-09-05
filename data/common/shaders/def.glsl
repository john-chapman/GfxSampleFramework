#ifndef common_def_glsl
#define common_def_glsl

#if !defined(COMPUTE_SHADER) && !defined(VERTEX_SHADER) && !defined(TESS_CONTROL_SHADER) && !defined(TESS_EVALUATION_SHADER) && !defined(GEOMETRY_SHADER) && !defined(FRAGMENT_SHADER)
	#error No shader stage defined.
#endif

#if !defined(FRM_NDC_Z_ZERO_TO_ONE) && !defined(FRM_NDC_Z_NEG_ONE_TO_ONE)
	#error FRM_NDC_Z_* not defined.
#endif

#if defined(COMPUTE_SHADER)
	#ifndef LOCAL_SIZE_X
		#define LOCAL_SIZE_X 1
	#endif
	#ifndef LOCAL_SIZE_Y
		#define LOCAL_SIZE_Y 1
	#endif
	#ifndef LOCAL_SIZE_Z
		#define LOCAL_SIZE_Z 1
	#endif
	layout(local_size_x = LOCAL_SIZE_X, local_size_y = LOCAL_SIZE_Y, local_size_z = LOCAL_SIZE_Z) in;
#endif

#if defined(VERTEX_SHADER)
	#define _VARYING(_interp, _type, _name) _interp out _type _name
	#define _VERTEX_IN(_location, _type, _name) layout(location=_location) in _type _name
	#define _FRAGMENT_OUT(_location, _type, _name)
#elif defined(FRAGMENT_SHADER)
	#define _VARYING(_interp, _type, _name) _interp in _type _name
	#define _VERTEX_IN(_location, _type, _name)
	#define _FRAGMENT_OUT(_location, _type, _name) layout(location=_location) out _type _name
#endif

struct DrawArraysIndirectCmd
{
	uint m_count;
	uint m_primCount;
	uint m_first;
	uint m_baseInstance;
};
struct DrawElementsIndirectCmd
{
	uint m_count;
	uint m_primCount;
	uint m_firstIndex;
	uint m_baseVertex;
	uint m_baseInstance;
};
struct DispatchIndirectCmd
{
	uint m_groupsX;
	uint m_groupsY;
	uint m_groupsZ;
};

#define CONCATENATE_TOKENS(_a, _b) _a ## _b

// Use for compile-time branching based on memory qualifiers - useful for buffers defined in common files, e.g.
// #define FooMemQualifier readonly
// layout(std430) FooMemQualifier buffer _bfFoo {};
// #if (MemoryQualifier(FooMemQualifier) == MemoryQualifier(readonly))
#define MemoryQualifier_            0
#define MemoryQualifier_readonly    1
#define MemoryQualifier_writeonly   2
#define MemoryQualifier(_qualifier) CONCATENATE_TOKENS(MemoryQualifier_, _qualifier)


#define Gamma_kExponent 2.2
float Gamma_Apply(in float _x)
{
	return pow(_x, Gamma_kExponent);
}
vec2 Gamma_Apply(in vec2 _v)
{
	return vec2(Gamma_Apply(_v.x), Gamma_Apply(_v.y));
}
vec3 Gamma_Apply(in vec3 _v)
{
	return vec3(Gamma_Apply(_v.x), Gamma_Apply(_v.y), Gamma_Apply(_v.z));
}
vec4 Gamma_Apply(in vec4 _v)
{
	return vec4(Gamma_Apply(_v.x), Gamma_Apply(_v.y), Gamma_Apply(_v.z), Gamma_Apply(_v.w));
}
float Gamma_ApplyInverse(in float _x)
{
	return pow(_x, 1.0/Gamma_kExponent);
}
vec2 Gamma_ApplyInverse(in vec2 _v)
{
	return vec2(Gamma_ApplyInverse(_v.x), Gamma_ApplyInverse(_v.y));
}
vec3 Gamma_ApplyInverse(in vec3 _v)
{
	return vec3(Gamma_ApplyInverse(_v.x), Gamma_ApplyInverse(_v.y), Gamma_ApplyInverse(_v.z));
}
vec4 Gamma_ApplyInverse(in vec4 _v)
{
	return vec4(Gamma_ApplyInverse(_v.x), Gamma_ApplyInverse(_v.y), Gamma_ApplyInverse(_v.z), Gamma_ApplyInverse(_v.w));
}

// Constants
#define kPi                          (3.14159265359)
#define k2Pi                         (6.28318530718)
#define kHalfPi                      (1.57079632679)

#define Color_Black                  vec3(0.0)
#define Color_White                  vec3(1.0)
#define Color_Red                    vec3(1.0, 0.0, 0.0)
#define Color_Green                  vec3(0.0, 1.0, 0.0)
#define Color_Blue                   vec3(0.0, 0.0, 1.0)
#define Color_Magenta                vec3(1.0, 0.0, 1.0)
#define Color_Yellow                 vec3(1.0, 1.0, 0.0)
#define Color_Cyan                   vec3(0.0, 1.0, 1.0)

// Functions
#define saturate(_x)                 clamp((_x), 0.0, 1.0)
#define length2(_v)                  dot(_v, _v)
#define sqrt_safe(_x)                sqrt(max(_x, 0.0))
#define length_safe(_v)              sqrt_safe(dot(_v, _v))
#define log10(x)                     (log2(x) / log2(10.0))
#define linearstep(_e0, _e1, _x)     saturate((_x) * (1.0 / ((_e1) - (_e0))) + (-(_e0) / ((_e1) - (_e0))))
float max3(in float _a, in float _b, in float _c)              { return max(_a, max(_b, _c)); }
float max3(in vec3 _v)                                         { return max3(_v.x, _v.y, _v.z); }
float min3(in float _a, in float _b, in float _c)              { return min(_a, min(_b, _c)); }
float min3(in vec3 _v)                                         { return min3(_v.x, _v.y, _v.z); }
float max4(in float _a, in float _b, in float _c, in float _d) { return max(_a, max(_b, max(_c, _d))); }
float max4(in vec4 _v)                                         { return max4(_v.x, _v.y, _v.z, _v.w); }
float min4(in float _a, in float _b, in float _c, in float _d) { return min(_a, min(_b, min(_c, _d))); }
float min4(in vec4 _v)                                         { return min4(_v.x, _v.y, _v.z, _v.w); }

// Recover view space depth from a depth buffer value given a perspective projection.
// This may return INF for infinite perspective projections.
float GetDepthV_Perspective(in float _depth, in mat4 _proj)
{
	#if FRM_NDC_Z_ZERO_TO_ONE
		float zndc = _depth;
	#else
		float zndc = _depth * 2.0 - 1.0;
	#endif
	return _proj[3][2] / (_proj[2][3] * zndc - _proj[2][2]);
}
// Recover view space depth from a depth buffer value given an orthographic projection.
// This may return INF for infinite perspective projections.
float GetDepthV_Orthographic(in float _depth, in mat4 _proj)
{
	#if FRM_NDC_Z_ZERO_TO_ONE
		float zndc = _depth;
	#else
		float zndc = _depth * 2.0 - 1.0;
	#endif
	return (zndc - _proj[3][2]) / _proj[2][2];
}

vec3 TransformPosition(in mat4 _m, in vec3 _v)
{
	return (_m * vec4(_v, 1.0)).xyz;
}
vec2 TransformPosition(in mat3 _m, in vec2 _v)
{
	return (_m * vec3(_v, 1.0)).xy;
}
vec3 TransformDirection(in mat4 _m, in vec3 _v)
{
	return mat3(_m) * _v;
}
vec2 TransformDirection(in mat3 _m, in vec2 _v)
{
	return (_m * vec3(_v, 0.0)).xy;
}

#endif // common_def_glsl
