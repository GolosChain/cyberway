#include <boost/test/unit_test.hpp>
#include <eosio/testing/tester.hpp>
#include <eosio/chain/abi_serializer.hpp>
#include <eosio/chain/resource_limits.hpp>

#include <eosio.system/eosio.system.wast.hpp>
#include <eosio.system/eosio.system.abi.hpp>
// These contracts are still under dev
#include <eosio.bios/eosio.bios.wast.hpp>
#include <eosio.bios/eosio.bios.abi.hpp>
#include <eosio.token/eosio.token.wast.hpp>
#include <eosio.token/eosio.token.abi.hpp>
#include <eosio.msig/eosio.msig.wast.hpp>
#include <eosio.msig/eosio.msig.abi.hpp>
#include <nested_trx/nested_trx.wast.hpp>
#include <nested_trx/nested_trx.abi.hpp>

#include <cyberway/chain/cyberway_contract_types.hpp>

#include <Runtime/Runtime.h>
#include <fc/variant_object.hpp>


#ifdef NON_VALIDATING_TEST
#   define TESTER tester
#else
#   define TESTER validating_tester
#endif

using namespace eosio;
using namespace eosio::chain;
using namespace eosio::chain::config;
using namespace eosio::testing;
using namespace fc;

using mvo = fc::mutable_variant_object;

class nested_tester : public TESTER {
public:

    void set_code_abi(
        const account_name& account, const char* wast, const char* abi, const private_key_type* signer = nullptr
    ) {
        wdump((account));
        set_code(account, wast, signer);
        set_abi(account, abi, signer);
        if (account == config::system_account_name) {
            const auto& accnt = control->chaindb().get<account_object>(account);
            abi_def abi_definition;
            BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi_definition), true);
            abi_ser.set_abi(abi_definition, abi_serializer_max_time);
        }
        produce_block();
    }

    void init() {
        create_accounts({alice, bob, carol, normal});
        set_code_abi(nester, nested_trx_wast, nested_trx_abi);
        set_code_abi(normal, nested_trx_wast, nested_trx_abi);
        const auto& nester_acc = control->chaindb().get<account_object>(nester);
        const auto& normal_acc = control->chaindb().get<account_object>(normal);
        BOOST_CHECK_EQUAL(nester_acc.privileged, true);
        BOOST_CHECK_EQUAL(normal_acc.privileged, false);
        produce_block();
    }

    abi_serializer abi_ser;
    const account_name nester = config::msig_account_name; // privileged contract
    const account_name normal = N(normal); // non-privileged
    const account_name alice = N(alice);
    const account_name bob = N(bob);
    const account_name carol = N(carol);


    // api
    auto push_action(name code, name action, name signer, const mvo& data, bool add_nested = false) {
        vector<permission_level> auths{{signer, N(active)}};
        return base_tester::push_action(code, action, auths, data, DEFAULT_EXPIRATION_DELTA, 0, add_nested);
    }
    auto auth(name contract, name signer, name arg) {
        return push_action(contract, N(auth), signer, mvo()("arg", arg));
    }
    auto check(name contract, name signer, int64_t arg) {
        return push_action(contract, N(check), signer, mvo()("arg", arg));
    }
    auto nested_check(name contract, name signer, int64_t arg) {
        return push_action(contract, N(nestedcheck), signer, mvo()("arg", arg), true);
    }
    auto nested_check2(name contract, name signer, int64_t arg) {
        return push_action(contract, N(nestedcheck2), signer, mvo()("arg", arg), true);
    }
    auto nested_check_inline(name contract, name signer, int64_t arg) {
        return push_action(contract, N(nestedchecki), signer, mvo()("arg", arg), true);
    }
    auto send_nested(
        name contract, name signer, name actor, name action, int64_t arg,
        name provide={}, uint32_t delay = 0, uint32_t expire = 30
    ) {
        auto data = mvo()
            ("actor", actor)
            ("action", action)
            ("arg", arg)
            ("delay", delay)
            ("expiration", expire)
            ("provide", provide);
        return push_action(contract, N(sendnested), signer, data, true);
    }
    auto send_nested_simple(
        name contract, name action, int64_t arg, uint32_t delay = 0, uint32_t expire = 30
    ) {
        return send_nested(contract, contract, contract, action, arg, {}, delay, expire);
    }

    auto store(name who) {
        return base_tester::push_action(nester, N(put), who, mvo()("who", who));
    }
};

