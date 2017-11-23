#ifndef WindowFunctions_glsl
#define WindowFunctions_glsl

// Rectangular window; return 1 if _x is inside the window else 0.
float Window_Rectangle(float _x, float _center, float _radius)
{
	return step(abs(_x - _center), _radius);
}

// Triangular window; map [0, _radius] in [1, 0] as a linear falloff from _center.
float Window_Triangle(float _x, float _center, float _radius)
{
	return 1.0 - min(abs(_x - _center) / _radius, 1.0);
}

// Cylindrical window; map [_radius - 10%, _radius] in [1, 0] as a smooth falloff from _center.
float Window_Cylinder(float _x, float _center, float _radius)
{
	_x = abs(_x - _center);
	return 1.0 - smoothstep(0.9 * _radius, _radius, _x);
}

// Cubic window; map [0, _radius] in [1, 0] as a cubic falloff from _center.
float Window_Cubic(float _x, float _center, float _radius)
{
	_x = min(abs(_x - _center) / _radius, 1.0);
	return 1.0 - _x * _x * (3.0 - 2.0 * _x);
}

#endif // WindowFunctions_glsl
