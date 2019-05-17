#pragma once

#include "golos_types.hpp"

namespace cyberway { namespace golos {

using gls_account_name_type = fc::fixed_string<>;

struct account_metadata_operation {
    gls_account_name_type account;
    std::string json_metadata;
};


} } // cyberway::golos

FC_REFLECT(cyberway::golos::account_metadata_operation, (account)(json_metadata))
