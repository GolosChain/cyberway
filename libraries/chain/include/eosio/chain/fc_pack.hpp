#pragma once

#include <chainbase/chainbase.hpp>
#include <cyberway/chaindb/index_object.hpp>

// Forward declaration
namespace eosio { namespace chain {
   struct asset_info;
   struct asset;
   struct symbol_code;
   struct symbol_info;
   class  symbol;
} } // namespace eosio::chain

namespace fc {
    class variant;

    template<typename T> inline void to_variant(const chainbase::oid<T>&, variant&);
    template<typename T> inline void from_variant(const variant&, chainbase::oid<T>&);
    template<typename T> inline void from_variant(const variant&, chainbase::oid<T>&);

    template<typename T> inline void to_variant(const cyberway::chaindb::oid<T>&, variant&);
    template<typename T> inline void from_variant(const variant&, cyberway::chaindb::oid<T>&);
    template<typename T> inline void from_variant(const variant&, cyberway::chaindb::oid<T>&);

    inline void to_variant(const std::vector<bool>& t, variant& vo);

    inline void from_variant(const variant&, eosio::chain::asset_info&);
    inline void to_variant(const eosio::chain::asset&, variant&);
    inline void from_variant(const fc::variant&, eosio::chain::asset&);

    inline void from_variant(const variant&, eosio::chain::symbol_info&);
    inline void to_variant(const eosio::chain::symbol&, fc::variant&);
    inline void from_variant(const fc::variant&, eosio::chain::symbol&);
    inline void to_variant(const eosio::chain::symbol_code&, fc::variant&);
    inline void from_variant(const fc::variant&, eosio::chain::symbol_code&);

    namespace raw {
        template<typename Stream, typename T> inline void pack(Stream& s, const chainbase::oid<T>& o);
        template<typename Stream, typename T> inline void unpack(Stream& s, chainbase::oid<T>& o);

        template<typename Stream, typename T> inline void pack(Stream& s, const cyberway::chaindb::oid<T>& o);
        template<typename Stream, typename T> inline void unpack(Stream& s, cyberway::chaindb::oid<T>& o);

        template<typename Stream> inline void unpack(Stream& s, std::vector<bool>& value);

        template<typename Stream> void pack(Stream&, const eosio::chain::symbol_info&);
        template<typename Stream> void unpack(Stream&, eosio::chain::symbol_info&);

        template<typename Stream> void pack(Stream&, const eosio::chain::asset_info&);
        template<typename Stream> void unpack(Stream&, eosio::chain::asset_info&);
    } // namespace raw
} // namespace fc

#include <fc/io/raw.hpp>

namespace fc {

    inline void to_variant(const std::vector<bool>& vect, variant& v) {
        std::vector<variant> vars(vect.size());
        size_t i = 0;
        for (bool b : vect) {
            vars[i++] = variant(b);
        }
        v = std::move(vars);
    }

    template<typename T> void to_variant(const cyberway::chaindb::oid<T>& oid, variant& v) {
        v = variant(oid._id);
    }

    template<typename T> void from_variant(const variant& v, cyberway::chaindb::oid<T>& oid) {
        from_variant(v, oid._id);
    }

    namespace raw {
        template<typename Stream, typename T> inline void pack(Stream& s, const chainbase::oid<T>& o) {
            fc::raw::pack(s, o._id);
        }

        template<typename Stream, typename T> inline void unpack(Stream& s, chainbase::oid<T>& o) {
            fc::raw::unpack(s, o._id);
        }

        template<typename Stream, typename T> inline void pack(Stream& s, const cyberway::chaindb::oid<T>& o) {
            fc::raw::pack(s, o._id);
        }

        template<typename Stream, typename T> inline void unpack(Stream& s, cyberway::chaindb::oid<T>& o) {
            fc::raw::unpack(s, o._id);
        }

        /** std::vector<bool> has custom implementation and pack bools as bits */
        template<typename Stream> inline void unpack(Stream& s, std::vector<bool>& value) {
            // TODO: can serialize as bitmap to save up to 8x storage (implement proper pack)
            unsigned_int size;
            fc::raw::unpack(s, size);
            FC_ASSERT(size.value <= MAX_NUM_ARRAY_ELEMENTS);
            value.resize(size.value);
            auto itr = value.begin();
            auto end = value.end();
            while (itr != end) {
                bool b = true;
                fc::raw::unpack(s, b);
                *itr = b;
                ++itr;
            }
        }
    } // namespace raw
} // namespace fc
