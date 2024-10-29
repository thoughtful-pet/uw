#include <stdarg.h>
#include <string.h>

#include "include/uw_value.h"
#include "src/uw_null_internal.h"  // for _uw_create_null
#include "src/uw_map_internal.h"

UwValuePtr _uw_create_map()
{
    UwValuePtr value = _uw_alloc_value();
    if (value) {
        value->type_id = UwTypeId_Map;
        value->refcount = 1;
        value->map_value = _uw_alloc_map(UWMAP_INITIAL_CAPACITY);
        if (!value->map_value) {
            goto error;
        }
        value->map_value->kv_pairs = _uw_alloc_list(UWMAP_INITIAL_CAPACITY * 2);
        if (!value->map_value->kv_pairs) {
            goto error;
        }
    }
    return value;

error:
    _uw_destroy_map(value);
    return nullptr;
}

void _uw_destroy_map(UwValuePtr self)
{
    if (self) {
        if (self->map_value) {
            _uw_delete_map(self->map_value);
        }
        _uw_free_value(self);
    }
}

void _uw_hash_map(UwValuePtr self, UwHashContext* ctx)
{
    _uw_hash_uint64(ctx, self->type_id);
    struct _UwList* list = self->map_value->kv_pairs;
    for (size_t i = 0; i < list->length; i++) {
        UwValuePtr item = list->items[i];
        _uw_call_hash(item, ctx);
    }
}

static bool update_map(UwValuePtr map, UwValuePtr key, UwValuePtr value);  // forward declaration

UwValuePtr _uw_copy_map(UwValuePtr self)
{
    struct _UwMap* map = self->map_value;
    size_t length = _uw_map_length(map);

    // find nearest power of two for hash table capacity
    size_t ht_capacity = UWMAP_INITIAL_CAPACITY;
    while (ht_capacity < length) {
        ht_capacity <<= 1;
    }

    length <<= 1;

    UwValuePtr result = _uw_alloc_value();
    result->type_id = UwTypeId_Map;
    result->refcount = 1;
    result->map_value = _uw_alloc_map(ht_capacity);
    result->map_value->kv_pairs = _uw_alloc_list(length);

    // deep copy
    for (size_t i = 0; i < length;) {
        UwValuePtr key = uw_copy(map->kv_pairs->items[i++]);
        UwValuePtr value = uw_copy(map->kv_pairs->items[i++]);
        if (key && value) {
            update_map(result, key, value);
            uw_delete(&key);
            uw_delete(&value);
        } else {
            uw_delete(&key);
            uw_delete(&value);
            goto error;
        }
    }
    return result;

error:
    _uw_destroy_map(result);
    return nullptr;
}

void _uw_dump_map(UwValuePtr self, int indent)
{
    _uw_dump_start(self, indent);

    struct _UwMap* map = self->map_value;
    printf("%zu items, capacity=%zu\n", map->kv_pairs->length >> 1, map->kv_pairs->capacity >> 1);

    indent += 4;
    for (size_t i = 0; i < map->kv_pairs->length; i++) {
        _uw_print_indent(indent);
        printf("Key:\n");
        _uw_call_dump(map->kv_pairs->items[i], indent);
        i++;
        _uw_print_indent(indent);
        printf("Value:\n");
        _uw_call_dump(map->kv_pairs->items[i], indent);
    }

    _uw_print_indent(indent);

    struct _UwHashTable* ht = &map->hash_table;
    printf("hash table item size %u, items_used=%zu, capacity=%zu (bitmask %llx)\n",
           ht->item_size, ht->items_used, ht->capacity, (unsigned long long) ht->hash_bitmask);

    for (size_t i = 0; i < ht->capacity; i++ ) {
        size_t kv_index = ht->get_item(ht, i);
        _uw_print_indent(indent);
        printf("%zx: %zu\n", i, kv_index);
    }
}

UwValuePtr _uw_map_to_string(UwValuePtr self)
{
    // XXX not implemented yet
    return nullptr;
}

bool _uw_map_is_true(UwValuePtr self)
{
    return _uw_map_length(self->map_value);
}

bool _uw_map_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return _uw_map_eq(self->map_value, other->map_value);
}

bool _uw_map_equal(UwValuePtr self, UwValuePtr other)
{
    if (other->type_id == UwTypeId_Map) {
        return _uw_map_eq(self->map_value, other->map_value);
    } else {
        return false;
    }
}

