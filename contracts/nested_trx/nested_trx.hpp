#pragma once
#include <eosiolib/eosio.hpp>


namespace cyber {

using namespace eosio;

class nested: public contract {
public:
    nested(account_name self): contract(self) {}

    void put(name who);
    void auth(name arg);
    void check(int64_t arg);
    void nestedcheck(int64_t arg);
    void nestedcheck2(int64_t arg);
    void nestedchecki(int64_t arg);
    void sendnested(name actor, name action, int64_t arg, uint32_t delay, uint32_t expiration, name provide = {});
    void sendnestedcfa(name arg);

    struct item {
        uint64_t value;
        uint64_t primary_key() const { return value; }
    };

    using item_tbl = eosio::multi_index<N(item), item>;
};

} // cyber
