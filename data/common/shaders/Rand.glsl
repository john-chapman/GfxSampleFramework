#ifndef Rand_glsl
#define Rand_glsl
// \todo see https://www.shadertoy.com/view/4t2SDh for triangular noise distribution


// PRNGs - generate a sequence of pseudorandom integers. Use a Rand_Hash* function to init _state_.
uint Rand_PRNG_LCG(inout uint _state_)
{
	_state_ = 1664525u * _state_ + 1013904223u;
	return _state_;
}
uint Rand_PRNG_XorShift(inout uint _state_)
{
	_state_ ^= (_state_ << 13u);
    _state_ ^= (_state_ >> 17u);
    _state_ ^= (_state_ << 5u);
    return _state_;
}

#define _Rand_PRNG(_state_) Rand_PRNG_XorShift(_state_)
uint Rand_PRNG(inout uint _state_) { return _Rand_PRNG(_state_); }
#undef _Rand_PRNG

// Hashes - generate a single random value.
uint Rand_Hash_Wang(in uint _seed)
{
	_seed = (_seed ^ 61u) ^ (_seed >> 16u);
	_seed *= 9u;
	_seed = _seed ^ (_seed >> 4u);
	_seed *= 0x27d4eb2du;
	_seed = _seed ^ (_seed >> 15u);
	return _seed;
}

#define _Rand_Hash(_seed) Rand_Hash_Wang(_seed)
uint Rand_Hash(in uint  _seed) { return _Rand_Hash(_seed); }
uint Rand_Hash(in uvec2 _seed) { return _Rand_Hash(_seed.x ^ _Rand_Hash(_seed.y)); }
uint Rand_Hash(in uvec3 _seed) { return _Rand_Hash(_seed.x ^ _Rand_Hash(_seed.y ^ _Rand_Hash(_seed.z))); }
uint Rand_Hash(in uvec4 _seed) { return _Rand_Hash(_seed.x ^ _Rand_Hash(_seed.y ^ _Rand_Hash(_seed.z ^ _Rand_Hash(_seed.w)))); }
#undef _Rand_Hash

// Inject _bits into a floating point mantissa.
float Rand_FloatMantissa(in uint _bits)
{
	_bits &= 0x007fffffu; // keep mantissa
	_bits |= 0x3f800000u; // add to 1
	return uintBitsToFloat(_bits) - 1.0;
}

// Return a random float in ]0,1[
float Rand(in uint  _seed) { return Rand_FloatMantissa(Rand_Hash(_seed)); }
float Rand(in uvec2 _seed) { return Rand_FloatMantissa(Rand_Hash(_seed)); }
float Rand(in uvec3 _seed) { return Rand_FloatMantissa(Rand_Hash(_seed)); }
float Rand(in uvec4 _seed) { return Rand_FloatMantissa(Rand_Hash(_seed)); }

// Pure floating point rand, may be faster than Rand(uvec2(_seed)) on some GPUs.
float Rand_Float(in vec2 _seed)
{
	return fract(sin(dot(_seed.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

#endif // Rand_glsl
