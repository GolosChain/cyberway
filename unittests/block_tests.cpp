/**
 *  @file
 *  @copyright defined in eos/LICENSE
 */

#include <boost/test/unit_test.hpp>
#include <eosio/testing/tester.hpp>
#include <eosio/chain/random.hpp>

using namespace eosio;
using namespace testing;
using namespace chain;

BOOST_AUTO_TEST_SUITE(block_tests)

BOOST_AUTO_TEST_CASE(block_with_invalid_tx_test)
{
   tester main;

   // First we create a valid block with valid transaction
   main.create_account(N(newacc));
   auto b = main.produce_block();

   // Make a copy of the valid block and corrupt the transaction
   auto copy_b = std::make_shared<signed_block>(std::move(*b));
   auto signed_tx = copy_b->transactions.back().trx.get<packed_transaction>().get_signed_transaction();
   auto& act = signed_tx.actions.back();
   auto act_data = act.data_as<newaccount>();
   // Make the transaction invalid by having the new account name the same as the creator name
   act_data.name = act_data.creator;
   act.data = fc::raw::pack(act_data);
   // Re-sign the transaction
   signed_tx.signatures.clear();
   signed_tx.sign(main.get_private_key(config::system_account_name, "active"), main.control->get_chain_id());
   // Replace the valid transaction with the invalid transaction 
   auto invalid_packed_tx = packed_transaction(signed_tx);
   copy_b->transactions.back().trx = invalid_packed_tx;

   // Re-sign the block
   auto header_bmroot = digest_type::hash( std::make_pair( copy_b->digest(), main.control->head_block_state()->blockroot_merkle.get_root() ) );
   auto sig_digest = digest_type::hash( std::make_pair(header_bmroot, main.control->head_block_state()->pending_schedule_hash) );
   copy_b->producer_signature = main.get_private_key(config::system_account_name, "active").sign(sig_digest);

   // Push block with invalid transaction to other chain
   tester validator;
   auto bs = validator.control->create_block_state_future( copy_b );
   validator.control->abort_block();
   BOOST_REQUIRE_EXCEPTION(validator.control->push_block( bs ), fc::exception ,
   [] (const fc::exception &e)->bool {
      return e.code() == account_name_exists_exception::code_value ;
   }) ;

}

std::pair<signed_block_ptr, signed_block_ptr> corrupt_trx_in_block(validating_tester& main, account_name act_name) {
   // First we create a valid block with valid transaction
   main.create_account(act_name);
   signed_block_ptr b = main.produce_block_no_validation();

   // Make a copy of the valid block and corrupt the transaction
   auto copy_b = std::make_shared<signed_block>(b->clone());
   const auto& packed_trx = copy_b->transactions.back().trx.get<packed_transaction>();
   auto signed_tx = packed_trx.get_signed_transaction();
   // Corrupt one signature
   signed_tx.signatures.clear();
   signed_tx.sign(main.get_private_key(act_name, "active"), main.control->get_chain_id());

   // Replace the valid transaction with the invalid transaction
   auto invalid_packed_tx = packed_transaction(signed_tx, packed_trx.get_compression());
   copy_b->transactions.back().trx = invalid_packed_tx;

   // Re-calculate the transaction merkle
   vector<digest_type> trx_digests;
   const auto& trxs = copy_b->transactions;
   trx_digests.reserve( trxs.size() );
   for( const auto& a : trxs )
      trx_digests.emplace_back( a.digest() );
   copy_b->transaction_mroot = merkle( move(trx_digests) );

   // Re-sign the block
   auto header_bmroot = digest_type::hash( std::make_pair( copy_b->digest(), main.control->head_block_state()->blockroot_merkle.get_root() ) );
   auto sig_digest = digest_type::hash( std::make_pair(header_bmroot, main.control->head_block_state()->pending_schedule_hash) );
   copy_b->producer_signature = main.get_private_key(copy_b->producer, "active").sign(sig_digest);
   return std::pair<signed_block_ptr, signed_block_ptr>(b, copy_b);
}

// verify that a block with a transaction with an incorrect signature, is blindly accepted from a trusted producer
BOOST_AUTO_TEST_CASE(trusted_producer_test)
{
   flat_set<account_name> trusted_producers = { N(defproducera), N(defproducerc), N(defproducere) };
   validating_tester main(trusted_producers);
   // only using validating_tester to keep the 2 chains in sync, not to validate that the validating_node matches the main node,
   // since it won't be
   main.skip_validate = true;

   // First we create a valid block with valid transaction
   std::set<account_name> producers = { N(defproducera), N(defproducerb), N(defproducerc), N(defproducerd), N(defproducere) };
   for (auto prod : producers)
       main.create_account(prod);
   auto b = main.produce_block();
   BOOST_REQUIRE_EQUAL(b->producer, config::system_account_name);

   std::vector<account_name> schedule(producers.cbegin(), producers.cend());
   auto trace = main.set_producers(schedule);

   while (b->producer == config::system_account_name) {
      b = main.produce_block();
   }

   shuffle(schedule, b->timestamp.slot - 1);

   auto itr = schedule.begin();
   ++itr; // skip producer of the current block
   for (; schedule.end() != itr && !trusted_producers.count(*itr); ++itr) { // find next trusted producer
      b = main.produce_block();
      BOOST_REQUIRE_EQUAL(b->producer, *itr);
   }

   auto blocks = corrupt_trx_in_block(main, N(tstproducera));
   main.validate_push_block( blocks.second );
}

