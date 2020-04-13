#pragma once

#include <cstdint>
#include <initializer_list>
#include <type_traits>

namespace frm {

namespace internal {

// Select an 8, 16, 32 or 64 bit type based on tEnum::_Count.
// \todo move to a core header
template <int kCount>
struct _SelectUint
{
	typedef typename std::conditional<kCount < 8,  uint8_t,
	        typename std::conditional<kCount < 16, uint16_t, 
	        typename std::conditional<kCount < 32, uint32_t, uint64_t>::type>::type>::type
		Type;
};

} // namespace internal

////////////////////////////////////////////////////////////////////////////////
// BitFlags
// Bitfield flags using an enum to index bits. 
// 
//
// Example:
//    enum class Mode
//    {
//       Foo,
//       Bar,
//
//       _Count,
//       _Default = (1 << Foo) | (1 << Bar)
//    };
//
//    BitFlags<Mode> flags; // set to Mode::_Default
//    bool isFoo = flags.isSet(Mode::Foo);
//    flags.set(Mode::Foo, false);
//       
////////////////////////////////////////////////////////////////////////////////
template <typename tEnum>
struct BitFlags
{
	static constexpr int kCount = (int)tEnum::_Count;
	static_assert(tEnum::_Default > (tEnum)0); // Ensure tEnum::_Default exists.

	typedef typename internal::_SelectUint<kCount>::Type ValueType;

	constexpr BitFlags(ValueType bits = (ValueType)tEnum::_Default)
		: _bits(bits) 
	{
	}

	constexpr BitFlags(std::initializer_list<tEnum> list)
	{
		if (list.size() > 0)
		{
			_bits = 0;
			for (tEnum i : list)
			{
				_bits |= (ValueType)1 << (ValueType)i;
			}
		}
	}
	
	// Return whether bit i is set. 
	constexpr bool isSet(tEnum i) const
	{
		const auto mask = _getMask(i);
		return (_bits & mask) != 0;
	}

	// Set bit i from value.
	constexpr void set(tEnum i, bool value)
	{
		const auto mask = _getMask(i);
		_bits = value ? (_bits | mask) : (_bits & ~mask);
	}

private:

	ValueType _bits = (ValueType)tEnum::_Default;

	constexpr ValueType _getMask(tEnum i) const
	{
		return (ValueType)1 << (ValueType)i;
	}
	
};

} // namespace frm