bool _uw_map_equal_ctype(UwValuePtr self, UwCType ctype, ...)
{
    bool result = false;
    va_list ap;
    va_start(ap);
    switch (ctype) {
        case uwc_value_ptr:
        case uwc_value: {
            UwValuePtr other = va_arg(ap, UwValuePtr);
            result = _uw_map_equal(self, other);
            break;
        }
        default: break;
    }
    va_end(ap);
    return result;
}

/****************************************************************
 * methods for acessing hash table
 */

#define HT_ITEM_METHODS(typename) \
    static size_t get_ht_item_##typename(struct _UwHashTable* ht, size_t index) \
    { \
        return ((typename*) (ht->items))[index]; \
    } \
    static void set_ht_item_##typename(struct _UwHashTable* ht, size_t index, size_t value) \
    { \
        ((typename*) (ht->items))[index] = (typename) value; \
    }

HT_ITEM_METHODS(uint8_t)
HT_ITEM_METHODS(uint16_t)
HT_ITEM_METHODS(uint32_t)
HT_ITEM_METHODS(uint64_t)

/*
 * methods for acessing hash table with any item size
 */
static size_t get_ht_item(struct _UwHashTable* ht, size_t index)
{
    uint8_t *item_ptr = &ht->items[index * ht->item_size];
    size_t result = 0;
    for (uint8_t i = ht->item_size; i > 0; i--) {
        result <<= 8;
        result += *item_ptr++;
    }
    return result;
}

static void set_ht_item(struct _UwHashTable* ht, size_t index, size_t value)
{
    uint8_t *item_ptr = &ht->items[(index + 1) * ht->item_size];
    for (uint8_t i = ht->item_size; i > 0; i--) {
        *(--item_ptr) = (uint8_t) value;
        value >>= 8;
    }
}

/****************************************************************
 * implementation
 */

static uint8_t get_item_size(size_t capacity)
/*
 * Return hash table item size for desired capacity.
 *
 * Index 0 in hash table means no item, so one byte is capable
 * to index 255 items, not 256.
 */
{
    uint8_t item_size = 1;

    for (size_t n = capacity; n > 255; n >>= 8) {
        item_size++;
    }
    return item_size;
}

/*
 * Notes on indexes and naming conventions.
 *
 * In hash map we store indexes of key-value pair (named `kv_index`),
 * not the index in kv_pairs list.
 *
 * key_index is the index of key in kv_pais list, suitable for passing to uw_list_get.
 *
 * So,
 *
 * value_index = key_index + 1
 * kv_index = key_index / 2
 */

struct _UwMap* _uw_alloc_map(size_t capacity)
/*
 * Create internal map structure for UwValue.map_value
 *
 * This function does not allocate kv_pairs because when
 * hash table is resized, which means allocating new map,
 * an existing kv_pairs will be used.
 */
{
    size_t item_size = get_item_size(capacity);
    struct _UwMap* map = malloc(sizeof(struct _UwMap) + item_size * capacity);
    if (!map) {
        perror(__func__);
        exit(1);
    }

    struct _UwHashTable* ht = &map->hash_table;

    ht->item_size = item_size;
    ht->hash_bitmask = capacity - 1;
    ht->items_used = 0;
    ht->capacity = capacity;

    memset(ht->items, 0,  item_size * capacity);

    switch (item_size) {
        case 1:
            ht->get_item = get_ht_item_uint8_t;
            ht->set_item = set_ht_item_uint8_t;
            break;
        case 2:
            ht->get_item = get_ht_item_uint16_t;
            ht->set_item = set_ht_item_uint16_t;
            break;
        case 4:
            ht->get_item = get_ht_item_uint32_t;
            ht->set_item = set_ht_item_uint32_t;
            break;
        case 8:
            ht->get_item = get_ht_item_uint64_t;
            ht->set_item = set_ht_item_uint64_t;
            break;
        default:
            ht->get_item = get_ht_item;
            ht->set_item = set_ht_item;
            break;
    }
    return map;
}

UwValuePtr uw_create_map_va(...)
{
    va_list ap;
    va_start(ap);
    UwValuePtr map = uw_create_map_ap(ap);
    va_end(ap);
    return map;
}

UwValuePtr uw_create_map_ap(va_list ap)
{
    UwValuePtr map = _uw_create_map();
    if (map) {
        if (!uw_map_update_ap(map, ap)) {
            uw_delete(&map);
        }
    }
    return map;
}

