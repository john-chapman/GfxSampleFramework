#pragma once

#include <frm/core/frm.h>
#include <frm/core/math.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// PRNG_CMWC
// Uniform PRNG via 'complimentary multiply-with-carry' (George Marsaglia's 
// 'Mother of All PRNGs'). Adfrmed from Agner Fog's implementation found here:
// http://www.agner.org/random/. Use as template parameter to Rand (see below).
////////////////////////////////////////////////////////////////////////////////
class PRNG_CMWC
{
public:
	PRNG_CMWC(uint32 _seed = 1) { seed(_seed); }

	void   seed(uint32 _seed);
	uint32 raw();

private:
	uint32 m_state[5];
};

////////////////////////////////////////////////////////////////////////////////
// Rand
// Uniform random number API, templated by generator type. Typical usage:
//    Rand<> rnd;                     // instantiate with default PRNG
//    rnd.get<bool>();                // return true/false
//    rnd.get<float>();               // in [0,1]
//    rnd.get<int>(-10,10);           // in [-10,10]
//    rnd.get<float>(-10.0f, 10.0f);  // in [-10,10]
////////////////////////////////////////////////////////////////////////////////
template <typename PRNG = PRNG_CMWC>
class Rand
{
public:
	Rand(uint32 _seed = 1): m_prng(_seed)   {}

	void   seed(uint32 _seed)               { m_prng.seed(_seed); }
	uint32 raw()                            { return m_prng.raw(); }

	template <typename tType>
	tType  get();
	template <typename tType>
	tType  get(tType _min, tType _max);

private:
	PRNG m_prng;
};

// Van der Corput sequence.
inline float RadicalInverse(uint32 _seed)
{
	uint32 s = _seed;
	#if 0
		s = (s << 16u) | (s >> 16u);
		s = ((s & 0x55555555u) << 1u) | ((s & 0xaaaaaaaau) >> 1u);
		s = ((s & 0x33333333u) << 2u) | ((s & 0xccccccccu) >> 2u);
		s = ((s & 0x0f0f0f0fu) << 4u) | ((s & 0xf0f0f0f0u) >> 4u);
		s = ((s & 0x00ff00ffu) << 8u) | ((s & 0xff00ff00u) >> 8u);
	#else
		s = BitfieldReverse(s);
	#endif
	return (float)(s * 2.3283064365386963e-10); // / 0x100000000
}

// Hammersley sequence at _i given 1/N (N is the number of points in the sequence).
inline vec2 Hammersley2d(uint32 _i, float _rn)
{
	return vec2(float(_i) * _rn, RadicalInverse(_i));
}

namespace internal {

template <typename tType> tType RandGetScalar(uint32 _raw);
template <typename tType> tType RandGetScalar(uint32 _raw, tType _min, tType _max);

template<> inline bool RandGetScalar<bool>(uint32 _raw)
{
	return (_raw >> 31) != 0;
}
template<> inline float32 RandGetScalar<float32>(uint32 _raw)
{
	internal::iee754_f32 x;
	x.u = _raw;
	x.u &= 0x007fffffu;
	x.u |= 0x3f800000u;
	return x.f - 1.0f;
}
template<> inline sint32 RandGetScalar(uint32 _raw, sint32 _min, sint32 _max)
{
	uint64 i = (uint64)_raw * (_max - _min + 1);
	uint32 j = (uint32)(i >> 32);
	return (sint32)j + _min;
}
template<> inline float32 RandGetScalar(uint32 _raw, float32 _min, float32 _max)
{
	float32 f = RandGetScalar<float32>(_raw);
	return _min + f * (_max - _min);
}


template<typename PRNG, typename tType> 
inline tType RandGet(Rand<PRNG>* _rand_, ScalarT)
{
	return RandGetScalar<tType>(_rand_->raw());
}

template<typename PRNG, typename tType> 
inline tType RandGet(Rand<PRNG>* _rand_, CompositeT)
{
	tType ret;
	for (int i = 0; i < FRM_TRAITS_COUNT(tType); ++i) {
		ret[i] = RandGetScalar<FRM_TRAITS_BASE_TYPE(tType)>(_rand_->raw());
	}
	return ret;
}

template<typename PRNG, typename tType>
inline tType RandGet(Rand<PRNG>* _rand_, tType _min, tType _max, ScalarT)
{
	return RandGetScalar(_rand_->raw(), _min, _max);
}

template<typename PRNG, typename tType>
inline tType RandGet(Rand<PRNG>* _rand_, tType _min, tType _max, CompositeT)
{
	tType ret;
	for (int i = 0; i < FRM_TRAITS_COUNT(tType); ++i) {
		ret[i] = RandGetScalar<FRM_TRAITS_BASE_TYPE(tType)>(_rand_->raw(), _min[i], _max[i]);
	}
	return ret;
}

} // namespace internal


template <typename PRNG>
template <typename tType> 
inline tType Rand<PRNG>::get() 
{
	return internal::RandGet<PRNG, tType>(this, FRM_TRAITS_FAMILY(tType)); 
}

template <typename PRNG>
template <typename tType> 
inline tType Rand<PRNG>::get(tType _min, tType _max) 
{
	return internal::RandGet<PRNG, tType>(this, _min, _max, FRM_TRAITS_FAMILY(tType));
}

} // namespace frm
