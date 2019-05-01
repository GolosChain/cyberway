#pragma once

#include "golos_types.hpp"

namespace cyberway { namespace golos {

struct comment_operation {
    uint64_t hash = 0;
    account_name_type parent_author;
    string parent_permlink;

    account_name_type author;
    string permlink;

    string title;
    string body;
    flat_set<string> tags;
};

struct delete_comment_operation {
    uint64_t hash = 0;
};

struct vote_operation {
    uint64_t hash = 0;
    account_name_type voter;
    account_name_type author;
    string permlink;
    int16_t weight = 0;
    fc::time_point_sec timestamp;
};

struct reblog_operation {
    uint64_t hash = 0;
    account_name_type account;
    account_name_type author;
    string permlink;
    string title;
    string body;
    fc::time_point_sec timestamp;
};

struct delete_reblog_operation {
    uint64_t hash = 0;
    account_name_type account;
};

struct transfer_operation {
    account_name_type from;
    /// Account to transfer asset to
    account_name_type to;
    /// The amount of asset to transfer from @ref from to @ref to
    asset amount;

    /// The memo is plain-text, any encryption on the memo is up to
    /// a higher level protocol.
    string memo;
};

struct author_reward_operation {
    account_name_type author;
    string permlink;
    asset sbd_payout;
    asset steem_payout;
    asset vesting_payout;
};

struct comment_benefactor_reward_operation {
    account_name_type benefactor;
    account_name_type author;
    string permlink;
    asset reward;
};

struct curation_reward_operation {
    account_name_type curator;
    asset reward;
    account_name_type comment_author;
    string comment_permlink;
};

struct auction_window_reward_operation {
    asset reward;
    account_name_type comment_author;
    string comment_permlink;
};

struct total_comment_reward_operation {
    uint64_t hash = 0;
    account_name_type author;
    string permlink;
    asset author_reward;
    asset benefactor_reward;
    asset curator_reward;
    int64_t net_rshares;
};

} } // cyberway::golos

FC_REFLECT(cyberway::golos::comment_operation, (hash)(parent_author)(parent_permlink)(author)(permlink)(title)(body)(tags))
FC_REFLECT(cyberway::golos::delete_comment_operation, (hash))
FC_REFLECT(cyberway::golos::vote_operation, (hash)(voter)(author)(permlink)(weight)(timestamp))
FC_REFLECT(cyberway::golos::reblog_operation, (hash)(account)(author)(permlink)(title)(body)(timestamp))
FC_REFLECT(cyberway::golos::delete_reblog_operation, (hash)(account))
FC_REFLECT(cyberway::golos::transfer_operation, (from)(to)(amount)(memo))

FC_REFLECT(cyberway::golos::author_reward_operation, (author)(permlink)(sbd_payout)(steem_payout)(vesting_payout))
FC_REFLECT(cyberway::golos::curation_reward_operation, (curator)(reward)(comment_author)(comment_permlink))
FC_REFLECT(cyberway::golos::auction_window_reward_operation, (reward)(comment_author)(comment_permlink))
FC_REFLECT(cyberway::golos::comment_benefactor_reward_operation, (benefactor)(author)(permlink)(reward))
FC_REFLECT(cyberway::golos::total_comment_reward_operation, (hash)(author)(permlink)(author_reward)(benefactor_reward)(curator_reward)
    (net_rshares))
