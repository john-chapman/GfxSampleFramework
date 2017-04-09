#ifndef Envmap_glsl
#define Envmap_glsl

#define Envmap_Face_XP 0
#define Envmap_Face_XN 1
#define Envmap_Face_YP 2
#define Envmap_Face_YN 3
#define Envmap_Face_ZP 4
#define Envmap_Face_ZN 5


// Given a face uv in [0,1] and face index in [0,5], return the uvw required to sample the texel/face.
vec3 Envmap_GetCubeFaceUvw(in vec2 _uv, in int _face)
{
	vec2 uv = _uv * 2.0 - 1.0;
	float w = 1.0;
	switch (_face) {
		case Envmap_Face_XP: return vec3(    w,  uv.y, -uv.x);
		case Envmap_Face_XN: return vec3(   -w,  uv.y,  uv.x);
		case Envmap_Face_YP: return vec3( uv.x,   -w,   uv.y);
		case Envmap_Face_YN: return vec3( uv.x,    w,  -uv.y);
		case Envmap_Face_ZP: return vec3( uv.x,  uv.y,     w);
		case Envmap_Face_ZN: return vec3(-uv.x,  uv.y,    -w);
		default: return vec3(0.0);
	};
}

// 
vec2 Envmap_GetSphereUv(in vec3 _dir) {
	float theta = atan(_dir.z, _dir.x) / kPi * 0.5 + 0.5;
	float phi = 1.0 - acos(_dir.y) / kPi; // assume length(_dir) = 1
	return vec2(theta, phi);
}


#endif // Envmap_glsl