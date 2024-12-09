#include <limits.h>
#include <stdarg.h>
#include <string.h>

#include "include/uw_c.h"
#include "src/uw_map_internal.h"

static inline unsigned get_map_length(struct _UwMap* map)
{
    return _uw_list_length(&map->kv_pairs) >> 1;
}

static uint8_t get_item_size(unsigned capacity)
/*
 * Return hash table item size for desired capacity.
 *
 * Index 0 in hash table means no item, so one byte is capable
 * to index 255 items, not 256.
 */
{
    uint8_t item_size = 1;

    for (unsigned n = capacity; n > 255; n >>= 8) {
        item_size++;
    }
    return item_size;
}

/****************************************************************
 * methods for acessing hash table
 */

#define HT_ITEM_METHODS(typename) \
    static unsigned get_ht_item_##typename(struct _UwHashTable* ht, unsigned index) \
    { \
        return ((typename*) (ht->items))[index]; \
    } \
    static void set_ht_item_##typename(struct _UwHashTable* ht, unsigned index, unsigned value) \
    { \
        ((typename*) (ht->items))[index] = (typename) value; \
    }

HT_ITEM_METHODS(uint8_t)
HT_ITEM_METHODS(uint16_t)
HT_ITEM_METHODS(uint32_t)

#if UINT_WIDTH > 32
    HT_ITEM_METHODS(uint64_t)
#endif

/*
 * methods for acessing hash table with any item size
 */
static unsigned get_ht_item(struct _UwHashTable* ht, unsigned index)
{
    uint8_t *item_ptr = &ht->items[index * ht->item_size];
    unsigned result = 0;
    for (uint8_t i = ht->item_size; i > 0; i--) {
        result <<= 8;
        result += *item_ptr++;
    }
    return result;
}

static void set_ht_item(struct _UwHashTable* ht, unsigned index, unsigned value)
{
    uint8_t *item_ptr = &ht->items[(index + 1) * ht->item_size];
    for (uint8_t i = ht->item_size; i > 0; i--) {
        *(--item_ptr) = (uint8_t) value;
        value >>= 8;
    }
}

/****************************************************************
 * implementation
 *
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

static bool init_hash_table(UwAllocId alloc_id, struct _UwHashTable* ht, unsigned capacity)
{
    unsigned item_size = get_item_size(capacity);
    unsigned memsize = item_size * capacity;

    // reallocate items
    // if map is new, ht is initialized to all zero
    // if map is doubled, this reallocates block
    uint8_t* new_items = _uw_allocators[alloc_id].realloc(ht->items, memsize);
    if (!new_items) {
        return false;
    }
    ht->items = new_items;

    memset(ht->items, 0, memsize);

    ht->item_size    = item_size;
    ht->capacity     = capacity;
    ht->hash_bitmask = capacity - 1;

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
#if UINT_WIDTH > 32
        case 8:
            ht->get_item = get_ht_item_uint64_t;
            ht->set_item = set_ht_item_uint64_t;
            break;
#endif
        default:
            ht->get_item = get_ht_item;
            ht->set_item = set_ht_item;
            break;
    }
    return true;
}

static bool init_map(UwAllocId alloc_id, struct _UwMap* map, unsigned ht_capacity, unsigned list_capacity)
/*
 * Initialize _UwMap structure.
 */
{
    struct _UwHashTable* ht = &map->hash_table;

    ht->items_used = 0;

    if (init_hash_table(alloc_id, ht, ht_capacity)) {
        if (_uw_alloc_list(alloc_id, &map->kv_pairs, list_capacity)) {
            return true;
        }
        _uw_allocators[alloc_id].free(ht->items);
        ht->items = nullptr;
    }
    return false;
}

static void delete_map(UwAllocId alloc_id, struct _UwMap* map)
/*
 * Call destructor for all items and free allocated data.
 */
{
    _uw_delete_list(alloc_id, &map->kv_pairs);

    if (map->hash_table.items) {
        _uw_allocators[alloc_id].free(map->hash_table.items);
        map->hash_table.items = nullptr;
    }
}

