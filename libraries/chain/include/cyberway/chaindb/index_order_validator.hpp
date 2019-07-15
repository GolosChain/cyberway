#pragma once

#include <cyberway/chaindb/common.hpp>

namespace fc {
    class variant;
}

namespace eosio { namespace chain {
    struct abi_serializer;
}}

namespace cyberway { namespace chaindb {

    class index_info;
    class index_order_validator
    {
    public:

        index_order_validator(const index_info& index);

        void verify(const fc::variant& key) const;

    private:

        const index_info& index_;
    };

}} // namespace cyberway::chaindb
