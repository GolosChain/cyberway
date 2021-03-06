#include <cyberway/chaindb/abi_info.hpp>
#include <cyberway/chaindb/table_info.hpp>
#include <cyberway/chaindb/driver_interface.hpp>

#include <boost/algorithm/string.hpp>

namespace cyberway { namespace chaindb {

    using eosio::chain::struct_def;
    using eosio::chain::type_name;

    namespace { namespace _detail {

        using struct_def_map_type = fc::flat_map<type_name, struct_def>;

        const struct_def_map_type& get_system_types() {
            static auto types = [](){
                struct_def_map_type init_types;

                init_types.emplace("asset", struct_def{
                    "asset", "", {
                        {"_amount", "uint64"},
                        {"_decs",   "uint8"},
                        {"_sym",    "symbol_code"},
                    }
                });

                init_types.emplace("symbol", struct_def{
                    "symbol", "", {
                        {"_decs", "uint8"},
                        {"_sym",  "symbol_code"},
                    }
                });

                return init_types;
            }();

            return types;
        }

        void validate_unique_field_names(const abi_info& abi, const table_def& table, const index_def& index) {
            fc::flat_set<field_name> fields;
            fields.reserve(index.orders.size());
            for (auto& order: index.orders) fields.emplace(order.field);
            CYBERWAY_ASSERT(fields.size() == index.orders.size(), unique_field_name_exception,
                "Index '${index}' has duplicates of fields",
                ("index", get_full_index_name(abi.code(), table, index)));
        }

        table_info generate_table_info(const abi_info& abi, const table_def& table) {
            table_info info(abi.code(), abi.code());
            info.table = &table;
            info.pk_order = abi.find_pk_order(table);
            return info;
        }

        index_info generate_index_info(const abi_info& abi, const table_def& table, const index_def& index) {
            index_info info(abi.code(), abi.code());
            info.table = &table;
            info.index = &index;
            info.pk_order = abi.find_pk_order(table);
            return info;
        }

        bool is_equal_index(const index_def& l, const index_def& r) {
            if (l.unique != r.unique)               return false;
            if (l.orders.size() != r.orders.size()) return false;

            return std::equal(l.orders.begin(), l.orders.end(), r.orders.begin(), [](auto& l, auto& r){
                return l.field == r.field && l.order.size() == r.order.size();
            });
        }
    } }

    class index_builder final {
        using table_map_type = fc::flat_map<table_name_t, table_def>;
        index_info info_;
        table_map_type& table_map_;
        abi_serializer& serializer_;
        const fc::microseconds& max_abi_time_;

    public:
        index_builder(
            const account_name& code, table_map_type& table_map,
            abi_serializer& serializer, const fc::microseconds& max_abi_time
        ): info_(code, code),
           table_map_(table_map),
           serializer_(serializer),
           max_abi_time_(max_abi_time) {
        }

        void build_indexes() {
            const bool check_field_name = serializer_.is_check_field_name();
            serializer_.set_check_field_name(false);
            auto reset_field_name = fc::make_scoped_exit([this, check_field_name]{
                serializer_.set_check_field_name(check_field_name);
            });

            for (auto& t: table_map_) try {
                // if abi was loaded instead of declared
                auto& table = t.second;
                auto& table_struct = serializer_.get_struct(serializer_.resolve_type(table.type));

                for (auto& index: table.indexes) {
                    build_index(table, index, table_struct);
                }
                validate_pk_index(table);

            } FC_CAPTURE_AND_RETHROW((t.second))
        }

    private:
        const struct_def& get_struct(const type_name& type) const {
            auto resolved_type = serializer_.resolve_type(type);

            auto& system_map = _detail::get_system_types();
            auto itr = system_map.find(resolved_type);
            if (system_map.end() != itr) {
                return itr->second;
            }

            return serializer_.get_struct(resolved_type);
        }

