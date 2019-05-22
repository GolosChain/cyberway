#pragma once
#include <eosio/chain/asset.hpp>
#include <eosio/chain/int_arithmetic.hpp>

namespace cyberway { namespace genesis {

using namespace eosio::chain;


class asset_converter {
    const int fract_bits = 18;                      // 18 bits is enough to cover rounding of all balances
    const uint64_t mask = (1 << fract_bits) - 1;

    uint32_t fraction;      // accumulating fraction from previous convertions to compensate rounding
    asset price_base;       // premultiplied
    asset price_quote;

public:
    asset_converter(const asset& base, const asset& quote)
    : price_base(asset(base.get_amount() << fract_bits, base.get_symbol()))
    , price_quote(quote) {
        reset();
    }
    asset convert(const asset& x) {
        auto extended = int_arithmetic::safe_prop(x.get_amount(), price_base.get_amount(), price_quote.get_amount());
        extended += fraction;
        fraction = extended & mask;
        return asset(extended >> fract_bits, price_base.get_symbol());
    }
    void reset() {
        fraction = mask;    // mask is equal to max fractional part. makes 1st division ceil to compensate accumulated rounding in multiplied units
    }

};

inline asset golos2sys(const asset& golos) {
    static const int64_t sys_precision = asset().get_symbol().precision();
    return asset(int_arithmetic::safe_prop(
        golos.get_amount(), sys_precision, static_cast<int64_t>(golos.get_symbol().precision())));
}


}} // cyberway::genesis
