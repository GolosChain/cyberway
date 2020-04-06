#pragma once

#include <cyberway/chaindb/common.hpp>
#include <cyberway/chaindb/typed_name.hpp>

#include <fc/variant.hpp>

namespace cyberway { namespace chaindb {

    enum class undo_record {
        Unknown,
        OldValue,
        RemovedValue,
        NewValue,
        NextPk,
    }; // enum class undo_type

    struct service_state final {
        primary_key_t  pk       = primary_key::Unset;
        account_name_t payer    = 0;
        int            size     = 0;
        bool           in_ram   = true;

        account_name_t code     = 0;
        scope_name_t   scope    = 0;
        table_name_t   table    = 0;

        revision_t     revision = impossible_revision;

        // the following fields are part of undo state,
        // TODO: refactor to move them into write_operation
        primary_key_t  undo_pk  = primary_key::Unset;
        undo_record    undo_rec = undo_record::Unknown;

        revision_t     undo_revision = impossible_revision;
        account_name_t undo_payer    = 0;
        size_t         undo_size     = 0;
        bool           undo_in_ram   = true;

        service_state() = default;
        service_state(service_state&&) = default;
        service_state(const service_state&) = default;

        service_state& operator=(service_state&&) = default;
        service_state& operator=(const service_state&) = default;

        void clear() {
            *this = {};
        }

        bool empty() const {
            return 0 == scope && 0 == code && 0 == table;
        }
    }; // struct service_state

    struct object_value final {
        service_state service;
        fc::variant   value;

        bool is_null() const {
            return value.is_null();
        }

        object_value clone_service() const {
            object_value obj;
            obj.service = service;
            return obj;
        }

        void clear() {
            service.clear();
            value.clear();
        }

        primary_key_t pk() const {
            return service.pk;
        }
    }; // struct object_value


    struct reflectable_service_state {

        reflectable_service_state() = default;

        reflectable_service_state(const cyberway::chaindb::service_state& service_state) :
                pk(service_state.pk),
                payer(service_state.payer),
                size(service_state.size),
                in_ram(service_state.in_ram),
                code(service_state.code),
                scope(service_state.scope),
                table(service_state.table),
                revision(service_state.revision),
                undo_pk(service_state.undo_pk),
                undo_rec(service_state.undo_rec),
                undo_revision(service_state.undo_revision),
                undo_payer(service_state.undo_payer),
                undo_size(service_state.undo_size),
                undo_in_ram(service_state.undo_in_ram) {}

        operator cyberway::chaindb::service_state () {
            return { pk,
                     payer,
                     size,
                     in_ram,
                     code,
                     scope,
                     table,
                     revision,
                     undo_pk,
                     undo_rec,
                     undo_revision,
                     undo_payer,
                     undo_size,
                     undo_in_ram};
        }

        primary_key_t  pk;
        account_name_t payer;
        int            size;
        bool           in_ram;
        account_name_t code;
        scope_name_t   scope;
        table_name_t   table;
        revision_t     revision;
        primary_key_t  undo_pk;
        undo_record    undo_rec;
        revision_t     undo_revision;
        account_name_t undo_payer;
        size_t         undo_size;
        bool           undo_in_ram;
    }; // struct reflectable_service_state

} } // namespace cyberway::chaindb

FC_REFLECT_ENUM(cyberway::chaindb::undo_record, (Unknown)(OldValue)(RemovedValue)(NewValue)(NextPk))
FC_REFLECT(cyberway::chaindb::service_state, (payer)(size)(in_ram))
FC_REFLECT(cyberway::chaindb::reflectable_service_state, (pk)(payer)(size)(in_ram)(code)(scope)(table)(revision)(undo_pk)(undo_rec)(undo_revision)(undo_payer)(undo_size)(undo_in_ram))
