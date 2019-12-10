#pragma once

#include <fc/variant_object.hpp>
#include <fc/reflect/reflect.hpp>

#include <vector>

namespace eosio {

   struct EventData {
       chain::account_name code;
       chain::event_name   event;
       fc::variant         args;
       chain::bytes        data;
   };

   struct ActionData {
       chain::account_name receiver;
       chain::account_name code;
       chain::action_name  action;
       std::vector<chain::permission_level> auth;
       fc::variant         args;
       chain::bytes        data;
       std::vector<EventData> events;
   };

   struct TrxMetadata {
       chain::transaction_id_type id;
       bool                       accepted;
       bool                       implicit;
       bool                       scheduled;
       variant                    trx;
       chain::bytes               hex_data;
       vector<signature_type>     signatures;

       TrxMetadata(const chain::transaction_metadata_ptr &meta, const fc::variant& trx)
       : id(meta->id)
       , accepted(meta->accepted)
       , implicit(meta->implicit)
       , scheduled(meta->scheduled)
       , trx(trx)
       , hex_data(meta->packed_trx->get_raw_transaction())
       , signatures(meta->packed_trx->get_signatures())
       {}
   };

    struct TrxReceipt final {
       using status_enum = chain::transaction_receipt_header::status_enum;

       chain::transaction_id_type         id;
       fc::enum_type<uint8_t,status_enum> status;
       fc::unsigned_int                   cpu_usage_us;
       fc::unsigned_int                   net_usage_words;
       fc::unsigned_int                   ram_kbytes;
       fc::signed_int                     storage_kbytes;


       TrxReceipt(const chain::transaction_id_type &id, const chain::transaction_receipt &receipt)
       : id(id)
       , status(receipt.status)
       , cpu_usage_us(receipt.cpu_usage_us)
       , net_usage_words(receipt.net_usage_words)
       , ram_kbytes(receipt.ram_kbytes)
       , storage_kbytes(receipt.storage_kbytes)
       { }
    };

   enum class MsgChannel {
       Blocks,
       Genesis
   };

   struct BaseMessage {
       enum MsgType {
           Unknown,
           AcceptBlock,
           CommitBlock,

           AcceptTrx,
           ApplyTrx,

           GenesisData,
       };

       MsgChannel msg_channel;
       MsgType msg_type;

       BaseMessage(MsgChannel channel, MsgType type = Unknown)
       : msg_channel(channel), msg_type(type)
       {}
   };

   struct GenesisDataMessage : public BaseMessage {
       uint32_t id;
       chain::name code;
       chain::name name;
       fc::variant data;

       GenesisDataMessage(MsgChannel msg_channel, BaseMessage::MsgType msg_type
       , uint32_t msg_id, const chain::name code, const chain::name name, const fc::variant& data)
       : BaseMessage(msg_channel, msg_type)
       , id(msg_id)
       , code(code)
       , name(name)
       , data(data)
       {}
   };

   const auto core_genesis_code = N(core);

   struct AcceptTrxMessage : public BaseMessage, TrxMetadata {
       AcceptTrxMessage(MsgChannel msg_channel, BaseMessage::MsgType msg_type, const chain::transaction_metadata_ptr &trx_meta, const fc::variant &trx = fc::variant())
       : BaseMessage(msg_channel, msg_type)
       , TrxMetadata(trx_meta, trx)
       {}
   };

   struct ApplyTrxMessage : public BaseMessage {
       chain::transaction_id_type         id;
       uint32_t                           block_num;
       chain::block_timestamp_type        block_time;
       fc::optional<chain::block_id_type> prod_block_id;
       std::vector<ActionData>            actions;
       fc::optional<fc::exception>        except;

       ApplyTrxMessage(MsgChannel msg_channel, MsgType msg_type, const chain::transaction_trace_ptr& trace)
       : BaseMessage(msg_channel, msg_type)
       , id(trace->id)
       , block_num(trace->block_num)
       , block_time(trace->block_time)
       , prod_block_id(trace->producer_block_id)
       , except(trace->except)
       {}
   };

   struct BlockMessage : public BaseMessage {
       chain::block_id_type          id;
       chain::block_id_type          previous;
       account_name                  producer;
       signature_type                producer_signature;
       uint32_t                      dpos_irreversible_blocknum;
       uint32_t                      scheduled_shuffle_slot;
       uint32_t                      scheduled_slot;
       std::vector<account_name>     active_schedule;
       std::vector<account_name>     next_schedule;
       uint32_t                      block_num;
       fc::time_point                block_time;
       uint32_t                      block_slot;
       fc::time_point                next_block_time;

