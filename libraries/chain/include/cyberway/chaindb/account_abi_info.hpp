#pragma once

#include <cyberway/chaindb/cache_item.hpp>
#include <cyberway/chaindb/abi_info.hpp>

#include <eosio/chain/account_object.hpp>

namespace cyberway { namespace chaindb {

    using account_object = eosio::chain::account_object;

    struct account_abi_info final {
        account_abi_info() = default;
        account_abi_info(cache_object_ptr);
        account_abi_info(account_name_t, abi_def);
        account_abi_info(account_name_t, blob);

        account_name code() const {
            if (has_abi_info()) {
                return abi_info_ptr_->code();
            }
            return account_name();
        }

        bool has_abi_info() const {
            return !!abi_info_ptr_;
        }

        const abi_info& abi() const {
            return *abi_info_ptr_;
        }

    private:
        abi_info_ptr abi_info_ptr_;

        template<typename Abi> void init(account_name_t, Abi);

        void init(cache_object_ptr);
    }; // struct account_abi_info

    struct system_abi_info final {
        system_abi_info() = delete;
        system_abi_info(const driver_interface&);
        ~system_abi_info();

        void init_abi() const;
        void set_abi(abi_def) const;

        const abi_info& abi() const {
            return info_.abi();
        }

        const index_info& account_index() const {
            return index_;
        }

        account_abi_info info() const {
            return info_;
        }

        service_state to_service(const primary_key_t code) const {
            service_state service;

            service.table = tag<account_object>::get_code();
            service.pk = code;

            return service;
        }

    private:
        std::unique_ptr<system_abi_info_impl> impl_;
        const account_abi_info& info_;
        const index_info& index_;
    }; // struct system_abi_info

} } // namespace cyberway::chaindb
