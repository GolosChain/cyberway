#pragma once

#include <string.h>

#include <fc/io/datastream.hpp>

#include <eosio/chain/config.hpp>

#include <vector>
#include <tuple>
#include <functional>
#include <utility>
#include <type_traits>
#include <iterator>
#include <limits>
#include <algorithm>
#include <memory>

#include <boost/hana.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/tuple/tuple_comparison.hpp>

#include <cyberway/chaindb/controller.hpp>
#include <cyberway/chaindb/exception.hpp>

#define CYBERWAY_INDEX_ASSERT(_EXPR, ...) EOS_ASSERT(_EXPR, index_exception, __VA_ARGS__)

namespace boost { namespace tuples {

namespace _detail {
    template<int I, int IMax> struct pack {
        template<typename T, typename V> void operator()(T& stream, const V& value) const {
            fc::raw::pack(stream, get<I>(value));
            pack<I + 1, IMax>()(stream, value);
        }
    }; // struct pack

    template<int I> struct pack<I, I> {
        template<typename... T> void operator()(T&&...) const { }
    }; // struct pack
} // namespace _detail

template<typename T, typename... Indicies>
fc::datastream<T>& operator<<(fc::datastream<T>& stream, const boost::tuple<Indicies...>& value) {
    _detail::pack<0, length<tuple<Indicies...>>::value>()(stream, value);
    return stream;
}

} } // namespace boost::tuples

namespace cyberway { namespace chaindb {
    template<class> struct tag;
} }

#define CHAINDB_TAG(_TYPE, _NAME)                   \
    namespace cyberway { namespace chaindb {        \
        template<> struct tag<_TYPE> {              \
            using type = _TYPE;                     \
            constexpr static uint64_t get_code() {  \
                return N(_NAME);                    \
            }                                       \
        };                                          \
    } }


namespace cyberway { namespace chaindb {
    namespace uninitilized_cursor {
        static constexpr cursor_t state = 0;
        static constexpr cursor_t end = -1;
        static constexpr cursor_t begin = -2;
        static constexpr cursor_t find_by_pk = -3;
    }

template<typename O>
void pack_object(const O& o, char* data, const size_t size) {
    fc::datastream<char*> ds(data, size);
    fc::raw::pack(ds, o);
}

template<typename O>
void unpack_object(O& o, const char* data, const size_t size) {
    fc::datastream<const char*> ds(data, size);
    fc::raw::unpack(ds, o);
}

template<typename Size, typename Lambda>
void safe_allocate(const Size size, const char* error_msg, Lambda&& callback) {
    CYBERWAY_INDEX_ASSERT(size > 0, error_msg);

    constexpr static size_t max_stack_data_size = 65536;

    struct allocator {
        char* data = nullptr;
        const bool use_malloc;
        const size_t size;
        allocator(const size_t s)
        : use_malloc(s > max_stack_data_size), size(s) {
            if (use_malloc) data = static_cast<char*>(malloc(s));
        }

        ~allocator() {
            if (use_malloc) free(data);
        }
    } alloc(static_cast<size_t>(size));

    if (!alloc.use_malloc) alloc.data = static_cast<char*>(alloca(alloc.size));
    CYBERWAY_INDEX_ASSERT(alloc.data != nullptr, "unable to allocate memory");

    callback(alloc.data, alloc.size);
}

using boost::multi_index::const_mem_fun;

namespace _detail {
    template <typename T> constexpr T& min(T& a, T& b) {
        return a > b ? b : a;
    }

    template<int I, int Max> struct comparator_helper {
        template<typename L, typename V>
        bool operator()(const L& left, const V& value) const {
            return boost::tuples::get<I>(left) == boost::tuples::get<I>(value) &&
                comparator_helper<I + 1, Max>()(left, value);
        }
    }; // struct comparator_helper

    template<int I> struct comparator_helper<I, I> {
        template<typename... L> bool operator()(L&&...) const { return true; }
    }; // struct comparator_helper

    template<int I, int Max> struct converter_helper {
        template<typename L, typename V> void operator()(L& left, const V& value) const {
            boost::tuples::get<I>(left) = boost::tuples::get<I>(value);
            converter_helper<I + 1, Max>()(left, value);
        }
    }; // struct converter_helper