void _uw_delete_map(struct _UwMap* map)
{
    _uw_delete_list(map->kv_pairs);
    _uw_free_map(map);
}

size_t _uw_map_length(struct _UwMap* map)
{
    return map->kv_pairs->length >> 1;
}

bool _uw_map_eq(struct _UwMap* a, struct _UwMap* b)
{
    if (a == b) {
        // compare with self
        return true;
    }
    return _uw_list_eq(a->kv_pairs, b->kv_pairs);
}

static size_t lookup(struct _UwMap* map, UwValuePtr key, size_t* ht_index, size_t* ht_offset)
/*
 * Lookup key starting from index = hash(key).
 *
 * Return index of key in kv_pairs or SIZE_MAX if hash table has no item matching `key`.
 *
 * If `ht_index` is not `nullptr`: write index of hash table item at which lookup has stopped.
 * If `ht_offset` is not `nullptr`: write the difference from final `ht_index` and initial `ht_index` to `ht_offset`;
 *
 * XXX leverage equal_ctype method for C type keys and simplify uw_map_get_* and uw_map_has_key_* functions
 */
{
    struct _UwHashTable* hash_table = &map->hash_table;
    UwType_Hash index = uw_hash(key) & hash_table->hash_bitmask;
    size_t offset = 0;

    do {
        size_t kv_index = hash_table->get_item(hash_table, index);

        if (kv_index == 0) {
            // no entry matching key
            if (ht_index) {
                *ht_index = index;
            }
            if (ht_offset) {
                *ht_offset = offset;
            }
            return SIZE_MAX;
        }

        // make index 0-based
        kv_index--;

        UwValuePtr k = map->kv_pairs->items[kv_index * 2];

        // compare keys
        if (_uw_equal(k, key)) {
            // found key
            if (ht_index) {
                *ht_index = index;
            }
            if (ht_offset) {
                *ht_offset = offset;
            }
            return kv_index * 2;
        }

        // probe next item
        index = (index + 1) & hash_table->hash_bitmask;
        offset++;

    } while (true);
}

static size_t set_hash_table_item(struct _UwHashTable* hash_table, size_t ht_index, size_t kv_index)
/*
 * Assign `kv_index` to `hash_table` at position `ht_index` & hash_bitmask.
 * If the position is already occupied, try next one.
 *
 * Return ht_index, possibly updated.
 */
{
    do {
        ht_index &= hash_table->hash_bitmask;
        if (hash_table->get_item(hash_table, ht_index)) {
            ht_index++;
        } else {
            hash_table->set_item(hash_table, ht_index, kv_index);
            return ht_index;
        }
    } while (true);
}

static inline struct _UwMap* double_hash_table(struct _UwMap* map)
/*
 * Helper functtion for uw_map_update.
 *
 * Double the capacity of map's hash table and rebuild it.
 * The hash table is embedded into map structure, so re-allocate entire map.
 */
{
    struct _UwMap* new_map = _uw_alloc_map(map->hash_table.capacity * 2);
    if (!new_map) {
        return nullptr;
    }
    new_map->kv_pairs = map->kv_pairs;
    new_map->hash_table.items_used = map->hash_table.items_used;

    struct _UwHashTable* hash_table = &new_map->hash_table;

    // rebuild hash table
    UwValuePtr* key_ptr = new_map->kv_pairs->items;
    size_t kv_index = 1;
    size_t n = _uw_list_length(new_map->kv_pairs);
    uw_assert((n & 1) == 0);
    while (n) {
        set_hash_table_item(hash_table, uw_hash(*key_ptr), kv_index);
        key_ptr += 2;
        n -= 2;
        kv_index++;
    }

    // dispose old map
    _uw_free_map(map);

    return new_map;
}

