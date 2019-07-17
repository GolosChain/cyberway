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
using chain::public_key_type;
using chain::permission_name;

class public_key_history_object : public cyberway::chaindb::object<chain::public_key_history_object_type, public_key_history_object> {
   CHAINDB_OBJECT_ID_CTOR(public_key_history_object)

   id_type           id;
   public_key_type   public_key;
   account_name      name;
   permission_name   permission;
};

struct by_pub_key;
struct by_account_permission;

using public_key_history_table = cyberway::chaindb::table_container<
    public_key_history_object,
    cyberway::chaindb::indexed_by<
        cyberway::chaindb::ordered_unique<cyberway::chaindb::tag<by_id>,
            BOOST_MULTI_INDEX_MEMBER(public_key_history_object, public_key_history_object::id_type, id)>,
        cyberway::chaindb::ordered_unique<cyberway::chaindb::tag<by_pub_key>,
            cyberway::chaindb::composite_key< public_key_history_object,
                BOOST_MULTI_INDEX_MEMBER(public_key_history_object, public_key_type, public_key),
                BOOST_MULTI_INDEX_MEMBER(public_key_history_object, public_key_history_object::id_type, id)
            >
        >,
        cyberway::chaindb::ordered_unique<cyberway::chaindb::tag<by_account_permission>,
            cyberway::chaindb::composite_key< public_key_history_object,
                BOOST_MULTI_INDEX_MEMBER(public_key_history_object, account_name, name),
                BOOST_MULTI_INDEX_MEMBER(public_key_history_object, permission_name, permission),
                BOOST_MULTI_INDEX_MEMBER(public_key_history_object, public_key_history_object::id_type, id)
            >
        >
    >
>;
}

CHAINDB_TAG(eosio::by_pub_key, bypubkey)
CHAINDB_TAG(eosio::by_account_permission, byaccperm)

CHAINDB_SET_TABLE_TYPE( eosio::public_key_history_object, eosio::public_key_history_table )
CHAINDB_TABLE_TAG(eosio::public_key_history_object, pubkeyhist, cyber.history)

FC_REFLECT( eosio::public_key_history_object, (id)(public_key)(name)(permission) )

