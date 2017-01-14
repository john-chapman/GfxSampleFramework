#include <frm/math.h>

using namespace frm;
using namespace apt;

mat4 frm::GetLookAtMatrix(const vec3& _from, const vec3& _to, const vec3& _up)
{
	vec3 z = -normalize(_to - _from); // negated to conform with camera projection along -z

 // orthonormal basis
	vec3 x, y;
	if_unlikely (fequal(z, _up) || fequal(z, -_up)) {
		vec3 k = _up + vec3(FLT_EPSILON);
		y = normalize(k - z * dot(k, z));
	} else {
		y = normalize(_up - z * dot(_up, z));
	}
	x = cross(y, z);

	return mat4(
		x.x,     x.y,     x.z,     0.0f,
		y.x,     y.y,     y.z,     0.0f,
		z.x,     z.y,     z.z,     0.0f,
		_from.x, _from.y, _from.z, 1.0f
		);
}
