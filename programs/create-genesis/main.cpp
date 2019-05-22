#include "golos_state/golos_state_reader.hpp" // Include 1st for custom_unpack
#include "common/genesis_info.hpp"
#include "genesis_create.hpp"
#include "ee_genesis/ee_genesis_builder.hpp"

#include <eosio/chain/abi_def.hpp>
#include <fc/io/raw.hpp>
#include <fc/io/json.hpp>
#include <fc/io/fstream.hpp>
#include <fc/variant.hpp>
#include <fc/filesystem.hpp>

// #include <boost/exception/diagnostic_information.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem/path.hpp>

namespace bfs = boost::filesystem;

namespace fc {

void to_variant(const bfs::path& path, fc::variant& v) {
    to_variant(path.generic_string(), v);
}
void from_variant(const fc::variant& v, bfs::path& path) {
    std::string t;
    from_variant(v, t);
    path = t;
}

} // fc


namespace cyberway { namespace genesis {

using namespace eosio::chain;
namespace bpo = boost::program_options;
using bpo::options_description;
using bpo::variables_map;
using std::string;


struct config_reader {
    void set_program_options(options_description& cli);
    void read_options(const variables_map& options);
    void read_config(const variables_map& options);
    void read_contracts();

    bfs::path info_file;
    bfs::path out_genesis_file;
    bfs::path out_ee_genesis_dir;
    bfs::path dump_dir;
    uint32_t last_block;

    bool create_genesis = false;
    bool create_ee_genesis = false;

