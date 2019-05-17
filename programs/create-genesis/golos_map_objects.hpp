#pragma once

#include <cyberway/genesis/golos_dump_container.hpp>
#include "golos_types.hpp"

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <chainbase/chainbase.hpp>

namespace cyberway { namespace genesis {

using namespace boost::multi_index;

using gls_account_name_type = fc::fixed_string<>;

enum object_type
{
    null_object_type = 0,
    account_metadata_object_type,
};

struct account_metadata : public chainbase::object<account_metadata_object_type, account_metadata> {
    template<typename Constructor, typename Allocator>
    account_metadata(Constructor &&c, chainbase::allocator<Allocator> a) {
        c(*this);
    }

    id_type id;
    gls_account_name_type account;
    uint64_t offset;
};

struct by_id;
struct by_account;

using account_metadata_index = chainbase::shared_multi_index_container<
    account_metadata,
    indexed_by<
        ordered_unique<
            tag<by_id>,
            member<account_metadata, account_metadata::id_type, &account_metadata::id>>,
        ordered_unique<
            tag<by_account>,
            member<account_metadata, gls_account_name_type, &account_metadata::account>>>
>;

} } // cyberway::genesis

CHAINBASE_SET_INDEX_TYPE(cyberway::genesis::account_metadata, cyberway::genesis::account_metadata_index)
