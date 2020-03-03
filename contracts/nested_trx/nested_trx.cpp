#include "nested_trx/nested_trx.hpp"
#include <eosiolib/transaction.hpp>
#include <eosiolib/time.hpp>

namespace cyber {

using namespace eosio;


void nested::put(name who) {
    require_auth(who);
    item_tbl items(_self, who.value);
    items.emplace(who, [](auto& i) {
        i.value = current_time();
    });
}

void nested::auth(name arg) {
    require_auth(arg);
}

void nested::check(int64_t arg) {
    require_auth(_self);
    eosio_assert(arg > 0, "Argument must be positive");
}

void nested::nestedcheck(int64_t arg) {
    transaction trx;
    trx.actions.emplace_back(action{{_self, N(active)}, _self, N(check), std::make_tuple(arg)});
    trx.send_nested();
    eosio_assert(arg < 100, "Argument must be < 100");
}

void nested::nestedcheck2(int64_t arg) {
    nestedcheck(arg);
    nestedcheck(arg);
}

void nested::nestedchecki(int64_t arg) {
    dispatch_inline(_self, N(nestedcheck), {{_self, N(active)}}, std::make_tuple(arg));
    eosio_assert(arg < 50, "Argument must be < 50");
}

void nested::sendnested(name actor, name action_name, int64_t arg, uint32_t delay, uint32_t expiration, name provide) {
    // require_auth(_self);
    transaction trx{time_point_sec(now() + delay + expiration)};
    trx.actions.emplace_back(action{{actor, N(active)}, _self, action_name, std::make_tuple(arg)});
    if (provide != name()) {
        trx.actions.emplace_back(
            action{{provide, N(active)}, SYSTEM_ACC, N(providebw), std::make_tuple(provide, actor)}
        );
    }
    trx.delay_sec = delay;
    trx.send_nested();
}

void nested::sendnestedcfa(name arg) {
    require_auth(arg);
    transaction trx{};
    action a{}; // create manually to use empty data
    a.account = _self;
    a.name = N(someaction);
    a.authorization = vector<permission_level>{{arg, N(active)}};
    trx.context_free_actions.emplace_back(a);
    trx.send_nested();
}


} // cyber

EOSIO_ABI(cyber::nested, (put)(auth)(check)(nestedcheck)(nestedcheck2)(nestedchecki)(sendnested)(sendnestedcfa))
