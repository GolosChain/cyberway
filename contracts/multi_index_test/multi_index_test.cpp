#include <eosiolib/eosio.hpp>
#include <eosiolib/dispatcher.hpp>
#include <eosiolib/multi_index.hpp>
#include <eosiolib/asset.hpp>
#include <sstream>

using namespace eosio;

#define REQUIRE(COND) eosio_assert(COND, #COND)

#define REQUIRE_OP(A, B, OP, WRONG_OP) {\
   std::ostringstream _msg_; \
   _msg_ << #A" "#OP" "#B": " << A << " "#WRONG_OP" " << B; \
   eosio_assert(A OP B, _msg_.str().c_str()); }

#define REQUIRE_EQUAL(A, B) REQUIRE_OP(A, B, ==, !=)

namespace multi_index_test {

struct limit_order {
   uint64_t     id;
   uint64_t     padding = 0;
   uint128_t    price;
   uint64_t     expiration;
   account_name owner;

      auto primary_key()const { return id; }
      uint64_t get_expiration()const { return expiration; }
      uint128_t get_price()const { return price; }
   };

   struct test_asset {
      uint64_t     id;
      asset        val;

      auto primary_key()const { return id; }
      asset get_val()const { return val; }
   };

   class snapshot_test {
      public:

         ACTION(N(multitest), trigger) {
            trigger(): what(0) {}
            trigger(uint32_t w): what(w) {}

            uint32_t what;

            EOSLIB_SERIALIZE(trigger, (what))
         };

