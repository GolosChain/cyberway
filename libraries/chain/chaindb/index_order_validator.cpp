#include <fc/variant.hpp>

#include <cyberway/chaindb/table_info.hpp>
#include <cyberway/chaindb/controller.hpp>
#include <cyberway/chaindb/index_order_validator.hpp>

#include <eosio/chain/exceptions.hpp>


namespace cyberway { namespace chaindb {
    index_order_validator::index_order_validator(const index_info& index) :
        index_(index) {
    }

    void index_order_validator::verify(const fc::variant& key) const {
        const static std::string error_message("The key object does not match to an abi description");
        try {
            index_.abi().to_bytes(index_, key);
        } catch (const eosio::chain::pack_exception&) {
            EOS_THROW(eosio::chain::abi_exception, error_message);
        } catch (const invalid_abi_store_type_exception&) {
            EOS_THROW(eosio::chain::abi_exception, error_message);
        }
    }

}} // namespace cyberway::chaindb