    genesis_info info;
    genesis_state genesis;
    contracts_map contracts;
};


void config_reader::set_program_options(options_description& cli) {
    cli.add_options() (
        "genesis-info,g", bpo::value<bfs::path>(&info_file)->default_value("genesis-info.json"),
        "Genesis info file (absolute or relative path)."
    ) (
        "state", bpo::value<bfs::path>(&out_genesis_file)->implicit_value("cyberway-genesis.dat")->default_value(""),
        "If set, create state genesis. If path set, it is output state genesis file (absolute or relative path)."
    ) (
        "ee", bpo::value<bfs::path>(&out_ee_genesis_dir)->implicit_value("event-genesis")->default_value(""),
        "If set, create EE genesis. If path set, it is output EE genesis dir (absolute or relative path)."
    ) (
        "dump,d", bpo::value<bfs::path>(&dump_dir)->default_value("/var/lib/golosd/witness_node_data_dir/operation_dump"),
        "EE genesis: operation dump dir from Golos (absolute or relative path)."
    ) (
        "last-block,l", bpo::value<uint32_t>(&last_block)->default_value(UINT32_MAX),
        "EE genesis: last block num to read operations from dump."
    ) (
        "help,h",
        "Print this help message and exit."
    );
}

void make_absolute(bfs::path& path, const string& title, bool file_exists = true) {
    path = bfs::absolute(path);
    if (file_exists) {
        EOS_ASSERT(fc::is_regular_file(path), genesis_exception,
            "${t} file '${f}' does not exist.", ("t",title)("f", path.generic_string()));
    }
}

void make_dir_absolute(bfs::path& path, const string& title, bool dir_exists = true) {
    path = bfs::absolute(path);
    if (dir_exists) {
        EOS_ASSERT(fc::is_directory(path), genesis_exception,
            "${t} directory '${f}' does not exist.", ("t",title)("f", path.generic_string()));
    } else {
        if (fc::exists(path)) {
            EOS_ASSERT(fc::is_directory(path), genesis_exception,
                "${t} directory expected but '${f}' is a file.", ("t",title)("f", path.generic_string()));
        } else {
            fc::create_directories(path);
        }
    }
}

fc::sha256 check_hash(const genesis_info::file_hash& fh) {
    std::string str;
    auto path = bfs::absolute(fh.path);
    fc::read_file_contents(path, str);
    EOS_ASSERT(!str.empty(), genesis_exception, "file is empty: ${f}", ("f", path));
    bytes data(str.begin(), str.end());
    auto hash = fc::sha256::hash(&(data[0]), data.size());
    if (fh.hash != fc::sha256()) {
        EOS_ASSERT(hash == fh.hash, genesis_exception,
            "Hash mismatch: ${f}; ${provided} / ${calculated}", ("f", path)("provided",fh.hash)("calculated",hash));
    }
    return hash;
}

void read_contract(const genesis_info::account& acc, contract_abicode& abicode) {
    abicode.update = acc.update && *acc.update;
    if (acc.abi) {
        auto fh = *acc.abi;
        check_hash(fh);
        auto abi_path = bfs::absolute(fh.path);
        abicode.abi = fc::raw::pack(fc::json::from_file(abi_path).as<abi_def>());
    }
    if (acc.code) {
        auto fh = *acc.code;
        abicode.code_hash = check_hash(fh);
        auto wasm_path = bfs::absolute(fh.path);
        std::string wasm;
        fc::read_file_contents(wasm_path, wasm);
        const string binary_wasm_header("\x00\x61\x73\x6d\x01\x00\x00\x00", 8);
        if (wasm.compare(0, 8, binary_wasm_header))
            std::cerr << "WARNING: " << wasm_path
                << " doesn't look like a binary WASM file. Trying anyways..." << std::endl;
        abicode.code = bytes(wasm.begin(), wasm.end());
    };
}

void config_reader::read_options(const variables_map& options) {
    if (!options["state"].as<bfs::path>().empty()) {
        create_genesis = true;
        make_absolute(out_genesis_file, "Output genesis file", false);
    }

    if (!options["ee"].as<bfs::path>().empty()) {
        create_ee_genesis = true;
        make_dir_absolute(dump_dir, "Operation dump", true);
        make_dir_absolute(out_ee_genesis_dir, "Output EE genesis dir", false);
    }

    EOS_ASSERT(create_genesis || create_ee_genesis, genesis_exception, "At least one type of output genesis should be selected");
}

void config_reader::read_config(const variables_map& options) {
    ilog("Genesis: read config");

    make_absolute(info_file, "Genesis info");

    info = fc::json::from_file(info_file).as<genesis_info>();
    make_absolute(info.state_file, "Golos state");
    make_absolute(info.genesis_json, "Genesis json");
    genesis = fc::json::from_file(info.genesis_json).as<genesis_state>();

    // base validation and init
    for (auto& a: info.accounts) {
        for (auto& p: a.permissions) {
            EOS_ASSERT(p.key.length() == 0 || p.keys.size() == 0, genesis_exception,
                "Account ${a} permission can't contain both `key` and `keys` fields at the same time", ("a",a.name));
            p.init();
        }
    }
    EOS_ASSERT(info.golos.max_supply >= 0, genesis_exception, "max_supply can't be negative");
    EOS_ASSERT(info.golos.sys_max_supply >= 0, genesis_exception, "sys_max_supply can't be negative");
}

void config_reader::read_contracts() {
    ilog("Reading pre-configured accounts");
    for (const auto& acc: info.accounts) {
        std::cout << "  " << acc.name << " account..." << std::flush;
        auto& data = contracts[acc.name];
        read_contract(acc, data);
        std::cout << " done: abi size: " << data.abi.size() << ", code size: " << data.code.size() << "." << std::endl;
    }
}


int main(int argc, char** argv) {
    fc::exception::enable_detailed_strace();
    std::ios::sync_with_stdio(false); // for potential performance boost
    options_description cli("create-genesis command line options");
    try {
        config_reader cr;
        cr.set_program_options(cli);
        variables_map vmap;
        bpo::store(bpo::parse_command_line(argc, argv, cli), vmap);
        bpo::notify(vmap);
        if (vmap.count("help") > 0) {
            cli.print(std::cerr);
            return 0;
        }

        cr.read_options(vmap);
        cr.read_config(vmap);

        state_object_visitor visitor;
        state_reader reader{cr.info.state_file};
        reader.read_state(visitor);

        if (cr.create_genesis) {
            cr.read_contracts();

            genesis_create builder{visitor, cr.info, cr.genesis, cr.contracts};
            builder.write_genesis(cr.out_genesis_file);
        }

        if (cr.create_ee_genesis) {
            bfs::remove_all("shared_memory");

            ee_genesis_builder ee_builder("shared_memory", visitor, cr.info, cr.last_block);
            ee_builder.read_operation_dump(cr.dump_dir);
            ee_builder.build(cr.out_ee_genesis_dir);

            bfs::remove_all("shared_memory");
        }
    } catch (const fc::exception& e) {
        elog("${e}", ("e", e.to_detail_string()));
        return -1;
    } catch (const boost::exception& e) {
        elog("${e}", ("e", boost::diagnostic_information(e)));
        return -1;
    } catch (const std::exception& e) {
        elog("${e}", ("e", e.what()));
        return -1;
    } catch (...) {
        elog("unknown exception");
        return -1;
    }
    return 0;
}

}} // cyberway::genesis

int main(int argc, char** argv) {
    return cyberway::genesis::main(argc, argv);
}
