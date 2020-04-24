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