    template<int I> struct converter_helper<I, I> {
        template<typename... L> void operator()(L&&...) const { }
    }; // struct converter_helper
} // namespace _detail

template<typename Key>
struct key_comparator {
    static bool compare_eq(const Key& left, const Key& right) {
        return left == right;
    }
}; // struct key_comparator

template<typename... Indices>
struct key_comparator<boost::tuple<Indices...>> {
    using key_type = boost::tuple<Indices...>;

    template<typename Value>
    static bool compare_eq(const key_type& left, const Value& value) {
        return boost::tuples::get<0>(left) == value;
    }

    template<typename... Values>
    static bool compare_eq(const key_type& left, const boost::tuple<Values...>& value) {
        using value_type = boost::tuple<Values...>;
        using namespace _detail;
        using boost::tuples::length;

        return comparator_helper<0, min(length<value_type>::value, length<key_type>::value)>()(left, value);
    }
}; // struct key_comparator

template<typename Key>
struct key_converter {
    static Key convert(const Key& key) {
        return key;
    }
}; // struct key_converter

template<typename... Indices>
struct key_converter<boost::tuple<Indices...>> {
    using key_type = boost::tuple<Indices...>;

    template<typename Value>
    static key_type convert(const Value& value) {
        key_type index;
        boost::tuples::get<0>(index) = value;
        return index;
    }

    template<typename... Values>
    static key_type convert(const boost::tuple<Values...>& value) {
        using namespace _detail;
        using boost::tuples::length;
        using value_type = boost::tuple<Values...>;

        key_type index;
        converter_helper<0, min(length<value_type>::value, length<key_type>::value)>()(index, value);
        return index;
    }
}; // struct key_converter

template<typename C>
struct code_extractor;

template<typename C>
struct code_extractor<tag<C>> {
    constexpr static uint64_t get_code() {
        return tag<C>::get_code();
    }
}; // struct code_extractor

template<uint64_t N>
struct code_extractor<std::integral_constant<uint64_t, N>> {
    constexpr static uint64_t get_code() {return N;}
}; // struct code_extractor

template<typename TableName, typename Index, typename T, typename... Indices>
struct multi_index {
public:
    template<typename IndexName, typename Extractor> class index;

private:
    static_assert(sizeof...(Indices) <= 16, "multi_index only supports a maximum of 16 secondary indices");

    struct item_data: public cache_item_data {
        struct item_type: public T {
            template<typename Constructor>
            item_type(cache_item& cache, Constructor&& constructor)
            : T(std::forward<Constructor>(constructor), 0),
              cache(cache) {
            }

            cache_item& cache;
        } item;

        template<typename Constructor>
        item_data(cache_item& cache, Constructor&& constructor)
        : item(cache, std::forward<Constructor>(constructor)) {
        }

        static const T& get_T(const cache_item_ptr& cache) {
            return static_cast<const T&>(static_cast<const item_data*>(cache->data.get())->item);
        }

        static cache_item& get_cache(const T& o) {
            return const_cast<cache_item&>(static_cast<const item_type&>(o).cache);
        }

        using value_type = T;
    }; // struct item_data

    chaindb_controller& controller_;

    struct variant_converter: public cache_converter_interface {
        variant_converter() = default;
        ~variant_converter() = default;

        cache_item_data_ptr convert_variant(cache_item& itm, const object_value& obj) const override {
            auto ptr = std::make_unique<item_data>(itm, [&](auto& o) {
                T& data = static_cast<T&>(o);
                fc::from_variant(obj.value, data);
            });

            auto& data = static_cast<const T&>(ptr->item);
            auto ptr_pk = primary_key_extractor_type()(data);
            CYBERWAY_INDEX_ASSERT(ptr_pk == obj.pk(),
                "invalid primary key ${pk} for the object ${obj}", ("pk", obj.pk())("obj", obj.value));
            return ptr;
        }
    } variant_converter_;

    using primary_key_extractor_type = typename Index::extractor;

    template<typename IndexName>
    class const_iterator_impl: public std::iterator<std::bidirectional_iterator_tag, const T> {
    public:
        friend bool operator == (const const_iterator_impl& a, const const_iterator_impl& b) {
            a.lazy_open();
            b.lazy_open();
            return a.primary_key_ == b.primary_key_;
        }
        friend bool operator != (const const_iterator_impl& a, const const_iterator_impl& b) {
            return !(operator == (a, b));
        }