        type_name get_root_index_name(const table_def& table, const index_def& index) {
            info_.table = &table;
            info_.index = &index;
            return get_full_index_name(info_);
        }

        void build_index(table_def& table, index_def& index, const struct_def& table_struct) {
            auto struct_name = get_root_index_name(table, index);

            CYBERWAY_ASSERT(!index.orders.empty(), invalid_index_description_exception,
                "Index ${index} doesn't have fields", ("index", struct_name));
            CYBERWAY_ASSERT(index.orders.size() <= abi_info::MaxFieldCnt, invalid_index_description_exception,
                "Index ${index} has too many fields ${orders} (> maximum = ${max})",
                ("index", struct_name)("orders", index.orders.size())("max", int(abi_info::MaxFieldCnt)));

            auto max_size = index.orders.size() * abi_info::MaxPathDepth;
            _detail::struct_def_map_type dst_struct_map;
            std::vector<struct_def*> dst_structs;

            dst_struct_map.reserve(max_size);
            dst_structs.reserve(max_size);

            auto& root_struct = dst_struct_map.emplace(struct_name, struct_def(struct_name, "", {})).first->second;

            dst_structs.push_back(&root_struct);
            for (auto& order: index.orders) {
                CYBERWAY_ASSERT(order.order == names::asc_order || order.order == names::desc_order,
                    invalid_index_description_exception,
                    "Invalid type '${type}' for index order for the index ${index}",
                    ("type", order.order)("index", root_struct.name));

                boost::split(order.path, order.field, [](char c){return c == '.';});
                CYBERWAY_ASSERT(!order.path.empty(), invalid_index_description_exception,
                    "Path in the index ${index} doesn't have any part ", ("index", root_struct.name));
                CYBERWAY_ASSERT(order.path.size() <= abi_info::MaxPathDepth, invalid_index_description_exception,
                    "Path in the index ${index} is too long", ("index", root_struct.name));

                auto dst_struct = &root_struct;
                auto src_struct = &table_struct;
                auto size = order.path.size();
                for (auto& key: order.path) {
                    bool was_key = false;
                    for (auto& src_field: src_struct->fields) {
                        if (src_field.name != key) continue;
                        CYBERWAY_ASSERT(!serializer_.is_array(src_field.type), array_field_exception,
                            "Field ${path} can't be used for the index ${index}",
                            ("path", order.field)("index", root_struct.name));

                        was_key = true;
                        --size;
                        if (!size) {
                            auto field = src_field;
                            field.type = serializer_.resolve_type(src_field.type);
                            order.type = field.type;
                            dst_struct->fields.emplace_back(std::move(field));

                            auto index_cnt_res = table.field_indexes.emplace(order.field, eosio::chain::index_names());
                            index_cnt_res.first->second.push_back(index.name);
                            CYBERWAY_ASSERT(index_cnt_res.first->second.size() <= table.indexes.size(),
                                invalid_index_description_exception, "Wrong description of the index ${index}",
                                ("index", root_struct.name));
                        } else {
                            src_struct = &get_struct(src_field.type);
                            struct_name.append(1, ':').append(key);
                            auto itr = dst_struct_map.find(struct_name);
                            if (dst_struct_map.end() == itr) {
                                dst_struct->fields.emplace_back(key, struct_name);
                                itr = dst_struct_map.emplace(struct_name, struct_def(struct_name, "", {})).first;
                                dst_structs.push_back(&itr->second);
                            }
                            dst_struct = &itr->second;
                        }
                        break;
                    }
                    if (!was_key) break;
                }
                CYBERWAY_ASSERT(!size && !order.type.empty(), invalid_index_description_exception,
                    "Can't find type for fields of the index ${index}", ("index", root_struct.name));

                struct_name = root_struct.name;
            }

            for (auto itr = dst_structs.rbegin(); dst_structs.rend() != itr; ++itr) {
                auto struct_ptr = *itr;
                serializer_.add_struct(std::move(*struct_ptr), max_abi_time_);
            }
        }

