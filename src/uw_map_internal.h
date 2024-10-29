#pragma once

/*
 * Map internals.
 */

#include "src/uw_list_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************
 * Basic interface methods
 */

UwValuePtr _uw_create_map        ();  // this prototype is duplicated in uw_map.h
void       _uw_destroy_map       (UwValuePtr self);
void       _uw_hash_map          (UwValuePtr self, UwHashContext* ctx);
UwValuePtr _uw_copy_map          (UwValuePtr self);
void       _uw_dump_map          (UwValuePtr self, int indent);
UwValuePtr _uw_map_to_string     (UwValuePtr self);
bool       _uw_map_is_true       (UwValuePtr self);
bool       _uw_map_equal_sametype(UwValuePtr self, UwValuePtr other);
bool       _uw_map_equal         (UwValuePtr self, UwValuePtr other);
bool       _uw_map_equal_ctype   (UwValuePtr self, UwCType ctype, ...);

/****************************************************************
 * Hash table and _UwMap structures
 */

struct _UwHashTable;

typedef size_t (*_UwHtGet)(struct _UwHashTable* ht, size_t index);
typedef void   (*_UwHtSet)(struct _UwHashTable* ht, size_t index, size_t value);

struct _UwHashTable {
    uint8_t item_size;   // in bytes
    UwType_Hash hash_bitmask;  // calculated from item_size
    size_t items_used;
    size_t capacity;
    _UwHtGet get_item;  // getter function for specific item size
    _UwHtSet set_item;  // setter function for specific item size
    uint8_t items[];    // items have variable size
};

struct _UwMap {
    struct _UwList* kv_pairs;        // key-value pairs in the insertion order
    struct _UwHashTable hash_table;
};

/****************************************************************
 * Allocator
 */

// capacity must be power of two, it doubles when map needs to grow
#define UWMAP_INITIAL_CAPACITY  8

struct _UwMap* _uw_alloc_map(size_t capacity);
#define _uw_free_map(map)  free(map)

/****************************************************************
 * Helpers
 */

void _uw_delete_map(struct _UwMap* map);
/*
 * Call destructor for all items and free the map itself.
 */

bool _uw_map_eq(struct _UwMap* a, struct _UwMap* b);
/*
 * Compare for equality.
 */

#ifdef __cplusplus
}
#endif
