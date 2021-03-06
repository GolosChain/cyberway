#include <eosio/chain/controller.hpp>
#include <eosio/chain/transaction_context.hpp>

#include <eosio/chain/block_log.hpp>
#include <eosio/chain/fork_database.hpp>
#include <eosio/chain/exceptions.hpp>

#include <eosio/chain/account_object.hpp>
#include <eosio/chain/block_summary_object.hpp>
#include <eosio/chain/eosio_contract.hpp>
#include <eosio/chain/global_property_object.hpp>
#include <eosio/chain/generated_transaction_object.hpp>
#include <eosio/chain/transaction_object.hpp>
#include <eosio/chain/reversible_block_object.hpp>

#include <eosio/chain/authorization_manager.hpp>
#include <eosio/chain/resource_limits.hpp>
#include <eosio/chain/stake.hpp>
#include <eosio/chain/thread_utils.hpp>
#include <eosio/chain/snapshot_controller.hpp>

#include <chainbase/chainbase.hpp>
#include <fc/io/json.hpp>
#include <fc/scoped_exit.hpp>
#include <fc/variant_object.hpp>

#include <cyberway/chaindb/controller.hpp>
#include <cyberway/chaindb/account_abi_info.hpp>
#include <cyberway/genesis/genesis_import.hpp>
#include <cyberway/chain/cyberway_contract_types.hpp>
#include <cyberway/chain/cyberway_contract.hpp>

namespace eosio { namespace chain {


using resource_limits::resource_limits_manager;
using cyberway::chaindb::cursor_kind;

using controller_index_set = table_set<
   account_table,
   account_sequence_table,
   global_property_table,
   dynamic_global_property_table,
   block_summary_table,
   transaction_table,
   generated_transaction_table,
   cyberway::chain::domain_table,
   cyberway::chain::username_table
>;

class maybe_session {
   public:
      maybe_session() = default;

      maybe_session( maybe_session&& other)
// TODO: removed by CyberWay
//      : _session(move(other._session)),
      : _chaindb_session(move(other._chaindb_session))
      {
      }

      explicit maybe_session(chaindb_controller& chaindb) {
// TODO: removed by CyberWay
//      explicit maybe_session(database& db, chaindb_controller& chaindb) {
//         _session = db.start_undo_session(true);
         _chaindb_session = chaindb.start_undo_session(true);
      }

      maybe_session(const maybe_session&) = delete;

      void squash() {
// TODO: removed by CyberWay
//         if (_session)
//            _session->squash();
         if (_chaindb_session) {
            _chaindb_session->squash();
         }
      }

      void undo() {
// TODO: removed by CyberWay
//         if (_session)
//            _session->undo();
         if (_chaindb_session) {
            _chaindb_session->undo();
         }
      }

      void push() {
// TODO: removed by CyberWay
//         if (_session)
//            _session->push();
         if (_chaindb_session) {
            _chaindb_session->push();
         }
      }

      void apply_changes() {
         if (_chaindb_session) {
            _chaindb_session->apply_changes();
         }
      }

      maybe_session& operator = ( maybe_session&& mv ) {
// TODO: removed by CyberWay
//         if (mv._session) {
//            _session = move(*mv._session);
//            mv._session.reset();
//         } else {
//            _session.reset();
//         }

         if (mv._chaindb_session) {
            _chaindb_session = move(*mv._chaindb_session);
            mv._chaindb_session.reset();
         } else {
            _chaindb_session.reset();
         }

         return *this;
      };

   private:
// TODO: removed by CyberWay
//      optional<database::session>     _session;
      optional<chaindb_session>       _chaindb_session;
};

struct pending_state {
   pending_state( maybe_session&& s )
   :_db_session( move(s) ){}

   maybe_session                      _db_session;

   block_state_ptr                    _pending_block_state;

   vector<action_receipt>             _actions;

   controller::block_status           _block_status = controller::block_status::incomplete;

   optional<block_id_type>            _producer_block_id;

   void push() {
      _db_session.push();
   }

   void apply_changes() {
      _db_session.apply_changes();
   }
};

struct controller_impl {
   controller&                    self;
   chaindb_controller             chaindb;
   chainbase::database            reversible_blocks; ///< a special database to persist blocks that have successfully been applied but are still reversible
   block_log                      blog;
   optional<pending_state>        pending;
   block_state_ptr                head;
   fork_database                  fork_db;
   wasm_interface                 wasmif;
   resource_limits_manager        resource_limits;
   authorization_manager          authorization;
   controller::config             conf;
   chain_id_type                  chain_id;
   bool                           replaying= false;
   optional<fc::time_point>       replay_head_time;
   db_read_mode                   read_mode = db_read_mode::SPECULATIVE;
   bool                           in_trx_requiring_checks = false; ///< if true, checks that are normally skipped on replay (e.g. auth checks) cannot be skipped
   optional<fc::microseconds>     subjective_cpu_leeway;
   bool                           trusted_producer_light_validation = false;
   uint32_t                       snapshot_head_block = 0;
   boost::asio::thread_pool       thread_pool;
   bool                           skip_bad_blocks_check = false;

   typedef pair<scope_name,action_name>                   handler_key;
   map< account_name, map<handler_key, apply_handler> >   apply_handlers;

   /**
    *  Transactions that were undone by pop_block or abort_block, transactions
    *  are removed from this list if they are re-applied in other blocks. Producers
    *  can query this list when scheduling new transactions into blocks.
    */
   unapplied_transactions_type     unapplied_transactions;

   /**
    *  List of receipts from apply_block to get nested explicid bill for nested trx
    */
   map<transaction_id_type, transaction_receipt_header> maybe_nested_receipts;

   void pop_block() {
      auto prev = fork_db.get_block( head->header.previous );
      EOS_ASSERT( prev, block_validate_exception, "attempt to pop beyond last irreversible block" );

      if( const auto* b = reversible_blocks.find<reversible_block_object,by_num>(head->block_num) )
      {
         reversible_blocks.remove( *b );
      }

      if ( read_mode == db_read_mode::SPECULATIVE ) {
         EOS_ASSERT( head->block, block_validate_exception, "attempting to pop a block that was sparsely loaded from a snapshot");
         for( const auto& t : head->trxs )
            unapplied_transactions[t->signed_id] = t;
      }
      head = prev;
// TODO: removed by CyberWay
//      db.undo();
      chaindb.undo_last_revision();
   }


   void set_apply_handler( account_name receiver, account_name contract, action_name action, apply_handler v ) {
      apply_handlers[receiver][make_pair(contract,action)] = v;
   }