static unsigned lookup(struct _UwMap* map, UwValuePtr key, unsigned* ht_index, unsigned* ht_offset)
/*
 * Lookup key starting from index = hash(key).
 *
 * Return index of key in kv_pairs or UINT_MAX if hash table has no item matching `key`.
 *
 * If `ht_index` is not `nullptr`: write index of hash table item at which lookup has stopped.
 * If `ht_offset` is not `nullptr`: write the difference from final `ht_index` and initial `ht_index` to `ht_offset`;
 *
 * XXX leverage equal_ctype method for C type keys and simplify uw_map_get_* and uw_map_has_key_* functions
 */
{
    struct _UwHashTable* ht = &map->hash_table;
    UwType_Hash index = uw_hash(key) & ht->hash_bitmask;
    unsigned offset = 0;

    do {
        unsigned kv_index = ht->get_item(ht, index);

        if (kv_index == 0) {
            // no entry matching key
            if (ht_index) {
                *ht_index = index;
            }
            if (ht_offset) {
                *ht_offset = offset;
            }
            return UINT_MAX;
        }

        // make index 0-based
        kv_index--;

        UwValuePtr k = _uw_list_item(&map->kv_pairs, kv_index * 2);

        // compare keys
        if (k->type_id == key->type_id && _uw_equal(k, key)) {
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
        index = (index + 1) & ht->hash_bitmask;
        offset++;

    } while (true);
}

static unsigned set_hash_table_item(struct _UwHashTable* hash_table, unsigned ht_index, unsigned kv_index)
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

static inline bool double_hash_table(UwAllocId alloc_id, struct _UwMap* map)
/*
 * Helper functtion for uw_map_update.
 *
 * Double the capacity of map's hash table and rebuild it.
 * The hash table is embedded into map structure, so re-allocate entire map.
 */
{
    struct _UwHashTable* ht = &map->hash_table;

    if (!init_hash_table(alloc_id, ht, ht->capacity * 2)) {
        return false;
    }

    // rebuild hash table
    UwValuePtr* key_ptr = map->kv_pairs.items;
    unsigned kv_index = 1;  // index is 1-based, zero means unused item in hash table
    unsigned n = _uw_list_length(&map->kv_pairs);
    uw_assert((n & 1) == 0);
    while (n) {
        set_hash_table_item(ht, uw_hash(*key_ptr), kv_index);
        key_ptr += 2;
        n -= 2;
        kv_index++;
    }
    return true;
}

static bool update_map(UwValuePtr self, UwValuePtr key, UwValuePtr value)
/*
 * This function increments refcount for key but does not do that for value!
 * That's because key may be added to the map or may not.
 * When adder, refcount is incremented.
 * The caller should always call uw_delete for key when this function succedes.
 */
{
    UwAllocId alloc_id = self->alloc_id;
    struct _UwMap* map = _uw_get_map_ptr(self);
    struct _UwHashTable* ht = &map->hash_table;

    // lookup key in the map

    unsigned ht_offset;
    unsigned key_index = lookup(map, key, nullptr, &ht_offset);

    if (key_index != UINT_MAX) {
        // found key, update value

        unsigned value_index = key_index + 1;
        UwValuePtr* v_ptr = _uw_list_item_ptr(&map->kv_pairs, value_index);

        // update only if value is different
        if (*v_ptr != value) {
            if ((*v_ptr)->compound) {
                _uw_unbrace(v_ptr);
            } else {
                uw_delete(v_ptr);
            }
            *v_ptr = value;
        }
        return true;
    }

    // key not found, insert

    unsigned quarter_cap = ht->capacity / 4;
    unsigned remaining_cap = ht->capacity - ht->items_used;
    if (ht_offset > quarter_cap || remaining_cap <= quarter_cap) {

        // too long lookup and too few space left --> resize!

        if (!double_hash_table(alloc_id, map)) {
            return false;
        }
    }

    unsigned kv_index = _uw_list_length(&map->kv_pairs) / 2;

    unsigned ht_index = set_hash_table_item(ht, uw_hash(key), kv_index + 1);

    UwValuePtr key_ref = uw_makeref(key);
    if (!_uw_list_append(alloc_id, &map->kv_pairs, key_ref)) {
        uw_delete(&key_ref);
        goto error;
    }
    if (!_uw_list_append(alloc_id, &map->kv_pairs, value)) {
        unsigned len = _uw_list_length(&map->kv_pairs);
        _uw_list_del(&map->kv_pairs, len - 1, len);
        goto error;
    }

    ht->items_used++;
    return true;

error:
    // append to list failed, reset hast table item
    ht->set_item(ht, ht_index, 0);
    return false;
}

/****************************************************************
 * Basic interface methods
 */

bool _uw_init_map(UwValuePtr self)
{
    return init_map(
        self->alloc_id,
        _uw_get_map_ptr(self),
        UWMAP_INITIAL_CAPACITY, UWMAP_INITIAL_CAPACITY * 2);
}

void _uw_fini_map(UwValuePtr self)
{
    delete_map(self->alloc_id, _uw_get_map_ptr(self));
}

void _uw_map_unbrace(UwValuePtr self)
{
    // unbrace values only
    // XXX when value is destroyed, do we neeed to re-create hash table?
    struct _UwMap* map = _uw_get_map_ptr(self);
    unsigned n = map->kv_pairs.length / 2;
    UwValuePtr* item_ref = map->kv_pairs.items + 1;
    while (n--) {
        _uw_unbrace(item_ref);
        item_ref += 2;
    }
}

void _uw_hash_map(UwValuePtr self, UwHashContext* ctx)
{
    _uw_hash_uint64(ctx, self->type_id);
    struct _UwMap* map = _uw_get_map_ptr(self);
    for (unsigned i = 0, n = _uw_list_length(&map->kv_pairs); i < n; i++) {
        UwValuePtr item = _uw_list_item(&map->kv_pairs, i);
        _uw_call_hash(item, ctx);
    }
}

UwValuePtr _uw_copy_map(UwValuePtr self)
{
    struct _UwMap* map = _uw_get_map_ptr(self);
    unsigned length = get_map_length(map);

    // find nearest power of two for hash table capacity
    unsigned ht_capacity = UWMAP_INITIAL_CAPACITY;
    while (ht_capacity < length) {
        ht_capacity <<= 1;
    }

    length <<= 1;  // list length from now on

    UwValuePtr result = _uw_alloc_value(UwTypeId_Map);
    if (!result) {
        return nullptr;
    }
    struct _UwMap* result_map = _uw_get_map_ptr(result);

    if (!init_map(result->alloc_id, result_map, ht_capacity, length)) {
        _uw_free_value(result);
        return nullptr;
    }

    // deep copy
    for (unsigned i = 0; i < length;) {
        // not using autocleaned variables here
        UwValuePtr key = uw_copy(map->kv_pairs.items[i++]);
        UwValuePtr value = uw_copy(map->kv_pairs.items[i++]);
        if (key && value) {
            if (value->compound) {
                _uw_embrace(value);
                value->refcount--;
            }
            if (update_map(result, key, value)) {
                // value is successfully added to the map
                // uw_delete should always be called for key
                uw_delete(&key);
                continue;
            }
        }
        // uh oh
        uw_delete(&key);
        _uw_unbrace(&value);
        uw_delete(&result);
        return nullptr;
    }
    return result;
}

void _uw_dump_map(UwValuePtr self, int indent, struct _UwValueChain* prev_compound)
{
    _uw_dump_start(self, indent);

    if (_uw_compound_value_seen(self, prev_compound))
    {
        puts(" already dumped, see above");
        return;
    }

    struct _UwMap* map = _uw_get_map_ptr(self);
    printf("%u items, capacity=%u\n", map->kv_pairs.length >> 1, map->kv_pairs.capacity >> 1);

    indent += 4;
    for (unsigned i = 0; i < map->kv_pairs.length; i++) {

        struct _UwValueChain* prevc;
        struct _UwValueChain this_compound;

        _uw_print_indent(indent);
        printf("Key:\n");
        UwValuePtr key = map->kv_pairs.items[i];

        if (key->compound) {
            prevc = &this_compound;
            this_compound.prev = prev_compound;
            this_compound.value = self;
        } else {
            prevc = prev_compound;
        }
        _uw_call_dump(key, indent, prevc);
        i++;

        _uw_print_indent(indent);
        printf("Value:\n");
        UwValuePtr value = map->kv_pairs.items[i];

        if (value->compound) {
            prevc = &this_compound;
            this_compound.prev = prev_compound;
            this_compound.value = self;
        } else {
            prevc = prev_compound;
        }
        _uw_call_dump(value, indent, prevc);
    }

    _uw_print_indent(indent);

    struct _UwHashTable* ht = &map->hash_table;
    printf("hash table item size %u, items_used=%u, capacity=%u (bitmask %llx)\n",
           ht->item_size, ht->items_used, ht->capacity, (unsigned long long) ht->hash_bitmask);

    for (unsigned i = 0; i < ht->capacity; i++ ) {
        unsigned kv_index = ht->get_item(ht, i);
        _uw_print_indent(indent);
        printf("%x: %u\n", i, kv_index);
    }
}

UwValuePtr _uw_map_to_string(UwValuePtr self)
{
    // XXX not implemented yet
    return nullptr;
}

bool _uw_map_is_true(UwValuePtr self)
{
    return get_map_length(_uw_get_map_ptr(self));
}

static inline bool map_eq(struct _UwMap* a, struct _UwMap* b)
{
    return _uw_list_eq(&a->kv_pairs, &b->kv_pairs);
}

bool _uw_map_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return map_eq(_uw_get_map_ptr(self), _uw_get_map_ptr(other));
}