static bool update_map(UwValuePtr map, UwValuePtr key, UwValuePtr value)
{
    struct _UwMap* m = map->map_value;
    struct _UwHashTable* hash_table = &m->hash_table;

    // lookup key in the map

    size_t ht_offset;
    size_t key_index = lookup(m, key, nullptr, &ht_offset);

    if (key_index != SIZE_MAX) {
        // found key, update value

        size_t value_index = key_index + 1;
        UwValuePtr* v_ptr = &m->kv_pairs->items[value_index];

        // update only if value is different
        if (*v_ptr != value) {
            uw_delete(v_ptr);
            *v_ptr = uw_makeref(value);
        }
        return true;
    }

    // key not found, insert

    size_t quarter_cap = hash_table->capacity / 4;
    if (ht_offset > quarter_cap || (hash_table->capacity - hash_table->items_used) < quarter_cap) {

        // too long lookup and too few space left --> resize!

        m = double_hash_table(m);
        if (!m) {
            return false;
        }

        // update changed stuff
        map->map_value = m;
        hash_table = &m->hash_table;
    }

    size_t kv_index = _uw_list_length(m->kv_pairs) / 2;

    size_t ht_index = set_hash_table_item(hash_table, uw_hash(key), kv_index + 1);

    if (!_uw_list_append(&m->kv_pairs, uw_makeref(key))) {
        goto error;
    }
    if (!_uw_list_append(&m->kv_pairs, uw_makeref(value))) {
        size_t len = m->kv_pairs->length;
        _uw_list_del(m->kv_pairs, len - 1, len);
        goto error;
    }

    hash_table->items_used++;
    return true;

error:
    // append to list failed, reset hast table item
    hash_table->set_item(hash_table, ht_index, 0);
    return false;
}

bool uw_map_update(UwValuePtr map, UwValuePtr key, UwValuePtr value)
{
    uw_assert_map(map);
    uw_assert(key != map);
    uw_assert(value != map);

    return update_map(map, key, value);
}

bool uw_map_update_va(UwValuePtr map, ...)
{
    va_list ap;
    va_start(ap);
    bool result = uw_map_update_ap(map, ap);
    va_end(ap);
    return result;
}

bool uw_map_update_ap(UwValuePtr map, va_list ap)
{
    uw_assert_map(map);

    for (;;) {
        int ctype = va_arg(ap, int);
        if (ctype == -1) {
            break;
        }
        {   // nested scope for autocleaning
            UwValue key = uw_create_from_ctype(ctype, ap);
            ctype = va_arg(ap, int);
            UwValue value = uw_create_from_ctype(ctype, ap);
            if (!value) {
                return false;
            }
            if (!update_map(map, key, value)) {
                return false;
            }
        }
    }
    return true;
}

bool _uw_map_has_key_null(UwValuePtr map, UwType_Null key)
{
    UwValue k = _uw_create_null();
    uw_assert(k != nullptr);
    return _uw_map_has_key_uw(map, k);
}

bool _uw_map_has_key_bool(UwValuePtr map, UwType_Bool key)
{
    UwValue k = _uwc_create_bool(key);
    uw_assert(k != nullptr);
    return _uw_map_has_key_uw(map, k);
}

bool _uw_map_has_key_int(UwValuePtr map, UwType_Int key)
{
    UwValue k = _uwc_create_int(key);
    uw_assert(k != nullptr);
    return _uw_map_has_key_uw(map, k);
}

bool _uw_map_has_key_float(UwValuePtr map, UwType_Float key)
{
    UwValue k = _uwc_create_float(key);
    uw_assert(k != nullptr);
    return _uw_map_has_key_uw(map, k);
}

bool _uw_map_has_key_u8(UwValuePtr map, char8_t* key)
{
    UwValue k = _uw_create_string_u8(key);
    uw_assert(k != nullptr);
    return _uw_map_has_key_uw(map, k);
}

bool _uw_map_has_key_u32(UwValuePtr map, char32_t* key)
{
    UwValue k = _uw_create_string_u32(key);
    uw_assert(k != nullptr);
    return _uw_map_has_key_uw(map, k);
}

bool _uw_map_has_key_uw(UwValuePtr map, UwValuePtr key)
{
    uw_assert_map(map);

    struct _UwMap* m = map->map_value;
    struct _UwHashTable* hash_table = &m->hash_table;

    // lookup key in the map

    return lookup(m, key, nullptr, nullptr) != SIZE_MAX;
}

UwValuePtr _uw_map_get_null(UwValuePtr map, UwType_Null key)
{
    UwValue k = _uw_create_null();
    uw_assert(k != nullptr);
    return _uw_map_get_uw(map, k);
}

UwValuePtr _uw_map_get_bool(UwValuePtr map, UwType_Bool key)
{
    UwValue k = _uwc_create_bool(key);
    uw_assert(k != nullptr);
    return _uw_map_get_uw(map, k);
}

