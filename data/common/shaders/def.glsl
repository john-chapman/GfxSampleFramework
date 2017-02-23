#ifndef common_def_glsl
#define common_def_glsl

#if !defined(COMPUTE_SHADER) && !defined(VERTEX_SHADER) && !defined(TESS_CONTROL_SHADER) && !defined(TESS_EVALUATION_SHADER) && !defined(GEOMETRY_SHADER) && !defined(FRAGMENT_SHADER)
	#error No shader stage defined.
#endif

#if !defined(Camera_ClipD3D) && !defined(Camera_ClipOGL)
	#error Camera_Clip* not defined.
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

#define kPi   (3.14159265359)
#define k2Pi  (6.28318530718)

#define saturate(_x) clamp((_x), 0.0, 1.0)


// Linearizing depth requires applying the inverse of Z part of the projection matrix, which depends on how the matrix was set up.
// The following variants correspond to ProjFlags_; see frm/Camera.h for more info.
// \todo revisit the formulae and clean this up
float LinearizeDepth(in float _depth, in float _near, in float _far) 
{
	#if   defined(Camera_ClipD3D)
	 	return (_far * _near) / (_far * -_depth + _far + (_near * _depth));
	#elif defined(Camera_ClipOGL)
		float zndc = _depth * 2.0 - 1.0;
		return 2.0 * _near * _far / (_far + _near - (_far - _near) * zndc);
	#endif
}
float LinearizeDepth_Infinite(in float _depth, in float _near) 
{
	#if   defined(Camera_ClipD3D)
		return -_near / (_depth - 1.0);
	#elif defined(Camera_ClipOGL)
		float zndc = _depth * 2.0 - 1.0;
		return -2.0 * _near / (zndc - 1.0);
	#endif
}
float LinearizeDepth_Reversed(in float _depth, in float _near, in float _far) 
{
	#if   defined(Camera_ClipD3D)
		return (_far * _near) / (_far * _depth + _near * -_depth + _near);
	#elif defined(Camera_ClipOGL)
		float zndc = _depth * 2.0 - 1.0;
		return (2.0 * _far * _near) / (_far * zndc + _far - _near * zndc + _near);
	#endif
}
float LinearizeDepth_InfiniteReversed(in float _depth, in float _near) 
{
	#if   defined(Camera_ClipD3D)
		return _near / _depth;
	#elif defined(Camera_ClipOGL)
		float zndc = _depth * 2.0 - 1.0;
		return 2.0 * _near / (zndc + 1.0);
	#endif
}


float Rand(in vec2 _seed)
{
	return fract(sin(dot(_seed.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

#endif // common_def_glsl