        void validate_pk_index(const table_def& table) const {
            CYBERWAY_ASSERT(!table.indexes.empty(), invalid_primary_key_exception,
                "Table ${table} must contain at least one index", ("table", get_table_name(table)));

            auto& p = table.indexes.front();
            CYBERWAY_ASSERT(p.unique && p.orders.size() == 1, invalid_primary_key_exception,
                "The primary index ${index} of the table ${table} should be unique and has one field",
                ("table", get_table_name(table))("index", get_index_name(p)));

            auto& k = p.orders.front();
            CYBERWAY_ASSERT(
                primary_key::is_valid_kind(k.type),
                invalid_primary_key_exception,
                "Field ${field} of table ${table} is declared as primary and it should has type: "
                "int64, uint64, name, symbol_code or symbol",
                ("field", k.field)("table", get_table_name(table)));

            CYBERWAY_ASSERT(
                scope_name::is_valid_kind(table.scope_type),
                invalid_scope_name_exception,
                "Scope type should has type: int64, uint64, name, symbol_code or symbol",
                ("field", k.field)("table", get_table_name(table)));

            for (auto& index: table.indexes) if (!index.unique) for (auto& order: index.orders) {
                CYBERWAY_ASSERT(order.field != k.field, invalid_index_description_exception,
                    "Not-unique index ${index} for the table ${table} can't contain primary key ${pk}",
                    ("table", get_table_name(table))("index", get_index_name(index))("pk", k.field));
            }
        }
    }; // struct index_builder

    const fc::microseconds abi_info::max_abi_time_ = fc::seconds(60);

    abi_info::abi_info(const account_name& code, abi_def def)
    : code_(code),
      serializer_() {
        init(std::move(def));
    }

    abi_info::abi_info(const account_name& code, blob b)
    : code_(code),
      serializer_() {
        abi_def def;
        abi_serializer::to_abi(b.data, def);
        init(std::move(def));
    }

    void abi_info::init(abi_def def) {
        if (!is_system_code(code_)) {
            serializer_.set_check_field_name(true);
        }
        serializer_.set_abi(def, max_abi_time_);

        CYBERWAY_ASSERT(def.tables.size() <= MaxTableCnt, max_table_count_exception,
            "Account '${code}' can't create more than ${max} tables",
            ("code", get_code_name(code_))("max", size_t(MaxTableCnt)));


        table_map_.reserve(def.tables.size());
        for (auto& table: def.tables) {
            CYBERWAY_ASSERT(table.indexes.size() <= MaxIndexCnt, max_index_count_exception,
                "Table '${table}' can't has more than ${max} indicies",
                ("table", get_full_table_name(code_, table))("max", size_t(MaxIndexCnt)));
            auto id = table.name;
            table_map_.emplace(id, std::move(table));
        }

        index_builder builder(code_, table_map_, serializer_, max_abi_time_);
        builder.build_indexes();
    }

    template<typename Type>
    variant abi_info::to_object_(
        abi_serializer::mode m,
        const char* value_type, Type&& db_type, const string& type,
        const void* data, const size_t size
    ) const {
        // begin()
        if (nullptr == data || 0 == size) return fc::variant_object();

        fc::datastream<const char*> ds(static_cast<const char*>(data), size);
        auto value = serializer_.binary_to_variant(type, ds, max_abi_time_, m);

//            dlog(
//                "The ${value_type} '${type}': ${value}",
//                ("value_type", value_type)("type", db_type())("value", value));

        CYBERWAY_ASSERT(value.is_object(), invalid_abi_store_type_exception,
            "ABI serializer returns bad type for ${value_type} for '${type}': ${value}",
            ("value_type", value_type)("type", db_type())("value", value));

        return value;
    }

