#pragma once

#include <cstdint>

namespace cyberway { namespace genesis {

struct golos_dump_header {
    char magic[13];
    uint16_t version;
    uint32_t block_num;
    uint16_t op_in_block;

    static constexpr auto expected_magic = "Golos\adumpOP";
};

} } // cyberway::genesis