        constexpr static table_name_t   table_name() { return code_extractor<TableName>::get_code(); }
        constexpr static index_name_t   index_name() { return code_extractor<IndexName>::get_code(); }
        constexpr static account_name_t get_code()   { return 0; }
        constexpr static account_name_t get_scope()  { return 0; }

        const T& operator*() const {
            lazy_load_object();
            return item_data::get_T(item_);
        }
        const T* operator->() const {
            lazy_load_object();
            return &item_data::get_T(item_);
        }

        const_iterator_impl operator++(int) {
            const_iterator_impl result(*this);
            ++(*this);
            return result;
        }
        const_iterator_impl& operator++() {
            lazy_open();
            CYBERWAY_INDEX_ASSERT(primary_key_ != end_primary_key, "cannot increment end iterator");
            primary_key_ = controller().next(get_cursor_request());
            item_.reset();
            return *this;
        }

        const_iterator_impl operator--(int) {
            const_iterator_impl result(*this);
            --(*this);
            return result;
        }
        const_iterator_impl& operator--() {
            lazy_open();
            primary_key_ = controller().prev(get_cursor_request());
            item_.reset();
            CYBERWAY_INDEX_ASSERT(primary_key_ != end_primary_key, "out of range on decrement of iterator");
            return *this;
        }

        const_iterator_impl() = default;

        const_iterator_impl(const_iterator_impl&& src) {
            this->operator=(std::move(src));
        }
        const_iterator_impl& operator=(const_iterator_impl&& src) {
            if (this == &src) return *this;

            multidx_ = src.multidx_;
            cursor_ = src.cursor_;
            primary_key_ = src.primary_key_;
            item_ = std::move(src.item_);

            src.cursor_ = uninitilized_cursor::state;
            src.primary_key_ = end_primary_key;

            return *this;
        }

        const_iterator_impl(const const_iterator_impl& src) {
            this->operator=(src);
        }
        const_iterator_impl& operator=(const const_iterator_impl& src) {
            if (this == &src) return *this;

            multidx_ = src.multidx_;
            primary_key_ = src.primary_key_;
            item_ = src.item_;

            if (src.is_cursor_initialized()) {
                cursor_ = controller().clone(src.get_cursor_request()).cursor;
            } else {
                cursor_ = src.cursor_;
            }

            return *this;
        }

        ~const_iterator_impl() {
            if (is_cursor_initialized()) controller().close(get_cursor_request());
        }

    private:
        bool is_cursor_initialized() const {
            return cursor_ > uninitilized_cursor::state;
        }

        friend multi_index;
        template<typename, typename> friend class index;

        const_iterator_impl(const multi_index* midx, const cursor_t cursor)
        : multidx_(midx), cursor_(cursor) { }

        const_iterator_impl(const multi_index* midx, const cursor_t cursor, const primary_key_t pk)
        : multidx_(midx), cursor_(cursor), primary_key_(pk) { }

        const_iterator_impl(const multi_index* midx, const cursor_t cursor, const primary_key_t pk, cache_item_ptr item)
        : multidx_(midx), cursor_(cursor), primary_key_(pk), item_(std::move(item)) { }

        const multi_index& multidx() const {
            CYBERWAY_INDEX_ASSERT(multidx_, "Iterator is not initialized");
            return *multidx_;
        }

        chaindb_controller& controller() const {
            return multidx().controller_;
        }

        const multi_index* multidx_ = nullptr;
        mutable cursor_t cursor_ = uninitilized_cursor::state;
        mutable primary_key_t primary_key_ = end_primary_key;
        mutable cache_item_ptr item_;

        void lazy_load_object() const {
            if (item_ && !item_->is_deleted()) return;

            lazy_open();
            CYBERWAY_INDEX_ASSERT(primary_key_ != end_primary_key, "cannot load object from end iterator");
            item_ = multidx().load_object(cursor_, primary_key_);
        }

        void lazy_open_begin() const {
           if (BOOST_LIKELY(uninitilized_cursor::begin != cursor_)) return;

            auto info = controller().begin(get_index_request());
            cursor_ = info.cursor;
            primary_key_ = info.pk;
        }

        void lazy_open_find_by_pk() const {
            auto info = controller().opt_find_by_pk(get_table_request(), primary_key_);
            cursor_ = info.cursor;
            primary_key_ = info.pk;
        }

