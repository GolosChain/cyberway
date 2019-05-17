#pragma once

#include <boost/filesystem.hpp>
#include <eosio/chain/exceptions.hpp>

namespace cyberway { namespace genesis {

namespace bfs = boost::filesystem;

// Types

struct golos_dump_header {
    char magic[13] = "";
    uint32_t version = 0;

    static constexpr auto expected_magic = "Golos\adumpOP";
    static constexpr uint32_t expected_version = 1;
};

using operation_number = std::pair<uint32_t, uint16_t>;

struct operation_header {
	operation_number num;
	uint64_t hash = 0;
};

// Exceptions

FC_DECLARE_EXCEPTION(operation_dump_exception, 9500000, "operation dump exception");

// Functions

golos_dump_header read_dump_header(bfs::ifstream& in);

bool read_op_header(bfs::ifstream& in, operation_header& op, uint32_t last_block);

} } // cyberway::genesis

FC_REFLECT(cyberway::genesis::operation_header, (num)(hash))
