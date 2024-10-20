#include <string.h>

#include "uw_value.h"

// capacity must be power of two, it doubles when map needs to grow
#define UWMAP_INITIAL_CAPACITY  8  // must be power of two

// forward declaration
static void update_map(UwValuePtr map, UwValuePtr key, UwValuePtr value);

/*
 * optimized methods for acessing hash table
 */
#define HT_ITEM_METHODS(typename) \
    static size_t get_ht_item_##typename(struct __UwHashTable* ht, size_t index) \
    { \
        return ((typename*) (ht->items))[index]; \
    } \
    static void set_ht_item_##typename(struct __UwHashTable* ht, size_t index, size_t value) \
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
static size_t get_ht_item(struct __UwHashTable* ht, size_t index)
{
    uint8_t *item_ptr = &ht->items[index * ht->item_size];
    size_t result = 0;
    for (uint8_t i = ht->item_size; i > 0; i--) {
        result <<= 8;
        result += *item_ptr++;
    }
    return result;
}

static void set_ht_item(struct __UwHashTable* ht, size_t index, size_t value)
{
    uint8_t *item_ptr = &ht->items[(index + 1) * ht->item_size];
    for (uint8_t i = ht->item_size; i > 0; i--) {
        *(--item_ptr) = (uint8_t) value;
        value >>= 8;
    }
}

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