bool _uw_map_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        if (t == UwTypeId_Map) {
            return map_eq(_uw_get_map_ptr(self), _uw_get_map_ptr(other));
        } else {
            // check base class
            t = _uw_types[t]->ancestor_id;
            if (t == UwTypeId_Null) {
                return false;
            }
        }
    }
}

bool _uw_map_equal_ctype(UwValuePtr self, UwCType ctype, ...)
{
    bool result = false;
    va_list ap;
    va_start(ap);
    switch (ctype) {
        case uwc_value_ptr:
        case uwc_value_makeref: {
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
 * misc functions
 */

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
    UwValuePtr map = uw_create_map();
    if (map) {
        if (!uw_map_update_ap(map, ap)) {
            uw_delete(&map);
        }
    }
    return map;
}

bool uw_map_update(UwValuePtr map, UwValuePtr key, UwValuePtr value)
{
    uw_assert_map(map);
    uw_assert(key != map);
    uw_assert(value != map);

    // use separate variables for proper cleaning on error
    UwValuePtr key_ref   = uw_copy(key);      // copy key for immutability
    UwValuePtr value_ref = nullptr;
    if (value->compound) {
        value_ref = _uw_embrace(value);
    } else {
        value_ref = uw_makeref(value);
    }

    if (update_map(map, key_ref, value_ref)) {
        // value is successfully added to the map
        // uw_delete should always be called for key
        uw_delete(&key_ref);
        return true;
    }
    // update failed
    uw_delete(&key_ref);
    if (value_ref->compound) {
        _uw_unbrace(&value_ref);
    } else {
        uw_delete(&value_ref);
    }
    return false;
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
        UwValuePtr key = uw_create_from_ctype(ctype, ap);
        if (!key) {
            return false;
        }
        ctype = va_arg(ap, int);
        UwValuePtr value = uw_create_from_ctype(ctype, ap);
        if (!value) {
            uw_delete(&key);
            return false;
        }
        if (value->compound) {
            _uw_embrace(value);
            value->refcount--;
        }
        if (!update_map(map, key, value)) {
            uw_delete(&key);
            _uw_unbrace(&value);  // no need to check for compound value
                                  // refcount is zero and it will be deleted anyway
            return false;
        }
        uw_delete(&key);
    }
    return true;
}

bool _uw_map_has_key_null(UwValuePtr map, UwType_Null key)
{
    UwValue k = uw_create_null();
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

bool _uw_map_has_key_uw(UwValuePtr self, UwValuePtr key)
{
    uw_assert_map(self);
    struct _UwMap* map = _uw_get_map_ptr(self);
    return lookup(map, key, nullptr, nullptr) != UINT_MAX;
}

UwValuePtr _uw_map_get_null(UwValuePtr map, UwType_Null key)
{
    UwValue k = uw_create_null();
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

UwValuePtr _uw_map_get_uw(UwValuePtr self, UwValuePtr key)
{
    uw_assert_map(self);
    struct _UwMap* map = _uw_get_map_ptr(self);

    // lookup key in the map
    unsigned key_index = lookup(map, key, nullptr, nullptr);

    if (key_index == UINT_MAX) {
        // key not found
        return nullptr;
    }

    // return value
    unsigned value_index = key_index + 1;
    return uw_makeref(_uw_list_item(&map->kv_pairs, value_index));
}

void _uw_map_del_null(UwValuePtr map, UwType_Null key)
{
    UwValue k = uw_create_null();
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

void _uw_map_del_uw(UwValuePtr self, UwValuePtr key)
{
    uw_assert_map(self);

    struct _UwMap* map = _uw_get_map_ptr(self);

    // lookup key in the map

    unsigned ht_index;
    unsigned key_index = lookup(map, key, &ht_index, nullptr);
    if (key_index == UINT_MAX) {
        // key not found
        return;
    }

    struct _UwHashTable* ht = &map->hash_table;

    // delete item from hash table
    ht->set_item(ht, ht_index, 0);
    ht->items_used--;

    // delete key-value pair
    _uw_list_del(&map->kv_pairs, key_index, key_index + 2);

    if (key_index + 2 < _uw_list_length(&map->kv_pairs)) {
        // key-value was not the last pair in the list,
        // decrement indexes in the hash table that are greater than index of the deleted pair
        unsigned threshold = (key_index + 2) >> 1;
        threshold++; // kv_indexes in hash table are 1-based
        for (unsigned i = 0; i < ht->capacity; i++) {
            unsigned kv_index = ht->get_item(ht, i);
            if (kv_index >= threshold) {
                ht->set_item(ht, i, kv_index - 1);
            }
        }
    }
}

unsigned uw_map_length(UwValuePtr self)
{
    uw_assert_map(self);
    return get_map_length(_uw_get_map_ptr(self));
}

bool uw_map_item(UwValuePtr self, unsigned index, UwValuePtr* key, UwValuePtr* value)
{
    uw_assert_map(self);

    struct _UwMap* map = _uw_get_map_ptr(self);

    index <<= 1;

    if (index < _uw_list_length(&map->kv_pairs)) {
        *key   = uw_makeref(_uw_list_item(&map->kv_pairs, index));
        *value = uw_makeref(_uw_list_item(&map->kv_pairs, index + 1));
        return true;

    } else {
        *key = nullptr;
        *value = nullptr;
        return false;
    }
}