// like trusted_producer_test, except verify that any entry in the trusted_producer list is accepted
BOOST_AUTO_TEST_CASE(trusted_producer_verify_2nd_test)
{
   flat_set<account_name> trusted_producers = { N(defproducera), N(defproducerc), N(defproducere) };
   std::deque<account_name> corrupted_accounts = {N(tstproducera), N(tstproducerc), N(tstproducere) };
   validating_tester main(trusted_producers);
   // only using validating_tester to keep the 2 chains in sync, not to validate that the validating_node matches the main node,
   // since it won't be
   main.skip_validate = true;

   // First we create a valid block with valid transaction
   std::set<account_name> producers = { N(defproducera), N(defproducerb), N(defproducerc), N(defproducerd), N(defproducere) };
   for (auto prod : producers)
       main.create_account(prod);
   auto b = main.produce_block();

   std::vector<account_name> schedule(producers.cbegin(), producers.cend());
   auto trace = main.set_producers(schedule);

   // waiting of applying of proposed producers to active state
   while (b->producer == config::system_account_name) {
      b = main.produce_block();
   }

   // the first round shuffle
   const auto promote_slot = main.control->head_block_state()->promoting_block.slot;
   shuffle(schedule, promote_slot);

   // generate the first round of active producers
   while (0 != (b->timestamp.slot - promote_slot) % schedule.size()) {
     b = main.produce_block();
   }

   // the new round in which we will try to generate corrupted blocks
   shuffle(schedule, b->timestamp.slot);

   for (auto& producer: schedule) {
      // for each trusted producer in the round
      if (trusted_producers.count(producer)) {
        auto blocks = corrupt_trx_in_block(main, corrupted_accounts.back());
        corrupted_accounts.pop_back();
        BOOST_REQUIRE_EQUAL(blocks.first->producer, producer);
        BOOST_REQUIRE_EQUAL(blocks.second->producer, producer);

        // push the corrupted block
        main.validate_push_block( blocks.second );
        // pop the corrupted block
        main.validating_node->pop_block();
        // push the normal block for success linking of the next in the round block
        main.validate_push_block( blocks.first );
      } else {
        b = main.produce_block();
        BOOST_REQUIRE_EQUAL(b->producer, producer);
      }
   }
}

// verify that a block with a transaction with an incorrect signature, is rejected if it is not from a trusted producer
BOOST_AUTO_TEST_CASE(untrusted_producer_test)
{
   flat_set<account_name> trusted_producers = { N(defproducera), N(defproducerc), N(defproducere) };
   std::deque<account_name> corrupted_accounts = {N(tstproducera), N(tstproducerc), N(tstproducere) };
   validating_tester main(trusted_producers);
   // only using validating_tester to keep the 2 chains in sync, not to validate that the validating_node matches the main node,
   // since it won't be
   main.skip_validate = true;

   // First we create a valid block with valid transaction
   std::set<account_name> producers = { N(defproducera), N(defproducerb), N(defproducerc), N(defproducerd), N(defproducere) };
   for (auto prod : producers)
       main.create_account(prod);
   auto b = main.produce_block();

   std::vector<account_name> schedule(producers.cbegin(), producers.cend());
   auto trace = main.set_producers(schedule);

   // waiting of applying of proposed producers to active state
   while (b->producer == config::system_account_name) {
      b = main.produce_block();
   }

   // the first round shuffle
   const auto promote_slot = main.control->head_block_state()->promoting_block.slot;
   shuffle(schedule, promote_slot);

   // generate the first round of active producers
   while (0 != (b->timestamp.slot - promote_slot) % schedule.size()) {
      b = main.produce_block();
   }

   // the new round in which we will try to generate corrupted blocks
   shuffle(schedule, b->timestamp.slot);

   for (auto& producer: schedule) {
      // for each not-trusted producer in the round
      if (!trusted_producers.count(producer)) {
         auto blocks = corrupt_trx_in_block(main, corrupted_accounts.back());
         corrupted_accounts.pop_back();
         BOOST_REQUIRE_EQUAL(blocks.first->producer, producer);
         BOOST_REQUIRE_EQUAL(blocks.second->producer, producer);

         BOOST_REQUIRE_EXCEPTION(main.validate_push_block( blocks.second ), fc::exception,
            [] (const fc::exception& e)->bool {
               return e.code() == unsatisfied_authorization::code_value;
            });
         // push the good block for success linking of the next in the round block
         main.validate_push_block( blocks.first );
      } else {
         b = main.produce_block();
         BOOST_REQUIRE_EQUAL(b->producer, producer);
      }
   }
}

BOOST_AUTO_TEST_SUITE_END()
