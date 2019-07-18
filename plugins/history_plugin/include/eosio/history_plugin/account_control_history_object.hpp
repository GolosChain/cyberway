/**
 *  @file
 *  @copyright defined in eos/LICENSE
 */
#pragma once

#include <eosio/chain/database_utils.hpp>
#include <eosio/chain/authority.hpp>
#include <eosio/chain/block_timestamp.hpp>
#include <eosio/chain/abi_def.hpp>
#include <eosio/chain/multi_index_includes.hpp>

#include <cyberway/chaindb/abi_info.hpp>

namespace eosio {
using chain::account_name;
using chain::permission_name;
using chain::shared_vector;
using chain::transaction_id_type;

class account_control_history_object : public cyberway::chaindb::object<chain::account_control_history_object_type, account_control_history_object> {
   CHAINDB_OBJECT_ID_CTOR(account_control_history_object)

   id_type                            id;
   account_name                       controlled_account;
   permission_name                    controlled_permission;
   account_name                       controlling_account;
};

struct by_controlling;
struct by_controlled_authority;

using account_control_history_table = cyberway::chaindb::table_container<
   account_control_history_object,
   cyberway::chaindb::indexed_by<
      cyberway::chaindb::ordered_unique<cyberway::chaindb::tag<by_id>, BOOST_MULTI_INDEX_MEMBER(account_control_history_object, account_control_history_object::id_type, id)>,
      cyberway::chaindb::ordered_unique<cyberway::chaindb::tag<by_controlling>,
         cyberway::chaindb::composite_key< account_control_history_object,
            BOOST_MULTI_INDEX_MEMBER(account_control_history_object, account_name, controlling_account),
            BOOST_MULTI_INDEX_MEMBER(account_control_history_object, account_control_history_object::id_type, id)
         >
      >,
      cyberway::chaindb::ordered_unique<cyberway::chaindb::tag<by_controlled_authority>,
         cyberway::chaindb::composite_key< account_control_history_object,
            BOOST_MULTI_INDEX_MEMBER(account_control_history_object, account_name, controlled_account),
            BOOST_MULTI_INDEX_MEMBER(account_control_history_object, permission_name, controlled_permission),
            BOOST_MULTI_INDEX_MEMBER(account_control_history_object, account_name, controlling_account)
         >
      >
   >
>;
}

CHAINDB_TAG(eosio::by_controlling, bycontrol)
CHAINDB_TAG(eosio::by_controlled_authority, controlauth)

CHAINDB_SET_TABLE_TYPE( eosio::account_control_history_object, eosio::account_control_history_table )
CHAINDB_TABLE_TAG(eosio::account_control_history_object, .history, ctrlhistory)
FC_REFLECT( eosio::account_control_history_object, (id)(controlled_account)(controlled_permission)(controlling_account) )