BOOST_AUTO_TEST_SUITE(nested_tests)

BOOST_FIXTURE_TEST_CASE(base, nested_tester) { try {
    BOOST_TEST_MESSAGE("Base nested tests");
    init();
    const auto any = [](auto&){return true;};
    const auto msg = [](auto& m){ return [&m](auto& e) {
        return e.top_message() == string("assertion failure with message: ") + m;
    };};
    const auto err_le0 = "Argument must be positive";
    const auto err_ge50 = "Argument must be < 50";
    const auto err_ge100 = "Argument must be < 100";

    BOOST_TEST_MESSAGE("--- Ensure nested_trx contract works as expected");
    BOOST_TEST_MESSAGE("------ auth success");
    auth(nester, nester, nester);
    auth(nester, bob, bob);
    BOOST_TEST_MESSAGE("------ auth fail");
    BOOST_CHECK_EXCEPTION(auth(nester, bob, alice), missing_auth_exception, any);

    BOOST_TEST_MESSAGE("------ check success");
    check(nester, nester, 1);
    check(nester, nester, 100);
    BOOST_TEST_MESSAGE("------ check fail (condition)");
    BOOST_CHECK_EXCEPTION(check(nester, nester, 0), eosio_assert_message_exception, msg(err_le0));
    BOOST_TEST_MESSAGE("------ check fail (auth)");
    BOOST_CHECK_EXCEPTION(check(nester, bob, 1), missing_auth_exception, any);

    BOOST_TEST_MESSAGE("------ nested_check success");
    nested_check(nester, nester, 2);
    nested_check_inline(nester, nester, 3);
    BOOST_TEST_MESSAGE("------ nested_check success nested auth (escalation)");
    nested_check(nester, bob, 4);
    nested_check_inline(nester, bob, 5);
    BOOST_TEST_MESSAGE("------ nested_check assert inside nested");
    BOOST_CHECK_EXCEPTION(nested_check(nester, nester, 0), eosio_assert_message_exception, msg(err_le0));
    BOOST_CHECK_EXCEPTION(nested_check(nester, bob, 0), eosio_assert_message_exception, msg(err_le0));
    BOOST_TEST_MESSAGE("------ nested_check assert after sending nested");
    BOOST_CHECK_EXCEPTION(nested_check(nester, nester, 100), eosio_assert_message_exception, msg(err_ge100));
    BOOST_CHECK_EXCEPTION(nested_check(nester, bob, 100), eosio_assert_message_exception, msg(err_ge100));
    BOOST_TEST_MESSAGE("------ nested_check assert after sending inline");
    BOOST_CHECK_EXCEPTION(nested_check_inline(nester, nester, 50), eosio_assert_message_exception, msg(err_ge50));
    BOOST_CHECK_EXCEPTION(nested_check_inline(nester, bob, 50), eosio_assert_message_exception, msg(err_ge50));
    BOOST_CHECK_EXCEPTION(nested_check(nester, bob, 0), eosio_assert_message_exception, msg(err_le0));

    BOOST_TEST_MESSAGE("------ send_nested success");
    send_nested_simple(nester, N(auth), nester.value);
    BOOST_TEST_MESSAGE("------ send_nested fail auth inside nested");
    BOOST_CHECK_EXCEPTION(send_nested_simple(nester, N(auth), bob.value), missing_auth_exception, any);

    BOOST_TEST_MESSAGE("--- Only privileged can nest trx");
    auth(normal, normal, normal);
    check(normal, normal, 1);
    BOOST_CHECK_EXCEPTION(nested_check(normal, normal, 1), not_privileged_nested_tx, any);
    BOOST_CHECK_EXCEPTION(nested_check_inline(normal, normal, 1), not_privileged_nested_tx, any);
    BOOST_CHECK_EXCEPTION(send_nested_simple(normal, N(auth), normal.value), not_privileged_nested_tx, any);

    BOOST_TEST_MESSAGE("--- Only one level nesting allowed");
    BOOST_CHECK_EXCEPTION(send_nested_simple(nester, N(nestedcheck), 5), second_nested_tx, any);
    BOOST_CHECK_EXCEPTION(send_nested_simple(nester, N(nestedchecki), 5), second_nested_tx, any);
    BOOST_CHECK_EXCEPTION(send_nested_simple(nester, N(nestedcheck2), 5), second_nested_tx, any);

    BOOST_TEST_MESSAGE("--- Only one nesting allowed in a trx");
    produce_block(); // avoid dups
    BOOST_CHECK_EXCEPTION(nested_check2(nester, nester, 1), second_nested_tx, any);
    signed_transaction trx;
    auto make_trx = [&](name action1, name action2, name action3 = {}) -> signed_transaction& {
        trx = signed_transaction{};
        trx.actions.push_back(get_action(nester, action1, {{nester, config::active_name}}, mvo()("arg",1)));
        trx.actions.push_back(get_action(nester, action2, {{nester, config::active_name}}, mvo()("arg",2)));
        if (action3 != name()) {
            trx.actions.push_back(get_action(nester, action3, {{nester, config::active_name}}, mvo()("arg",3)));
        }
        set_transaction_headers(trx);
        trx.sign(get_private_key(nester, "active"), control->get_chain_id());
        return trx;
    };
    BOOST_REQUIRE_EXCEPTION(push_transaction(make_trx(N(nestedcheck), N(nestedcheck))), second_nested_tx, any);
    BOOST_REQUIRE_EXCEPTION(push_transaction(make_trx(N(nestedchecki), N(nestedcheck))), second_nested_tx, any);
    BOOST_REQUIRE_EXCEPTION(push_transaction(make_trx(N(nestedcheck), N(nestedchecki))), second_nested_tx, any);
    BOOST_REQUIRE_EXCEPTION(push_transaction(make_trx(N(nestedchecki), N(nestedchecki))), second_nested_tx, any);
    BOOST_REQUIRE_EXCEPTION(push_transaction(make_trx(N(check), N(nestedcheck2))), second_nested_tx, any);
    BOOST_REQUIRE_EXCEPTION(push_transaction(make_trx(N(nestedcheck),N(check),N(nestedcheck))), second_nested_tx, any);
    BOOST_REQUIRE_EXCEPTION(push_transaction(make_trx(N(nestedcheck),N(check),N(nestedchecki))), second_nested_tx, any);

    BOOST_TEST_MESSAGE("--- No context-free actions allowed in a nesting trx");
    BOOST_REQUIRE_EXCEPTION(base_tester::push_action(nester, N(sendnestedcfa), nester, mvo("arg", nester)),
        transaction_exception,
        [](const auto& e) {return e.top_message() == "context free actions are not allowed in nested transactions";}
    );

} FC_LOG_AND_RETHROW() }

