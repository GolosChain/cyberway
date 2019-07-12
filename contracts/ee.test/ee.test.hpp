#pragma once
#include <eosiolib/eosio.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/asset.hpp>


namespace cyber {

using namespace eosio;

class ee_test: public contract {
public:
    ee_test(action_name self):contract(self){}

    void dummy(uint64_t arg);
    void check(int64_t arg);
    void senddeferred(name action, int64_t arg, uint128_t senderid, uint32_t delay, bool replace, uint32_t expiration = 60);
    void canceldefer(uint128_t senderid);
};

} // cyber