       BlockMessage(MsgChannel msg_channel, MsgType msg_type, const chain::block_state_ptr& bstate)
       : BaseMessage(msg_channel, msg_type)
       , id(bstate->block->id())
       , previous(bstate->header.previous)
       , producer(bstate->header.producer)
       , producer_signature(bstate->header.producer_signature)
       , dpos_irreversible_blocknum(bstate->dpos_irreversible_blocknum)
       , scheduled_shuffle_slot(bstate->scheduled_shuffle_slot)
       , block_num(bstate->block->block_num())
       , block_time(bstate->header.timestamp.to_time_point())
       , block_slot(bstate->header.timestamp.slot)
       , next_block_time(bstate->header.timestamp.to_time_point() + fc::microseconds(chain::config::block_interval_us))
       {
           auto fill_producer_names = [&](const auto& from, auto& to) {
              to.reserve(from.producers.size());
              for (const auto& producer : from.producers) {
                 to.push_back(producer.producer_name);
              }
           };
           fill_producer_names(bstate->active_schedule, active_schedule);
           auto next_sched = bstate->generate_next(next_block_time);
           next_sched.update_active_schedule();
           fill_producer_names(next_sched.active_schedule, next_schedule);

           scheduled_slot = (block_slot - (scheduled_shuffle_slot + 1)) % bstate->active_schedule.producers.size();
       }
   };

   struct AcceptedBlockMessage : public BlockMessage {
       std::vector<TrxReceipt> trxs;
       std::vector<EventData> events;

       AcceptedBlockMessage(MsgChannel msg_channel, MsgType msg_type, const chain::block_state_ptr& bstate)
       : BlockMessage(msg_channel, msg_type, bstate)
       {
           if (!bstate->block) {
               return;
           }

           trxs.reserve(bstate->block->transactions.size());
           for (auto const &receipt: bstate->block->transactions) {
               chain::transaction_id_type tid;
               if (receipt.trx.contains<chain::transaction_id_type>()) {
                   tid = receipt.trx.get<chain::transaction_id_type>();
               } else {
                   tid = receipt.trx.get<chain::packed_transaction>().id();
               }

               TrxReceipt trx_receipt(tid, receipt);
               trxs.push_back(std::move(trx_receipt));
           }
       }
   };

} // namespace eosio

FC_REFLECT(eosio::EventData, (code)(event)(data)(args))
FC_REFLECT(eosio::ActionData, (receiver)(code)(action)(auth)(data)(args)(events))
FC_REFLECT(eosio::TrxMetadata, (id)(accepted)(implicit)(scheduled)(trx)(hex_data)(signatures))
FC_REFLECT(eosio::TrxReceipt, (id)(status)(cpu_usage_us)(net_usage_words)(ram_kbytes)(storage_kbytes))

FC_REFLECT_ENUM(eosio::MsgChannel, (Blocks)(Genesis))
FC_REFLECT_ENUM(eosio::BaseMessage::MsgType, (Unknown)(GenesisData)(AcceptBlock)(CommitBlock)(AcceptTrx)(ApplyTrx))
FC_REFLECT(eosio::BaseMessage, (msg_channel)(msg_type))
FC_REFLECT_DERIVED(eosio::GenesisDataMessage, (eosio::BaseMessage), (id)(code)(name)(data))
FC_REFLECT_DERIVED(eosio::BlockMessage, (eosio::BaseMessage), (id)(previous)(producer)(producer_signature)(dpos_irreversible_blocknum)
   (scheduled_shuffle_slot)(scheduled_slot)(active_schedule)(next_schedule)(block_num)(block_time)(block_slot)(next_block_time))
FC_REFLECT_DERIVED(eosio::AcceptedBlockMessage, (eosio::BlockMessage), (trxs)(events))
FC_REFLECT_DERIVED(eosio::AcceptTrxMessage, (eosio::BaseMessage)(eosio::TrxMetadata), )
FC_REFLECT_DERIVED(eosio::ApplyTrxMessage, (eosio::BaseMessage), (id)(block_num)(block_time)(prod_block_id)(actions)(except))
