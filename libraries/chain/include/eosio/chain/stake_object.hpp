/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/symbol.hpp>
#include <eosio/chain/types.hpp>
#include <eosio/chain/database_utils.hpp>
#include <eosio/chain/block_timestamp.hpp>
#include <eosio/chain/multi_index_includes.hpp>
#include <eosio/chain/int_arithmetic.hpp>

namespace eosio { namespace chain {

class stake_agent_object : public cyberway::chaindb::object<stake_agent_object_type, stake_agent_object> {
    CHAINDB_OBJECT_ID_CTOR(stake_agent_object)
    id_type id;  
    symbol_code token_code;
    account_name account;
    uint8_t proxy_level;
    time_point_sec last_proxied_update;
    int64_t balance;
    int64_t proxied;
    int64_t shares_sum;
    int64_t own_share;
    int16_t fee;
    int64_t min_own_staked;
    int64_t provided;
    int64_t received;
    int64_t get_total_funds()const { return balance + proxied; };
        
    int64_t get_own_funds() const {
        auto total_funds = get_total_funds();
        EOS_ASSERT(total_funds >= 0, chain_exception, "SYSTEM: incorrect total_funds value");
        EOS_ASSERT(own_share >= 0, chain_exception, "SYSTEM: incorrect own_share value");
        EOS_ASSERT(shares_sum >= 0, chain_exception, "SYSTEM: incorrect shares_sum value");
        EOS_ASSERT((total_funds == 0) == (shares_sum == 0), chain_exception, "SYSTEM: incorrect total_funds or shares_sum");
        return total_funds ? int_arithmetic::safe_prop(total_funds, own_share, shares_sum) : 0;
    }
    
