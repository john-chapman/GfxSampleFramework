#pragma once

#include <initializer_list>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// BitFlags
// Bitfield flags using an enum to index bits. 
// 
// Example:
//    enum class Mode
//    {
//       Foo,
//       Bar,
//
//       BIT_FLAGS_COUNT_DEFAULT(Foo, Bar)
//    };
//
//    BitFlags<Mode> flags; // set to Mode::_Default
//    bool isFoo = flags.get(Mode::Foo);
//    flags.set(Mode::Foo, false);
//
// Note that BIT_FLAGS_COUNT_DEFAULT(Foo, Bar) in the example above is equivalent 
// to:
//
//   _Count,
//   _Default = BIT_FLAGS_DEFAULT(Foo, Bar)
//
////////////////////////////////////////////////////////////////////////////////
template <typename tEnum>
struct BitFlags
{
	static constexpr int kCount = (int)tEnum::_Count;
	static_assert(tEnum::_Default >= (tEnum)0, "BitFlags tEnum::_Default missing.");

	// Select ValueType for kCount bits.
	using ValueType = typename std::conditional<kCount < 8,  uint8_t,
	                  typename std::conditional<kCount < 16, uint16_t, 
	                  typename std::conditional<kCount < 32, uint32_t, uint64_t>::type>::type>::type;

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

	constexpr BitFlags(tEnum bit)
	{
		_bits = (ValueType)1 << (ValueType)bit;
	}
	
	// Return whether bit i is set. 
	constexpr bool get(tEnum i) const
	{
		const auto mask = _getMask(i);
		return (_bits & mask) != 0;
	}

	// Return whether any bits are set.
	constexpr bool any() const
	{
		return _bits != 0;
	}

	// Set bit i from value.
	constexpr void set(tEnum i, bool value)
	{
		const auto mask = _getMask(i);
		_bits = value ? (_bits | mask) : (_bits & ~mask);
	}

	// Clear all bits.
	constexpr void clear()
	{
		_bits = 0;
	}

	constexpr bool operator==(BitFlags<tEnum> rhs)
	{
		return _bits == rhs._bits;
	}

	constexpr bool operator!=(BitFlags<tEnum> rhs)
	{
		return _bits != rhs._bits;
	}

private:

	ValueType _bits = (ValueType)tEnum::_Default;

	constexpr ValueType _getMask(tEnum i) const
	{
		return (ValueType)1 << (ValueType)i;
	}
	
};

namespace internal {

template <typename tType>
constexpr tType _BitFlagsDefault(std::initializer_list<tType> list = {})
{
	tType ret = (tType)0;
	if (list.size() > 0)
	{
		for (tType i : list)
		{
			ret |= (tType)1 << (tType)i;
		}
	}
	return ret;
}
#define BIT_FLAGS_DEFAULT(...) frm::internal::_BitFlagsDefault({ __VA_ARGS__ })
#define BIT_FLAGS_COUNT_DEFAULT(...) _Count, _Default = frm::internal::_BitFlagsDefault({ __VA_ARGS__ })
#define BIT_FLAGS_COUNT_DEFAULT_ZERO(...) _Count, _Default = 0 // \todo This is a bit ugly, possible to make it work as just BIT_FLAGS_COUNT_DEFAULT()?

} // namespace internal

} // namespace gef