static _UwMap* allocate_map(size_t capacity)
/*
 * Create internal map structure for UwValue.map_value
 *
 * This function does not allocate kv_pairs because when
 * hash table is resized, which means allocating new map,
 * an existing kv_pairs will be used.
 */
{
    size_t item_size = get_item_size(capacity);
    _UwMap* map = malloc(sizeof(_UwMap) + item_size * capacity);
    if (!map) {
        perror(__func__);
        exit(1);
    }

    _UwHashTable* ht = &map->hash_table;

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

UwValuePtr uw_create_map()
{
    UwValuePtr value = _uw_alloc_value();
    value->type_id = UwTypeId_Map;
    value->map_value = allocate_map(UWMAP_INITIAL_CAPACITY);
    value->map_value->kv_pairs = _uw_alloc_list(UWMAP_INITIAL_CAPACITY * 2);
    return value;
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
    UwValue map = uw_create_map();
    uw_map_update_ap(map, ap);
    return uw_ptr(map);
}

void _uw_delete_map(_UwMap* map)
{
    _uw_delete_list(map->kv_pairs);
    free(map);
}

int _uw_map_cmp(_UwMap* a, _UwMap* b)
{
    if (a == b) {
        // compare with self
        return UW_EQ;
    }
    return _uw_list_cmp(a->kv_pairs, b->kv_pairs);
}

bool _uw_map_eq(_UwMap* a, _UwMap* b)
{
    if (a == b) {
        // compare with self
        return true;
    }
    return _uw_list_eq(a->kv_pairs, b->kv_pairs);
}

static size_t lookup(_UwMap* map, UwValuePtr key, size_t* ht_index, size_t* ht_offset)
/*
 * Lookup key starting from index = hash(key).
 *
 * Return index of key in kv_pairs or SIZE_MAX if hash table has no item matching `key`.
 *
 * If `ht_index` is not `nullptr`: write index of hash table item at which lookup has stopped.
 * If `ht_offset` is not `nullptr`: write the difference from final `ht_index` and initial `ht_index` to `ht_offset`;
 */
{
    _UwHashTable* hash_table = &map->hash_table;
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

        UwValuePtr k = *_uw_list_item_ptr(map->kv_pairs, kv_index * 2);

        // compare keys
        if (uw_equal(k, key)) {
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

static void set_hash_table_item(_UwHashTable* hash_table, size_t ht_index, size_t kv_index)
/*
 * Assign `kv_index` to `hash_table` at position `ht_index` & hash_bitmask.
 * If the position is already occupied, try next one.
 */
{
    do {
        ht_index &= hash_table->hash_bitmask;
        if (hash_table->get_item(hash_table, ht_index)) {
            ht_index++;
        } else {
            hash_table->set_item(hash_table, ht_index, kv_index);
            return;
        }
    } while (true);
}

static inline _UwMap* double_hash_table(_UwMap* map)
/*
 * Helper functtion for uw_map_update.
 *
 * Double the capacity of map's hash table and rebuild it.
 * The hash table is embedded into map structure, so re-allocate entire map.
 */
{
    _UwMap* new_map = allocate_map(map->hash_table.capacity * 2);
    new_map->kv_pairs = map->kv_pairs;
    new_map->hash_table.items_used = map->hash_table.items_used;

    _UwHashTable* hash_table = &new_map->hash_table;

    // rebuild hash table
    UwValuePtr* key_ptr = _uw_list_item_ptr(new_map->kv_pairs, 0);
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
    free(map);

    return new_map;
}

static void update_map(UwValuePtr map, UwValuePtr key, UwValuePtr value)
{
    _UwMap* m = map->map_value;
    _UwHashTable* hash_table = &m->hash_table;

    // lookup key in the map

    size_t ht_offset;
    size_t key_index = lookup(m, key, nullptr, &ht_offset);

    if (key_index != SIZE_MAX) {
        // found key, update value

        size_t value_index = key_index + 1;
        UwValuePtr* v_ptr = _uw_list_item_ptr(m->kv_pairs, value_index);

        // update only if value is different
        if (*v_ptr != value) {
            uw_delete_value(v_ptr);
            *v_ptr = uw_makeref(value);
        }
        return;
    }

    // key not found, insert

    size_t quarter_cap = hash_table->capacity / 4;
    if (ht_offset > quarter_cap || (hash_table->capacity - hash_table->items_used) < quarter_cap) {

        // too long lookup and too few space left --> resize!

        m = double_hash_table(m);

        // update changed stuff
        map->map_value = m;
        hash_table = &m->hash_table;
    }

    size_t kv_index = _uw_list_length(m->kv_pairs) / 2;

    set_hash_table_item(hash_table, uw_hash(key), kv_index + 1);

    _uw_list_append(&m->kv_pairs, key);
    _uw_list_append(&m->kv_pairs, value);

    hash_table->items_used++;
}

void uw_map_update(UwValuePtr map, UwValuePtr key, UwValuePtr value)
{
    uw_assert_map(map);
    uw_assert(key != map);
    uw_assert(value != map);

    update_map(map, key, value);
}

void uw_map_update_va(UwValuePtr map, ...)
{
    va_list ap;
    va_start(ap);
    uw_map_update_ap(map, ap);
    va_end(ap);
}

void uw_map_update_ap(UwValuePtr map, va_list ap)
{
    uw_assert_map(map);

    for (;;) {
        int ctype = va_arg(ap, int);
        if (ctype == -1) {
            break;
        }
        UwValue key = uw_create_from_ctype(ctype, ap);

        ctype = va_arg(ap, int);
        if (ctype == -1) {
            break;
        }
        UwValue value = uw_create_from_ctype(ctype, ap);

        update_map(map, key, value);

        uw_delete_value(&key);
        uw_delete_value(&value);
    }
}

bool _uw_map_has_key_null(UwValuePtr map, UwType_Null key)
{
    UwValue k = _uw_create_null(nullptr);
    return _uw_map_has_key_uw(map, k);
}

bool _uw_map_has_key_bool(UwValuePtr map, UwType_Bool key)
{
    UwValue k = _uw_create_bool(key);
    return _uw_map_has_key_uw(map, k);
}

bool _uw_map_has_key_int(UwValuePtr map, UwType_Int key)
{
    UwValue k = _uw_create_int(key);
    return _uw_map_has_key_uw(map, k);
}

bool _uw_map_has_key_float(UwValuePtr map, UwType_Float key)
{
    UwValue k = _uw_create_float(key);
    return _uw_map_has_key_uw(map, k);
}

bool _uw_map_has_key_u8_wrapper(UwValuePtr map, char* key)
{
    UwValue k = _uw_create_string_u8_wrapper(key);
    return _uw_map_has_key_uw(map, k);
}

bool _uw_map_has_key_u8(UwValuePtr map, char8_t* key)
{
    UwValue k = _uw_create_string_u8(key);
    return _uw_map_has_key_uw(map, k);
}

bool _uw_map_has_key_u32(UwValuePtr map, char32_t* key)
{
    UwValue k = _uw_create_string_u32(key);
    return _uw_map_has_key_uw(map, k);
}

bool _uw_map_has_key_uw(UwValuePtr map, UwValuePtr key)
{
    uw_assert_map(map);

    _UwMap* m = map->map_value;
    _UwHashTable* hash_table = &m->hash_table;

    // lookup key in the map

    return lookup(m, key, nullptr, nullptr) != SIZE_MAX;
}

UwValuePtr _uw_map_get_null(UwValuePtr map, UwType_Null key)
{
    UwValue k = _uw_create_null(nullptr);
    return _uw_map_get_uw(map, k);
}

UwValuePtr _uw_map_get_bool(UwValuePtr map, UwType_Bool key)
{
    UwValue k = _uw_create_bool(key);
    return _uw_map_get_uw(map, k);
}

UwValuePtr _uw_map_get_int(UwValuePtr map, UwType_Int key)
{
    UwValue k = _uw_create_int(key);
    return _uw_map_get_uw(map, k);
}

UwValuePtr _uw_map_get_float(UwValuePtr map, UwType_Float key)
{
    UwValue k = _uw_create_float(key);
    return _uw_map_get_uw(map, k);
}

UwValuePtr _uw_map_get_u8_wrapper(UwValuePtr map, char* key)
{
    UwValue k = _uw_create_string_u8_wrapper(key);
    return _uw_map_get_uw(map, k);
}

UwValuePtr _uw_map_get_u8(UwValuePtr map, char8_t* key)
{
    UwValue k = _uw_create_string_u8(key);
    return _uw_map_get_uw(map, k);
}

UwValuePtr _uw_map_get_u32(UwValuePtr map, char32_t* key)
{
    UwValue k = _uw_create_string_u32(key);
    return _uw_map_get_uw(map, k);
}

UwValuePtr _uw_map_get_uw(UwValuePtr map, UwValuePtr key)
{
    uw_assert_map(map);

    _UwMap* m = map->map_value;
    _UwHashTable* hash_table = &m->hash_table;

    // lookup key in the map

    size_t key_index = lookup(m, key, nullptr, nullptr);

    if (key_index == SIZE_MAX) {
        // key not found
        return nullptr;
    }

    // return value
    size_t value_index = key_index + 1;
    UwValuePtr v = *_uw_list_item_ptr(m->kv_pairs, value_index);
    return uw_makeref(v);
}

void _uw_map_del_null(UwValuePtr map, UwType_Null key)
{
    UwValue k = _uw_create_null(nullptr);
    _uw_map_del_uw(map, k);
}

void _uw_map_del_bool(UwValuePtr map, UwType_Bool key)
{
    UwValue k = _uw_create_bool(key);
    _uw_map_del_uw(map, k);
}

void _uw_map_del_int(UwValuePtr map, UwType_Int key)
{
    UwValue k = _uw_create_int(key);
    _uw_map_del_uw(map, k);
}

void _uw_map_del_float(UwValuePtr map, UwType_Float key)
{
    UwValue k = _uw_create_float(key);
    _uw_map_del_uw(map, k);
}

void _uw_map_del_u8_wrapper(UwValuePtr map, char* key)
{
    UwValue k = _uw_create_string_u8_wrapper(key);
    _uw_map_del_uw(map, k);
}

void _uw_map_del_u8(UwValuePtr map, char8_t* key)
{
    UwValue k = _uw_create_string_u8(key);
    _uw_map_del_uw(map, k);
}

void _uw_map_del_u32(UwValuePtr map, char32_t* key)
{
    UwValue k = _uw_create_string_u32(key);
    _uw_map_del_uw(map, k);
}

void _uw_map_del_uw(UwValuePtr map, UwValuePtr key)
{
    uw_assert_map(map);

    _UwMap* m = map->map_value;
    _UwHashTable* hash_table = &m->hash_table;

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
    _uw_list_del(m->kv_pairs, key_index, key_index + 1);

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

    _UwMap* m = map->map_value;

    index <<= 1;

    if (index < _uw_list_length(m->kv_pairs)) {
        UwValuePtr* ptr = _uw_list_item_ptr(m->kv_pairs, index);
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

UwValuePtr _uw_copy_map(_UwMap* map)
{
    size_t length = _uw_list_length(map->kv_pairs) / 2;

    // find nearest power of two for hash table capacity
    size_t ht_capacity = UWMAP_INITIAL_CAPACITY;
    while (ht_capacity < length) {
        ht_capacity <<= 1;
    }

    length *= 2;

    UwValuePtr new_map = _uw_alloc_value();
    new_map->type_id = UwTypeId_Map;
    new_map->map_value = allocate_map(ht_capacity);
    new_map->map_value->kv_pairs = _uw_alloc_list(length);

    // deep copy
    for (size_t i = 0; i < length;) {
        UwValuePtr key = uw_copy(map->kv_pairs->items[i++]);
        UwValuePtr value = uw_copy(map->kv_pairs->items[i++]);
        update_map(new_map, key, value);
        uw_delete_value(&key);
        uw_delete_value(&value);
    }
    return new_map;
}

#ifdef DEBUG

void _uw_dump_map(_UwMap* map, int indent)
{
    // continue current line
    printf("%zu key+values, capacity=%zu\n", map->kv_pairs->length, map->kv_pairs->capacity);

    indent += 4;
    for (size_t i = 0; i < map->kv_pairs->length; i++) {
        char label[64];
        sprintf(label, "Key : ");
        _uw_dump_value(map->kv_pairs->items[i], indent, label);
        i++;
        sprintf(label, "Value: ");
        _uw_dump_value(map->kv_pairs->items[i], indent, label);
    }

    _uw_print_indent(indent);

    _UwHashTable* ht = &map->hash_table;
    printf("hash table item size %u, items_used=%zu, capacity=%zu (bitmask %llx)\n",
           ht->item_size, ht->items_used, ht->capacity, (unsigned long long) ht->hash_bitmask);

    for (size_t i = 0; i < ht->capacity; i++ ) {
        size_t kv_index = ht->get_item(ht, i);
        _uw_print_indent(indent);
        printf("%zx: %zu\n", i, kv_index);
    }
}

#endif
