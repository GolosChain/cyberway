#pragma once

#include "state_reader.hpp" // Include 1st for custom_unpack
#include "genesis_info.hpp"
#include "golos_dump_container.hpp"
#include "event_engine_genesis.hpp"
#include "map_objects.hpp"

#include <boost/filesystem.hpp>
#include <eosio/chain/genesis_state.hpp>
#include <fc/exception/exception.hpp>
#include <chainbase/chainbase.hpp>
#include <cyberway/genesis/ee_genesis_container.hpp>

namespace cyberway { namespace genesis {

using namespace cyberway::golos;
namespace bfs = boost::filesystem;

FC_DECLARE_EXCEPTION(golos_dump_exception, 12000000, "Golos operation dump exception");

class genesis_ee_builder final {
public:
    genesis_ee_builder(const genesis_ee_builder&) = delete;
    genesis_ee_builder(const std::string& shared_file, state_object_visitor& state, const genesis_info& info, uint32_t last_block);
    ~genesis_ee_builder();

    void read_operation_dump(const bfs::path& in_dump_dir);
    void build(const bfs::path& out_dir);
private:
    golos_dump_header read_header(bfs::ifstream& in);
    template<typename Operation>
    bool read_operation(bfs::ifstream& in, Operation& op);

    void process_delete_comments();
    void process_comments();
    void process_rewards();
    void process_votes();
    void process_reblogs();
    void process_delete_reblogs();
    void process_follows();

    void build_usernames();
    void build_balances();
    void build_votes(std::vector<vote_info>& votes, uint64_t msg_hash, operation_number msg_created);
    void build_reblogs(std::vector<reblog_info>& reblogs, uint64_t msg_hash, operation_number msg_created, bfs::ifstream& dump_reblogs);
    void build_messages();
    void build_transfers();
    void build_pinblocks();

    bfs::path in_dump_dir_;
    state_object_visitor _state;
    genesis_info _info;
    uint32_t last_block_;
    chainbase::database maps_;
    event_engine_genesis out_;
};

} } // cyberway::genesis