#define NOTHING
#define INIT_USAGES GET_USAGES1(auto)
#define GET_USAGES GET_USAGES1(NOTHING)
#define GET_USAGES1(PREFIX) \
    PREFIX ua1 = rlm.get_account_usage(alice); \
    PREFIX ub1 = rlm.get_account_usage(bob); \
    PREFIX uc1 = rlm.get_account_usage(carol);
#define FIX_USAGES ua0 = ua1; ub0 = ub1; uc0 = uc1;
#define CHECK_USAGE_EQ(PREV, NOW) BOOST_CHECK_EQUAL_COLLECTIONS(PREV.begin(), PREV.end(), NOW.begin(), NOW.end());
#define CHECK_USAGE_INC(PREV, NOW) BOOST_CHECK(PREV[0] < NOW[0] && PREV[2] < NOW[2] && PREV[1] <= (NOW[1]+1)); // NET restores too fast, so inc

string usage_diff(const vector<uint64_t>& a, const vector<uint64_t>& b) {
    string result = ": (";
    bool diff = false;
    for (int i = 0; i < 4; i++) {
        const auto t = int64_t(b[i]) - int64_t(a[i]);
        if (t) {
            diff = true;
        }
        //CPU, NET, RAM, STORAGE
        result = result + ("CNRS"[i]) + (t > 0 ? "+" : "") + std::to_string(t) + (i == 3 ? ")" : ", ");
    }
    return diff ? result : ": same";
}