    int64_t get_effective_stake() const {
        auto own_funds = get_own_funds();
        EOS_ASSERT(provided <= own_funds, chain_exception, "SYSTEM: incorrect provided or own_funds");
        EOS_ASSERT(received >= 0, chain_exception, "SYSTEM: incorrect received");
        return (own_funds - provided) + received;
    }
    struct by_key {};
};

using stake_agent_table = cyberway::chaindb::table_container<
    stake_agent_object,
    cyberway::chaindb::indexed_by<
        cyberway::chaindb::ordered_unique<cyberway::chaindb::tag<by_id>, 
           BOOST_MULTI_INDEX_MEMBER(stake_agent_object, stake_agent_object::id_type, id)>,
        cyberway::chaindb::ordered_unique<cyberway::chaindb::tag<stake_agent_object::by_key>,
           cyberway::chaindb::composite_key<stake_agent_object,
              BOOST_MULTI_INDEX_MEMBER(stake_agent_object, symbol_code, token_code),
              BOOST_MULTI_INDEX_MEMBER(stake_agent_object, account_name, account)>
        >
    >
>;

class stake_candidate_object : public cyberway::chaindb::object<stake_candidate_object_type, stake_candidate_object> {
    CHAINDB_OBJECT_ID_CTOR(stake_candidate_object)
    id_type id;  
    symbol_code token_code;
    account_name account;
    time_point_sec latest_pick;
    int64_t votes;
    int64_t priority;
    public_key_type signing_key;
    bool enabled;
    void set_votes(int64_t arg, int64_t cur_supply) {
        EOS_ASSERT(arg >= 0, transaction_exception, "SYSTEM: votes can't be nagative");
        votes = arg;
        if (!votes) {
            priority = std::numeric_limits<int64_t>::max();
        }
        else {
            static constexpr int128_t int64_max = std::numeric_limits<int64_t>::max();
            auto priority128 = std::min(static_cast<int128_t>(cur_supply) * config::priority_precision / votes, int64_max);
            priority128 += latest_pick.sec_since_epoch() * config::priority_precision;
            priority = static_cast<int64_t>(std::min(priority128, int64_max));
        }
    }
    struct by_key {};
    struct by_votes {};
    struct by_prior {};
};

using stake_candidate_table = cyberway::chaindb::table_container<
    stake_candidate_object,
    cyberway::chaindb::indexed_by<
        cyberway::chaindb::ordered_unique<cyberway::chaindb::tag<by_id>, 
           BOOST_MULTI_INDEX_MEMBER(stake_candidate_object, stake_candidate_object::id_type, id)>,
        cyberway::chaindb::ordered_unique<cyberway::chaindb::tag<stake_candidate_object::by_key>,
           cyberway::chaindb::composite_key<stake_candidate_object,
              BOOST_MULTI_INDEX_MEMBER(stake_candidate_object, symbol_code, token_code),
              BOOST_MULTI_INDEX_MEMBER(stake_candidate_object, account_name, account)>
        >,
        cyberway::chaindb::ordered_unique<cyberway::chaindb::tag<stake_candidate_object::by_votes>,
           cyberway::chaindb::composite_key<stake_candidate_object,
              BOOST_MULTI_INDEX_MEMBER(stake_candidate_object, symbol_code, token_code),
              BOOST_MULTI_INDEX_MEMBER(stake_candidate_object, bool, enabled),
              BOOST_MULTI_INDEX_MEMBER(stake_candidate_object, int64_t, votes),
              BOOST_MULTI_INDEX_MEMBER(stake_candidate_object, account_name, account)>
        >,
        cyberway::chaindb::ordered_unique<cyberway::chaindb::tag<stake_candidate_object::by_votes>,
           cyberway::chaindb::composite_key<stake_candidate_object,
              BOOST_MULTI_INDEX_MEMBER(stake_candidate_object, symbol_code, token_code),
              BOOST_MULTI_INDEX_MEMBER(stake_candidate_object, bool, enabled),
              BOOST_MULTI_INDEX_MEMBER(stake_candidate_object, int64_t, priority),
              BOOST_MULTI_INDEX_MEMBER(stake_candidate_object, int64_t, votes),
              BOOST_MULTI_INDEX_MEMBER(stake_candidate_object, account_name, account)>
        >
    >
>;

class stake_auto_recall_object : public cyberway::chaindb::object<stake_auto_recall_object_type, stake_auto_recall_object> {
    CHAINDB_OBJECT_ID_CTOR(stake_auto_recall_object)
    id_type id;
    symbol_code token_code;
    account_name account;
    bool break_fee_enabled = false;
    bool break_min_stake_enabled = false;
    struct by_key {};
};

using stake_auto_recall_table = cyberway::chaindb::table_container<
    stake_auto_recall_object,
    cyberway::chaindb::indexed_by<
        cyberway::chaindb::ordered_unique<cyberway::chaindb::tag<by_id>, 
           BOOST_MULTI_INDEX_MEMBER(stake_auto_recall_object, stake_auto_recall_object::id_type, id)>,
        cyberway::chaindb::ordered_unique<cyberway::chaindb::tag<stake_auto_recall_object::by_key>,
           cyberway::chaindb::composite_key<stake_auto_recall_object,
              BOOST_MULTI_INDEX_MEMBER(stake_auto_recall_object, symbol_code, token_code),
              BOOST_MULTI_INDEX_MEMBER(stake_auto_recall_object, account_name, account)>
        >
    >
>;

class stake_grant_object : public cyberway::chaindb::object<stake_grant_object_type, stake_grant_object> {
    CHAINDB_OBJECT_ID_CTOR(stake_grant_object)
    id_type id;
    symbol_code token_code;
    account_name grantor_name;
    account_name recipient_name;
    int16_t pct;
    int64_t share;
    int16_t break_fee;
    int64_t break_min_own_staked;
    