         static void on(const trigger& act)
         {
            auto payer = act.get_account();
            print("Acting on trigger action.\n");
            switch(act.what)
            {
               case 0:
               {
                  print("Testing uint128_t secondary index.\n");
                  eosio::multi_index<N(orders), limit_order,
                     indexed_by< N(byexp),   const_mem_fun<limit_order, uint64_t, &limit_order::get_expiration> >,
                     indexed_by< N(byprice), const_mem_fun<limit_order, uint128_t, &limit_order::get_price> >
                     > orders( N(multitest), N(multitest) );

                  orders.emplace( payer, [&]( auto& o ) {
                    o.id = 1;
                    o.expiration = 300;
                    o.owner = N(dan);
                  });

                  auto order2 = orders.emplace( payer, [&]( auto& o ) {
                     o.id = 2;
                     o.expiration = 200;
                     o.owner = N(alice);
                  });

                  print("Items sorted by primary key:\n");
                  for( const auto& item : orders ) {
                     print(" ID=", item.id, ", expiration=", item.expiration, ", owner=", name{item.owner}, "\n");
                  }
                  {
                     auto prim = orders.begin();
                     REQUIRE(prim != orders.end());
                     REQUIRE_EQUAL(prim->id, 1);
                     REQUIRE_EQUAL(prim->expiration, 300);
                     REQUIRE_EQUAL(prim->owner, N(dan));
                     prim++;
                     REQUIRE(prim != orders.end());
                     REQUIRE_EQUAL(prim->id, 2);
                     REQUIRE_EQUAL(prim->expiration, 200);
                     REQUIRE_EQUAL(prim->owner, N(alice));
                     prim++;
                     REQUIRE(prim == orders.end());
                  }

                  auto expidx = orders.get_index<N(byexp)>();

                  print("Items sorted by expiration:\n");
                  for( const auto& item : expidx ) {
                     print(" ID=", item.id, ", expiration=", item.expiration, ", owner=", name{item.owner}, "\n");
                  }
                  {
                     auto exp = expidx.begin();
                     REQUIRE(exp != expidx.end());
                     REQUIRE_EQUAL(exp->id, 2);
                     REQUIRE_EQUAL(exp->expiration, 200);
                     REQUIRE_EQUAL(exp->owner, N(alice));
                     exp++;
                     REQUIRE(exp != expidx.end());
                     REQUIRE_EQUAL(exp->id, 1);
                     REQUIRE_EQUAL(exp->expiration, 300);
                     REQUIRE_EQUAL(exp->owner, N(dan));
                     exp++;
                     REQUIRE(exp == expidx.end());
                  }

                  print("Modifying expiration of order with ID=2 to 400.\n");
                  orders.modify( order2, payer, [&]( auto& o ) {
                     o.expiration = 400;
                  });

                  print("Items sorted by expiration:\n");
                  for( const auto& item : expidx ) {
                     print(" ID=", item.id, ", expiration=", item.expiration, ", owner=", name{item.owner}, "\n");
                  }
                  {
                     auto exp2 = expidx.begin();
                     REQUIRE(exp2 != expidx.end());
                     REQUIRE_EQUAL(exp2->id, 1);
                     REQUIRE_EQUAL(exp2->expiration, 300);
                     REQUIRE_EQUAL(exp2->owner, N(dan));
                     exp2++;
                     REQUIRE(exp2 != expidx.end());
                     REQUIRE_EQUAL(exp2->id, 2);
                     REQUIRE_EQUAL(exp2->expiration, 400);
                     REQUIRE_EQUAL(exp2->owner, N(alice));
                     exp2++;
                     REQUIRE(exp2 == expidx.end());
                  }

                  auto lower = expidx.lower_bound(100);

                  print("First order with an expiration of at least 100 has ID=", lower->id, " and expiration=", lower->get_expiration(), "\n");
                  REQUIRE_EQUAL(lower->id, 1);
                  REQUIRE_EQUAL(lower->expiration, 300);
               }
               break;
               case 1: // Test asset secondary index
               {
                  print("Testing asset secondary index.\n");
                  eosio::multi_index<N(test1), test_asset,
                     indexed_by< N(byval), const_mem_fun<test_asset, asset, &test_asset::get_val> >
                  > testtable( N(multitest), N(exchange) ); // Code must be same as the receiver? Scope doesn't have to be.

                  testtable.emplace( payer, [&]( auto& o ) {
                     o.id = 1;
                     o.val = asset(42);
                  });

                  testtable.emplace( payer, [&]( auto& o ) {
                     o.id = 2;
                     o.val = asset(1234);
                  });

                  testtable.emplace( payer, [&]( auto& o ) {
                     o.id = 3;
                     o.val = asset(42);
                  });

                  auto itr = testtable.find( 2 );

                  print("Items sorted by primary key:\n");
                  for( const auto& item : testtable ) {
                     print(" ID=", item.primary_key(), ", val=", item.val, "\n");
                  }
                  {
                     auto prim = testtable.begin();
                     REQUIRE(prim != testtable.end());
                     REQUIRE_EQUAL(prim->id, 1);
                     REQUIRE_EQUAL(prim->val, asset(42));
                     prim++;
                     REQUIRE(prim != testtable.end());
                     REQUIRE_EQUAL(prim->id, 2);
                     REQUIRE_EQUAL(prim->val, asset(1234));
                     prim++;
                     REQUIRE(prim != testtable.end());
                     REQUIRE_EQUAL(prim->id, 3);
                     REQUIRE_EQUAL(prim->val, asset(42));
                     prim++;
                     REQUIRE(prim == testtable.end());
                  }

                  auto validx = testtable.get_index<N(byval)>();

                  auto lower1 = validx.lower_bound(asset(40));
                  print("First entry with a val of at least 40 has ID=", lower1->id, ".\n");
                  REQUIRE_EQUAL(lower1->id, 1);

                  auto lower2 = validx.lower_bound(asset(50));
                  print("First entry with a val of at least 50 has ID=", lower2->id, ".\n");
                  REQUIRE_EQUAL(lower2->id, 2);

                  if( testtable.iterator_to(*lower2) == itr ) {
                     print("Previously found entry is the same as the one found earlier with a primary key value of 2.\n");
                  }
                  REQUIRE(testtable.iterator_to(*lower2) == itr);

                  print("Items sorted by val (secondary key):\n");
                  for( const auto& item : validx ) {
                     print(" ID=", item.primary_key(), ", val=", item.val, "\n");
                  }
                  {
                     auto val = validx.begin();
                     REQUIRE(val != validx.end());
                     REQUIRE_EQUAL(val->id, 1);
                     REQUIRE_EQUAL(val->val, asset(42));
                     val++;
                     REQUIRE(val != validx.end());
                     REQUIRE_EQUAL(val->id, 3);
                     REQUIRE_EQUAL(val->val, asset(42));
                     val++;
                     REQUIRE(val != validx.end());
                     REQUIRE_EQUAL(val->id, 2);
                     REQUIRE_EQUAL(val->val, asset(1234));
                     val++;
                     REQUIRE(val == validx.end());
                  }

                  auto upper = validx.upper_bound(asset(42));

                  print("First entry with a val greater than 42 has ID=", upper->id, ".\n");
                  REQUIRE_EQUAL(upper->id, 2);

                  print("Removed entry with ID=", lower1->id, ".\n");
                  validx.erase( lower1 );

                  print("Items sorted by primary key:\n");
                  for( const auto& item : testtable ) {
                     print(" ID=", item.primary_key(), ", val=", item.val, "\n");
                  }
                  {
                     auto val2 = validx.begin();
                     REQUIRE(val2 != validx.end());
                     REQUIRE_EQUAL(val2->id, 3);
                     REQUIRE_EQUAL(val2->val, asset(42));
                     val2++;
                     REQUIRE(val2 != validx.end());
                     REQUIRE_EQUAL(val2->id, 2);
                     REQUIRE_EQUAL(val2->val, asset(1234));
                     val2++;
                     REQUIRE(val2 == validx.end());
                  }

               }
               break;
               default:
                  eosio_assert(0, "Given what code is not supported.");
               break;
            }
         }
   };

} /// multi_index_test

namespace multi_index_test {
   extern "C" {
      /// The apply method implements the dispatch of events to this contract
      void apply( uint64_t /* receiver */, uint64_t code, uint64_t action ) {
         require_auth(code);
         eosio_assert(eosio::dispatch<snapshot_test, snapshot_test::trigger>(code, action),
                      "Could not dispatch");
      }
   }
}
