#pragma once

/*
 * Map internals.
 */

#include "src/uw_list_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

// capacity must be power of two, it doubles when map needs to grow
#define UWMAP_INITIAL_CAPACITY  8

struct _UwHashTable;

typedef unsigned (*_UwHtGet)(struct _UwHashTable* ht, unsigned index);
typedef void     (*_UwHtSet)(struct _UwHashTable* ht, unsigned index, unsigned value);

struct _UwHashTable {
    uint8_t item_size;   // in bytes
    UwType_Hash hash_bitmask;  // calculated from item_size
    unsigned items_used;
    unsigned capacity;
    _UwHtGet get_item;  // getter function for specific item size
    _UwHtSet set_item;  // setter function for specific item size
    uint8_t* items;     // items have variable size
};

struct _UwMap {
    struct _UwList kv_pairs;        // key-value pairs in the insertion order
    struct _UwHashTable hash_table;
};

#define _uw_get_map_ptr(value)  \
    (  \
        (struct _UwMap*) (  \
            ((uint8_t*) (value)) + sizeof(struct _UwValue) \
        )  \
    )

/****************************************************************
 * Basic interface methods
 */

bool       _uw_init_map          (UwValuePtr self);
void       _uw_fini_map          (UwValuePtr self);
void       _uw_map_unbrace       (UwValuePtr self);
void       _uw_hash_map          (UwValuePtr self, UwHashContext* ctx);
UwValuePtr _uw_copy_map          (UwValuePtr self);
void       _uw_dump_map          (UwValuePtr self, int indent, struct _UwValueChain* prev_compound);
UwValuePtr _uw_map_to_string     (UwValuePtr self);
bool       _uw_map_is_true       (UwValuePtr self);
bool       _uw_map_equal_sametype(UwValuePtr self, UwValuePtr other);
bool       _uw_map_equal         (UwValuePtr self, UwValuePtr other);
bool       _uw_map_equal_ctype   (UwValuePtr self, UwCType ctype, ...);

#ifdef __cplusplus
}
#endif