        void lazy_open() const {
            if (BOOST_LIKELY(is_cursor_initialized())) return;

            switch (cursor_) {
                case uninitilized_cursor::begin:
                    lazy_open_begin();
                    break;

                case uninitilized_cursor::end:
                    cursor_ = controller().end(get_index_request()).cursor;
                    break;

                case uninitilized_cursor::find_by_pk:
                    lazy_open_find_by_pk();
                    break;

                default:
                    break;
            }
            CYBERWAY_INDEX_ASSERT(is_cursor_initialized(), "unable to open cursor");
        }

        table_request get_table_request() const {
            return {get_code(), get_scope(), table_name()};
        }

        index_request get_index_request() const {
            return {get_code(), get_scope(), table_name(), index_name()};
        }

        cursor_request get_cursor_request() const {
            return {get_code(), cursor_};
        }
    }; // struct multi_index::const_iterator_impl

    cache_item_ptr load_object(const cursor_t& cursor, const primary_key_t& pk) const {
        // controller will call as to convert_object()
        auto ptr = controller_.get_cache_item({get_code(), cursor}, get_table_request(), pk);

        auto& obj = item_data::get_T(ptr);
        auto ptr_pk = primary_key_extractor_type()(obj);
        CYBERWAY_INDEX_ASSERT(ptr_pk == pk, "invalid primary key of object");
        return ptr;
    }

    template<typename... Idx>
    struct index_container {
        using type = decltype(boost::hana::tuple<Idx...>());
    };

    using indices_type = typename index_container<Indices...>::type;

    indices_type indices_;

    using primary_index_type = index<typename Index::tag, typename Index::extractor>;
    primary_index_type primary_idx_;

public:
    template<typename IndexName, typename Extractor>
    class index {
    public:
        using extractor_type = Extractor;
        using key_type = typename std::decay<decltype(Extractor()(static_cast<const T&>(*(const T*)nullptr)))>::type;
        using const_iterator = const_iterator_impl<IndexName>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        constexpr static table_name_t   table_name() { return code_extractor<TableName>::get_code(); }
        constexpr static index_name_t   index_name() { return code_extractor<IndexName>::get_code(); }
        constexpr static account_name_t get_code()   { return 0; }
        constexpr static account_name_t get_scope()  { return 0; }

        index(const multi_index* midx)
        : multidx_(midx) {
        }

        const_iterator cbegin() const {
            return const_iterator(multidx_, uninitilized_cursor::begin);
        }
        const_iterator begin() const  { return cbegin(); }

        const_iterator cend() const {
            return const_iterator(multidx_, uninitilized_cursor::end);
        }
        const_iterator end() const  { return cend(); }

        const_reverse_iterator crbegin() const { return const_reverse_iterator(cend()); }
        const_reverse_iterator rbegin() const  { return crbegin(); }

        const_reverse_iterator crend() const { return const_reverse_iterator(cbegin()); }
        const_reverse_iterator rend() const  { return crend(); }

        const_iterator find(key_type&& key) const {
            return find(key);
        }
        template<typename Value>
        const_iterator find(const Value& value) const {
            auto key = key_converter<key_type>::convert(value);
            auto itr = lower_bound(key);
            auto etr = cend();
            if (itr == etr) return etr;
            if (!key_comparator<key_type>::compare_eq(extractor_type()(*itr), value)) return etr;
            return itr;
        }
        const_iterator find(const key_type& key) const {
            auto itr = lower_bound(key);
            auto etr = cend();
            if (itr == etr) return etr;
            if (key != extractor_type()(*itr)) return etr;
            return itr;
        }

        const_iterator require_find(key_type&& key, const char* error_msg = "unable to find key") const {
            return require_find(key, error_msg);
        }
        const_iterator require_find(const key_type& key, const char* error_msg = "unable to find key") const {
            auto itr = lower_bound(key);
            CYBERWAY_INDEX_ASSERT(itr != cend(), error_msg);
            CYBERWAY_INDEX_ASSERT(key == extractor_type()(*itr), error_msg);
            return itr;
        }

        const T& get(key_type&& key, const char* error_msg = "unable to find key") const {
            return get(key, error_msg);
        }
        const T& get(const key_type& key, const char* error_msg = "unable to find key") const {
            auto itr = find(key);
            CYBERWAY_INDEX_ASSERT(itr != cend(), error_msg);
            return *itr;
        }