    template<typename Type>
    bytes abi_info::to_bytes_(const char* value_type, Type&& db_type, const string& type, const variant& value) const {

//            dlog(
//                "The ${value_type} '${type}': ${value}",
//                ("value", value_type)("type", db_type())("value", value));

        CYBERWAY_ASSERT(value.is_object(), invalid_abi_store_type_exception,
            "ABI serializer receive wrong type for ${value_type} for '${type}': ${value}",
            ("value_type", value_type)("type", db_type())("value", value));

        return serializer_.variant_to_binary(type, value, max_abi_time_);
    }

    variant abi_info::to_object(const table_info& info, const void* data, const size_t size) const {
        assert(info.table);
        auto db_type = [&]{return get_full_table_name(info);};
        return to_object_(abi_serializer::DBMode, "table", std::move(db_type), info.table->type, data, size);
    }

    variant abi_info::to_object(const index_info& info, const void* data, const size_t size) const {
        assert(info.index);
        auto type = get_full_index_name(info);
        auto db_type = [&](){return type;};
        return to_object_(abi_serializer::DBMode, "index", std::move(db_type), type, data, size);
    }

    variant abi_info::to_object(const string& type, const void* data, const size_t size) const {
        auto db_type = [&](){return type;};
        return to_object_(abi_serializer::PublicMode, "object", std::move(db_type), type, data, size);
    }

    bytes abi_info::to_bytes(const table_info& info, const variant& value) const {
        assert(info.table);
        auto db_type = [&]{return get_full_table_name(info);};
        return to_bytes_("table", std::move(db_type), info.table->type, value);
    }

    bytes abi_info::to_bytes(const index_info& info, const variant& value) const {
        assert(info.index);
        auto type = get_full_index_name(info);
        auto db_type = [&]{return type;};
        return to_bytes_("index", std::move(db_type), type, value);
    }

    void abi_info::verify_tables_structure(const driver_interface& driver) const {
        fc::flat_map<table_name, const table_def*> tables;
        fc::flat_map<index_name, const index_def*> indexes;
        tables.reserve(tables.size());
        for (auto& table: table_map_) tables.emplace(table.second.name, &table.second);

        std::vector<table_info> drop_tables;
        std::vector<index_info> drop_indexes;
        std::vector<index_info> create_indexes;
        std::string path;
        fc::flat_set<std::string> paths;

        auto db_tables = driver.db_tables(code_);
        const auto max_db_index_cnt = db_tables.size() * MaxIndexCnt;
        drop_tables.reserve(    db_tables.size());
        drop_indexes.reserve(   max_db_index_cnt);
        create_indexes.reserve( max_db_index_cnt);
        path.reserve(           max_db_index_cnt * 256);
        paths.reserve(          max_db_index_cnt);
        indexes.reserve(        max_db_index_cnt);

        for (auto& db_table: db_tables) {
            auto ttr = tables.find(db_table.name);
            auto table = _detail::generate_table_info(*this, db_table);
            if (tables.end() == ttr || !ttr->second) {
                CYBERWAY_ASSERT(db_table.row_count == 0, table_has_rows_exception,
                    "Can't drop table '${table}', because it has ${row_cnt} rows",
                    ("table", get_full_table_name(table))("row_cnt", db_table.row_count));

                drop_tables.emplace_back(std::move(table));
                continue;
            }

            paths.clear();
            indexes.clear();

            auto pk_order = find_pk_order(*ttr->second);
            for (auto& index: ttr->second->indexes) {
                indexes.emplace(index.name, &index);
                path.clear();
                for (auto& order: index.orders) {
                    path.append(":").append(order.order).append("+").append(order.field);
                }
                paths.insert(path);
            }
            CYBERWAY_ASSERT(ttr->second->indexes.size() == indexes.size() && indexes.size() == paths.size(),
                unique_index_name_exception, "The table '${table}' has indices with the same field list",
                ("table", get_full_table_name(table)));

            for (auto& db_index: db_table.indexes) {
                auto itr   = indexes.find(db_index.name);
                auto index = _detail::generate_index_info(*this, db_table, db_index);
                if (indexes.end() == itr || !itr->second || !_detail::is_equal_index(*itr->second, db_index)) {
                    CYBERWAY_ASSERT(db_table.row_count == 0, table_has_rows_exception,
                        "Can't drop index '${index}', because the table '${table}' has ${row_cnt} rows",
                        ("index", get_full_index_name(table, db_index))
                        ("table", get_full_table_name(table))("row_cnt", db_table.row_count));
                    drop_indexes.emplace_back(std::move(index));
                    continue;
                }
                itr->second = nullptr;
            }

            for (auto& index: indexes) if (index.second) {
                CYBERWAY_ASSERT(db_table.row_count == 0, table_has_rows_exception,
                    "Can't create index '${index}', because the table '${table}' has ${row_cnt} rows",
                    ("index", get_full_index_name(table, index.second))
                    ("table", get_full_table_name(table))("row_cnt", db_table.row_count));
                _detail::validate_unique_field_names(*this, *ttr->second, *index.second);
                create_indexes.emplace_back(_detail::generate_index_info(*this, *ttr->second, *index.second));
            }

            ttr->second = nullptr;
        }

        for (auto& table: tables) if (table.second) for (auto& index: table.second->indexes) {
            _detail::validate_unique_field_names(*this, *table.second, index);
            create_indexes.emplace_back(_detail::generate_index_info(*this, *table.second, index));
        }

        for (auto& table: drop_tables)    driver.drop_table(  table);
        for (auto& index: drop_indexes)   driver.drop_index(  index);
        for (auto& index: create_indexes) driver.create_index(index);
    }

