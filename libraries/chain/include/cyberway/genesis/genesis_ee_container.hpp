#pragma once

#include <eosio/chain/types.hpp>
#include <cyberway/chaindb/controller.hpp>
#include <fc/reflect/reflect.hpp>

namespace cyberway { namespace genesis {

using namespace eosio::chain;
using namespace chaindb;

struct genesis_ee_header {
    char magic[14] = "CyberwayEEGen";
    uint32_t version = 1;

    bool is_valid() {
        genesis_ee_header oth;
        return string(magic) == oth.magic && version == oth.version;
    }
};

enum class table_ee_type {
    messages
};

struct table_ee_header {
    table_ee_type type;

    table_ee_header(table_ee_type type_)
            : type(type_) {
    }
};

struct message_ee_object {
    account_name parent_author;
    string parent_permlink;
    account_name author;
    string permlink;
    string title;
    string body;
    string json_metadata;
    int64_t net_rshares;
    asset author_reward;
    asset benefactor_reward;
    asset curator_reward;
};

}} // cyberway::genesis

FC_REFLECT_ENUM(cyberway::genesis::table_ee_type, (messages))
FC_REFLECT(cyberway::genesis::table_ee_header, (type))

FC_REFLECT(cyberway::genesis::message_ee_object, (parent_author)(parent_permlink)(author)(permlink)(title)(body)(json_metadata)
    (net_rshares)(author_reward)(benefactor_reward)(curator_reward))