        const_iterator lower_bound(key_type&& key) const {
            return lower_bound(key);
        }
        const_iterator lower_bound(const key_type& key) const {
            find_info info;
            safe_allocate(fc::raw::pack_size(key), "invalid size of key", [&](auto& data, auto& size) {
                pack_object(key, data, size);
                info = controller().lower_bound(get_index_request(), data, size);
            });
            return const_iterator(multidx_, info.cursor, info.pk);
        }

        template<typename Value>
        const_iterator lower_bound(const Value& value) const {
            return lower_bound(key_converter<key_type>::convert(value));
        }
        const_iterator upper_bound(key_type&& key) const {
            return upper_bound(key);
        }
        const_iterator upper_bound(const key_type& key) const {
            find_info info;
            safe_allocate(fc::raw::pack_size(key), "invalid size of key", [&](auto& data, auto& size) {
                pack_object(key, data, size);
                info = controller().upper_bound(get_index_request(), data, size);
            });
            return const_iterator(multidx_, info.cursor, info.pk);
        }

        std::pair<const_iterator,const_iterator> equal_range(key_type&& key) const {
            return upper_bound(key);
        }
        std::pair<const_iterator,const_iterator> equal_range(const key_type& key) const {
            return make_pair(lower_bound(key), upper_bound(key));
        }

        template<typename Value>
        std::pair<const_iterator,const_iterator> equal_range(const Value& value) const {
            const_iterator lower = lower_bound(value);
            const_iterator upper = lower;
            auto etr = cend();
            while(upper != etr && key_comparator<key_type>::compare_eq(extractor_type()(*upper), value)) {
                ++upper;
            }

            return make_pair(lower, upper);
        }

        const_iterator iterator_to(const T& obj) const {
            const auto& d = item_data::get_cache(obj);
            CYBERWAY_INDEX_ASSERT(d.is_valid_table(table_name()), "object passed to iterator_to is not in multi_index");

            auto key = extractor_type()(obj);
            auto pk = primary_key_extractor_type()(obj);
            find_info info;
            safe_allocate(fc::raw::pack_size(key), "invalid size of key", [&](auto& data, auto& size) {
                pack_object(key, data, size);
                info = controller().find(get_index_request(), pk, data, size);
            });
            return const_iterator(multidx_, info.cursor, info.pk);
        }

        template<typename Lambda>
        void modify(const const_iterator& itr, const account_name& payer, Lambda&& updater) const {
            CYBERWAY_INDEX_ASSERT(itr != cend(), "cannot pass end iterator to modify");
            multidx_().modify(*itr, payer, std::forward<Lambda&&>(updater));
        }

        const_iterator erase(const_iterator itr) const {
            CYBERWAY_INDEX_ASSERT(itr != cend(), "cannot pass end iterator to erase");
            const auto& obj = *itr;
            ++itr;
            multidx_->erase(obj);
            return itr;
        }

        static auto extract_key(const T& obj) { return extractor_type()(obj); }

    private:
        const multi_index& multidx() const {
            CYBERWAY_INDEX_ASSERT(multidx_, "Index is not initialized");
            return *multidx_;
        }

        chaindb_controller& controller() const {
            return multidx().controller_;
        }

        index_request get_index_request() const {
            return {get_code(), get_scope(), table_name(), index_name()};
        }

        const multi_index* const multidx_ = nullptr;
    }; // struct multi_index::index

    using const_iterator = typename primary_index_type::const_iterator;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

public:
    multi_index(chaindb_controller& controller)
    : controller_(controller),
      primary_idx_(this) {
        controller_.set_cache_converter(get_table_request(), variant_converter_);
    }

    constexpr static table_name_t   table_name() { return code_extractor<TableName>::get_code(); }
    constexpr static account_name_t get_code()   { return 0; }
    constexpr static account_name_t get_scope()  { return 0; }

    table_request get_table_request() const {
        return {get_code(), get_scope(), table_name()};
    }

    const_iterator cbegin() const { return primary_idx_.cbegin(); }
    const_iterator begin() const  { return cbegin(); }

    const_iterator cend() const  { return primary_idx_.cend(); }
    const_iterator end() const   { return cend(); }

