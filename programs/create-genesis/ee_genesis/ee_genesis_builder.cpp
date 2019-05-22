#include "ee_genesis_builder.hpp"
#include "golos_operations.hpp"
#include "../common/config.hpp"
#include "../common/asset_converter.hpp"
#include "../common/generate_name.hpp"

#define MEGABYTE 1024*1024

// Comments:
// 8000000 * 192 = 1.6 GB
// +
// 8000000 * 144 * 5 (votes on comment) = 6.0 GB
// +
// 1000000 * 128 (reblogs on comment) = 0.2 GB
// Follows:
// 2300000 * 160 = 0.4 GB
#define MAP_FILE_SIZE uint64_t(22*1024)*MEGABYTE

namespace cyberway { namespace genesis {

using mvo = fc::mutable_variant_object;

ee_genesis_builder::ee_genesis_builder(const std::string& shared_file, state_object_visitor& state, const genesis_info& info, uint32_t last_block)
        : _state(state), _info(info), last_block_(last_block), maps_(shared_file, chainbase::database::read_write, MAP_FILE_SIZE) {
    maps_.add_index<comment_header_index>();
    maps_.add_index<vote_header_index>();
    maps_.add_index<reblog_header_index>();
    maps_.add_index<follow_header_index>();
}

ee_genesis_builder::~ee_genesis_builder() {
}

golos_dump_header ee_genesis_builder::read_header(bfs::ifstream& in) {
    golos_dump_header h;
    in.read((char*)&h, sizeof(h));
    if (in) {
        EOS_ASSERT(std::string(h.magic) == golos_dump_header::expected_magic, golos_dump_exception,
            "Unknown format of the operation dump file.");
        EOS_ASSERT(h.version == golos_dump_header::expected_version, golos_dump_exception,
            "Wrong version of the operation dump file.");
    }
    return h;
}

template<typename Operation>
bool ee_genesis_builder::read_operation(bfs::ifstream& in, Operation& op) {
    auto op_offset = in.tellg();
    fc::raw::unpack(in, op);
    op.offset = op_offset;

    if (!in) {
        return false;
    }

    if (op.num.first > last_block_) {
        return false;
    }
    return true;
}

void ee_genesis_builder::process_delete_comments() {
    std::cout << "-> Reading comment deletions..." << std::endl;

    const auto& comments = maps_.get_index<comment_header_index, by_hash>();

    bfs::ifstream in(in_dump_dir_ / "delete_comments");
    read_header(in);

    delete_comment_operation op;
    while (read_operation(in, op)) {
        auto comment = comments.find(op.hash);
        if (comment != comments.end()) {
            maps_.modify(*comment, [&](auto& c) {
                c.last_delete_op = op.num;
            });
            continue;
        }

        maps_.create<comment_header>([&](auto& c) {
            c.hash = op.hash;
            c.last_delete_op = op.num;
        });
    }
}

void ee_genesis_builder::process_comments() {
    std::cout << "-> Reading comments..." << std::endl;

    const auto& comments = maps_.get_index<comment_header_index, by_hash>();

    bfs::ifstream in(in_dump_dir_ / "comments");
    read_header(in);

    comment_operation op;
    while (read_operation(in, op)) {
        auto comment = comments.find(op.hash);
        if (comment != comments.end()) {
            if (comment->last_delete_op > op.num) {
                continue;
            }

            maps_.modify(*comment, [&](auto& c) {
                c.offset = op.offset;
                c.create_op = op.num;
                if (c.created == fc::time_point_sec::min()) {
                    c.created = op.timestamp;
                }
            });
            continue;
        }

        maps_.create<comment_header>([&](auto& c) {
            c.hash = op.hash;
            c.offset = op.offset;
            c.create_op = op.num;
            c.created = op.timestamp;
        });
    }
}

void ee_genesis_builder::process_rewards() {
    std::cout << "-> Reading rewards..." << std::endl;

    const auto& comments = maps_.get_index<comment_header_index, by_hash>();

    bfs::ifstream in(in_dump_dir_ / "total_comment_rewards");
    read_header(in);

    total_comment_reward_operation op;
    while (read_operation(in, op)) {
        auto comment = comments.find(op.hash);
        if (comment != comments.end() && op.num > comment->last_delete_op) {
            maps_.modify(*comment, [&](auto& c) {
                c.author_reward += op.author_reward.get_amount();
                c.benefactor_reward += op.benefactor_reward.get_amount();
                c.curator_reward += op.curator_reward.get_amount();
                c.net_rshares = op.net_rshares;
            });
        }
    }
}

void ee_genesis_builder::process_votes() {
    std::cout << "-> Reading votes..." << std::endl;

    const auto& votes = maps_.get_index<vote_header_index, by_hash_voter>();

    bfs::ifstream in(in_dump_dir_ / "votes");
    read_header(in);

    vote_operation op;
    while (read_operation(in, op)) {
        auto vote = votes.find(std::make_tuple(op.hash, op.voter));
        if (vote != votes.end()) {
            maps_.modify(*vote, [&](auto& v) {
                v.op_num = op.num;
                v.weight = op.weight;
                v.rshares = op.rshares;
                v.timestamp = op.timestamp;
            });
            continue;
        }

        maps_.create<vote_header>([&](auto& v) {
            v.hash = op.hash;
            v.voter = op.voter;
            v.op_num = op.num;
            v.weight = op.weight;
            v.rshares = op.rshares;
            v.timestamp = op.timestamp;
        });
    }
}

void ee_genesis_builder::process_reblogs() {
    std::cout << "-> Reading reblogs..." << std::endl;

    const auto& reblogs = maps_.get_index<reblog_header_index, by_hash_account>();

    bfs::ifstream in(in_dump_dir_ / "reblogs");
    read_header(in);

    reblog_operation op;
    while (read_operation(in, op)) {
        auto reblog = reblogs.find(std::make_tuple(op.hash, op.account));
        if (reblog != reblogs.end()) {
            maps_.modify(*reblog, [&](auto& r) {
                r.op_num = op.num;
                r.offset = op.offset;
            });
            continue;
        }

        maps_.create<reblog_header>([&](auto& r) {
            r.hash = op.hash;
            r.account = op.account;
            r.op_num = op.num;
            r.offset = op.offset;
        });
    }
}

void ee_genesis_builder::process_delete_reblogs() {
    std::cout << "-> Reading delete reblogs..." << std::endl;

    const auto& reblogs = maps_.get_index<reblog_header_index, by_hash_account>();

    bfs::ifstream in(in_dump_dir_ / "delete_reblogs");
    read_header(in);

    delete_reblog_operation op;
    while (read_operation(in, op)) {
        auto reblog = reblogs.find(std::make_tuple(op.hash, op.account));
        if (op.num > reblog->op_num) {
            maps_.remove(*reblog);
        }
    }
}

void ee_genesis_builder::process_follows() {
    std::cout << "-> Reading follows..." << std::endl;

    const auto& follows = maps_.get_index<follow_header_index, by_pair>();

    bfs::ifstream in(in_dump_dir_ / "follows");
    read_header(in);

    follow_operation op;
    while (read_operation(in, op)) {
        bool ignores = false;
        if (op.what & (1 << ignore)) {
            ignores = true;
        }

        auto follow = follows.find(std::make_tuple(op.follower, op.following));
        if (follow != follows.end()) {
            if (op.what == 0) {
                maps_.remove(*follow);
                continue;
            }

            maps_.modify(*follow, [&](auto& f) {
                f.ignores = ignores;
            });
            continue;
        }

        if (op.what == 0) {
            continue;
        }

        maps_.create<follow_header>([&](auto& f) {
            f.follower = op.follower;
            f.following = op.following;
            f.ignores = ignores;
        });
    }
}

void ee_genesis_builder::read_operation_dump(const bfs::path& in_dump_dir) {
    in_dump_dir_ = in_dump_dir;

    std::cout << "Reading operation dump from " << in_dump_dir_ << "..." << std::endl;

    process_delete_comments();
    process_comments();
    process_rewards();
    process_votes();
    process_reblogs();
    process_delete_reblogs();
    process_follows();
}

void ee_genesis_builder::build_votes(std::vector<vote_info>& votes, uint64_t msg_hash, operation_number msg_created) {
    const auto& vote_idx = maps_.get_index<vote_header_index, by_hash_voter>();

    auto vote_itr = vote_idx.lower_bound(msg_hash);
    for (; vote_itr != vote_idx.end() && vote_itr->hash == msg_hash; ++vote_itr) {
        auto& vote = *vote_itr;

        if (vote.op_num < msg_created) {
            continue;
        }

        votes.emplace_back([&](auto& v) {
            v.voter = generate_name(vote.voter);
            v.weight = vote.weight;
            v.time = vote.timestamp;
            v.rshares = vote.rshares;
        }, 0);

        std::sort(votes.begin(), votes.end(), [&](const auto& a, const auto& b) {
            return a.rshares > b.rshares;
        });
    }
}

void ee_genesis_builder::build_reblogs(std::vector<reblog_info>& reblogs, uint64_t msg_hash, operation_number msg_created, bfs::ifstream& dump_reblogs) {
    const auto& reblog_idx = maps_.get_index<reblog_header_index, by_hash_account>();

    auto reblog_itr = reblog_idx.lower_bound(msg_hash);
    for (; reblog_itr != reblog_idx.end() && reblog_itr->hash == msg_hash; ++reblog_itr) {
        auto& reblog = *reblog_itr;

        if (reblog.op_num < msg_created) {
            continue;
        }

        dump_reblogs.seekg(reblog.offset);
        reblog_operation op;
        read_operation(dump_reblogs, op);

        reblogs.emplace_back([&](auto& r) {
            r.account = generate_name(reblog.account);
            r.title = op.title;
            r.body = op.body;
            r.time = op.timestamp;
        }, 0);
    }
}

void ee_genesis_builder::build_messages() {
    std::cout << "-> Writing messages..." << std::endl;

    out_.messages.start_section(_info.golos.names.posting, N(message), "message_info");

    bfs::ifstream dump_comments(in_dump_dir_ / "comments");
    bfs::ifstream dump_reblogs(in_dump_dir_ / "reblogs");

    const auto& comment_idx = maps_.get_index<comment_header_index, by_created>();

    auto comment_itr = comment_idx.upper_bound(fc::time_point_sec::min());
    for (; comment_itr != comment_idx.end(); ++comment_itr) {
        auto& comment = *comment_itr;

        dump_comments.seekg(comment.offset);
        comment_operation op;
        read_operation(dump_comments, op);

        out_.messages.emplace<comment_info>([&](auto& c) {
            c.parent_author = generate_name(op.parent_author);
            c.parent_permlink = op.parent_permlink;
            c.author = generate_name(op.author);
            c.permlink = op.permlink;
            c.title = op.title;
            c.body = op.body;
            c.tags = op.tags;
            c.language = op.language;
            c.created = comment.created;
            c.net_rshares = comment.net_rshares;
            c.author_reward = asset(comment.author_reward, symbol(GLS));
            c.benefactor_reward = asset(comment.benefactor_reward, symbol(GLS));
            c.curator_reward = asset(comment.curator_reward, symbol(GLS));
            build_votes(c.votes, comment.hash, comment.last_delete_op);
            build_reblogs(c.reblogs, comment.hash, comment.last_delete_op, dump_reblogs);
        });
    }
}

void ee_genesis_builder::build_transfers() {
    std::cout << "-> Writing transfers..." << std::endl;

    out_.transfers.start_section(config::token_account_name, N(transfer), "transfer");

    bfs::ifstream in(in_dump_dir_ / "transfers");
    read_header(in);

    transfer_operation op;
    while (read_operation(in, op)) {
        out_.transfers.emplace<transfer_info>([&](auto& t) {
            t.from = generate_name(op.from);
            t.to = generate_name(op.to);
            t.quantity = op.amount;
            t.memo = op.memo;
            t.time = op.timestamp;
        });
    }
}

void ee_genesis_builder::build_pinblocks() {
    std::cout << "-> Writing pinblocks..." << std::endl;

    const auto& follow_index = maps_.get_index<follow_header_index, by_id>();

    out_.pinblocks.start_section(_info.golos.names.social, N(pin), "pin");

    for (const auto& follow : follow_index) {
        if (follow.ignores) {
            continue;
        }
        out_.pinblocks.emplace<pin_info>([&](auto& p) {
            p.pinner = generate_name(follow.follower);
            p.pinning = generate_name(follow.following);
        });
    }

    out_.pinblocks.start_section(_info.golos.names.social, N(block), "block");

    for (const auto& follow : follow_index) {
        if (!follow.ignores) {
            continue;
        }
        out_.pinblocks.emplace<block_info>([&](auto& b) {
            b.blocker = generate_name(follow.follower);
            b.blocking = generate_name(follow.following);
        });
    }
}

void ee_genesis_builder::build_usernames() {
    const auto& app = _info.golos.names.issuer;

    out_.usernames.start_section(config::system_account_name, N(domain), "domain_info");

    out_.usernames.insert(mvo
        ("owner", app)
        ("linked_to", app)
        ("name", _info.golos.domain)
    );

    out_.usernames.start_section(config::system_account_name, N(username), "username_info");

    for (const auto& auth : _state.auths) {
        const auto& name = auth.account.str(_state.accs_map);
        const auto& hash = generate_name(name);
        out_.usernames.insert(mvo
            ("creator", app)
            ("owner", hash)
            ("name", name)
        );
    }
}

void ee_genesis_builder::build_balances() {
    out_.balances.start_section(config::token_account_name, N(currency), "currency_stats");

    EOS_ASSERT(_state.gpo.is_forced_min_price, ee_genesis_exception, "Not implemented");

    asset_converter to_gls(_state.gpo.current_supply, asset(9 * _state.gpo.current_sbd_supply.get_amount(), symbol(GBG)));

    auto supply = _state.gpo.current_supply + to_gls.convert(_state.gpo.current_sbd_supply);
    auto insert_stat = [&](const asset& supply, int64_t max_supply, name issuer) {
        const symbol& sym = supply.get_symbol();
        out_.balances.insert(mvo
            ("supply", supply)
            ("max_supply", asset(max_supply * sym.precision(), sym))
            ("issuer", issuer)
        );
    };

    auto sys_supply = golos2sys(supply -_state.gpo.total_reward_fund_steem);
    insert_stat(sys_supply, _info.golos.sys_max_supply, config::system_account_name);
    insert_stat(supply, _info.golos.max_supply, _info.golos.names.issuer);

    out_.balances.start_section(config::token_account_name, N(balance), "balance_event");

    auto insert_balance = [&](name account, const asset& balance) {
        out_.balances.insert(mvo
            ("balance", balance)
            ("payments", asset(0, balance.get_symbol()))
            ("account", account)
        );
    };

    // funds
    insert_balance(config::stake_account_name, golos2sys(_state.gpo.total_vesting_fund_steem));
    insert_balance(_info.golos.names.vesting, _state.gpo.total_vesting_fund_steem);
    insert_balance(_info.golos.names.posting, _state.gpo.total_reward_fund_steem);

    // accounts
    for (const auto& balance : _state.gbg) {
        auto acc = balance.first;
        auto gbg = balance.second;
        auto gls = _state.gls[acc] + to_gls.convert(gbg);
        auto n = generate_name(_state.accs_map[acc]);
        insert_balance(n, gls);
        insert_balance(n, golos2sys(gls));
    }
}

void ee_genesis_builder::build(const bfs::path& out_dir) {
    std::cout << "Writing genesis to " << out_dir << "..." << std::endl;

    out_.start(out_dir, fc::sha256());

    build_messages();
    build_transfers();
    build_pinblocks();

    out_.finalize();
}

} } // cyberway::genesis
