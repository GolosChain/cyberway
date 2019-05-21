#pragma once

#include "state_reader.hpp" // Include 1st for custom_unpack
#include "genesis_info.hpp"
#include <eosio/chain/genesis_state.hpp>
#include <fc/crypto/sha256.hpp>
#include <boost/filesystem.hpp>


namespace cyberway { namespace genesis {

FC_DECLARE_EXCEPTION(genesis_exception, 9000000, "genesis create exception");

using namespace eosio::chain;
namespace bfs = boost::filesystem;

struct contract_abicode {
    bool update;
    bytes abi;
    bytes code;
    fc::sha256 code_hash;
};
using contracts_map = std::map<name, contract_abicode>;


class genesis_create final {
public:
    genesis_create(const genesis_create&) = delete;
    genesis_create(state_object_visitor& visitor, const genesis_info& info, const genesis_state& conf, const contracts_map& contracts);
    ~genesis_create();

    void write_genesis(const bfs::path& out_file, const bfs::path& ee_directory);

private:
    struct genesis_create_impl;
    std::unique_ptr<genesis_create_impl> _impl;
};


}} // cyberway::genesis
