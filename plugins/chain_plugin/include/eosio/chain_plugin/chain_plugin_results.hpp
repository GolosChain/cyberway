#pragma once

#include <eosio/chain/types.hpp>
#include <eosio/chain/name.hpp>
#include <eosio/chain/resource_limits.hpp>
#include <eosio/chain/authority.hpp>
#include <eosio/chain/asset.hpp>

#include <fc/optional.hpp>

namespace eosio {

    struct permission {
        chain::name      perm_name;
        chain::name      parent;
        chain::authority required_auth;
    };

    struct get_account_results {
       chain::name                                           account_name;
       uint32_t                                              head_block_num = 0;
       fc::time_point                                        head_block_time;
       bool                                                  privileged = false;
       fc::time_point                                        last_code_update;
       fc::time_point                                        created;
       fc::optional<chain::asset>                            core_liquid_balance;
    //TODO: replace it (? with ram/net/cpu staked)
       int64_t                                               ram_quota  = 0;
       int64_t                                                net_weight = 0;
       int64_t                                               cpu_weight = 0;

       eosio::chain::resource_limits::account_resource_limit net_limit;
       eosio::chain::resource_limits::account_resource_limit cpu_limit;
       int64_t                                               ram_usage = 0;
       std::vector<permission>                               permissions;
       fc::variant                                           total_resources;
       fc::variant                                           self_delegated_bandwidth;
       fc::variant                                           refund_request;
       fc::variant                                           voter_info;
    };

    struct get_info_results {
        std::string             server_version;
        chain::chain_id_type    chain_id;
        uint32_t                head_block_num = 0;
        uint32_t                last_irreversible_block_num = 0;
        chain::block_id_type    last_irreversible_block_id;
        chain::block_id_type    head_block_id;
        fc::time_point          head_block_time;
        chain::account_name            head_block_producer;

        uint64_t                virtual_block_cpu_limit = 0;
        uint64_t                virtual_block_net_limit = 0;

        uint64_t                block_cpu_limit = 0;
        uint64_t                block_net_limit = 0;
        fc::optional<std::string>        server_version_string;
    };

    struct push_block_results{};

    struct push_transaction_results {
        chain::transaction_id_type  transaction_id;
        fc::variant                 processed;
    };

    using push_transactions_results = std::vector<push_transaction_results>;
}
FC_REFLECT( eosio::permission, (perm_name)(parent)(required_auth) )
FC_REFLECT( eosio::get_account_results,
            (account_name)(head_block_num)(head_block_time)(privileged)(last_code_update)(created)
            (core_liquid_balance)(ram_quota)(net_weight)(cpu_weight)(net_limit)(cpu_limit)(ram_usage)(permissions)
            (total_resources)(self_delegated_bandwidth)(refund_request)(voter_info) )

FC_REFLECT(eosio::get_info_results,
           (server_version)(chain_id)(head_block_num)(last_irreversible_block_num)(last_irreversible_block_id)
           (head_block_id)(head_block_time)(head_block_producer)(virtual_block_cpu_limit)(virtual_block_net_limit)
           (block_cpu_limit)(block_net_limit)(server_version_string) )

FC_REFLECT_EMPTY( eosio::push_block_results)
FC_REFLECT( eosio::push_transaction_results, (transaction_id)(processed) )