BOOST_FIXTURE_TEST_CASE(providebw, nested_tester) { try {
    BOOST_TEST_MESSAGE("providebw nesting tests");
    init();
    const auto any = [](auto&){return true;};

#   define PRINT_USAGES \
    BOOST_TEST_MESSAGE("       alice used: " << fc::json::to_string(ua1) << usage_diff(ua0, ua1)); \
    BOOST_TEST_MESSAGE("         bob used: " << fc::json::to_string(ub1) << usage_diff(ub0, ub1)); \
    BOOST_TEST_MESSAGE("       carol used: " << fc::json::to_string(uc1) << usage_diff(uc0, uc1));

    auto& rlm = control->get_mutable_resource_limits_manager();
    // auto stake1 = rlm.get_account_balance(control->head_block_time(), alice, rlm.get_pricelist(), false);
    // auto stake2 = rlm.get_account_balance(control->head_block_time(), bob, rlm.get_pricelist(), false);
    // BOOST_TEST_MESSAGE("... alice stake: " << fc::json::to_string(stake1));
    // BOOST_TEST_MESSAGE("...   bob stake: " << fc::json::to_string(stake2));
    INIT_USAGES;
    auto ua0 = ua1;
    auto ub0 = ub1;
    auto uc0 = uc1;
    PRINT_USAGES;

#   define CHECK_ALICE_BOB_CAROL_USAGES(A, B, C) \
    GET_USAGES; \
    PRINT_USAGES; \
    CHECK_USAGE_##A(ua0, ua1); \
    CHECK_USAGE_##B(ub0, ub1); \
    CHECK_USAGE_##C(uc0, uc1); \
    FIX_USAGES; \
    produce_block();

    BOOST_TEST_MESSAGE("--- without nesting");
    BOOST_TEST_MESSAGE("...... alice uses own bw");
    store(alice);
    CHECK_ALICE_BOB_CAROL_USAGES(INC, EQ, EQ);

    BOOST_TEST_MESSAGE("...... bob bw-> alice");
    signed_transaction trx;
    auto make_provide_trx = [&](name actor, name provider) -> signed_transaction& {
        trx = signed_transaction{};
        trx.actions.push_back(get_action(nester, N(put), {{actor, config::active_name}}, mvo()("who",actor)));
        trx.actions.emplace_back(
            vector<permission_level>{{provider, config::active_name}}, cyberway::chain::providebw(provider, actor)
        );
        set_transaction_headers(trx);
        trx.sign(get_private_key(actor, "active"), control->get_chain_id());
        trx.sign(get_private_key(provider, "active"), control->get_chain_id());
        return trx;
    };
    push_transaction(make_provide_trx(alice, bob));
    CHECK_ALICE_BOB_CAROL_USAGES(EQ, INC, EQ);

    BOOST_TEST_MESSAGE("--- nesting with inner provide");
    BOOST_TEST_MESSAGE("------ inner provide works");
    BOOST_TEST_MESSAGE("...... alice sends trx and bw-> for nested");
    send_nested(nester, alice, bob, N(put), bob.value, alice);
    CHECK_ALICE_BOB_CAROL_USAGES(INC, EQ, EQ);
    BOOST_TEST_MESSAGE("...... nester sends trx and bw-> for nested");
    send_nested(nester, nester, bob, N(put), bob.value, nester);
    CHECK_ALICE_BOB_CAROL_USAGES(EQ, EQ, EQ);
    BOOST_TEST_MESSAGE("...... nester sends trx, alice bw-> for nested");
    send_nested(nester, nester, bob, N(put), bob.value, alice);
    CHECK_ALICE_BOB_CAROL_USAGES(INC, EQ, EQ);
    BOOST_TEST_MESSAGE("...... alice sends trx, carol bw-> for nested");
    send_nested(nester, alice, bob, N(put), bob.value, carol);
    CHECK_ALICE_BOB_CAROL_USAGES(INC, EQ, INC);

    BOOST_TEST_MESSAGE("------ only inner usage");
    BOOST_TEST_MESSAGE("...... bob sends trx, alice bw-> bob for nested");
    send_nested(nester, bob, bob, N(put), bob.value, alice);
    CHECK_ALICE_BOB_CAROL_USAGES(INC, INC, EQ);

    BOOST_TEST_MESSAGE("--- nesting with outer provide");
    auto make_trx = [&](name sender, name actor, name provider, name n_actor, name n_prov = {}) -> signed_transaction& {
        trx = signed_transaction{};
        trx.actions.push_back(get_action(nester, N(sendnested), {{sender, config::active_name}}, mvo()
            ("actor", n_actor)
            ("action", N(put))
            ("arg", n_actor.value)
            ("delay", 0)
            ("expiration", 30)
            ("provide", n_prov)
        ));
        trx.actions.emplace_back(
            vector<permission_level>{{provider, config::active_name}}, cyberway::chain::providebw(provider, actor)
        );
        set_transaction_headers(trx);
        trx.sign(get_private_key(sender, "active"), control->get_chain_id());
        if (sender != provider)
            trx.sign(get_private_key(provider, "active"), control->get_chain_id());
        return trx;
    };
    BOOST_TEST_MESSAGE("------ outer provide works");
    BOOST_TEST_MESSAGE("...... alice bw-> bob who sends trx, carol acts in nested");
    push_transaction2(make_trx(bob, bob, alice, carol), true);
    CHECK_ALICE_BOB_CAROL_USAGES(INC, EQ, INC);
    BOOST_TEST_MESSAGE("------ provides to inner too");
    BOOST_TEST_MESSAGE("...... alice sends trx and bw-> bob who acts in nested");
    push_transaction2(make_trx(alice, bob, alice, bob), true);
    CHECK_ALICE_BOB_CAROL_USAGES(INC, EQ, EQ);
    BOOST_TEST_MESSAGE("...... alice bw-> bob who sends trx and acts in nested");
    push_transaction2(make_trx(bob, bob, alice, bob), true);
    CHECK_ALICE_BOB_CAROL_USAGES(INC, EQ, EQ);
    BOOST_TEST_MESSAGE("...... alice sends trx, carol bw-> bob who acts in nested");
    push_transaction2(make_trx(alice, bob, carol, bob), true);
    CHECK_ALICE_BOB_CAROL_USAGES(INC, EQ, INC);

    BOOST_TEST_MESSAGE("--- nesting with both outer and inner provides");
    BOOST_TEST_MESSAGE("------ overriding outer provider in a nested trx fails");
    BOOST_REQUIRE_EXCEPTION(push_transaction2(make_trx(alice, bob, carol, bob, carol), true), bw_provider_error, any);
    BOOST_REQUIRE_EXCEPTION(push_transaction2(make_trx(alice, bob, carol, bob, alice), true), bw_provider_error, any);
    BOOST_TEST_MESSAGE("------ inner provide = inner usage; outer provide = any usage");
    BOOST_TEST_MESSAGE("...... alice sends trx and acts in nested; carol bw-> bob, inner carol bw-> alice");
    push_transaction2(make_trx(alice, bob, carol, alice, carol), true);
    CHECK_ALICE_BOB_CAROL_USAGES(INC, EQ, INC);
    BOOST_TEST_MESSAGE("------ same provider for inner and outer");
    BOOST_TEST_MESSAGE("...... alice sends trx, carol bw-> alice; bob acts in nested, inner carol bw-> bob also");
    push_transaction2(make_trx(alice, alice, carol, bob, carol), true);
    CHECK_ALICE_BOB_CAROL_USAGES(EQ, EQ, INC);

} FC_LOG_AND_RETHROW() }

