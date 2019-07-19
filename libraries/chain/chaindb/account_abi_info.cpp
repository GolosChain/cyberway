#include <cyberway/chaindb/account_abi_info.hpp>
#include <cyberway/chaindb/table_info.hpp>
#include <cyberway/chaindb/driver_interface.hpp>

namespace eosio { namespace chain {
    namespace chaindb = cyberway::chaindb;

    cyberway::chaindb::abi_info_ptr account_object::generate_abi_info() {
        if (!abi_info_ptr_) {
            abi_info_ptr_.reset(new cyberway::chaindb::abi_info(name, get_abi()));
        }
        return abi_info_ptr_;
    }

} } // namespace eosio::chain

namespace cyberway { namespace  chaindb {

    account_abi_info::account_abi_info(cache_object_ptr account_ptr) {
        init(std::move(account_ptr));
    }

    account_abi_info::account_abi_info(account_name_t code, abi_def def) {
        init(code, std::move(def));
    }

    account_abi_info::account_abi_info(account_name_t code, blob b) {
        init(code, std::move(b));
    }

    template<typename Def> void account_abi_info::init(account_name_t code, Def def) {
        abi_info_ptr_.reset(new cyberway::chaindb::abi_info(code, std::move(def)));
    }

    void account_abi_info::init(cache_object_ptr account_ptr) {
        if (!account_ptr) {
            return;
        }

        assert(account_ptr->service().table == chaindb::tag<account_object>::get_code());

        auto& a = multi_index_item_data<account_object>::get_T(account_ptr);
        if (a.abi.empty()) {
            return;
        }

        abi_info_ptr_ = const_cast<account_object&>(a).generate_abi_info();
    }

    struct system_abi_info_impl final {
        account_abi_info info;
        index_info       index;

        system_abi_info_impl(const driver_interface& driver)
        : driver_(driver) {
            init_info();
        }

        void init_info() {
            init_info(eosio::chain::eosio_contract_abi());

            try {
                auto obj = driver_.object_by_pk(index, config::system_account_name);
                if (obj.value.is_object()) {
                    auto& abi = obj.value["abi"];
                    if (abi.is_blob() && !abi.get_blob().data.empty()) {
                        init_info(abi.get_blob());
                    }
                }
            } catch (...) {
                // fail to read abi from db
            }
        }

        template <typename Def> void init_info(Def def) {
            info = account_abi_info(config::system_account_name, std::move(def));
            init_index();
        }

    private:
        const driver_interface& driver_;

        void init_index() {
            auto& abi   = info.abi();
            auto  table = abi.find_table(chaindb::tag<account_object>::get_code());
            index.table    = table;
            index.index    = abi.find_pk_index(*table);
            index.pk_order = abi.find_pk_order(*table);
        }
    }; // struct system_abi_info_impl

    system_abi_info::system_abi_info(const driver_interface& driver)
    : impl_(std::make_unique<system_abi_info_impl>(driver)),
      info_(impl_->info),
      index_(impl_->index) {
    }

    system_abi_info::~system_abi_info() = default;

    void system_abi_info::init_abi() const {
        impl_->init_info();
    }

    void system_abi_info::set_abi(abi_def def) const {
        impl_->init_info(std::move(def));
    }

} } // namespace cyberway::chaindb