UwValuePtr _uw_map_get_int(UwValuePtr map, UwType_Int key)
{
    UwValue k = _uwc_create_int(key);
    uw_assert(k != nullptr);
    return _uw_map_get_uw(map, k);
}

UwValuePtr _uw_map_get_float(UwValuePtr map, UwType_Float key)
{
    UwValue k = _uwc_create_float(key);
    uw_assert(k != nullptr);
    return _uw_map_get_uw(map, k);
}

UwValuePtr _uw_map_get_u8(UwValuePtr map, char8_t* key)
{
    UwValue k = _uw_create_string_u8(key);
    uw_assert(k != nullptr);
    return _uw_map_get_uw(map, k);
}

UwValuePtr _uw_map_get_u32(UwValuePtr map, char32_t* key)
{
    UwValue k = _uw_create_string_u32(key);
    uw_assert(k != nullptr);
    return _uw_map_get_uw(map, k);
}

UwValuePtr _uw_map_get_uw(UwValuePtr map, UwValuePtr key)
{
    uw_assert_map(map);

    struct _UwMap* m = map->map_value;
    struct _UwHashTable* hash_table = &m->hash_table;

    // lookup key in the map

    size_t key_index = lookup(m, key, nullptr, nullptr);

    if (key_index == SIZE_MAX) {
        // key not found
        return nullptr;
    }

    // return value
    size_t value_index = key_index + 1;
    UwValuePtr v = m->kv_pairs->items[value_index];
    return uw_makeref(v);
}

void _uw_map_del_null(UwValuePtr map, UwType_Null key)
{
    UwValue k = _uw_create_null();
    uw_assert(k != nullptr);
    _uw_map_del_uw(map, k);
}

void _uw_map_del_bool(UwValuePtr map, UwType_Bool key)
{
    UwValue k = _uwc_create_bool(key);
    uw_assert(k != nullptr);
    _uw_map_del_uw(map, k);
}

void _uw_map_del_int(UwValuePtr map, UwType_Int key)
{
    UwValue k = _uwc_create_int(key);
    uw_assert(k != nullptr);
    _uw_map_del_uw(map, k);
}

void _uw_map_del_float(UwValuePtr map, UwType_Float key)
{
    UwValue k = _uwc_create_float(key);
    uw_assert(k != nullptr);
    _uw_map_del_uw(map, k);
}

void _uw_map_del_u8(UwValuePtr map, char8_t* key)
{
    UwValue k = _uw_create_string_u8(key);
    uw_assert(k != nullptr);
    _uw_map_del_uw(map, k);
}

void _uw_map_del_u32(UwValuePtr map, char32_t* key)
{
    UwValue k = _uw_create_string_u32(key);
    uw_assert(k != nullptr);
    _uw_map_del_uw(map, k);
}

void _uw_map_del_uw(UwValuePtr map, UwValuePtr key)
{
    uw_assert_map(map);

    struct _UwMap* m = map->map_value;
    struct _UwHashTable* hash_table = &m->hash_table;

    // lookup key in the map

    size_t ht_index;
    size_t key_index = lookup(m, key, &ht_index, nullptr);

    if (key_index == SIZE_MAX) {
        // key not found
        return;
    }

    // delete item from hash table
    hash_table->set_item(hash_table, ht_index, 0);
    hash_table->items_used--;

    // delete key-value pair
    _uw_list_del(m->kv_pairs, key_index, key_index + 2);

    if (key_index + 2 < m->kv_pairs->length) {
        // key-value was not the last pair in the list,
        // decrement indexes in the hash table that are greater than index of the deleted pair
        size_t threshold = (key_index + 2) >> 1;
        threshold++; // kv_indexes in hash table are 1-based
        for (size_t i = 0; i < hash_table->capacity; i++) {
            size_t kv_index = hash_table->get_item(hash_table, i);
            if (kv_index >= threshold) {
                hash_table->set_item(hash_table, i, kv_index - 1);
            }
        }
    }
}

bool uw_map_item(UwValuePtr map, size_t index, UwValuePtr* key, UwValuePtr* value)
{
    uw_assert_map(map);

    struct _UwMap* m = map->map_value;

    index <<= 1;

    if (index < _uw_list_length(m->kv_pairs)) {
        UwValuePtr* ptr = &m->kv_pairs->items[index];
        *key = uw_makeref(*ptr);
        ptr++;
        *value = uw_makeref(*ptr);
        return true;

    } else {
        *key = nullptr;
        *value = nullptr;
        return false;
    }
}