    //-------------------------------------

    abi_def unpack_abi_def(const bytes& src) {
        abi_def dst;
        abi_serializer::to_abi(src, dst);
        return dst;
    };

    abi_def merge_abi_def(abi_def src1, abi_def src2) {
        abi_def dst;

        auto merge  = [&](auto&& param, auto&& cmp) {
            std::sort( (src1.*param).begin(), (src1.*param).end(), cmp);
            std::sort( (src2.*param).begin(), (src2.*param).end(), cmp);

            std::set_union((src1.*param).begin(), (src1.*param).end(),
                (src2.*param).begin(), (src2.*param).end(),
                std::back_inserter(dst.*param), cmp);
        };

        merge(&abi_def::types,    [&](const auto& l, const auto& r) { return l.new_type_name < r.new_type_name; });
        merge(&abi_def::structs,  [&](const auto& l, const auto& r) { return l.name < r.name; });
        merge(&abi_def::actions,  [&](const auto& l, const auto& r) { return l.name < r.name; });
        merge(&abi_def::events,   [&](const auto& l, const auto& r) { return l.name < r.name; });
        merge(&abi_def::tables,   [&](const auto& l, const auto& r) { return l.name < r.name; });
        merge(&abi_def::variants, [&](const auto& l, const auto& r) { return l.name < r.name; });

        merge(&abi_def::error_messages, [&](const auto& l, const auto& r) { return l.error_code < r.error_code; });
        merge(&abi_def::abi_extensions, [&](const auto& l, const auto& r) { return l.first      < r.first; }     );

        return dst;
    }

    bytes merge_abi_def(abi_def src1, const bytes& b2) {
        auto dst = merge_abi_def(std::move(src1), unpack_abi_def(b2));

        bytes result;
        result.resize(fc::raw::pack_size(dst));
        fc::datastream<char*> ds(const_cast<char*>(result.data()), result.size());
        fc::raw::pack(ds, dst);

        return result;
    }

    bytes merge_abi_def(const bytes& b1, const bytes& b2) {
        return merge_abi_def(unpack_abi_def(b1), b2);
    }

} } // namespace cyberway::chaindb