BOOST_FIXTURE_TEST_CASE(expired, nested_tester) { try {
    BOOST_TEST_MESSAGE("Test nested trx fields validation");
    init();
    BOOST_TEST_MESSAGE("--- Delay takes no effect (used by msig)");
    BOOST_CHECK_EXCEPTION(send_nested_simple(nester, N(auth), bob.value, 300),
        missing_auth_exception, [](auto&){return true;});
    send_nested_simple(nester, N(auth), nester.value, -1);
    produce_block();
    send_nested_simple(nester, N(auth), nester.value, 300);
    BOOST_TEST_MESSAGE("--- Expiration should be >= 0 (takes no effect, used by msig)");
    produce_blocks(5); // avoid dups: expiration resets before trx_id calculation
    send_nested_simple(nester, N(auth), nester.value, 0, -1);
    produce_block();
    send_nested_simple(nester, N(auth), nester.value, 0, 0);
    produce_block();
    send_nested_simple(nester, N(auth), nester.value, 0, 1);

} FC_LOG_AND_RETHROW() }

void print_block(const signed_block_ptr b) {
    const auto& trxs = b->transactions;
    string o = "";
    o += "Transaction(s) = " + std::to_string(trxs.size()) + ": [\n";
    for (const auto& r: trxs) {
        bool have_id = r.trx.contains<transaction_id_type>();
        if (have_id) {
            const auto& id = r.trx.get<transaction_id_type>();
            o += string("  id: ") + id.str();
        } else {
            const auto& id = r.trx.get<packed_transaction>().packed_digest();
            o += string("  pkd:") + id.str();
        }
        o += string(" status: ") + std::to_string(r.status);
        o += '\n';
    }
    o += ']';
    BOOST_TEST_MESSAGE("--- block: " << o);
}