   controller_impl( const controller::config& cfg, controller& s  )
   :self(s),
    chaindb(cfg.chaindb_address_type, cfg.chaindb_address, cfg.chaindb_sys_name),
// TODO: removed by CyberWay
//    db( cfg.state_dir,
//        cfg.read_only ? database::read_only : database::read_write,
//        cfg.state_size ),
    reversible_blocks( cfg.blocks_dir/config::reversible_blocks_dir_name,
        cfg.read_only ? database::read_only : database::read_write,
        cfg.reversible_cache_size ),
    blog( cfg.blocks_dir ),
    fork_db( cfg.state_dir ),
    wasmif( cfg.wasm_runtime ),
    resource_limits( chaindb ),
    authorization( s, chaindb ),
    conf( cfg ),
    chain_id( cfg.genesis.compute_chain_id() ),
    read_mode( cfg.read_mode ),
    thread_pool( cfg.thread_pool_size )
   {

#define SET_APP_HANDLER( receiver, contract, action) \
   set_apply_handler( #receiver, #contract, #action, &BOOST_PP_CAT(apply_, BOOST_PP_CAT(contract, BOOST_PP_CAT(_,action) ) ) )

#define SET_CYBER_HANDLER( receiver, contract, action) \
   set_apply_handler( #receiver, #contract, #action, &BOOST_PP_CAT(cyberway::chain::apply_, BOOST_PP_CAT(contract, BOOST_PP_CAT(_,action) ) ) )

   SET_APP_HANDLER(cyber, cyber, newaccount);
   SET_APP_HANDLER(cyber, cyber, setcode);
   SET_APP_HANDLER(cyber, cyber, setabi);
   SET_APP_HANDLER(cyber, cyber, checkversion);
   SET_APP_HANDLER(cyber, cyber, updateauth);
   SET_APP_HANDLER(cyber, cyber, deleteauth);
   SET_APP_HANDLER(cyber, cyber, linkauth);
   SET_APP_HANDLER(cyber, cyber, unlinkauth);
   SET_APP_HANDLER(cyber, cyber, canceldelay);

   SET_CYBER_HANDLER(cyber, cyber, providebw);
//TODO: requestbw
//   SET_CYBER_HANDLER(cyber, cyber, requestbw);
/*
   SET_CYBER_HANDLER(cyber, cyber, postrecovery);
   SET_CYBER_HANDLER(cyber, cyber, passrecovery);
   SET_CYBER_HANDLER(cyber, cyber, vetorecovery);
*/


#define SET_CONTRACT_HANDLER(contract, action, function) set_apply_handler(contract, contract, #action, function);
#define SET_DOTCONTRACT_HANDLER(base, sub, action) SET_CONTRACT_HANDLER(#base "." #sub, action, \
    &BOOST_PP_CAT(cyberway::chain::apply_, BOOST_PP_CAT(base, BOOST_PP_CAT(_, BOOST_PP_CAT(sub, BOOST_PP_CAT(_,action))))))
#define SET_CYBER_DOMAIN_HANDLER(action) SET_DOTCONTRACT_HANDLER(cyber, domain, action)

    SET_CYBER_DOMAIN_HANDLER(newusername);
    SET_CYBER_DOMAIN_HANDLER(newdomain);
    SET_CYBER_DOMAIN_HANDLER(passdomain);
    SET_CYBER_DOMAIN_HANDLER(linkdomain);
    SET_CYBER_DOMAIN_HANDLER(unlinkdomain);

   fork_db.irreversible.connect( [&]( auto b ) {
                                 on_irreversible(b);
                                 });

   }

   /**
    *  Plugins / observers listening to signals emited (such as accepted_transaction) might trigger
    *  errors and throw exceptions. Unless those exceptions are caught it could impact consensus and/or
    *  cause a node to fork.
    *
    *  If it is ever desirable to let a signal handler bubble an exception out of this method
    *  a full audit of its uses needs to be undertaken.
    *
    */
   template<typename Signal, typename Arg>
   void emit( const Signal& s, Arg&& a ) {
      try {
        s(std::forward<Arg>(a));
      } catch (boost::interprocess::bad_alloc& e) {
         wlog( "bad alloc" );
         throw e;
      } catch ( controller_emit_signal_exception& e ) {
         wlog( "${details}", ("details", e.to_detail_string()) );
         throw e;
      } catch ( fc::exception& e ) {
         wlog( "${details}", ("details", e.to_detail_string()) );
      } catch ( ... ) {
         wlog( "signal handler threw exception" );
      }
   }

   void on_irreversible( const block_state_ptr& s ) {
      if( !blog.head() )
         blog.read_head();

      const auto& log_head = blog.head();
      bool append_to_blog = false;
      if (!log_head) {
         if (s->block) {
            EOS_ASSERT(s->block_num == blog.first_block_num(), block_log_exception, "block log has no blocks and is appending the wrong first block.  Expected ${expected}, but received: ${actual}",
                      ("expected", blog.first_block_num())("actual", s->block_num));
            append_to_blog = true;
         } else {
            EOS_ASSERT(s->block_num == blog.first_block_num() - 1, block_log_exception, "block log has no blocks and is not properly set up to start after the snapshot");
         }
      } else {
         auto lh_block_num = log_head->block_num();
         if (s->block_num > lh_block_num) {
            EOS_ASSERT(s->block_num - 1 == lh_block_num, unlinkable_block_exception, "unlinkable block", ("s->block_num", s->block_num)("lh_block_num", lh_block_num));
            EOS_ASSERT(s->block->previous == log_head->id(), unlinkable_block_exception, "irreversible doesn't link to block log head");
            append_to_blog = true;
         }
      }

// TODO: removed by CyberWay
//      db.commit( s->block_num );
      chaindb.commit_revision( s->block_num );

      if( append_to_blog ) {
         blog.append(s->block);
      }

      const auto& ubi = reversible_blocks.get_index<reversible_block_index,by_num>();
      auto objitr = ubi.begin();
      while( objitr != ubi.end() && objitr->blocknum <= s->block_num ) {
         reversible_blocks.remove( *objitr );
         objitr = ubi.begin();
      }

      // the "head" block when a snapshot is loaded is virtual and has no block data, all of its effects
      // should already have been loaded from the snapshot so, it cannot be applied
      if (s->block) {
         if (read_mode == db_read_mode::IRREVERSIBLE) {
            // when applying a snapshot, head may not be present
            // when not applying a snapshot, make sure this is the next block
            if (!head || s->block_num == head->block_num + 1) {
               apply_block(s->block, controller::block_status::complete);
               head = s;
            } else {
               // otherwise, assert the one odd case where initializing a chain
               // from genesis creates and applies the first block automatically.
               // when syncing from another chain, this is pushed in again
               EOS_ASSERT(!head || head->block_num == 1, block_validate_exception, "Attempting to re-apply an irreversible block that was not the implied genesis block");
            }

            fork_db.mark_in_current_chain(head, true);
            fork_db.set_validity(head, true);
         }
         emit(self.irreversible_block, s);
      }
   }

   void set_revision(uint64_t revision) {
// TODO: removed by CyberWay
//      db.set_revision(revision);
      chaindb.set_revision(revision);
   }

   void replay(std::function<bool()> shutdown) {
      auto blog_head = blog.read_head();
      auto blog_head_time = blog_head->timestamp.to_time_point();
      replaying = true;
      replay_head_time = blog_head_time;
      auto start_block_num = head->block_num + 1;
      ilog( "existing block log, attempting to replay from ${s} to ${n} blocks",
            ("s", start_block_num)("n", blog_head->block_num()) );

      auto start = fc::time_point::now();
      auto skip_session = self.skip_db_sessions( controller::block_status::irreversible );
      while( auto next = blog.read_block_by_num( head->block_num + 1 ) ) {
         if( skip_session ) {
            set_revision( next->block_num() );
         }
         replay_push_block( next, controller::block_status::irreversible );
         if( next->block_num() % 500 == 0 ) {
            if( (next->block_num() % 10000) == 0 && skip_session ) {
               chaindb.apply_all_changes();
            }
            ilog( "${n} of ${head}", ("n", next->block_num())("head", blog_head->block_num()) );
            if( shutdown() ) break;
         }
      }
      ilog( "${n} blocks replayed", ("n", head->block_num - start_block_num) );

      // if the irreversible log is played without undo sessions enabled, we need to sync the
      // revision ordinal to the appropriate expected value here.
      if( skip_session ) {
         chaindb.apply_all_changes();
      }

      int rev = 0;
      auto total = blog_head->block_num() + reversible_blocks.get_index<reversible_block_index>().indices().size();
      while( auto obj = reversible_blocks.find<reversible_block_object,by_num>(head->block_num+1) ) {
         ++rev;
         replay_push_block( obj->get_block(), controller::block_status::validated );
         if( obj->get_block()->block_num() % 100 == 0 ) {
            std::cerr << std::setw(10) << obj->get_block()->block_num() << " of " << total << "\r";
            if( shutdown() ) break;
         }
      }

      ilog( "${n} reversible blocks replayed", ("n",rev) );
      auto end = fc::time_point::now();
      ilog( "replayed ${n} blocks in ${duration} seconds, ${mspb} ms/block",
            ("n", head->block_num - start_block_num)("duration", (end-start).count()/1000000)
            ("mspb", ((end-start).count()/1000.0)/(head->block_num-start_block_num)) );
      replaying = false;
      replay_head_time.reset();
   }

   void init(std::function<bool()> shutdown, snapshot_reader_ptr snapshot) {

      bool report_integrity_hash = !!snapshot;
      bool initialized = false;

      if (snapshot) {

         chaindb.drop_db();

         snapshot_head_block = snapshot_controller(chaindb, resource_limits, fork_db, reversible_blocks, head, conf.genesis).read_snapshot(std::move(snapshot));

         auto end = blog.read_head();
         if( !end ) {
            EOS_THROW(block_log_not_found, "Truncted block log not supported");
         } else if( end->block_num() > head->block_num ) {
            replay( shutdown );
         } else {
            EOS_ASSERT(fork_db.get_child_block(end->id()), fork_database_exception,
                        "Block log provided with snapshot has no blocks before frok db" );
         }
      } else {
         if( !head ) {
            initialize_fork_db(); // set head to genesis state
            initialized = true;
         }

         auto end = blog.read_head();
         if( !end ) {
            blog.reset( conf.genesis, head->block );
         } else if( end->block_num() > head->block_num ) {
            replay( shutdown );
            report_integrity_hash = true;
         }
      }

      chain_id = conf.genesis.compute_chain_id();

      if( shutdown() ) return;

      const auto& ubi = reversible_blocks.get_index<reversible_block_index,by_num>();
      auto objitr = ubi.rbegin();
      if( objitr != ubi.rend() ) {
         EOS_ASSERT( objitr->blocknum == head->block_num, fork_database_exception,
                    "reversible block database is inconsistent with fork database, replay blockchain",
                    ("head",head->block_num)("unconfimed", objitr->blocknum)         );
      } else {
         auto end = blog.read_head();
         EOS_ASSERT( !end || end->block_num() == head->block_num, fork_database_exception,
                    "fork database exists but reversible block database does not, replay blockchain",
                    ("blog_head",end->block_num())("head",head->block_num)  );
      }

      chaindb.restore_db();

      EOS_ASSERT(chaindb.revision() >= head->block_num, fork_database_exception, "fork database is inconsistent with shared memory",
                 ("db",chaindb.revision())("head",head->block_num));

      if (chaindb.revision() > head->block_num) {
         wlog( "warning: database revision (${db}) is greater than head block number (${head}), "
               "attempting to undo pending changes",
               ("db",chaindb.revision())("head",head->block_num) );
      }
      while (chaindb.revision() > head->block_num) {
// TODO: removed by CyberWay
//         db.undo();
         chaindb.undo_last_revision();
      }

      if( !initialized ) {
         initialize_caches();
      }

      if( report_integrity_hash ) {
// TODO: removed by CyberWay
//         const auto hash = calculate_integrity_hash();
//         ilog( "database initialized with hash: ${hash}", ("hash", hash) );
          wlog( "integrity hash is disabled" );
      }
   }

   ~controller_impl() {
      pending.reset();

// TODO: removed by CyberWay
//      db.flush();

      reversible_blocks.flush();
      try {
         chaindb.apply_all_changes();
      } catch ( const guard_exception& e ) {
         dlog("Details: ${details}", ("details", e.to_detail_string()));
      }
   }

   void add_indices() {
      reversible_blocks.add_index<reversible_block_index>();

      controller_index_set::add_tables(chaindb);

      authorization.add_indices();
      resource_limits.add_indices();
      stake::stake_index_set::add_tables(chaindb);
   }

// TODO: removed by CyberWay
//   void clear_all_undo() {
//      // Rewind the database to the last irreversible block
//      db.undo_all();
//   }

   sha256 calculate_integrity_hash() {
      sha256::encoder enc;
      auto hash_writer = std::make_unique<integrity_hash_snapshot_writer>(enc);

      snapshot_controller(chaindb, resource_limits, fork_db, reversible_blocks, head, conf.genesis).write_snapshot(std::move(hash_writer));

      return enc.result();
   }


    void read_genesis(block_state_ptr &block) {
        if (conf.read_genesis) {
            cyberway::genesis::genesis_import reader(conf.genesis_file, self);
            reader.import(block);
        }
    }

   /**
    *  Sets fork database head to the genesis state.
    */
   void initialize_fork_db() {
      wlog( " Initializing new blockchain with genesis state                  " );
      chaindb.initialize_db();
      producer_schedule_type initial_schedule{ 0, {{config::system_account_name, conf.genesis.initial_key}} };

      block_header_state genheader;
      genheader.active_schedule       = initial_schedule;
      genheader.pending_schedule      = initial_schedule;
      genheader.pending_schedule_hash = fc::sha256::hash(initial_schedule);
      genheader.header.timestamp      = conf.genesis.initial_timestamp;
      genheader.header.action_mroot   = conf.genesis.compute_chain_id();
      genheader.id                    = genheader.header.id();
      genheader.block_num             = genheader.header.block_num();

      head = std::make_shared<block_state>( genheader );
      head->block = std::make_shared<signed_block>(genheader.header);
      set_revision(head->block_num);

      bool need_native_accounts = !conf.read_genesis;
      initialize_database(need_native_accounts);
      read_genesis( head );

      fork_db.set( head );
   }

   void initialize_caches() {
       auto block_summary_table = chaindb.get_table<block_summary_object>();
       for (auto& value: block_summary_table) {
           // only load to RAM
       }

       auto transaction_table = chaindb.get_table<transaction_object>();
       for (auto& value: transaction_table) {
           // only load to RAM
       }
   }

   void create_native_account( account_name name, const authority& owner, const authority& active, bool is_privileged = false ) {
      chaindb.emplace<account_object>(name.value, [&](auto& a) {
         a.creation_date = conf.genesis.initial_timestamp;
         a.privileged = is_privileged;

         if (name == config::system_account_name) {
            a.set_abi(eosio_contract_abi());
         } else if (name == config::domain_account_name) {
            a.set_abi(domain_contract_abi());
         }
      });
      chaindb.emplace<account_sequence_object>(name.value, [&](auto & a) { });
      const auto& owner_permission  = authorization.create_permission({}, name, config::owner_name, 0,
                                                                      owner, conf.genesis.initial_timestamp );
      const auto& active_permission = authorization.create_permission({}, name, config::active_name, owner_permission.id,
                                                                      active, conf.genesis.initial_timestamp );

      resource_limits.initialize_account(name, {});

      // Does exist any reason to calculate ram usage for system accounts?

// TODO: Removed by CyberWay
//      int64_t ram_delta = config::overhead_per_account_ram_bytes;
//      ram_delta += 2*config::billable_size_v<permission_object>;
//      ram_delta += owner_permission.auth.get_billable_size();
//      ram_delta += active_permission.auth.get_billable_size();
//
//      resource_limits.add_pending_ram_usage(name, ram_delta);
//      resource_limits.verify_account_ram_usage(name);
   }

   void initialize_database(bool need_native_accounts = true) {
      // Initialize block summary index
      auto blocksum_table = chaindb.get_table<block_summary_object>();
      for (int i = 0; i < 0x10000; i++)
         blocksum_table.emplace([&](block_summary_object&) {});

      const auto& tapos_block_summary = blocksum_table.get(1);
      blocksum_table.modify( tapos_block_summary, [&]( auto& bs ) {
        bs.block_id = head->id;
      });

      conf.genesis.initial_configuration.validate();
      chaindb.emplace<global_property_object>([&](auto& gpo ){
        gpo.configuration = conf.genesis.initial_configuration;
      });
      chaindb.emplace<dynamic_global_property_object>([](auto&){});

      authorization.initialize_database();
      resource_limits.initialize_database();

      if (need_native_accounts) {
          authority system_auth(conf.genesis.initial_key);
          create_native_account( config::system_account_name, system_auth, system_auth, true );
          create_native_account(config::msig_account_name, system_auth, system_auth, true);
          create_native_account(config::domain_account_name, system_auth, system_auth);
          create_native_account(config::govern_account_name, system_auth, system_auth, true);
          create_native_account(config::stake_account_name, system_auth, system_auth, true);
    
          auto empty_authority = authority(1, {}, {});
          auto active_producers_authority = authority(1, {}, {});
          active_producers_authority.accounts.push_back({{config::system_account_name, config::active_name}, 1});
    
          create_native_account( config::null_account_name, empty_authority, empty_authority );
          create_native_account( config::producers_account_name, empty_authority, active_producers_authority );
          const auto& active_permission       = authorization.get_permission({config::producers_account_name, config::active_name});
          const auto& majority_permission     = authorization.create_permission( {},
                                                                                 config::producers_account_name,
                                                                                 config::majority_producers_permission_name,
                                                                                 active_permission.id,
                                                                                 active_producers_authority,
                                                                                 conf.genesis.initial_timestamp );
          const auto& minority_permission     = authorization.create_permission( {},
                                                                                 config::producers_account_name,
                                                                                 config::minority_producers_permission_name,
                                                                                 majority_permission.id,
                                                                                 active_producers_authority,
                                                                                 conf.genesis.initial_timestamp );
      } else {
          chaindb.emplace<account_object>(config::system_account_name, [&](auto& a) {
             a.creation_date = conf.genesis.initial_timestamp;
             a.privileged = true;
             a.set_abi(eosio_contract_abi());
          });
      }
   }



   /**
    * @post regardless of the success of commit block there is no active pending block
    */
   void commit_block( bool add_to_fork_db ) {
      auto reset_pending_on_exit = fc::make_scoped_exit([this]{
         pending.reset();
      });

      try {
         pending->apply_changes();

         if (add_to_fork_db) {
            pending->_pending_block_state->validated = true;
            auto new_bsp = fork_db.add(pending->_pending_block_state, true);
            emit(self.accepted_block_header, pending->_pending_block_state);
            head = fork_db.head();
            EOS_ASSERT(new_bsp == head, fork_database_exception, "committed block did not become the new head in fork database");
         }

         if( !replaying ) {
            reversible_blocks.create<reversible_block_object>( [&]( auto& ubo ) {
               ubo.blocknum = pending->_pending_block_state->block_num;
               ubo.set_block( pending->_pending_block_state->block );
            });
         }

         emit( self.accepted_block, pending->_pending_block_state );
      } catch (...) {
         // dont bother resetting pending, instead abort the block
         reset_pending_on_exit.cancel();
         abort_block();
         throw;
      }

      // push the state for pending.
      pending->push();
   }

   // The returned scoped_exit should not exceed the lifetime of the pending which existed when make_block_restore_point was called.
   fc::scoped_exit<std::function<void()>> make_block_restore_point() {
      auto orig_block_transactions_size = pending->_pending_block_state->block->transactions.size();
      auto orig_state_transactions_size = pending->_pending_block_state->trxs.size();
      auto orig_state_actions_size      = pending->_actions.size();

      std::function<void()> callback = [this,
                                        orig_block_transactions_size,
                                        orig_state_transactions_size,
                                        orig_state_actions_size]()
      {
         pending->_pending_block_state->block->transactions.resize(orig_block_transactions_size);
         pending->_pending_block_state->trxs.resize(orig_state_transactions_size);
         pending->_actions.resize(orig_state_actions_size);
      };

      return fc::make_scoped_exit( std::move(callback) );
   }

   transaction_trace_ptr apply_onerror( const generated_transaction& gtrx,
                                        const transaction_context& src_ctx,
                                        fc::time_point deadline,
                                        uint32_t& cpu_time_to_bill_us, // only set on failure
                                        uint64_t& ram_to_bill_bytes, // only set on failure
                                        const billed_bw_usage& billed
                                      )
   {
      signed_transaction etrx;
      // Deliver onerror action containing the failed deferred transaction directly back to the sender.
      etrx.actions.emplace_back( vector<permission_level>{{gtrx.sender, config::active_name}},
                                 onerror( gtrx.sender_id, gtrx.packed_trx.data(), gtrx.packed_trx.size() ) );
      etrx.expiration = self.pending_block_time() + fc::microseconds(999'999); // Round up to avoid appearing expired
      etrx.set_reference_block( self.head_block_id() );

      transaction_context trx_context( self, etrx, etrx.id(), src_ctx.pseudo_start );
      trx_context.caller_set_deadline = deadline;
      trx_context.explicit_billed_cpu_time = billed.explicit_usage;
      trx_context.billed_cpu_time_us = billed.cpu_time_us;
      trx_context.explicit_billed_ram_bytes = billed.explicit_usage;
      trx_context.billed_ram_bytes = billed.ram_bytes;
      transaction_trace_ptr trace = trx_context.trace;

      trx_context.storage_providers = src_ctx.storage_providers;
      auto provider = src_ctx.storage_providers.find(gtrx.sender);
      if (src_ctx.storage_providers.end() != provider) {
         trx_context.bill_to_accounts.emplace(provider->second);
      }
      try {
         trx_context.init_for_implicit_trx();
         trx_context.published = gtrx.published;
         trx_context.trace->action_traces.emplace_back();
         trx_context.dispatch_action( trx_context.trace->action_traces.back(), etrx.actions.back(), gtrx.sender );
         trx_context.finalize(); // Automatically rounds up network and CPU usage in trace and bills payers if successful

         auto restore = make_block_restore_point();
         trace->receipt = push_receipt( gtrx.trx_id, transaction_receipt::soft_fail,
                                        trx_context.billed_cpu_time_us, trace->net_usage,
                                        trx_context.billed_ram_bytes, trace->storage_bytes );
         fc::move_append( pending->_actions, move(trx_context.executed) );

         trx_context.squash();
         restore.cancel();
         return trace;
      } catch( const guard_exception& e ) {
         throw;
      } catch( const fc::exception& e ) {
         cpu_time_to_bill_us = trx_context.update_billed_cpu_time( fc::time_point::now() );
         ram_to_bill_bytes = trx_context.update_billed_ram_bytes();
         trace->except = e;
         trace->except_ptr = std::current_exception();
      }
      return trace;
   }

   void remove_scheduled_transaction( const generated_transaction_object& gto ) {
// TODO: Removed by CyberWay
//      resource_limits.add_pending_ram_usage(
//         gto.payer,
//         -(config::billable_size_v<generated_transaction_object> + gto.packed_trx.size())
//      );
      // No need to verify_account_ram_usage since we are only reducing memory
      chaindb.erase( gto, resource_limits.get_storage_payer(self.pending_block_slot(), account_name()));
   }



   transaction_trace_ptr push_scheduled_transaction( const transaction_id_type& trxid, fc::time_point deadline, const billed_bw_usage& billed ) {
      auto idx = chaindb.get_index<generated_transaction_object, by_trx_id>();
      auto itr = idx.find( trxid, cyberway::chaindb::cursor_kind::OneRecord );
      EOS_ASSERT( itr != idx.end(), unknown_transaction_exception, "unknown transaction" );
      return push_scheduled_transaction( *itr, deadline, billed );
   }

// TODO: request bw, why provided?
//    void call_approvebw_actions(signed_transaction& call_provide_trx, transaction_context& trx_context) {
//        call_provide_trx.expiration = self.pending_block_time() + fc::microseconds(999'999); // Round up to avoid appearing expired
//        call_provide_trx.set_reference_block( self.head_block_id() );
//
//        trx_context.init_for_implicit_trx();
//        trx_context.exec();
//        trx_context.validate_bw_usage();
//
//        auto restore = make_block_restore_point();
//        fc::move_append( pending->_actions, move(trx_context.executed) );
//        trx_context.squash();
//        restore.cancel();
//    }
//
//    bandwith_request_result get_provided_bandwith(const vector<action>& actions, fc::time_point deadline)  {
//        signed_transaction call_provide_trx;
//        transaction_context trx_context( self, call_provide_trx, call_provide_trx.id());
//        for (const auto& action : actions) {
//            if (action.account == config::system_account_name && action.name == config::request_bw_action) {
//                const auto request_bw = action.data_as<requestbw>();
//                call_provide_trx.actions.emplace_back(
//                    vector<permission_level>{{request_bw.provider, config::active_name}},
//                    request_bw.provider,
//                    config::approve_bw_action,
//                    fc::raw::pack(approvebw(request_bw.account)));
//            }
//        }
//
//        if (!call_provide_trx.actions.empty()) {
//            trx_context.deadline = deadline;
//            call_approvebw_actions(call_provide_trx, trx_context);
//        }
//
//        return {trx_context.get_provided_bandwith(), trx_context.get_net_usage(), trx_context.get_cpu_usage()};
//    }

   bool is_softfail_transaction( const transaction_trace& trace ) const {
       if (skip_bad_blocks_check) {
           return false;
       }

       struct soft_fail_block {
           block_id_type block_id;
           fc::flat_set<transaction_id_type> trx_ids;
       };

       static fc::flat_map<uint32_t, soft_fail_block> blocks_with_soft_fail = {
           { uint32_t(508310), {
               block_id_type("0007c1967213d961df9d1934cc73ff9cacd68fe5c460e9fcae4fc019d3ec8b62"),
               { transaction_id_type("f811650021f1048738b3c7c653c7459c529e3f95da2ae5ecad4bd9307187f56a")}}},
           { uint32_t(528192), {
               block_id_type("00080f40a2c957bd3fc039d8d88f06ea39517ae26ccd681e9f1caed4346e0b52"),
               { transaction_id_type("a88270bb59e6933ca1b400d24c75917b656483abfde677c62cab17d026e9b99b")}}},
           { uint32_t(542256), {
               block_id_type("00084630b3a3c3bf8facac9f753613e1de85bf80332f40ad7a2a3c4778330928"),
               { transaction_id_type("3766bcbf214bf7a8e8192c693e71e9b678638723c7be3daab7b675f11636918f")}}},
           { uint32_t(542259), {
               block_id_type("000846336f2f0053b6e4d0d6947cd443f5daabd8d682f5c6f87c714825aaa6ba"),
               { transaction_id_type("81ff40bff8af6660043507356484d9cf4724d1c26c6752454c2dcca4e6abd47a")}}},
           { uint32_t(551879), {
               block_id_type("00086bc72fa03e2aad959ad3ef666f68c8e7d298c88e17e690d047f8c006501a"),
               { transaction_id_type("ecd2badd55c72c14c7d86051bdf0c860013e6801b573c8b68e4021beed32c9dc")}}}
       };

       auto itr = blocks_with_soft_fail.find(trace.block_num);
       if (blocks_with_soft_fail.end() == itr) {
           return false;
       }

       EOS_ASSERT( *self.pending_producer_block_id() == itr->second.block_id, block_validate_exception,
            "Bad ID (${head} != ${valid}) for the block ${num}",
            ("head", head->id)("valid", itr->second.block_id)("num", *self.pending_producer_block_id()) );

       auto ttr = itr->second.trx_ids.find(trace.id);
       if (itr->second.trx_ids.end() == ttr) {
           return false;
       }

       return true;
   }

   transaction_trace_ptr push_scheduled_transaction( const generated_transaction_object& gto, fc::time_point deadline, const billed_bw_usage& billed )
   try {
      maybe_session undo_session;
      if ( !self.skip_db_sessions() )
         undo_session = maybe_session(chaindb);

      auto gtrx = generated_transaction(gto);

      // remove the generated transaction object after making a copy
      // this will ensure that anything which affects the GTO multi-index-container will not invalidate
      // data we need to successfully retire this transaction.
      //
      // IF the transaction FAILs in a subjective way, `undo_session` should expire without being squashed
      // resulting in the GTO being restored and available for a future block to retire.
      remove_scheduled_transaction(gto);

      fc::datastream<const char*> ds( gtrx.packed_trx.data(), gtrx.packed_trx.size() );

      EOS_ASSERT( gtrx.delay_until <= self.pending_block_time(), transaction_exception, "this transaction isn't ready",
                 ("gtrx.delay_until",gtrx.delay_until)("pbt",self.pending_block_time())          );

      signed_transaction dtrx;
      fc::raw::unpack(ds,static_cast<transaction&>(dtrx) );
      transaction_metadata_ptr trx = std::make_shared<transaction_metadata>( dtrx );
      trx->accepted = true;
      trx->scheduled = true;

      transaction_trace_ptr trace;
      if( gtrx.expiration < self.pending_block_time() ) {
         trace = std::make_shared<transaction_trace>();
         trace->id = gtrx.trx_id;
         trace->block_num = self.pending_block_state()->block_num;
         trace->block_time = self.pending_block_time();
         trace->producer_block_id = self.pending_producer_block_id();
         trace->scheduled = true;
         trace->receipt = push_receipt( gtrx.trx_id, transaction_receipt::expired, billed.cpu_time_us, 0,
             billed.ram_bytes, 0 ); // expire the transaction
         emit( self.accepted_transaction, trx );
         emit( self.applied_transaction, trace );
         undo_session.squash();
         return trace;
      }

      auto reset_in_trx_requiring_checks = fc::make_scoped_exit([old_value=in_trx_requiring_checks,this](){
         in_trx_requiring_checks = old_value;
      });
      in_trx_requiring_checks = true;

      uint32_t cpu_time_to_bill_us = billed.cpu_time_us;
      uint64_t ram_to_bill_bytes = billed.ram_bytes;

      transaction_context trx_context( self, dtrx, gtrx.trx_id );
      trx_context.leeway =  fc::microseconds(0); // avoid stealing cpu resource
      trx_context.caller_set_deadline = deadline;
      trx_context.explicit_billed_cpu_time = billed.explicit_usage;
      trx_context.billed_cpu_time_us = billed.cpu_time_us;
      trx_context.explicit_billed_ram_bytes = billed.explicit_usage;
      trx_context.billed_ram_bytes = billed.ram_bytes;
      trace = trx_context.trace;

      // CyberWay speccific for disabled onerror handler
      const bool is_softfailed_trx = is_softfail_transaction(*trace);
      if (is_softfailed_trx) {
          resource_limits.validate_storage_price(true);
      }

      try {
         auto restore_storage_price = fc::make_scoped_exit([&is_softfailed_trx, this](){
            if( is_softfailed_trx ) {
               resource_limits.validate_storage_price(false);
            }
         });

         //TODO: request bw, why provided?
         //auto bandwith_request_result = get_provided_bandwith(dtrx.actions, deadline);
         //trx_context.set_provided_bandwith(std::move(bandwith_request_result.bandwith));
         //trx_context.add_cpu_usage(bandwith_request_result.used_cpu);
         //trx_context.add_net_usage(bandwith_request_result.used_net);

         trx_context.init_for_deferred_trx( gtrx.published );
         trx_context.exec();
         trx_context.finalize(); // Automatically rounds up network and CPU usage in trace and bills payers if successful
         EOS_ASSERT(!trx_context.nested_trx, transaction_exception, "deferred trx can't start nested trx");

         auto restore = make_block_restore_point();

         trace->receipt = push_receipt( gtrx.trx_id,
                                        transaction_receipt::executed,
                                        trx_context.billed_cpu_time_us,
                                        trace->net_usage,
                                        trx_context.billed_ram_bytes,
                                        trace->storage_bytes);

         fc::move_append( pending->_actions, move(trx_context.executed) );

         emit( self.accepted_transaction, trx );
         emit( self.applied_transaction, trace );

         trx_context.squash();
         undo_session.squash();

         restore.cancel();

         return trace;
      } catch( const guard_exception& e ) {
         throw;
      } catch( const fc::exception& e ) {
         cpu_time_to_bill_us = trx_context.update_billed_cpu_time( fc::time_point::now() );
         ram_to_bill_bytes = trx_context.update_billed_ram_bytes();
         trace->except = e;
         trace->except_ptr = std::current_exception();
         trace->elapsed = fc::time_point::now() - trx_context.start;
      }
      trx_context.undo();

      // Only subjective OR soft OR hard failure logic below:

// Disabled in CyberWay
// the result receipt contains resource usage from two different transactions with different actors
//   as result there is no way to validate the account resources on a validator node.
//
      if( is_softfailed_trx ) {
         // Attempt error handling for the generated transaction.

         auto error_trace = apply_onerror( gtrx, trx_context, deadline,
                                           cpu_time_to_bill_us, ram_to_bill_bytes, billed);
         error_trace->failed_dtrx_trace = trace;
         trace = error_trace;
         if( !trace->except_ptr ) {
            emit( self.accepted_transaction, trx );
            emit( self.applied_transaction, trace );
            undo_session.squash();
            return trace;
         }
         trace->elapsed = fc::time_point::now() - trx_context.start;
      }

      // Only subjective OR hard failure logic below:

      // subjectivity changes based on producing vs validating
      if ( !controller::scheduled_failure_is_subjective((*trace->except), !billed.explicit_usage) ) {
         // hard failure logic
         if( !billed.explicit_usage ) {
            auto& rl = self.get_mutable_resource_limits_manager();
            rl.update_account_usage( trx_context.bill_to_accounts, self.pending_block_slot() );

            cpu_time_to_bill_us = static_cast<uint32_t>(std::min(
                static_cast<uint64_t>(cpu_time_to_bill_us), trx_context.hard_limits[resource_limits::CPU].max));
            ram_to_bill_bytes = std::min(ram_to_bill_bytes, trx_context.hard_limits[resource_limits::RAM].max);
         }
         
         resource_limits.add_transaction_usage( trx_context.bill_to_accounts, trx_context.pricelist,
             cpu_time_to_bill_us, 0, ram_to_bill_bytes, self.pending_block_time(), false ); // Should never fail
         trace->receipt = push_receipt(gtrx.trx_id, transaction_receipt::hard_fail, cpu_time_to_bill_us, 0, ram_to_bill_bytes, 0);

         emit( self.accepted_transaction, trx );
         emit( self.applied_transaction, trace );

         undo_session.squash();
      } else {
         emit( self.accepted_transaction, trx );
         emit( self.applied_transaction, trace );
      }
      return trace;
   } FC_CAPTURE_AND_RETHROW() /// push_scheduled_transaction


   /**
    *  Adds the transaction receipt to the pending block and returns it.
    */
   template<typename T>
   const transaction_receipt& push_receipt( const T& trx, transaction_receipt_header::status_enum status,
                                            uint64_t cpu_usage_us, uint64_t net_usage,
                                            uint64_t ram_bytes, int64_t storage_bytes ) {
      uint64_t net_usage_words = net_usage / 8;
      EOS_ASSERT( net_usage_words*8 == net_usage, transaction_exception, "net_usage is not divisible by 8" );
      pending->_pending_block_state->block->transactions.emplace_back( trx );
      transaction_receipt& r = pending->_pending_block_state->block->transactions.back();
      r.cpu_usage_us         = cpu_usage_us;
      r.net_usage_words      = net_usage_words;
      r.ram_kbytes           = ram_bytes >> 10;
      r.storage_kbytes       = storage_bytes >> 10;
      r.status               = status;
      return r;
   }

   /**
    *  This is the entry point for new transactions to the block state. It will check authorization and
    *  determine whether to execute it now or to delay it. Lastly it inserts a transaction receipt into
    *  the pending block.
    */
   transaction_trace_ptr push_transaction( const transaction_metadata_ptr& trx,
                                           fc::time_point deadline,
                                           const billed_bw_usage& billed,
                                           bool valid_maybe_nested = false)
   {
      EOS_ASSERT(deadline != fc::time_point(), transaction_exception, "deadline cannot be uninitialized");

      transaction_trace_ptr trace;
      try {
         auto start = fc::time_point::now();
         const bool check_auth = !self.skip_auth_check() && !trx->implicit;
         // call recover keys so that trx->sig_cpu_usage is set correctly
         const fc::microseconds sig_cpu_usage = check_auth ? std::get<0>( trx->recover_keys( chain_id ) ) : fc::microseconds();
         const flat_set<public_key_type>& recovered_keys = check_auth ? std::get<1>( trx->recover_keys( chain_id ) ) : flat_set<public_key_type>();
         if( !billed.explicit_usage ) {
            fc::microseconds already_consumed_time( EOS_PERCENT(sig_cpu_usage.count(), conf.sig_cpu_bill_pct) );

            if( start.time_since_epoch() <  already_consumed_time ) {
               start = fc::time_point();
            } else {
               start -= already_consumed_time;
            }
         }

         const signed_transaction& trn = trx->packed_trx->get_signed_transaction();
         transaction_context trx_context(self, trn, trx->id, start);
         if ((bool)subjective_cpu_leeway && pending->_block_status == controller::block_status::incomplete) {
            trx_context.leeway = *subjective_cpu_leeway;
         }
         trx_context.caller_set_deadline = deadline;
         trx_context.explicit_billed_cpu_time = billed.explicit_usage;
         trx_context.billed_cpu_time_us = billed.cpu_time_us;
         trx_context.explicit_billed_ram_bytes = billed.explicit_usage;
         trx_context.billed_ram_bytes = billed.ram_bytes;
         trace = trx_context.trace;
         try {
            //TODO: request bw, why provided?
            //auto bandwith_request_result = get_provided_bandwith(trn.actions, deadline);

            //trx_context.set_provided_bandwith(std::move(bandwith_request_result.bandwith));
            //trx_context.add_cpu_usage(bandwith_request_result.used_cpu);
            //trx_context.add_net_usage(bandwith_request_result.used_net);

            if( trx->implicit ) {
               trx_context.init_for_implicit_trx();
            } else {
               bool skip_recording = replay_head_time && (time_point(trn.expiration) <= *replay_head_time);
               trx_context.init_for_input_trx( trx->packed_trx->get_unprunable_size(),
                                               trx->packed_trx->get_prunable_size(),
                                               skip_recording);
            }

            trx_context.delay = fc::seconds(trn.delay_sec);

            if( check_auth ) {
               authorization.check_authorization(
                       trn.actions,
                       recovered_keys,
                       {},
                       trx_context.delay,
                       [&trx_context](){ trx_context.checktime(); },
                       false
               );
            }

            auto restore = make_block_restore_point();

            trx_context.exec();
            trx_context.finalize(); // Automatically rounds up network and CPU usage in trace and bills payers if successful

            if (!trx->implicit) {
               transaction_receipt::status_enum s = (trx_context.delay == fc::seconds(0))
                                                    ? transaction_receipt::executed
                                                    : transaction_receipt::delayed;
               trace->receipt = push_receipt(*trx->packed_trx, s, trx_context.billed_cpu_time_us, trace->net_usage,
                   trx_context.billed_ram_bytes, trace->storage_bytes);
               pending->_pending_block_state->trxs.emplace_back(trx);
            } else {
               transaction_receipt_header r;
               r.status = transaction_receipt::executed;
               r.cpu_usage_us = trx_context.billed_cpu_time_us;
               r.net_usage_words = trace->net_usage / 8;
               r.ram_kbytes = trx_context.billed_ram_bytes >> 10;
               r.storage_kbytes = trace->storage_bytes >> 10;
               trace->receipt = r;
            }

            fc::move_append(pending->_actions, move(trx_context.executed));

            fc::optional<std::pair<transaction_trace_ptr, transaction_metadata_ptr>> nested_info;
            if (trx_context.nested_trx) {
               EOS_ASSERT(!trx->implicit, transaction_exception, "implicit trx can't start nested trx");
               trace->sent_nested = true;
               nested_info = push_nested_trx(trx_context, deadline, billed, valid_maybe_nested);
            }

            // call the accept signal but only once for this transaction
            if (!trx->accepted) {
               trx->accepted = true;
               emit( self.accepted_transaction, trx);
               if (nested_info) {
                  emit(self.accepted_transaction, nested_info->second);
               }
            }

            emit(self.applied_transaction, trace);
            if (nested_info) {
               emit(self.applied_transaction, nested_info->first);
            }


            if ( read_mode != db_read_mode::SPECULATIVE && pending->_block_status == controller::block_status::incomplete ) {
               //this may happen automatically in destructor, but I prefere make it more explicit
               trx_context.undo();
            } else {
               restore.cancel();
               trx_context.squash();
            }

            if (!trx->implicit) {
               unapplied_transactions.erase( trx->signed_id );
            }
            return trace;
         } catch (const guard_exception& e) {
            throw;
         } catch (const fc::exception& e) {
            trace->except = e;
            trace->except_ptr = std::current_exception();
         }

         if (!controller::failure_is_subjective(*trace->except)) {
            unapplied_transactions.erase( trx->signed_id );
         }

         emit( self.accepted_transaction, trx );
         emit( self.applied_transaction, trace );

         return trace;
      } FC_CAPTURE_AND_RETHROW((trace))
   } /// push_transaction

   std::pair<transaction_trace_ptr, transaction_metadata_ptr> push_nested_trx(
      const transaction_context& trx_context,
      fc::time_point deadline,
      const billed_bw_usage& billed,
      bool valid_maybe_nested
   ) {
      signed_transaction trx{transaction{*trx_context.nested_trx}, vector<signature_type>{}, vector<bytes>{}};
      EOS_ASSERT(trx.delay_sec.value == 0, transaction_exception, "SYSTEM: nested trx delay != 0");
      const auto nested_id = trx.id();
      transaction_context nested_ctx{self, trx, nested_id}; // TODO: fix `start` arg
      bool explicit_usage = billed.explicit_usage;
      transaction_receipt_header usage_receipt; // contains 0 for cpu/ram
      if (explicit_usage) {
         EOS_ASSERT(valid_maybe_nested, transaction_exception, "SYSTEM: no explicit receipts for nested trx");
         const auto& itr = maybe_nested_receipts.find(nested_id);
         if (itr == maybe_nested_receipts.end()) {
            // Workaround for tests only (which use explicit billing for normal trxs). TODO: fix
            const auto& itr2 = maybe_nested_receipts.find(transaction_id_type{});
            EOS_ASSERT(itr2 != maybe_nested_receipts.end(),
               transaction_exception, "SYSTEM: no explicit receipt found for nested trx");
            usage_receipt = itr2->second;
         } else {
            // EOS_ASSERT(itr != maybe_nested_receipts.end(),
            //    transaction_exception, "SYSTEM: no explicit receipt found for nested trx");
            usage_receipt = itr->second;
         }
      }
      nested_ctx.delay = fc::seconds(trx.delay_sec);
      if ((bool)subjective_cpu_leeway && pending->_block_status == controller::block_status::incomplete) {
         nested_ctx.leeway = *subjective_cpu_leeway;
      }
      nested_ctx.caller_set_deadline = deadline; // set the same deadline. outer will stop processing. TODO: doublecheck
      nested_ctx.explicit_billed_cpu_time = explicit_usage;
      nested_ctx.billed_cpu_time_us = usage_receipt.cpu_usage_us;
      nested_ctx.explicit_billed_ram_bytes = explicit_usage;
      nested_ctx.billed_ram_bytes = usage_receipt.ram_kbytes << 10;
      transaction_trace_ptr trace = nested_ctx.trace;
      trace->nested = true;

      nested_ctx.is_nested = true;
      nested_ctx.storage_providers = fc::flat_map<account_name, account_name>{trx_context.storage_providers};
      // nested_ctx.bill_to_accounts = fc::flat_set<account_name>{trx_context.bill_to_accounts};
      const auto& cfg = self.get_global_properties().configuration;
      auto initial_net_usage = static_cast<uint64_t>(cfg.base_per_transaction_net_usage)
         + static_cast<uint64_t>(config::transaction_id_net_usage);
      nested_ctx.init_for_implicit_trx(initial_net_usage); // can reuse implicit initializer
      nested_ctx.record_transaction(nested_id, trx.expiration); /// checks for dupes

      nested_ctx.exec();
      nested_ctx.finalize();

      trace->receipt = push_receipt(nested_id, transaction_receipt::executed,
         nested_ctx.billed_cpu_time_us, trace->net_usage, nested_ctx.billed_ram_bytes, trace->storage_bytes);

      transaction_metadata_ptr meta = std::make_shared<transaction_metadata>(trx);
      meta->nested = true;
      pending->_pending_block_state->trxs.emplace_back(meta);
      fc::move_append(pending->_actions, move(nested_ctx.executed));

      nested_ctx.squash();
      return std::make_pair(trace, meta);
   }


   void start_block( block_timestamp_type when, uint16_t confirm_block_count, controller::block_status s,
                     const optional<block_id_type>& producer_block_id )
   {
      EOS_ASSERT( !pending, block_validate_exception, "pending block already exists" );

      auto guard_pending = fc::make_scoped_exit([this](){
         pending.reset();
      });

      if (!self.skip_db_sessions(s)) {
         EOS_ASSERT(chaindb.revision() == head->block_num, database_exception, "chaibdb revision is not on par with head block",
                     ("chaindb.revision()", chaindb.revision())("controller_head_block", head->block_num)("fork_db_head_block", fork_db.head()->block_num) );

         pending.emplace(maybe_session(chaindb));
      } else {
         pending.emplace(maybe_session());
      }

      pending->_block_status = s;
      pending->_producer_block_id = producer_block_id;
      pending->_pending_block_state = std::make_shared<block_state>( *head, when ); // promotes pending schedule (if any) to active
      pending->_pending_block_state->in_current_chain = true;

      pending->_pending_block_state->set_confirmed(confirm_block_count);

      auto was_pending_promoted = pending->_pending_block_state->update_active_schedule();

      //modify state in speculative block only if we are speculative reads mode (other wise we need clean state for head or irreversible reads)
      if ( read_mode == db_read_mode::SPECULATIVE || pending->_block_status != controller::block_status::incomplete ) {

         auto global_table = chaindb.get_table<global_property_object>();
         const auto& gpo = global_table.get();
         if( gpo.proposed_schedule_block_num.valid() && // if there is a proposed schedule that was proposed in a block ...
             ( *gpo.proposed_schedule_block_num <= pending->_pending_block_state->dpos_irreversible_blocknum ) && // ... that has now become irreversible ...
             pending->_pending_block_state->pending_schedule.producers.size() == 0 && // ... and there is room for a new pending schedule ...
             !was_pending_promoted // ... and not just because it was promoted to active at the start of this block, then:
         )
            {
               // Promote proposed schedule to pending schedule.
               if( !replaying ) {
                  ilog( "promoting proposed schedule (set in block ${proposed_num}) to pending; current block: ${n} lib: ${lib} schedule: ${schedule} ",
                        ("proposed_num", *gpo.proposed_schedule_block_num)("n", pending->_pending_block_state->block_num)
                        ("lib", pending->_pending_block_state->dpos_irreversible_blocknum)
                        ("schedule", static_cast<producer_schedule_type>(gpo.proposed_schedule) ) );
               }
               pending->_pending_block_state->set_new_producers( gpo.proposed_schedule );
               global_table.modify( gpo, [&]( auto& gp ) {
                     gp.proposed_schedule_block_num = optional<block_num_type>();
                     gp.proposed_schedule.clear();
                  });
            }

         push_sys_transaction(config::system_account_name, N(onblock), fc::raw::pack(self.head_block_header()));

         clear_expired_input_transactions();
         update_producers_authority();
      }

      guard_pending.cancel();
   } // start_block



   void sign_block( const std::function<signature_type( const digest_type& )>& signer_callback  ) {
      auto p = pending->_pending_block_state;

      p->sign( signer_callback );

      static_cast<signed_block_header&>(*p->block) = p->header;
   } /// sign_block

   void apply_block( const signed_block_ptr& b, controller::block_status s ) { try {
      try {
         EOS_ASSERT( b->block_extensions.size() == 0, block_validate_exception, "no supported extensions" );
         auto producer_block_id = b->id();
         start_block( b->timestamp, b->confirmed, s , producer_block_id);

         maybe_nested_receipts.clear();
         std::vector<transaction_metadata_ptr> packed_transactions;
         packed_transactions.reserve( b->transactions.size() );
         for( const auto& receipt : b->transactions ) {
            if( receipt.trx.contains<packed_transaction>()) {
               auto& pt = receipt.trx.get<packed_transaction>();
               auto mtrx = std::make_shared<transaction_metadata>( std::make_shared<packed_transaction>( pt ) );
               if( !self.skip_auth_check() ) {
                  transaction_metadata::start_recover_keys( mtrx, thread_pool, chain_id, microseconds::maximum() );
               }
               packed_transactions.emplace_back( std::move( mtrx ) );
            } else if (receipt.trx.contains<transaction_id_type>()) {
               auto& id = receipt.trx.get<transaction_id_type>();
               maybe_nested_receipts.emplace(id, receipt);
            }
         }

         transaction_trace_ptr trace;

         optional<transaction_id_type> wait_nested;
         size_t packed_idx = 0;
         for( const auto& receipt : b->transactions ) {
            auto num_pending_receipts = pending->_pending_block_state->block->transactions.size();
            if( receipt.trx.contains<packed_transaction>() ) {
               EOS_ASSERT(!wait_nested, block_validate_exception, "expected nested trx receipt");
               trace = push_transaction(packed_transactions.at(packed_idx++), fc::time_point::maximum(), {receipt}, true);
            } else if( receipt.trx.contains<transaction_id_type>() ) {
               auto& id = receipt.trx.get<transaction_id_type>();
               if (wait_nested) {
                  EOS_ASSERT(id == wait_nested, block_validate_exception, "encountered unexpected nested trx_id");
                  wait_nested.reset();
                  continue;
               }
               trace = push_scheduled_transaction(id, fc::time_point::maximum(), {receipt} );
            } else {
               EOS_ASSERT( false, block_validate_exception, "encountered unexpected receipt type" );
            }

            bool transaction_failed =  trace && trace->except;
            bool transaction_can_fail = receipt.status == transaction_receipt_header::hard_fail && receipt.trx.contains<transaction_id_type>();
            if( transaction_failed && !transaction_can_fail) {
               edump((*trace));
               throw *trace->except;
            }

            EOS_ASSERT( pending->_pending_block_state->block->transactions.size() > 0,
                        block_validate_exception, "expected a receipt",
                        ("block", *b)("expected_receipt", receipt)
                      );
            const auto trx_count = pending->_pending_block_state->block->transactions.size();
            const bool got_nested = trx_count == num_pending_receipts + 2 && trace->sent_nested;
            EOS_ASSERT( pending->_pending_block_state->block->transactions.size() == num_pending_receipts + 1
                        || got_nested,
                        block_validate_exception, "expected receipt was not added",
                        ("block", *b)("expected_receipt", receipt)
                      );
            if (got_nested) {
               const transaction_receipt& nested = pending->_pending_block_state->block->transactions.back();
               EOS_ASSERT(nested.trx.contains<transaction_id_type>(), block_validate_exception,
                  "encountered unexpected nested trx receipt type");
               const auto& id = nested.trx.get<transaction_id_type>();
               EOS_ASSERT(maybe_nested_receipts.count(id) > 0, block_validate_exception,
                  "encountered unexpected nested trx id");
               const auto& nested_receipt = maybe_nested_receipts[id];
               EOS_ASSERT(nested_receipt == static_cast<const transaction_receipt_header&>(nested),
                  block_validate_exception, "nested receipt does not match",
                  ("producer_receipt", nested_receipt)("validator_receipt", nested));
               wait_nested = id;
               maybe_nested_receipts.erase(id);
            }
            const transaction_receipt_header& r = got_nested
               ? pending->_pending_block_state->block->transactions[num_pending_receipts]
               : pending->_pending_block_state->block->transactions.back();
            EOS_ASSERT( r == static_cast<const transaction_receipt_header&>(receipt),
                        block_validate_exception, "receipt does not match",
                        ("producer_receipt", receipt)("validator_receipt", pending->_pending_block_state->block->transactions.back()) );
         }
         EOS_ASSERT(!wait_nested, block_validate_exception, "out of receipts while expected nested trx receipt");

         finalize_block();

         // this implicitly asserts that all header fields (less the signature) are identical
         EOS_ASSERT(producer_block_id == pending->_pending_block_state->header.id(),
                   block_validate_exception, "Block ID does not match",
                   ("producer_block_id",producer_block_id)("validator_block_id",pending->_pending_block_state->header.id())
                   ("producer_block", static_cast<const block_header&>(*b))
                   ("validator_block", static_cast<const block_header&>(pending->_pending_block_state->header))
                   ("num", b->block_num()));

         // We need to fill out the pending block state's block because that gets serialized in the reversible block log
         // in the future we can optimize this by serializing the original and not the copy

         // we can always trust this signature because,
         //   - prior to apply_block, we call fork_db.add which does a signature check IFF the block is untrusted
         //   - OTHERWISE the block is trusted and therefore we trust that the signature is valid
         // Also, as ::sign_block does not lazily calculate the digest of the block, we can just short-circuit to save cycles
         pending->_pending_block_state->header.producer_signature = b->producer_signature;
         static_cast<signed_block_header&>(*pending->_pending_block_state->block) =  pending->_pending_block_state->header;

         commit_block(false);
         return;
      } catch ( const fc::exception& e ) {
         edump((e.to_detail_string()));
         abort_block();
         throw;
      }
   } FC_CAPTURE_AND_RETHROW() } /// apply_block

   std::future<block_state_ptr> create_block_state_future( const signed_block_ptr& b ) {
      EOS_ASSERT( b, block_validate_exception, "null block" );

      auto id = b->id();

      // no reason for a block_state if fork_db already knows about block
      auto existing = fork_db.get_block( id );
      EOS_ASSERT( !existing, fork_database_exception, "we already know about this block: ${id}", ("id", id) );

      auto prev = fork_db.get_block( b->previous );
      EOS_ASSERT( prev, unlinkable_block_exception, "unlinkable block ${id}", ("id", id)("previous", b->previous) );

      return async_thread_pool( thread_pool, [b, prev]() {
         const bool skip_validate_signee = false;
         return std::make_shared<block_state>( *prev, move( b ), skip_validate_signee );
      } );
   }

   void push_block( std::future<block_state_ptr>& block_state_future ) {
      controller::block_status s = controller::block_status::complete;
      EOS_ASSERT(!pending, block_validate_exception, "it is not valid to push a block when there is a pending block");

      auto reset_prod_light_validation = fc::make_scoped_exit([old_value=trusted_producer_light_validation, this]() {
         trusted_producer_light_validation = old_value;
      });
      try {
         block_state_ptr new_header_state = block_state_future.get();
         auto& b = new_header_state->block;

         emit( self.pre_accepted_block, b );

         fork_db.add( new_header_state, false );

         if (conf.trusted_producers.count(b->producer)) {
            trusted_producer_light_validation = true;
         };
         emit( self.accepted_block_header, new_header_state );

         if ( read_mode != db_read_mode::IRREVERSIBLE ) {
            maybe_switch_forks( s );
         }

      } FC_LOG_AND_RETHROW( )
   }

   void replay_push_block( const signed_block_ptr& b, controller::block_status s ) {
      self.validate_db_available_size();
      self.validate_reversible_available_size();

      EOS_ASSERT(!pending, block_validate_exception, "it is not valid to push a block when there is a pending block");

      try {
         EOS_ASSERT( b, block_validate_exception, "trying to push empty block" );
         EOS_ASSERT( (s == controller::block_status::irreversible || s == controller::block_status::validated),
                     block_validate_exception, "invalid block status for replay" );
         emit( self.pre_accepted_block, b );
         const bool skip_validate_signee = !conf.force_all_checks;
         auto new_header_state = fork_db.add( b, skip_validate_signee );

         emit( self.accepted_block_header, new_header_state );

         if ( read_mode != db_read_mode::IRREVERSIBLE ) {
            maybe_switch_forks( s );
         }

         // on replay irreversible is not emitted by fork database, so emit it explicitly here
         if( s == controller::block_status::irreversible )
            emit( self.irreversible_block, new_header_state );

      } FC_LOG_AND_RETHROW( )
   }

   void prepare_lost_transactions(const block_state_ptr& head) {
       if (skip_bad_blocks_check) {
           return;
       }

       // https://github.com/cyberway/cyberway/issues/1094

       // Transactions from this methods will be rollbacks in prepare_block_with_bad_receipt()

       struct lost_transaction final {
           transaction_id_type id;
           std::string data;
       };

       struct lost_block_transaction final {
           block_id_type id;
           std::vector<lost_transaction> transactions;
       };

       static fc::flat_map<uint32_t, lost_block_transaction> blocks_with_lost_trxs = {
           { uint32_t(189001), {
               block_id_type("0002e24956da83fb638155a409e4fffde3f6ca10df847c50da5f22d634ded6b2"),
               { {
                   transaction_id_type("5e0b6aaaa9cf980ef8af6a31f9fc9164b8529827a129c80fce6ce50a16a75a08"),
                 R"(
{
    "id": "5e0b6aaaa9cf980ef8af6a31f9fc9164b8529827a129c80fce6ce50a16a75a08"
    "expiration": "2019-08-22T05:19:09.000",
    "ref_block_num": 57924,
    "ref_block_prefix": 1446147132,
    "actions": [{
        "account": "gls.publish",
        "name": "upvote",
        "authorization": [{
            "actor": "rtvmqvz3i1vt",
            "permission": "posting"
        }],
        "data": "907770e36f2b77be900a7cce5278777922666f746f6f6b686f74612d70746963792d3437352d313536363435313039303535320100"
    }],
}
                     )"
               } }
           } },
       };

       auto itr = blocks_with_lost_trxs.find(head->block_num);
       if (blocks_with_lost_trxs.end() == itr) {
           return;
       }

       for (auto& t: itr->second.transactions) {
           signed_transaction trn;
           fc::from_variant(fc::json::from_string(t.data), trn);

           transaction_context trx_context(self, trn, t.id, fc::time_point::now());
           trx_context.caller_set_deadline = fc::time_point::maximum();
           trx_context.explicit_billed_cpu_time = true;
           trx_context.billed_cpu_time_us = 100;
           trx_context.explicit_billed_ram_bytes = true;
           trx_context.billed_ram_bytes = 100;
           trx_context.init_for_input_trx( 100, 100, true);

           trx_context.exec();
           trx_context.finalize();
           trx_context.squash();
       }
   }

   void prepare_block_with_bad_receipt(const block_state_ptr& head, controller::block_status s) {
       if (skip_bad_blocks_check) {
           return;
       }

       // https://github.com/cyberway/cyberway/issues/1094

       // This method create a pending block with transaction from block with bad receipt and rollback it

       struct bad_receipt_block final {
           block_id_type id;
           size_t transaction_limit = 10240;
       };

       static fc::flat_map<uint32_t, bad_receipt_block> blocks_with_bad_receipt = {
           { uint32_t(121492), { block_id_type("0001da94bc481aea6ea437e20cc8e0460fb3a7cee597b95569dc536cd71e9c8a") } },
           { uint32_t(129069), { block_id_type("0001f82d403d7c6545bd8c79899f497be7fdb4a3bf5ed0c1fbb43357838f31a4") } },
           { uint32_t(189001), { block_id_type("0002e24956da83fb638155a409e4fffde3f6ca10df847c50da5f22d634ded6b2") } },
           { uint32_t(195902), { block_id_type("0002fd3ea5bb7387278fbacbbdba531da54ef0d8af72a1e085ecf35b62c5d8da"), 2 } },
       };

       auto itr = blocks_with_bad_receipt.find(head->block_num);
       if (blocks_with_bad_receipt.end() == itr) {
           return;
       }

       EOS_ASSERT( head->id == itr->second.id, block_validate_exception,
           "Bad ID (${head} != ${valid}) for the block ${num}",
           ("head", head->id)("valid", itr->second.id)("num", head->block_num) );

       auto skip_sessions = self.skip_db_sessions(s);
       if (skip_sessions) {
           set_revision(head->block_num - 1);
       }

       chaindb.enable_rev_bad_update();

       auto& b = head->block;
       auto  b_time = block_timestamp_type(b->timestamp.to_time_point() - fc::seconds(3));
       start_block( b_time, b->confirmed, controller::block_status::incomplete, optional<block_id_type>() );

       std::vector<transaction_metadata_ptr> packed_transactions;
       packed_transactions.reserve( b->transactions.size() );
       for( const auto& receipt : b->transactions ) {
           if( receipt.trx.contains<packed_transaction>()) {
               auto& pt = receipt.trx.get<packed_transaction>();
               auto mtrx = std::make_shared<transaction_metadata>( std::make_shared<packed_transaction>( pt ) );
               if( !self.skip_auth_check() ) {
                   transaction_metadata::start_recover_keys( mtrx, thread_pool, chain_id, microseconds::maximum() );
               }
               packed_transactions.emplace_back( std::move( mtrx ) );
           }
       }


       size_t packed_idx = 0;
       for( const auto& receipt : b->transactions ) {
           if (receipt.trx.contains<packed_transaction>()) {
               push_transaction(packed_transactions.at(packed_idx++), fc::time_point::maximum(), {receipt});
               if (packed_idx >= itr->second.transaction_limit) {
                   break;
               }
           } else {
               EOS_ASSERT(false, block_validate_exception, "encountered unexpected receipt type");
           }
       }

       prepare_lost_transactions(head);

       chaindb.apply_all_changes();
       pending.reset();
       chaindb.apply_all_changes();
       chaindb.disable_rev_bad_update();

       if (skip_sessions) {
           set_revision(head->block_num);
       }
   }

   void maybe_switch_forks( controller::block_status s ) {
      auto new_head = fork_db.head();

      prepare_block_with_bad_receipt(new_head, s);

      if( new_head->header.previous == head->id ) {
         try {
            apply_block( new_head->block, s );
            fork_db.mark_in_current_chain( new_head, true );
            fork_db.set_validity( new_head, true );
            head = new_head;
         } catch ( const fc::exception& e ) {
            fork_db.set_validity( new_head, false ); // Removes new_head from fork_db index, so no need to mark it as not in the current chain.
            throw;
         }
      } else if( new_head->id != head->id ) {
         ilog("switching forks from ${current_head_id} (block number ${current_head_num}) to ${new_head_id} (block number ${new_head_num})",
              ("current_head_id", head->id)("current_head_num", head->block_num)("new_head_id", new_head->id)("new_head_num", new_head->block_num) );
         auto branches = fork_db.fetch_branch_from( new_head->id, head->id );

         for( auto itr = branches.second.begin(); itr != branches.second.end(); ++itr ) {
            fork_db.mark_in_current_chain( *itr, false );
            pop_block();
         }
         EOS_ASSERT( self.head_block_id() == branches.second.back()->header.previous, fork_database_exception,
                     "loss of sync between fork_db and chainbase during fork switch" ); // _should_ never fail

         for( auto ritr = branches.first.rbegin(); ritr != branches.first.rend(); ++ritr ) {
            optional<fc::exception> except;
            try {
               apply_block( (*ritr)->block, (*ritr)->validated ? controller::block_status::validated : controller::block_status::complete );
               head = *ritr;
               fork_db.mark_in_current_chain( *ritr, true );
               (*ritr)->validated = true;
            } catch (const guard_exception& e ) {
               throw;
            } catch (const fc::exception& e) {
               except = e;
            }
            if (except) {
               elog("exception thrown while switching forks ${e}", ("e", except->to_detail_string()));

               // ritr currently points to the block that threw
               // if we mark it invalid it will automatically remove all forks built off it.
               fork_db.set_validity( *ritr, false );

               // pop all blocks from the bad fork
               // ritr base is a forward itr to the last block successfully applied
               auto applied_itr = ritr.base();
               for( auto itr = applied_itr; itr != branches.first.end(); ++itr ) {
                  fork_db.mark_in_current_chain( *itr, false );
                  pop_block();
               }
               EOS_ASSERT( self.head_block_id() == branches.second.back()->header.previous, fork_database_exception,
                           "loss of sync between fork_db and chainbase during fork switch reversal" ); // _should_ never fail

               // re-apply good blocks
               for( auto ritr = branches.second.rbegin(); ritr != branches.second.rend(); ++ritr ) {
                  apply_block( (*ritr)->block, controller::block_status::validated /* we previously validated these blocks*/ );
                  head = *ritr;
                  fork_db.mark_in_current_chain( *ritr, true );
               }
               throw *except;
            } // end if exception
         } /// end for each block in branch
         ilog("successfully switched fork to new head ${new_head_id}", ("new_head_id", new_head->id) );
      }
   } /// push_block

   void abort_block() {
      if( pending ) {
         if ( read_mode == db_read_mode::SPECULATIVE ) {
            for( const auto& t : pending->_pending_block_state->trxs )
               unapplied_transactions[t->signed_id] = t;
         }
         pending.reset();
      }
   }


   bool should_enforce_runtime_limits()const {
      return false;
   }

   void set_action_merkle() {
      vector<digest_type> action_digests;
      action_digests.reserve( pending->_actions.size() );
      for( const auto& a : pending->_actions )
         action_digests.emplace_back( a.digest() );

      pending->_pending_block_state->header.action_mroot = merkle( move(action_digests) );
   }

   void set_trx_merkle() {
      vector<digest_type> trx_digests;
      const auto& trxs = pending->_pending_block_state->block->transactions;
      trx_digests.reserve( trxs.size() );
      for( const auto& a : trxs )
         trx_digests.emplace_back( a.digest() );

      pending->_pending_block_state->header.transaction_mroot = merkle( move(trx_digests) );
   }


   void finalize_block()
   {
      EOS_ASSERT(pending, block_validate_exception, "it is not valid to finalize when there is no pending block");
      try {


      /*
      ilog( "finalize block ${n} (${id}) at ${t} by ${p} (${signing_key}); schedule_version: ${v} lib: ${lib} #dtrxs: ${ndtrxs} ${np}",
            ("n",pending->_pending_block_state->block_num)
            ("id",pending->_pending_block_state->header.id())
            ("t",pending->_pending_block_state->header.timestamp)
            ("p",pending->_pending_block_state->header.producer)
            ("signing_key", pending->_pending_block_state->block_signing_key)
            ("v",pending->_pending_block_state->header.schedule_version)
            ("lib",pending->_pending_block_state->dpos_irreversible_blocknum)
            ("ndtrxs",db.get_index<generated_transaction_multi_index,by_trx_id>().size())
            ("np",pending->_pending_block_state->header.new_producers)
            );
      */
      resource_limits.set_limit_params(self.get_global_properties().configuration);
      resource_limits.process_block_usage(self.pending_block_slot());

      set_action_merkle();
      set_trx_merkle();

      auto p = pending->_pending_block_state;
      p->id = p->header.id();

      create_block_summary(p->id);

   } FC_CAPTURE_AND_RETHROW() }

   void update_producers_authority() {
      const auto& producers = pending->_pending_block_state->active_schedule.producers;

      auto update_permission = [&]( auto& permission, auto threshold ) {
         auto auth = authority( threshold, {}, {});
         for( auto& p : producers ) {
            auth.accounts.push_back({{p.producer_name, config::active_name}, 1});
         }

         if( static_cast<authority>(permission.auth) != auth ) { // TODO: use a more efficient way to check that authority has not changed
            chaindb.modify(permission, [&]( auto& po ) {
               po.auth = auth;
            });
         }
      };

      uint32_t num_producers = producers.size();
      auto calculate_threshold = [=]( uint32_t numerator, uint32_t denominator ) {
         return ( (num_producers * numerator) / denominator ) + 1;
      };

      update_permission( authorization.get_permission({config::producers_account_name,
                                                       config::active_name}),
                         calculate_threshold( 2, 3 ) /* more than two-thirds */                      );

      update_permission( authorization.get_permission({config::producers_account_name,
                                                       config::majority_producers_permission_name}),
                         calculate_threshold( 1, 2 ) /* more than one-half */                        );

      update_permission( authorization.get_permission({config::producers_account_name,
                                                       config::minority_producers_permission_name}),
                         calculate_threshold( 1, 3 ) /* more than one-third */                       );

      //TODO: Add tests
   }

   void create_block_summary(const block_id_type& id) {
      auto block_num = block_header::num_from_id(id);
      auto sid = block_num & 0xffff;
      auto blocksum_table = chaindb.get_table<block_summary_object>();
      blocksum_table.modify( blocksum_table.get(sid), [&](block_summary_object& bso ) {
          bso.block_id = id;
      });
   }


   void clear_expired_input_transactions() {
      //Look for expired transactions in the deduplication list, and remove them.
      auto dedupe_idx = chaindb.get_index<transaction_object, by_expiration>();
      auto now = self.pending_block_time();
      for (auto itr = dedupe_idx.begin(), etr = dedupe_idx.end(); etr != itr; ) {
         auto& trx = *itr;
         if (now <= fc::time_point(trx.expiration)) break;

         ++itr;
         dedupe_idx.erase(trx);
      }
   }

   void check_actor_list( const flat_set<account_name>& actors )const {
      // CYBERWAY: whitelist/blacklist removed
   }

   void check_contract_list( account_name code )const {
      // CYBERWAY: whitelist/blacklist removed
   }

   void check_action_list( account_name code, action_name action )const {
      // CYBERWAY: blacklist removed
   }

   void check_key_list( const public_key_type& key )const {
      // CYBERWAY: blacklist removed
   }

   /*
   bool should_check_tapos()const { return true; }

   void validate_tapos( const transaction& trx )const {
      if( !should_check_tapos() ) return;

      const auto& tapos_block_summary = db.get<block_summary_object>((uint16_t)trx.ref_block_num);

      //Verify TaPoS block summary has correct ID prefix, and that this block's time is not past the expiration
      EOS_ASSERT(trx.verify_reference_block(tapos_block_summary.block_id), invalid_ref_block_exception,
                 "Transaction's reference block did not match. Is this transaction from a different fork?",
                 ("tapos_summary", tapos_block_summary));
   }
   */


   /**
    *  At the start of each block we notify the system contract with a transaction that passes in
    *  the block header of the prior block (which is currently our head block)
    */

    signed_transaction get_sys_transaction(account_name account, action_name name, const bytes& data) {
        signed_transaction trx;
        trx.actions.emplace_back(action(
            vector<permission_level>{{account, config::active_name}}, account, name, data));
        trx.set_reference_block(self.head_block_id());
        trx.expiration = self.pending_block_time() + fc::microseconds(999'999); // Round up to nearest second to avoid appearing expired
        return trx;
    }
   
    void push_sys_transaction(account_name account, action_name name, const bytes& data) {
        try {
            auto onbtrx = std::make_shared<transaction_metadata>(get_sys_transaction(account, name, data));
            onbtrx->implicit = true;
            auto reset_in_trx_requiring_checks = fc::make_scoped_exit([old_value=in_trx_requiring_checks,this](){
                  in_trx_requiring_checks = old_value;
               });
            in_trx_requiring_checks = true;
            auto trace = push_transaction( onbtrx, fc::time_point::maximum(), { self.get_global_properties().configuration } );

            if(trace && trace->except) {
                edump((*trace));
                throw *trace->except;
            }
        } catch( const boost::interprocess::bad_alloc& e  ) {
            elog( "system transaction failed due to a bad allocation" );
            throw;
        } catch( const guard_exception& e ) {
            elog( "system transaction failed due to a guard_exception" );
            throw;
        } catch( const fc::exception& e ) {
            wlog( "system transaction failed, but shouldn't impact block generation, system contract needs update" );
            edump((e.to_detail_string()));
        } catch( ... ) {
            wlog( "system transaction failed" );
        }
    }

}; /// controller_impl

const resource_limits_manager&   controller::get_resource_limits_manager()const
{
   return my->resource_limits;
}
resource_limits_manager&         controller::get_mutable_resource_limits_manager()
{
   return my->resource_limits;
}

const authorization_manager&   controller::get_authorization_manager()const
{
   return my->authorization;
}
authorization_manager&         controller::get_mutable_authorization_manager()
{
   return my->authorization;
}

controller::controller( const controller::config& cfg )
:my( new controller_impl( cfg, *this ) )
{
}

controller::~controller() {
   my->abort_block();
   //close fork_db here, because it can generate "irreversible" signal to this controller,
   //in case if read-mode == IRREVERSIBLE, we will apply latest irreversible block
   //for that we need 'my' to be valid pointer pointing to valid controller_impl.
   my->fork_db.close();
}

void controller::add_indices() {
   my->add_indices();
}

void controller::startup( std::function<bool()> shutdown, snapshot_reader_ptr snapshot ) {
   my->head = my->fork_db.head();
   if( snapshot ) {
      ilog( "Starting initialization from snapshot, this may take a significant amount of time" );
   } else if( !my->head ) {
      elog( "No head block in fork db, perhaps we need to replay" );
   }

   try {
      my->init(shutdown, std::move(snapshot));
   } catch (boost::interprocess::bad_alloc& e) {
      if ( snapshot )
         elog( "db storage not configured to have enough storage for the provided snapshot, please increase and retry snapshot" );
      throw e;
   }
// TODO: Removed by CyberWay
//   if( snapshot ) {
//      ilog( "Finished initialization from snapshot" );
//   }
}

fork_database& controller::fork_db() const { return my->fork_db; }

chaindb_controller& controller::chaindb() const { return my->chaindb; }


void controller::start_block( block_timestamp_type when, uint16_t confirm_block_count) {
   validate_db_available_size();
   my->start_block(when, confirm_block_count, block_status::incomplete, optional<block_id_type>() );
}

void controller::finalize_block() {
   validate_db_available_size();
   my->finalize_block();
}

void controller::sign_block( const std::function<signature_type( const digest_type& )>& signer_callback ) {
   my->sign_block( signer_callback );
}

void controller::commit_block() {
   validate_db_available_size();
   validate_reversible_available_size();
   my->commit_block(true);
}

void controller::abort_block() {
   my->abort_block();
}

boost::asio::thread_pool& controller::get_thread_pool() {
   return my->thread_pool;
}

std::future<block_state_ptr> controller::create_block_state_future( const signed_block_ptr& b ) {
   return my->create_block_state_future( b );
}

void controller::push_block( std::future<block_state_ptr>& block_state_future ) {
   validate_db_available_size();
   validate_reversible_available_size();
   my->push_block( block_state_future );
}

transaction_trace_ptr controller::push_transaction(const transaction_metadata_ptr& trx, fc::time_point deadline,
   const billed_bw_usage& billed, bool bill_nested
) {
   validate_db_available_size();
   EOS_ASSERT( get_read_mode() != chain::db_read_mode::READ_ONLY, transaction_type_exception, "push transaction not allowed in read-only mode" );
   EOS_ASSERT( trx && !trx->implicit && !trx->scheduled, transaction_type_exception, "Implicit/Scheduled transaction not allowed" );
   EOS_ASSERT( !trx->nested, transaction_type_exception, "Nested transaction not allowed" );
   if (bill_nested) {
      std::map<transaction_id_type, transaction_receipt_header> nested_bill;
      transaction_receipt_header hdr;
      hdr.cpu_usage_us = billed.cpu_time_us;
      hdr.ram_kbytes = billed.ram_bytes >> 10;
      my->maybe_nested_receipts.clear();
      my->maybe_nested_receipts.emplace(transaction_id_type(), hdr);
   }
   return my->push_transaction(trx, deadline, billed, bill_nested);
}

transaction_trace_ptr controller::push_scheduled_transaction( const transaction_id_type& trxid, fc::time_point deadline, const billed_bw_usage& billed )
{
   validate_db_available_size();
   return my->push_scheduled_transaction( trxid, deadline, billed );
}


uint32_t controller::head_block_num()const {
   return my->head->block_num;
}
time_point controller::head_block_time()const {
   return my->head->header.timestamp;
}
block_id_type controller::head_block_id()const {
   return my->head->id;
}
account_name  controller::head_block_producer()const {
   return my->head->header.producer;
}
const block_header& controller::head_block_header()const {
   return my->head->header;
}
block_state_ptr controller::head_block_state()const {
   return my->head;
}

uint32_t controller::fork_db_head_block_num()const {
   return my->fork_db.head()->block_num;
}

block_id_type controller::fork_db_head_block_id()const {
   return my->fork_db.head()->id;
}

time_point controller::fork_db_head_block_time()const {
   return my->fork_db.head()->header.timestamp;
}

account_name  controller::fork_db_head_block_producer()const {
   return my->fork_db.head()->header.producer;
}

block_state_ptr controller::pending_block_state()const {
   if( my->pending ) return my->pending->_pending_block_state;
   return block_state_ptr();
}
time_point controller::pending_block_time()const {
   EOS_ASSERT( my->pending, block_validate_exception, "no pending block" );
   return my->pending->_pending_block_state->header.timestamp;
}
uint32_t controller::pending_block_slot()const {
   EOS_ASSERT( my->pending, block_validate_exception, "no pending block" );
   return my->pending->_pending_block_state->header.timestamp.slot;
}

optional<block_id_type> controller::pending_producer_block_id()const {
   EOS_ASSERT( my->pending, block_validate_exception, "no pending block" );
   return my->pending->_producer_block_id;
}

uint32_t controller::last_irreversible_block_num() const {
   return std::max(std::max(my->head->bft_irreversible_blocknum, my->head->dpos_irreversible_blocknum), my->snapshot_head_block);
}

block_id_type controller::last_irreversible_block_id() const {
   auto lib_num = last_irreversible_block_num();
   const auto& tapos_block_summary = chaindb().get<block_summary_object>((uint16_t)lib_num);

   if( block_header::num_from_id(tapos_block_summary.block_id) == lib_num )
      return tapos_block_summary.block_id;

   return fetch_block_by_number(lib_num)->id();

}

const dynamic_global_property_object& controller::get_dynamic_global_properties()const {
  return my->chaindb.get<dynamic_global_property_object>();
}
const global_property_object& controller::get_global_properties()const {
  return my->chaindb.get<global_property_object>();
}

signed_block_ptr controller::fetch_block_by_id( block_id_type id )const {
   auto state = my->fork_db.get_block(id);
   if( state && state->block ) return state->block;
   auto bptr = fetch_block_by_number( block_header::num_from_id(id) );
   if( bptr && bptr->id() == id ) return bptr;
   return signed_block_ptr();
}

signed_block_ptr controller::fetch_block_by_number( uint32_t block_num )const  { try {
   auto blk_state = my->fork_db.get_block_in_current_chain_by_num( block_num );
   if( blk_state && blk_state->block ) {
      return blk_state->block;
   }

   return my->blog.read_block_by_num(block_num);
} FC_CAPTURE_AND_RETHROW( (block_num) ) }

block_state_ptr controller::fetch_block_state_by_id( block_id_type id )const {
   auto state = my->fork_db.get_block(id);
   return state;
}

block_state_ptr controller::fetch_block_state_by_number( uint32_t block_num )const  { try {
   auto blk_state = my->fork_db.get_block_in_current_chain_by_num( block_num );
   return blk_state;
} FC_CAPTURE_AND_RETHROW( (block_num) ) }

block_id_type controller::get_block_id_for_num( uint32_t block_num )const { try {
   auto blk_state = my->fork_db.get_block_in_current_chain_by_num( block_num );
   if( blk_state ) {
      return blk_state->id;
   }

   auto signed_blk = my->blog.read_block_by_num(block_num);

   EOS_ASSERT( BOOST_LIKELY( signed_blk != nullptr ), unknown_block_exception,
               "Could not find block: ${block}", ("block", block_num) );

   return signed_blk->id();
} FC_CAPTURE_AND_RETHROW( (block_num) ) }

sha256 controller::calculate_integrity_hash()const { try {
   return my->calculate_integrity_hash();
} FC_LOG_AND_RETHROW() }

void controller::write_snapshot(snapshot_writer_ptr snapshot ) {
   EOS_ASSERT( !my->pending, block_validate_exception, "cannot take a consistent snapshot with a pending block" );
   snapshot_controller(my->chaindb, my->resource_limits, my->fork_db, my->reversible_blocks, my->head, my->conf.genesis).write_snapshot(std::move(snapshot));
}

void controller::pop_block() {
   my->pop_block();
}

int64_t controller::set_proposed_producers( vector<producer_key> producers ) {
   const auto& gpo = get_global_properties();
   auto cur_block_num = head_block_num() + 1;

   if( gpo.proposed_schedule_block_num.valid() ) {
      if( *gpo.proposed_schedule_block_num != cur_block_num )
         return -1; // there is already a proposed schedule set in a previous block, wait for it to become pending

      if( std::equal( producers.begin(), producers.end(),
                      gpo.proposed_schedule.producers.begin(), gpo.proposed_schedule.producers.end() ) )
         return -1; // the proposed producer schedule does not change
   }

   producer_schedule_type sch;

   decltype(sch.producers.cend()) end;
   decltype(end)                  begin;

   if( my->pending->_pending_block_state->pending_schedule.producers.size() == 0 ) {
      const auto& active_sch = my->pending->_pending_block_state->active_schedule;
      begin = active_sch.producers.begin();
      end   = active_sch.producers.end();
      sch.version = active_sch.version + 1;
   } else {
      const auto& pending_sch = my->pending->_pending_block_state->pending_schedule;
      begin = pending_sch.producers.begin();
      end   = pending_sch.producers.end();
      sch.version = pending_sch.version + 1;
   }

   if( std::equal( producers.begin(), producers.end(), begin, end ) )
      return -1; // the producer schedule would not change

   sch.producers = std::move(producers);

   int64_t version = sch.version;

   my->chaindb.modify( gpo, [&]( auto& gp ) {
      gp.proposed_schedule_block_num = cur_block_num;
      gp.proposed_schedule = std::move(sch);
   });
   return version;
}

const producer_schedule_type&    controller::active_producers()const {
   if ( !(my->pending) )
      return  my->head->active_schedule;
   return my->pending->_pending_block_state->active_schedule;
}

const producer_schedule_type&    controller::pending_producers()const {
   if ( !(my->pending) )
      return  my->head->pending_schedule;
   return my->pending->_pending_block_state->pending_schedule;
}

optional<producer_schedule_type> controller::proposed_producers()const {
   const auto& gpo = get_global_properties();
   if( !gpo.proposed_schedule_block_num.valid() )
      return optional<producer_schedule_type>();

   return gpo.proposed_schedule;
}

bool controller::light_validation_allowed(bool replay_opts_disabled_by_policy) const {
   if (!my->pending || my->in_trx_requiring_checks) {
      return false;
   }

   const auto pb_status = my->pending->_block_status;

   // in a pending irreversible or previously validated block and we have forcing all checks
   const bool consider_skipping_on_replay = (pb_status == block_status::irreversible || pb_status == block_status::validated) && !replay_opts_disabled_by_policy;

   // OR in a signed block and in light validation mode
   const bool consider_skipping_on_validate = (pb_status == block_status::complete &&
         (my->conf.block_validation_mode == validation_mode::LIGHT || my->trusted_producer_light_validation));

   return consider_skipping_on_replay || consider_skipping_on_validate;
}


bool controller::skip_auth_check() const {
   return light_validation_allowed(my->conf.force_all_checks);
}

bool controller::skip_db_sessions( block_status bs ) const {
   bool consider_skipping = bs == block_status::irreversible;
   return consider_skipping
      && !my->conf.disable_replay_opts
      && !my->in_trx_requiring_checks;
}

bool controller::skip_db_sessions( ) const {
   if (my->pending) {
      return skip_db_sessions(my->pending->_block_status);
   } else {
      return false;
   }
}

bool controller::skip_trx_checks() const {
   return light_validation_allowed(my->conf.disable_replay_opts);
}

bool controller::contracts_console()const {
   return my->conf.contracts_console;
}

chain_id_type controller::get_chain_id()const {
   return my->chain_id;
}

db_read_mode controller::get_read_mode()const {
   return my->read_mode;
}

validation_mode controller::get_validation_mode()const {
   return my->conf.block_validation_mode;
}

const apply_handler* controller::find_apply_handler( account_name receiver, account_name scope, action_name act ) const
{
   auto native_handler_scope = my->apply_handlers.find( receiver );
   if( native_handler_scope != my->apply_handlers.end() ) {
      auto handler = native_handler_scope->second.find( make_pair( scope, act ) );
      if( handler != native_handler_scope->second.end() )
         return &handler->second;
   }
   return nullptr;
}
wasm_interface& controller::get_wasm_interface() {
   return my->wasmif;
}

optional_ptr<abi_serializer> controller::get_abi_serializer( account_name n, const fc::microseconds& max_serialization_time )const {
    if( n.good() ) {
        auto a = my->chaindb.get_account_abi_info(n);
        if( a.has_abi_info() ) {
           return optional_ptr<abi_serializer>( a.abi().serializer() );
        }
    }
    return optional_ptr<abi_serializer>();
}

const account_object& controller::get_account( account_name name )const
{ try {
   return my->chaindb.get<account_object>(name);
} FC_CAPTURE_AND_RETHROW( (name) ) }

const domain_object& controller::get_domain(const domain_name& name) const { try {
    const auto* d = my->chaindb.find<domain_object, by_name>(name, cursor_kind::OneRecord);
    EOS_ASSERT(d != nullptr, chain::domain_query_exception, "domain `${name}` not found", ("name", name));
    return *d;
} FC_CAPTURE_AND_RETHROW((name)) }

const username_object& controller::get_username(account_name scope, const username& name) const { try {
    const auto* user = my->chaindb.find<username_object, by_scope_name>(boost::make_tuple(scope,name), cursor_kind::OneRecord);
    EOS_ASSERT(user != nullptr, username_query_exception,
        "username `${name}` not found in scope `${scope}`", ("name",name)("scope",scope));
    return *user;
} FC_CAPTURE_AND_RETHROW((scope)(name)) }

unapplied_transactions_type& controller::get_unapplied_transactions() {
   if ( my->read_mode != db_read_mode::SPECULATIVE ) {
      EOS_ASSERT( my->unapplied_transactions.empty(), transaction_exception,
                  "not empty unapplied_transactions in non-speculative mode" ); //should never happen
   }
   return my->unapplied_transactions;
}

void controller::check_actor_list( const flat_set<account_name>& actors )const {
   my->check_actor_list( actors );
}

void controller::check_contract_list( account_name code )const {
   my->check_contract_list( code );
}

void controller::check_action_list( account_name code, action_name action )const {
   my->check_action_list( code, action );
}

void controller::check_key_list( const public_key_type& key )const {
   my->check_key_list( key );
}

bool controller::is_producing_block()const {
   if( !my->pending ) return false;

   return (my->pending->_block_status == block_status::incomplete);
}

bool controller::is_ram_billing_in_notify_allowed()const {
   return !is_producing_block(); // || my->conf.allow_ram_billing_in_notify;
}

void controller::validate_expiration( const transaction& trx )const { try {
   const auto& chain_configuration = get_global_properties().configuration;

   EOS_ASSERT( time_point(trx.expiration) >= pending_block_time(),
               expired_tx_exception,
               "transaction has expired, "
               "expiration is ${trx.expiration} and pending block time is ${pending_block_time}",
               ("trx.expiration",trx.expiration)("pending_block_time",pending_block_time()));
   EOS_ASSERT( time_point(trx.expiration) <= pending_block_time() + fc::seconds(chain_configuration.max_transaction_lifetime),
               tx_exp_too_far_exception,
               "Transaction expiration is too far in the future relative to the reference time of ${reference_time}, "
               "expiration is ${trx.expiration} and the maximum transaction lifetime is ${max_til_exp} seconds",
               ("trx.expiration",trx.expiration)("reference_time",pending_block_time())
               ("max_til_exp",chain_configuration.max_transaction_lifetime) );
} FC_CAPTURE_AND_RETHROW((trx)) }

void controller::validate_tapos( const transaction& trx )const { try {
   const auto& tapos_block_summary = chaindb().get<block_summary_object>((uint16_t)trx.ref_block_num);

   //Verify TaPoS block summary has correct ID prefix, and that this block's time is not past the expiration
   EOS_ASSERT(trx.verify_reference_block(tapos_block_summary.block_id), invalid_ref_block_exception,
              "Transaction's reference block did not match. Is this transaction from a different fork?",
              ("tapos_summary", tapos_block_summary));
} FC_CAPTURE_AND_RETHROW() }

void controller::validate_db_available_size() const {
   // TODO: Removed by CyberWay
}

void controller::validate_reversible_available_size() const {
   const auto free = my->reversible_blocks.get_segment_manager()->get_free_memory();
   const auto guard = my->conf.reversible_guard_size;
   EOS_ASSERT(free >= guard, reversible_guard_exception, "reversible free: ${f}, guard size: ${g}", ("f", free)("g",guard));
}

bool controller::is_known_unexpired_transaction( const transaction_id_type& id) const {
   return chaindb().find<transaction_object, by_trx_id>(id, cyberway::chaindb::cursor_kind::InRAM);
}

void controller::set_subjective_cpu_leeway(fc::microseconds leeway) {
   my->subjective_cpu_leeway = leeway;
}

void controller::skip_bad_blocks_check() {
   my->skip_bad_blocks_check = true;
}

} } /// eosio::chain
