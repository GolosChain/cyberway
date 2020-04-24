#pragma once

#include <memory>
#include <map>

#include <cyberway/chaindb/controller.hpp>

namespace cyberway { namespace chaindb {
    class chaindb_controller;
}}

namespace eosio { namespace chain {
    class snapshot_writer;
    class snapshot_reader;
    class fork_database;
    class controller;

    class snapshot_controller {
    public:
        snapshot_controller(cyberway::chaindb::chaindb_controller& chaindb_controller,
                            resource_limits_manager& resource_limits,
                            fork_database& fork_db,
                            block_state_ptr& head,
                            genesis_state& genesis);

        void write_snapshot(std::unique_ptr<snapshot_writer> writer);

        uint32_t read_snapshot(std::unique_ptr<snapshot_reader> reader);

    private:
        void dump_accounts();
        void dump_undo_state() const;
        void dump_contract_tables(const cyberway::chaindb::abi_info& abi) const;
        void dump_table(const cyberway::chaindb::table_def& table, const cyberway::chaindb::abi_info& abi) const;
        void restore_accounts();
        void restore_undo_state();
        void insert_undo(cyberway::chaindb::service_state service, fc::variant value);
        void restore_contract(const cyberway::chaindb::abi_info& abi);
        void restore_table(const cyberway::chaindb::table_def& table, const cyberway::chaindb::abi_info& abi);
        void restore_object(cyberway::chaindb::reflectable_service_state service, bytes bytes, const cyberway::chaindb::table_name& table, const cyberway::chaindb::abi_info& abi);
        void insert_object(cyberway::chaindb::service_state service, fc::variant value, cyberway::chaindb::table_name_t table, account_name code);

    private:
        cyberway::chaindb::chaindb_controller& chaindb_controller;
        resource_limits_manager& resource_limits;
        fork_database& fork_db;
        block_state_ptr& head;
        genesis_state& genesis;

        std::map<const cyberway::chaindb::account_name_t, const cyberway::chaindb::abi_info> abies;

        std::unique_ptr<snapshot_writer> writer;
        std::unique_ptr<snapshot_reader> reader;
    };

}} // namespace eosio::chain
