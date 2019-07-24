#include <eosiolib/eosio.hpp>
using namespace eosio;

struct integration_test : public eosio::contract {
      using contract::contract;
      constexpr static int base_size = 4;
      constexpr static int obj_num = 10;

      struct payload {
         uint64_t            key;
         vector<uint64_t>    data;

         uint64_t primary_key()const { return key; }
      };
      typedef eosio::multi_index<N(payloads), payload> payloads;

      /// @abi action 
      void store( account_name from,
                  account_name to,
                  uint64_t     num ) {
         require_auth( from );
         eosio_assert( is_account( to ), "to account does not exist");
         eosio_assert( num < std::numeric_limits<size_t>::max(), "num to large");
         payloads data ( _self, from );
         for (size_t i = 0; i < obj_num; i++) {
             data.emplace(from, [&]( auto& g ) {
                g.key = data.available_primary_key();
                g.data = vector<uint64_t>( static_cast<size_t>((num / obj_num) / (8 + base_size)), 42);
             });
         }
      }
      
      /// @abi action 
      void modify(account_name from, uint64_t val) {
         payloads data ( _self, from );
         for (auto i = data.begin(); i != data.end(); i++) {
            data.modify(i, name(), [&]( auto &g ) {
            for (auto& d : g.data) {
                d = val; 
            }});
         }
      }
};

EOSIO_ABI( integration_test, (store)(modify) )
