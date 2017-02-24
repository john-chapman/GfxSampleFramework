#include <frm/math.h>

using namespace frm;
using namespace apt;

mat4 frm::LookAt(const vec3& _from, const vec3& _to, const vec3& _up)
{
	mat4 ret = AlignZ(normalize(_to - _from), _up);
	ret[3] = vec4(_from, 1.0f);
	return ret;
}

mat4 frm::AlignX(const vec3& _axis, const vec3& _up)
{
	vec3 y, z;
	y = _up - _axis * dot(_up, _axis);
	float ylen = length(y);
	if_unlikely (ylen < FLT_EPSILON) {
		vec3 k = vec3(1.0f, 0.0f, 0.0f);
		y = k - _axis * dot(k, _axis);
		ylen = length(y);
		if_unlikely (ylen < FLT_EPSILON) {
			k = vec3(0.0f, 0.0f, 1.0f);
			y = k - _axis * dot(k, _axis);
			ylen = length(y);
		}
	}
	y = y / ylen;
	z = cross(_axis, y);

	return mat4(
		_axis.x, _axis.y, _axis.z, 0.0f,
		y.x,     y.y,     y.z,     0.0f,
		z.x,     z.y,     z.z,     0.0f,
		0.0f,    0.0f,    0.0f,    1.0f
		);
}
mat4 frm::AlignY(const vec3& _axis, const vec3& _up)
{
	vec3 x, z;
	z = _up - _axis * dot(_up, _axis);
	float zlen = length(z);
	if_unlikely (zlen < FLT_EPSILON) {
		vec3 k = vec3(1.0f, 0.0f, 0.0f);
		z = k - _axis * dot(k, _axis);
		zlen = length(z);
		if_unlikely (zlen < FLT_EPSILON) {
			k = vec3(0.0f, 0.0f, 1.0f);
			z = k - _axis * dot(k, _axis);
			zlen = length(z);
		}
	}
	z = z / zlen;
	x = cross(z, _axis);

	return mat4(
		x.x,     x.y,     x.z,     0.0f,
		_axis.x, _axis.y, _axis.z, 0.0f,
		z.x,     z.y,     z.z,     0.0f,
		0.0f,    0.0f,    0.0f,    1.0f
		);
}
mat4 frm::AlignZ(const vec3& _axis, const vec3& _up)
{
	vec3 x, y;
	y = _up - _axis * dot(_up, _axis);
	float ylen = length(y);
	if_unlikely (ylen < FLT_EPSILON) {
		vec3 k = vec3(1.0f, 0.0f, 0.0f);
		y = k - _axis * dot(k, _axis);
		ylen = length(y);
		if_unlikely (ylen < FLT_EPSILON) {
			k = vec3(0.0f, 0.0f, 1.0f);
			y = k - _axis * dot(k, _axis);
			ylen = length(y);
		}
	}
	y = y / ylen;
	x = cross(y, _axis);

	return mat4(
		x.x,     x.y,     x.z,     0.0f,
		y.x,     y.y,     y.z,     0.0f,
		_axis.x, _axis.y, _axis.z, 0.0f,
		0.0f,    0.0f,    0.0f,    1.0f
		);
}

vec3 frm::GetTranslation(const mat4& _m)
{
	return vec3(column(_m, 3));
}
mat3 frm::GetRotation(const mat4& _m)
{
	mat3 ret = mat3(_m);
	ret[0] = normalize(ret[0]);
	ret[1] = normalize(ret[1]);
	ret[2] = normalize(ret[2]);
	return ret;
}
vec3 frm::GetScale(const mat4& _m)
{
	vec3 ret;
	ret.x = length(vec3(column(_m, 0)));
	ret.y = length(vec3(column(_m, 1)));
	ret.z = length(vec3(column(_m, 2)));
	return ret;
}

vec3 frm::ToEulerXYZ(const mat3& _m)
{
 // http://www.staff.city.ac.uk/~sbbh653/publications/euler.pdf
	vec3 ret;
	if_likely (fabs(_m[0][2]) < 1.0f) {
		ret.y = -asinf(_m[0][2]);
		float c = 1.0f / cosf(ret.y);
		ret.x = atan2f(_m[1][2] * c, _m[2][2] * c);
		ret.z = atan2f(_m[0][1] * c, _m[0][0] * c);
	} else {
		ret.z = 0.0f;
		if (!(_m[0][2] > -1.0f)) {
			ret.x = ret.z + atan2f(_m[1][0], _m[2][0]);
			ret.y = half_pi<float>();
		} else {
			ret.x = -ret.z + atan2f(-_m[1][0], -_m[2][0]);			
			ret.y = -half_pi<float>();
		}
	}
	return ret;
}

mat3 frm::FromEulerXYZ(const vec3& _euler)
{
	float cx = cosf(_euler.x);
	float sx = sinf(_euler.x);
	float cy = cosf(_euler.y);
	float sy = cosf(_euler.y);
	float cz = cosf(_euler.z);
	float sz = cosf(_euler.z);
	return mat3(
		cy * cz,                                 cy * sz,                      -sz,
		sz * sy * cz - cx * sz,   sx * sy * sz + cx * cz,                  sx * cy,
		cx * sy * cz + sx * sz,                  cx * sy * sz - sx * cz,   cx * cy
		);
}
