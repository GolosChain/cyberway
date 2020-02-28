/**
 *  @file
 *  @copyright defined in eos/LICENSE
 */
#pragma once

#include <softfloat.hpp>

#include <fc/io/raw.hpp>

#include <cyberway/chaindb/controller.hpp>

namespace cyberway { namespace chaindb {
    class chaindb_controller;
} } // namespace cyberway::chaindb

namespace eosio { namespace chain {

   template<typename ...Tables>
   class table_set;

   template<typename Table>
   class table_utils {
      using chaindb_controller = cyberway::chaindb::chaindb_controller;
      public:
         template<typename F>
         static void walk(const chaindb_controller& chaindb, const F& function) {
            const auto table = Table(chaindb);
            const auto& first = table.begin();
            const auto& last = table.end();
            for (auto itr = first; itr != last; ++itr) {
               function(*itr);
            }
         }

         template<typename Key>
         static size_t size_range(const chaindb_controller& chaindb, const Key& begin_key, const Key& end_key ) {
            const auto table = Table(chaindb);
            auto begin_itr = table.lower_bound(begin_key);
            auto end_itr = table.lower_bound(end_key);
            size_t res = 0;
            while (begin_itr != end_itr) {
               res++; ++begin_itr;
            }
            return res;
         }
   };

   template<typename Table>
   class table_set<Table> {
   public:
      static void add_tables( cyberway::chaindb::chaindb_controller& chaindb ) {
         Table::set_cache_converter(chaindb);
      }

      template<typename F>
      static void walk_tables( F function ) {
         function( table_utils<Table>() );
      }
   };

   template<typename FirstTable, typename ...RemainingTables>
   class table_set<FirstTable, RemainingTables...> {
   public:
      static void add_tables(cyberway::chaindb::chaindb_controller& chaindb) {
         table_set<FirstTable>::add_tables(chaindb);
         table_set<RemainingTables...>::add_tables(chaindb);
      }

      template<typename F>
      static void walk_tables( F function ) {
         table_set<FirstTable>::walk_tables(function);
         table_set<RemainingTables...>::walk_tables(function);
      }
   };

   template<typename DataStream>
   DataStream& operator << ( DataStream& ds, const shared_blob& b ) {
      fc::raw::pack(ds, static_cast<const shared_string&>(b));
      return ds;
   }

   template<typename DataStream>
   DataStream& operator >> ( DataStream& ds, shared_blob& b ) {
      fc::raw::unpack(ds, static_cast<shared_string &>(b));
      return ds;
   }
} }

namespace fc {

   // overloads for to/from_variant
   template<typename OidType>
   void to_variant( const chainbase::oid<OidType>& oid, variant& v ) {
      v = variant(oid._id);
   }

   template<typename OidType>
   void from_variant( const variant& v, chainbase::oid<OidType>& oid ) {
      from_variant(v, oid._id);
   }

   inline
   void to_variant( const float64_t& f, variant& v ) {
      v = variant(*reinterpret_cast<const double*>(&f));
   }

   inline
   void from_variant( const variant& v, float64_t& f ) {
      from_variant(v, *reinterpret_cast<double*>(&f));
   }

   inline
   void to_variant( const float128_t& f, variant& v ) {
      v = variant(*reinterpret_cast<const uint128_t*>(&f));
   }

   inline
   void from_variant( const variant& v, float128_t& f ) {
      from_variant(v, *reinterpret_cast<uint128_t*>(&f));
   }

   inline
   void to_variant( const eosio::chain::shared_string& s, variant& v ) {
      v = variant(std::string(s.begin(), s.end()));
   }

   inline
   void from_variant( const variant& v, eosio::chain::shared_string& s ) {
      string _s;
      from_variant(v, _s);
      s = eosio::chain::shared_string(_s.begin(), _s.end(), s.get_allocator());
   }

   inline
   void from_variant( const variant& v, eosio::chain::shared_blob& b ) {
      string _s = base64_decode(v.as_string());
      b = eosio::chain::shared_blob(_s.begin(), _s.end(), b.get_allocator());
   }

   inline
   void to_variant( const blob& b, variant& v ) {
      v = variant(base64_encode(b.data));
   }

   inline
   void from_variant( const variant& v, blob& b ) {
      string _s = base64_decode(v.as_string());
      b.data = std::vector<char>(_s.begin(), _s.end());
   }

   template<typename T>
   void to_variant( const eosio::chain::shared_vector<T>& sv, variant& v ) {
      to_variant(std::vector<T>(sv.begin(), sv.end()), v);
   }

   template<typename T>
   void from_variant( const variant& v, eosio::chain::shared_vector<T>& sv ) {
      std::vector<T> _v;
      from_variant(v, _v);
      sv = eosio::chain::shared_vector<T>(_v.begin(), _v.end(), sv.get_allocator());
   }
}

namespace chainbase {
   // overloads for OID packing
   template<typename DataStream, typename OidType>
   DataStream& operator << ( DataStream& ds, const oid<OidType>& oid ) {
      fc::raw::pack(ds, oid._id);
      return ds;
   }

   template<typename DataStream, typename OidType>
   DataStream& operator >> ( DataStream& ds, oid<OidType>& oid ) {
      fc::raw::unpack(ds, oid._id);
      return ds;
   }
}

// overloads for softfloat packing
template<typename DataStream>
DataStream& operator << ( DataStream& ds, const float64_t& v ) {
   fc::raw::pack(ds, *reinterpret_cast<const double *>(&v));
   return ds;
}

template<typename DataStream>
DataStream& operator >> ( DataStream& ds, float64_t& v ) {
   fc::raw::unpack(ds, *reinterpret_cast<double *>(&v));
   return ds;
}

template<typename DataStream>
DataStream& operator << ( DataStream& ds, const float128_t& v ) {
   fc::raw::pack(ds, *reinterpret_cast<const eosio::chain::uint128_t*>(&v));
   return ds;
}

template<typename DataStream>
DataStream& operator >> ( DataStream& ds, float128_t& v ) {
   fc::raw::unpack(ds, *reinterpret_cast<eosio::chain::uint128_t*>(&v));
   return ds;
}
