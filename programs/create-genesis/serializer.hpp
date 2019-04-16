#pragma once
#include "ofstream_sha256.hpp"
#include "genesis_create.hpp"
#include <cyberway/chaindb/common.hpp>
#include <cyberway/genesis/genesis_container.hpp>
#include <eosio/chain/controller.hpp>
#include <eosio/chain/authorization_manager.hpp>
#include <eosio/chain/resource_limits.hpp>
#include <eosio/chain/resource_limits_private.hpp>  // public interface is not enough
#include <eosio/chain/abi_serializer.hpp>


#define IGNORE_SYSTEM_ABI

namespace cyberway { namespace genesis {

using namespace chaindb;
const fc::microseconds abi_serializer_max_time = fc::seconds(10);


using resource_limits::resource_usage_object;
// There sould be proper way to get type name for abi, but it was faster to implement this
template<typename T> bool get_type_name_fail() { return false; }
template<typename T> type_name get_type_name() { static_assert(get_type_name_fail<T>(), "specialize type"); return ""; }
template<> type_name get_type_name<permission_object>()         { return "permission_object"; }
template<> type_name get_type_name<permission_usage_object>()   { return "permission_usage_object"; }
template<> type_name get_type_name<account_object>()            { return "account_object"; }
template<> type_name get_type_name<account_sequence_object>()   { return "account_sequence_object"; }
template<> type_name get_type_name<resource_usage_object>()     { return "resource_usage_object"; }
template<> type_name get_type_name<domain_object>()             { return "domain_object"; }
template<> type_name get_type_name<username_object>()           { return "username_object"; }
template<> type_name get_type_name<stake_agent_object>()        { return "stake_agent_object"; }
template<> type_name get_type_name<stake_grant_object>()        { return "stake_grant_object"; }
template<> type_name get_type_name<stake_param_object>()        { return "stake_param_object"; }
template<> type_name get_type_name<stake_stat_object>()         { return "stake_stat_object"; }


enum class stored_contract_tables: int {
    domains,        usernames,
    token_stats,    vesting_stats,
    token_balance,  vesting_balance,
    delegation,     rdelegation,
    witness_vote,   witness_info,
    // the following are system tables, but it's simpler to have them here
    stake_agents,   stake_grants,
    stake_stats,    stake_params,

    _max                // to simplify setting tables_count of genesis container
};


struct genesis_serializer {
    fc::flat_map<uint16_t,uint64_t> autoincrement;
    std::map<account_name, abi_serializer> abis;

private:
    ofstream_sha256 out;
    int _row_count = 0;
    int _section_count = 0;
    table_header _section;

public:
    void start(const bfs::path& out_file, int n_sections) {
        out.exceptions(std::ofstream::failbit | std::ofstream::badbit);
        out.open(out_file, std::ios_base::binary);
        genesis_header hdr;
        hdr.tables_count = n_sections;
        out.write((char*)(&hdr), sizeof(hdr));
        _section_count = n_sections;
    }

    void finalize() {
        finish_section();
        EOS_ASSERT(_section_count == 0, genesis_exception, "Genesis contains wrong number of sections",
            ("diff",_section_count));
        std::cout << "Genesis hash: " << std::endl << out.hash().str() << std::endl;
    }

    void start_section(account_name code, table_name name, std::string abi_type, uint32_t count) {
        wlog("Starting section: ${s}", ("s", table_header{code, name, abi_type, count}));
        finish_section();
        table_header h{code, name, abi_type, count};
        fc::raw::pack(out, h);
        _row_count = count;
        _section = h;
        _section_count--;
    }

    void finish_section() {
        EOS_ASSERT(_row_count == 0, genesis_exception, "Section contains wrong number of rows",
            ("section",_section)("diff",_row_count));
    }

    void prepare_serializers(const contracts_map& contracts) {
        abi_def abi;
        abis[name()] = abi_serializer(eosio_contract_abi(abi), abi_serializer_max_time);
        for (const auto& c: contracts) {
            auto abi_bytes = c.second.abi;
            if (abi_bytes.size() > 0) {
                auto acc = c.first;
                if (abi_serializer::to_abi(abi_bytes, abi)) {
                    abis[acc] = abi_serializer(abi, abi_serializer_max_time);
                } else {
                    elog("ABI for contract ${a) not found", ("a", acc.to_string()));
                }
            }
        }
    }

    template<typename T>
    void set_autoincrement(uint64_t val) {
        constexpr auto tid = T::type_id;
        autoincrement[tid] = val;
    }

    template<typename T>
    uint64_t get_autoincrement() {
        constexpr auto tid = T::type_id;
        return autoincrement[tid];
    }

    template<typename T, typename Lambda>
    const T emplace(Lambda&& constructor) {
        return emplace<T>({}, constructor);
    }

    template<typename T, typename Lambda>
    const T emplace(const ram_payer_info& ram, Lambda&& constructor) {
        T obj(constructor, 0);
        constexpr auto tid = T::type_id;
        auto& id = autoincrement[tid];
        obj.id = id++;
        if ((id & 0x7FFF) == 0) {
            ilog("AINC ${t} = ${n}", ("t",tid)("n",id));
        }
#ifndef IGNORE_SYSTEM_ABI
        static abi_serializer& ser = abis[name()];
        variant v;
        to_variant(obj, v);
        bytes data = ser.variant_to_binary(get_type_name<T>(), v, abi_serializer_max_time);
#else
        // bytes data(1024*1024);
        // datastream<char*> ds(bytes.data(), bytes.size());
        // fc::raw::pack(ds, obj);
        // data.resize(ds.tellp());
        bytes data = fc::raw::pack(obj);
#endif
        sys_table_row record{{}, data};
        fc::raw::pack(out, record);
        _row_count--;
        return obj;
    }

    // TODO: there should be way to get type from table name
    void insert(//const string& type,
        const table_request& req, primary_key_t pk, variant v, const ram_payer_info& ram
    ) {
        EOS_ASSERT(abis.count(req.code) > 0, genesis_exception, "ABI not found");
        auto& ser = abis[req.code];
        bytes data = ser.variant_to_binary(_section.abi_type, v, abi_serializer_max_time);
        table_row record{{ram.payer, data}, pk, req.scope};
        fc::raw::pack(out, record);
        _row_count--;
    }
};


}} // cyberway::genesis
