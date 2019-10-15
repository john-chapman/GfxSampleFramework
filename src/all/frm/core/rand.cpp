#include "rand.h"

using namespace frm;

/*******************************************************************************

                                  PRNG_CMWC

*******************************************************************************/

// PUBLIC

void PRNG_CMWC::seed(uint32 _seed)
{
	for (auto& x: m_state) {
		_seed = _seed * 29943829 - 1;
		x = _seed;
	}
}

uint32 PRNG_CMWC::raw()
{
	uint64 sum =
		(uint64)2111111111ul * (uint64)m_state[3] +
		(uint64)1492ul       * (uint64)m_state[2] +
		(uint64)1776ul       * (uint64)m_state[1] +
		(uint64)5115ul       * (uint64)m_state[0] +
		(uint64)m_state[4]
		;
	m_state[3] = m_state[2];
	m_state[2] = m_state[1];
	m_state[1] = m_state[0];

	m_state[4] = (uint32)(sum >> 32); // carry
	m_state[0] = (uint32)sum;         // discard high bits
	return (uint32)sum;
}
