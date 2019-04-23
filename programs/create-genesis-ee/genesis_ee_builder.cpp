#include "genesis_ee_builder.hpp"
#include "golos_operations.hpp"
#include <cyberway/genesis/genesis_ee_container.hpp>

#define MEGABYTE 1024*1024

#define MAP_FILE_SIZE uint64_t(1.5*1024)*MEGABYTE

namespace cyberway { namespace genesis {

genesis_ee_builder::genesis_ee_builder(const std::string& shared_file)
        : maps_(shared_file, chainbase::database::read_write, MAP_FILE_SIZE) {
    maps_.add_index<comment_header_index>();
}

genesis_ee_builder::~genesis_ee_builder() {
}

golos_dump_header genesis_ee_builder::read_header(bfs::fstream& in, uint32_t expected_version) {
    golos_dump_header h{"", 0, 0};
    in.read((char*)&h, sizeof(h));
    if (in) {
        EOS_ASSERT(std::string(h.magic) == golos_dump_header::expected_magic && h.version == expected_version, genesis_exception,
            "Unknown format of the operation dump file.");
    }
    return h;
}

bool genesis_ee_builder::is_newer(const golos_dump_header& h, const comment_header& comment) {
    return h.block_num > comment.block_num || (h.block_num == comment.block_num && h.op_in_block > comment.op_in_block);
}

void genesis_ee_builder::process_comments() {
    std::cout << "-> Reading comments..." << std::endl;

    bfs::fstream in(in_dump_dir_ / "comments");
    while (in) {
        auto h = read_header(in, 1);

        if (!in) {
            break;
        }

        uint64_t hash = 0;
        fc::raw::unpack(in, hash);

        auto comment_offset = uint64_t(in.tellg());

        cyberway::golos::comment_operation cop;
        fc::raw::unpack(in, cop);

        const auto& comment_index = maps_.get_index<comment_header_index, by_hash>();
        auto comment_itr = comment_index.find(hash);
        if (comment_itr != comment_index.end()) {
            maps_.remove(*comment_itr);
        }

        maps_.create<comment_header>([&](auto& comment) {
            comment.hash = hash;
            comment.offset = comment_offset;
            comment.block_num = h.block_num;
            comment.op_in_block = h.op_in_block;
        });
    }
}

void genesis_ee_builder::process_delete_comments() {
    std::cout << "-> Reading comment deletions..." << std::endl;

    bfs::fstream in(in_dump_dir_ / "delete_comments");
    while (in) {
        auto h = read_header(in, 1);

        if (!in) {
            break;
        }

        uint64_t hash = 0;
        fc::raw::unpack(in, hash);

        const auto& comment_index = maps_.get_index<comment_header_index, by_hash>();
        auto comment_itr = comment_index.find(hash);
        if (is_newer(h, *comment_itr)) {
            maps_.remove(*comment_itr);
        }
    }
}

void genesis_ee_builder::process_rewards() {
    std::cout << "-> Reading rewards..." << std::endl;

    bfs::fstream in(in_dump_dir_ / "total_comment_rewards");
    while (in) {
        auto h = read_header(in, 1);

        if (!in) {
            break;
        }

        uint64_t hash = 0;
        fc::raw::unpack(in, hash);

        cyberway::golos::total_comment_reward_operation tcrop;
        fc::raw::unpack(in, tcrop);

        int64_t net_rshares = 0;
        fc::raw::unpack(in, net_rshares);

        const auto& comment_index = maps_.get_index<comment_header_index, by_hash>();
        auto comment_itr = comment_index.find(hash);
        if (is_newer(h, *comment_itr)) {
            maps_.modify(*comment_itr, [&](auto& comment) {
                comment.author_reward = tcrop.author_reward.get_amount();
                comment.benefactor_reward = tcrop.benefactor_reward.get_amount();
                comment.curator_reward = tcrop.curator_reward.get_amount();
                comment.net_rshares = net_rshares;
            });
        }
    }
}

// TODO: Move to common library

account_name generate_name(string n) {
    // TODO: replace with better function
    // TODO: remove dots from result (+append trailing to length of 12 characters)
    uint64_t h = std::hash<std::string>()(n);
    return account_name(h & 0xFFFFFFFFFFFFFFF0);
}

asset golos2sys(const asset& golos) {
    return asset(golos.get_amount() * (10000/1000));
}

void genesis_ee_builder::build_messages() {
    std::cout << "-> Writing messages..." << std::endl;

    fc::raw::pack(out_, table_ee_header(table_ee_type::messages));

    bfs::fstream dump_comments(in_dump_dir_ / "comments");

    const auto& comment_index = maps_.get_index<comment_header_index, by_id>();
    for (auto comment_itr = comment_index.begin(); comment_itr != comment_index.end(); ++comment_itr) {
        dump_comments.seekg(comment_itr->offset);
        cyberway::golos::comment_operation cop;
        fc::raw::unpack(dump_comments, cop);

        message_ee_object msg;
        msg.author = generate_name(std::string(cop.author));
        msg.permlink = cop.permlink;
        msg.parent_author = generate_name(std::string(cop.parent_author));
        msg.parent_permlink = cop.parent_permlink;
        msg.title = cop.title;
        msg.body = cop.body;
        msg.json_metadata = cop.json_metadata;
        msg.net_rshares = comment_itr->net_rshares;
        msg.author_reward = golos2sys(asset(comment_itr->author_reward));
        msg.benefactor_reward = golos2sys(asset(comment_itr->benefactor_reward));
        msg.curator_reward = golos2sys(asset(comment_itr->curator_reward));
        fc::raw::pack(out_, msg);
    }
}

void genesis_ee_builder::build(const bfs::path& in_dump_dir, const bfs::path& out_file) {
    in_dump_dir_ = in_dump_dir;

    std::cout << "Reading operation dump from " << in_dump_dir_ << "..." << std::endl;

    process_comments();
    process_delete_comments();
    process_rewards();

    std::cout << "Writing genesis to " << out_file << "..." << std::endl;

    out_.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    out_.open(out_file, std::ios_base::binary);

    genesis_ee_header hdr;
    out_.write((const char*)&hdr, sizeof(genesis_ee_header));

    build_messages();
}

} } // cyberway::genesis
