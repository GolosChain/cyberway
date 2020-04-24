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
        const cyberway::chaindb::table_name_t UNDO_TABLE = table_name("undo").value;
        const cyberway::chaindb::table_name_t UNDO_PRIMARY_INDEX = index_name("primary").value;

        const std::string ACCOUNTS_TABLE_SECTION = "account_table";
        const std::string UNDO_TABLE_SECTION = "undo_table";


        bool skip_processing_table(cyberway::chaindb::table_name_t table) {
            return table == UNDO_TABLE || table == account_table::table_name();
        }

        bool skip_processing_table(account_name code, cyberway::chaindb::table_name_t table) {
            return cyberway::chaindb::is_system_code(code) && (skip_processing_table(table));
        }
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

        dump_undo_state();

        for (const auto& abi : abies) {
           dump_contract_tables(abi.second);
        }
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

    void snapshot_controller::dump_undo_state() const {
        const cyberway::chaindb::index_request request = {config::system_account_name, config::ignore_scope_account, UNDO_TABLE, UNDO_PRIMARY_INDEX};

        writer->write_section(UNDO_TABLE_SECTION, [&] (auto& section) {
            auto begin = chaindb_controller.begin(request);
            const auto end = chaindb_controller.end(request);
            for (cyberway::chaindb::primary_key_t key = begin.pk; key != end.pk; key = chaindb_controller.next({abies.at(config::system_account_name).code(), begin.cursor})) {
                const auto object = chaindb_controller.object_at_cursor({abies.at(config::system_account_name).code(), begin.cursor});

                section.add_row(cyberway::chaindb::reflectable_service_state(object.service));

                if (object.service.table == UNDO_TABLE && object.value.get_object().contains("npk")) {
                    section.add_row(static_cast<int>(undo_data_type::undo_npk));
                } else if (object.value.get_object().size() == 0) {
                    section.add_row(static_cast<int>(undo_data_type::empty_object));
                } else {
                    section.add_row(static_cast<int>(undo_data_type::normal_object));

                    const auto code = object.service.code == 0 ? config::system_account_name : object.service.code;

                    const auto serialized = chaindb_controller.serialize(abies.at(code), object);
                    section.add_row(serialized);

                    continue;
                }
                section.add_row(object.value);

            }
        });
    }

    void snapshot_controller::dump_contract_tables(const cyberway::chaindb::abi_info& abi) const {
       for (const auto& table : abi.tables()) {
           if (skip_processing_table(abi.code(), table.first)) {
               continue;
           }
           dump_table(table.second, abi);
       }
    }

    void snapshot_controller::dump_table(const cyberway::chaindb::table_def& table, const cyberway::chaindb::abi_info& abi) const {
        const cyberway::chaindb::index_request request = {abi.code(), config::ignore_scope_account, table.name, abi.find_pk_index(table)->name};

        writer->write_section(abi.code().to_string() + "_" + table.name.to_string(), [&, this](auto& section){
            auto begin = chaindb_controller.begin(request);
            const auto end = chaindb_controller.end(request);
            for (cyberway::chaindb::primary_key_t key = begin.pk; key != end.pk; key = chaindb_controller.next({abi.code(), begin.cursor})) {
                auto object = chaindb_controller.object_at_cursor({abi.code(), begin.cursor});
                const auto serialized = chaindb_controller.serialize(abi, object);

                section.add_row(cyberway::chaindb::reflectable_service_state(object.service));
                section.add_row(serialized);
            }
        });
    }

    uint32_t snapshot_controller::read_snapshot(std::unique_ptr<snapshot_reader> reader) {
        this->reader = std::move(reader);
        this->reader->validate();

        restore_accounts();

        restore_undo_state();

        for (const auto& abi : abies) {
           restore_contract(abi.second);
        }

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

    void snapshot_controller::restore_undo_state() {
        reader->read_section(UNDO_TABLE_SECTION, [&] (auto& section) {
            if (section.empty()) {
                return;
            }

            bool has_more = false;
            do {
               cyberway::chaindb::reflectable_service_state service;
               section.read_row(service);

               int restored_type;
               section.read_row(restored_type);
               const auto type = static_cast<undo_data_type>(restored_type);

               if (type == undo_data_type::undo_npk || type == undo_data_type::empty_object) {
                    fc::variant value;
                    has_more = section.read_row(value);

                    insert_undo(service, std::move(value));
                }  else {
                    bytes bytes;
                    has_more = section.read_row(bytes);

                    const auto code = service.code == 0 ? config::system_account_name : service.code;

                    auto value = chaindb_controller.deserialize({service.code, service.scope, service.table}, abies.at(code), bytes);

                    insert_undo(service, std::move(value));
                }

            } while(has_more);

        });

        chaindb_controller.apply_all_changes();
    }

    void snapshot_controller::insert_undo(cyberway::chaindb::service_state service, fc::variant value) {
        const auto code = abies.at(config::system_account_name).code();

        insert_object(std::move(service), std::move(value), UNDO_TABLE, code);
    }

    void snapshot_controller::restore_contract(const cyberway::chaindb::abi_info& abi) {
        for (const auto& table : abi.tables()) {
            if (skip_processing_table(abi.code(), table.first)) {
                continue;
            }
            restore_table(table.second, abi);
        }
    }

    void snapshot_controller::restore_table(const cyberway::chaindb::table_def& table, const cyberway::chaindb::abi_info& abi) {
        reader->read_section(abi.code().to_string() + "_" + table.name.to_string(), [&, this] (auto& section) {
            if (section.empty()) {
                return;
            }

            bool has_more = false;

            do {
               cyberway::chaindb::reflectable_service_state service;
               section.read_row(service);

               bytes bytes;
               has_more = section.read_row(bytes);

               restore_object(std::move(service), std::move(bytes), table.name, abi);
            } while(has_more);
        });
        chaindb_controller.apply_all_changes();
    }

    void snapshot_controller::restore_object(cyberway::chaindb::reflectable_service_state service,
                                             bytes bytes,
                                             const cyberway::chaindb::table_name& table,
                                             const cyberway::chaindb::abi_info& abi) {
        auto value = chaindb_controller.deserialize({service.code, service.scope, service.table}, abi, bytes);

        insert_object(service, value, table, abi.code());
    }

    void snapshot_controller::insert_object(cyberway::chaindb::service_state service,
                                            fc::variant value,
                                            cyberway::chaindb::table_name_t table,
                                            account_name code) {

        cyberway::chaindb::storage_payer_info payer(resource_limits, service.payer, service.payer, 0);
        chaindb_controller.insert(table, code, {std::move(service), std::move(value)}, std::move(payer));
    }

}} // namespace eosio::chain