BOOST_FIXTURE_TEST_CASE(deferred, nested_tester) { try {
    BOOST_TEST_MESSAGE("Test nesting from deferred");
    init();
    BOOST_TEST_MESSAGE("--- Deffered transaction cannot send nested one");
    uint32_t delay = 3;
    signed_transaction trx{};
    trx.actions.push_back(get_action(nester, N(nestedcheck), {{bob, config::active_name}}, mvo()("arg", 1)));
    set_transaction_headers(trx, 300, delay);
    trx.sign(get_private_key(bob, "active"), control->get_chain_id());
    push_transaction2(trx, true);
    BOOST_TEST_MESSAGE("--- schedule");
    auto block = produce_block();
    print_block(block);
    // Can't catch this
    // BOOST_REQUIRE_EXCEPTION(produce_block(), transaction_exception, [](const auto& e){
    //     return e.top_message() == "deferred trx can't start nested trx";
    // });
    BOOST_TEST_MESSAGE("--- ensure that deffered transaction hard-failed");
    ignore_scheduled_fail = true;
    block = produce_block();
    BOOST_REQUIRE_EQUAL(block->transactions.size(), 1);
    auto receipt = block->transactions[0];
    BOOST_CHECK_EQUAL(receipt.trx.contains<transaction_id_type>(), true);
    BOOST_CHECK_EQUAL(receipt.status, transaction_receipt_header::hard_fail);
    print_block(block);
    ignore_scheduled_fail = false;
    produce_blocks(2);

} FC_LOG_AND_RETHROW() }

BOOST_FIXTURE_TEST_CASE(duplicate, nested_tester) { try {
    BOOST_TEST_MESSAGE("Test several nested with same trx_id");
    init();

    signed_transaction trx;
    auto make_trx = [&](name action) -> signed_transaction& {
        trx = signed_transaction{};
        trx.actions.push_back(get_action(nester, action, {{bob, config::active_name}}, mvo()("arg", 1)));
        set_transaction_headers(trx);
        trx.sign(get_private_key(bob, "active"), control->get_chain_id());
        return trx;
    };
    // push different transactions, which produce same nested transaction
    push_transaction2(make_trx(N(nestedcheck)), true);
    BOOST_REQUIRE_EXCEPTION(push_transaction2(make_trx(N(nestedchecki)), true);, tx_duplicate, [](auto&){return true;});
    auto block = produce_block();
    print_block(block);

} FC_LOG_AND_RETHROW() }


BOOST_AUTO_TEST_SUITE_END()
