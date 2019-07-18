#pragma once

#include <cyberway/chaindb/common.hpp>

namespace fc {
    class variant;
}

namespace eosio { namespace chain {
    struct abi_serializer;
}}

namespace cyberway { namespace chaindb {

    struct index_info;
    class index_order_validator final {
    public:
        index_order_validator(const index_info& index);
        void verify(const fc::variant& key) const;

    private:
        const index_info& index_;
    };

}} // namespace cyberway::chaindb