    const_reverse_iterator crbegin() const { return std::make_reverse_iterator(cend()); }
    const_reverse_iterator rbegin() const  { return crbegin(); }

    const_reverse_iterator crend() const { return std::make_reverse_iterator(cbegin()); }
    const_reverse_iterator rend() const  { return crend(); }

    const_iterator lower_bound(primary_key_t pk) const {
        return primary_idx_.lower_bound(pk);
    }

    const_iterator upper_bound(primary_key_t pk) const {
        return primary_idx_.upper_bound(pk);
    }

    primary_key_t available_primary_key() const {
        return controller_.available_pk(get_table_request());
    }

    template<typename IndexTag>
    auto get_index() const {
        namespace hana = boost::hana;

        auto res = hana::find_if(indices_, [](auto&& in) {
            return std::is_same<typename std::decay<decltype(in)>::type::tag_type, tag<IndexTag>>();
        });

        static_assert(res != hana::nothing, "name provided is not the name of any secondary index within multi_index");

        return index<IndexTag, typename std::decay<decltype(res.value())>::type::extractor_type>(this);
    }

    const_iterator iterator_to(const T& obj) const {
        return primary_idx_.iterator_to(obj);
    }

    template<typename Lambda>
    const_iterator emplace(const account_name& payer, Lambda&& constructor) const {
        auto itm = controller_.create_cache_item(get_table_request());
        try {
            auto pk = itm->pk();
            itm->data = std::make_unique<item_data>(*itm.get(), [&](auto& o) {
                constructor(static_cast<T&>(o));
                o.id = pk;
            });

            auto& obj = item_data::get_T(itm);
            auto obj_pk = primary_key_extractor_type()(obj);
            CYBERWAY_INDEX_ASSERT(obj_pk == pk, "invalid value of primary key");

            fc::variant value;
            fc::to_variant(obj, value);
            controller_.insert(*itm.get(), std::move(value), payer);

            return const_iterator(this, uninitilized_cursor::find_by_pk, pk, std::move(itm));
        } catch (...) {
            itm->mark_deleted();
            throw;
        }
    }

    template<typename Lambda>
    void modify(const_iterator itr, const account_name& payer, Lambda&& updater) const {
        CYBERWAY_INDEX_ASSERT(itr != end(), "cannot pass end iterator to modify");
        modify(*itr, payer, std::forward<Lambda&&>(updater));
    }

    template<typename Lambda>
    void modify(const T& obj, const account_name& payer, Lambda&& updater) const {
        auto& itm = item_data::get_cache(obj);
        CYBERWAY_INDEX_ASSERT(itm.is_valid_table(table_name()), "object passed to modify is not in multi_index");

        auto pk = primary_key_extractor_type()(obj);

        auto& mobj = const_cast<T&>(obj);
        updater(mobj);

        auto mpk = primary_key_extractor_type()(obj);
        CYBERWAY_INDEX_ASSERT(pk == mpk, "updater cannot change primary key when modifying an object");

        fc::variant value;
        fc::to_variant(obj, value);
        auto upk = controller_.update(itm, std::move(value), payer);
        CYBERWAY_INDEX_ASSERT(upk == pk, "unable to update object");
    }

    const T& get(const primary_key_t pk, const char* error_msg = "unable to find key") const {
        auto itr = find(pk);
        CYBERWAY_INDEX_ASSERT(itr != cend(), error_msg);
        return *itr;
    }

    const_iterator find(const primary_key_t pk) const {
        return primary_idx_.find(pk);
    }

    const_iterator require_find(const primary_key_t pk, const char* error_msg = "unable to find key") const {
        return primary_idx_.require_find(pk, error_msg);
    }

    const_iterator erase(const_iterator itr) const {
        CYBERWAY_INDEX_ASSERT(itr != end(), "cannot pass end iterator to erase");

        const auto& obj = *itr;
        ++itr;
        erase(obj);
        return itr;
    }

    void erase(const T& obj) const {
        auto& itm = item_data::get_cache(obj);
        CYBERWAY_INDEX_ASSERT(itm.is_valid_table(table_name()), "object passed to delete is not in multi_index");

        auto pk = primary_key_extractor_type()(obj);
        auto dpk = controller_.remove(itm);
        CYBERWAY_INDEX_ASSERT(dpk == pk, "unable to delete object");
    }
}; // struct multi_index

} }  // namespace cyberway::chaindb
