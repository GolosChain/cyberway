#include <boost/filesystem.hpp>

#include <eosio/chain/controller.hpp>
#include <eosio/chain/snapshot.hpp>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/config.hpp>
#include <eosio/chain/fork_database.hpp>
#include <eosio/chain/snapshot_controller.hpp>

enum class undo_data_type {
    normal_object,
    undo_npk,
    empty_object
};

namespace eosio { namespace chain {


    snapshot_controller::snapshot_controller(cyberway::chaindb::chaindb_controller& chaindb_controller,
                                             resource_limits_manager& resource_limits,
                                             fork_database& fork_db,
                                             block_state_ptr& head,
                                             genesis_state& genesis) :
        chaindb_controller(chaindb_controller),
        resource_limits(resource_limits),
        fork_db(fork_db),
        head(head),
        genesis(genesis) {}

    void snapshot_controller::write_snapshot(std::unique_ptr<snapshot_writer> writer) {
        this->writer = std::move(writer);

        this->writer->finalize();
    }


    uint32_t snapshot_controller::read_snapshot(std::unique_ptr<snapshot_reader> reader) {
        this->reader = std::move(reader);
        this->reader->validate();

    }

}} // namespace eosio::chain
