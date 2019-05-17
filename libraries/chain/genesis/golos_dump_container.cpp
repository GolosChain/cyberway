#include <cyberway/genesis/golos_dump_container.hpp>
#include <fc/io/raw.hpp>

namespace cyberway { namespace genesis {

golos_dump_header read_dump_header(bfs::ifstream& in) {
    golos_dump_header h;
    in.read((char*)&h, sizeof(h));
    if (in) {
        EOS_ASSERT(std::string(h.magic) == golos_dump_header::expected_magic, operation_dump_exception,
            "Unknown format of the operation dump file.");
        EOS_ASSERT(h.version == golos_dump_header::expected_version, operation_dump_exception,
            "Wrong version of the operation dump file.");
    }
    return h;
}

bool read_op_header(bfs::ifstream& in, operation_header& op, uint32_t last_block) {
    fc::raw::unpack(in, op);

    if (!in) {
        return false;
    }

    if (op.num.first > last_block) {
        return false;
    }
    return true;
}

} } // cyberway::genesis
