#include "ee.test/ee.test.hpp"
#include <eosiolib/transaction.hpp>
#include <eosiolib/time.hpp>

namespace cyber {

using namespace eosio;
using std::vector;


void ee_test::dummy(uint64_t arg) {
    (void)arg;
    for(;;) {}
}

void ee_test::check(int64_t arg) {
    require_auth(_self);
    eosio_assert(arg > 0, "Argument must be positive");
}

void ee_test::senddeferred(name action_name, int64_t arg, uint128_t senderid, uint32_t delay, bool replace, uint32_t expiration) {
    require_auth(_self);

    transaction trx(time_point_sec(now() + delay + expiration));
    trx.actions.emplace_back(action{
        permission_level(_self, N(active)), 
        _self, action_name, 
        std::tuple<int64_t>(arg)});
    trx.delay_sec = delay;
    trx.send(senderid, _self, replace);
}

void ee_test::canceldefer(uint128_t senderid) {
    require_auth(_self);
    cancel_deferred(senderid);
}


} // cyber

EOSIO_ABI(cyber::ee_test, (check)(senddeferred)(canceldefer)(dummy))
