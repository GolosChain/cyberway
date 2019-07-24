#pragma once
#include <eosio/chain/config.hpp>

namespace eosio { namespace chain { 
namespace int_arithmetic {

template<typename UnsignedIntType>
constexpr UnsignedIntType integer_divide_ceil(UnsignedIntType num, UnsignedIntType den ) {
    return (num / den) + ((num % den) > 0 ? 1 : 0);
}

template<typename LesserIntType, typename GreaterIntType>
constexpr bool is_valid_downgrade_cast =
    std::is_integral<LesserIntType>::value &&  // remove overloads where type is not integral
    std::is_integral<GreaterIntType>::value && // remove overloads where type is not integral
    (std::numeric_limits<LesserIntType>::max() <= std::numeric_limits<GreaterIntType>::max()); // remove overloads which are upgrades not downgrades

template<typename LesserIntType, typename GreaterIntType>
constexpr auto downgrade_cast(GreaterIntType val, bool strict = true) ->
    std::enable_if_t<is_valid_downgrade_cast<LesserIntType,GreaterIntType>, LesserIntType>
{
    constexpr bool signedness_mismatch = std::is_signed<LesserIntType>::value != std::is_signed<GreaterIntType>::value;
    constexpr GreaterIntType max = std::numeric_limits<LesserIntType>::max();
    constexpr GreaterIntType min = signedness_mismatch ? 0 : std::numeric_limits<LesserIntType>::min();
    if (strict) {
        EOS_ASSERT( val >= min && val <= max, rate_limiting_state_inconsistent, "Casting a higher bit integer value ${v} to a lower bit integer value which cannot contain the value, valid range is [${min}, ${max}]", ("v", val)("min", min)("max",max) );
    }
    else {
        val = std::max(std::min(val, max), min);
    }
    return LesserIntType(val);
};

template<typename T> struct greater_int_t {};
template<> struct greater_int_t<int64_t>  { using type = int128_t;  };
template<> struct greater_int_t<uint64_t> { using type = uint128_t; };

template<typename T>
T safe_prop(T arg, T numer, T denom, bool strict = true) {
    return !arg || !numer ? T(0) : downgrade_cast<T>((static_cast<typename greater_int_t<T>::type>(arg) * numer) / denom, strict);
}

template<typename T>
T safe_prop_ceil(T arg, T numer, T denom, bool strict = true) {
    if (!arg || !numer) {
        return T(0);
    }
    
    constexpr T max = std::numeric_limits<T>::max();
    constexpr T min = std::numeric_limits<T>::min();
    
    auto mul = static_cast<typename greater_int_t<T>::type>(arg) * numer;
    auto div = downgrade_cast<T>(mul / denom, strict);
    return (mul % denom) ? ((mul >= 0) == (denom >= T(0)) ? (div != max ? div + 1 : div) : (div != min ? div - 1 : div)) : div;
}

template<typename T> T safe_pct(T arg, T total, bool strict = true) { return safe_prop(arg, total, static_cast<T>(config::percent_100), strict); }
template<typename T> T safe_share_to_pct(T arg, T total, bool strict = true) { return safe_prop(arg, static_cast<T>(config::percent_100), total, strict); }

    
} } }/// eosio::chain::int_arithmetic