    struct by_key {};
};

using stake_grant_table = cyberway::chaindb::table_container<
    stake_grant_object,
    cyberway::chaindb::indexed_by<
        cyberway::chaindb::ordered_unique<cyberway::chaindb::tag<by_id>,
            BOOST_MULTI_INDEX_MEMBER(stake_grant_object, stake_grant_object::id_type, id)>,
        cyberway::chaindb::ordered_unique<cyberway::chaindb::tag<stake_grant_object::by_key>,
           cyberway::chaindb::composite_key<stake_grant_object,
              BOOST_MULTI_INDEX_MEMBER(stake_grant_object, symbol_code, token_code),
              BOOST_MULTI_INDEX_MEMBER(stake_grant_object, account_name, grantor_name),
              BOOST_MULTI_INDEX_MEMBER(stake_grant_object, account_name, recipient_name)>
        >
    >
>;

class stake_param_object : public cyberway::chaindb::object<stake_param_object_type, stake_param_object> {
    CHAINDB_OBJECT_ID_CTOR(stake_param_object)
    id_type id;
    symbol token_symbol;
    std::vector<uint8_t> max_proxies;
    int64_t depriving_window;
    int64_t min_own_staked_for_election = 0;
};

using stake_param_table = cyberway::chaindb::table_container<
    stake_param_object,
    cyberway::chaindb::indexed_by<
        cyberway::chaindb::ordered_unique<cyberway::chaindb::tag<by_id>, BOOST_MULTI_INDEX_MEMBER(stake_param_object, stake_param_object::id_type, id)>
    >
>;
    
class stake_stat_object : public cyberway::chaindb::object<stake_stat_object_type, stake_stat_object> {
    CHAINDB_OBJECT_ID_CTOR(stake_stat_object)
    id_type id;
    symbol_code token_code;
    int64_t total_staked;
    int64_t total_votes;
    time_point_sec last_reward;
    bool enabled;
};

using stake_stat_table = cyberway::chaindb::table_container<
    stake_stat_object,
    cyberway::chaindb::indexed_by<
        cyberway::chaindb::ordered_unique<cyberway::chaindb::tag<by_id>, BOOST_MULTI_INDEX_MEMBER(stake_stat_object, stake_stat_object::id_type, id)>
    >
>;

} } // eosio::chain

CHAINDB_SET_TABLE_TYPE(eosio::chain::stake_agent_object, eosio::chain::stake_agent_table)
CHAINDB_TAG(eosio::chain::stake_agent_object::by_key, bykey)
CHAINDB_TAG(eosio::chain::stake_agent_object, stake.agent)

CHAINDB_SET_TABLE_TYPE(eosio::chain::stake_candidate_object, eosio::chain::stake_candidate_table)
CHAINDB_TAG(eosio::chain::stake_candidate_object::by_key, bykey)
CHAINDB_TAG(eosio::chain::stake_candidate_object::by_votes, byvotes)
CHAINDB_TAG(eosio::chain::stake_candidate_object::by_prior, byprior)
CHAINDB_TAG(eosio::chain::stake_candidate_object, stake.cand)

CHAINDB_SET_TABLE_TYPE(eosio::chain::stake_auto_recall_object, eosio::chain::stake_auto_recall_table)
CHAINDB_TAG(eosio::chain::stake_auto_recall_object::by_key, bykey)
CHAINDB_TAG(eosio::chain::stake_auto_recall_object, stake.autorc)

CHAINDB_SET_TABLE_TYPE(eosio::chain::stake_grant_object, eosio::chain::stake_grant_table)
CHAINDB_TAG(eosio::chain::stake_grant_object::by_key, bykey)
CHAINDB_TAG(eosio::chain::stake_grant_object, stake.grant)

CHAINDB_SET_TABLE_TYPE(eosio::chain::stake_param_object, eosio::chain::stake_param_table)
CHAINDB_TAG(eosio::chain::stake_param_object, stake.param)

CHAINDB_SET_TABLE_TYPE(eosio::chain::stake_stat_object, eosio::chain::stake_stat_table)
CHAINDB_TAG(eosio::chain::stake_stat_object, stake.stat)

FC_REFLECT(eosio::chain::stake_agent_object, 
    (id)(token_code)(account)(proxy_level)(last_proxied_update)(balance)
    (proxied)(shares_sum)(own_share)(fee)(min_own_staked)(provided)(received))
    
FC_REFLECT(eosio::chain::stake_candidate_object, 
    (id)(token_code)(account)(latest_pick)(votes)(priority)(signing_key)(enabled))
    
FC_REFLECT(eosio::chain::stake_auto_recall_object, 
     (id)(token_code)(account)(break_fee_enabled)(break_min_stake_enabled))
    
FC_REFLECT(eosio::chain::stake_grant_object, 
    (id)(token_code)(grantor_name)(recipient_name)(pct)(share)(break_fee)(break_min_own_staked))
    
FC_REFLECT(eosio::chain::stake_param_object, 
    (id)(token_symbol)(max_proxies)(depriving_window)(min_own_staked_for_election))
    
FC_REFLECT(eosio::chain::stake_stat_object, 
    (id)(token_code)(total_staked)(total_votes)(last_reward)(enabled))
