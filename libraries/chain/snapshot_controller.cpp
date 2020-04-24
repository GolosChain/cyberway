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

    namespace {
        const std::string ACCOUNTS_TABLE_SECTION = "account_table";
    }

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

        dump_accounts();
        this->writer->finalize();
    }

    void snapshot_controller::dump_accounts() {
        writer->write_section("account_table", [&] (auto& section) {
            table_utils<account_table>::walk(chaindb_controller, [&](const auto& account) {
                if (!account.abi.empty()) {
                    abies.emplace(std::make_pair<const cyberway::chaindb::account_name_t, const cyberway::chaindb::abi_info>(std::move(account.name.value), {account.name.value, account.get_abi()}));
                }
                section.add_row(account);
            });
        });
    }

    uint32_t snapshot_controller::read_snapshot(std::unique_ptr<snapshot_reader> reader) {
        this->reader = std::move(reader);
        this->reader->validate();

        restore_accounts();

        return 0;
    }

    void snapshot_controller::restore_accounts() {
         account_table accounts(chaindb_controller);
         reader->read_section(ACCOUNTS_TABLE_SECTION, [&](auto& section){
             account_object object(account_name(), [](auto){});
             if (section.empty()) {
                 return;
             }

             bool hasMore = true;
             do {
                 hasMore = section.read_row(object);
                 accounts.emplace(object.name, cyberway::chaindb::storage_payer_info(), [&](auto& value) {
                     value = object;
                     if (!value.abi.empty()) {
                         abies.emplace(std::make_pair<const cyberway::chaindb::account_name_t, const cyberway::chaindb::abi_info>(value.name, {value.name, value.get_abi()}));
                     }
                 });
             } while(hasMore);
         });
    }

}} // namespace eosio::chain
