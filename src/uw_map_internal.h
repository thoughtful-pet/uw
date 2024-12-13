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

struct _UwMapExtraData {
    _UwCompoundData value_data;
    struct _UwMap   map_data;
};

#define _uw_get_map_ptr(value)  \
    (  \
        &((struct _UwMapExtraData*) ((value)->extra_data))->map_data \
    )

/****************************************************************
 * Basic interface methods
 */

void     _uw_map_destroy       (UwValuePtr self);
UwResult _uw_map_init          (UwValuePtr self, va_list ap);
void     _uw_map_fini          (UwValuePtr self);
UwResult _uw_map_clone         (UwValuePtr self);
void     _uw_map_hash          (UwValuePtr self, UwHashContext* ctx);
UwResult _uw_map_deepcopy      (UwValuePtr self);
void     _uw_map_dump          (UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail);
UwResult _uw_map_to_string     (UwValuePtr self);
bool     _uw_map_is_true       (UwValuePtr self);
bool     _uw_map_equal_sametype(UwValuePtr self, UwValuePtr other);
bool     _uw_map_equal         (UwValuePtr self, UwValuePtr other);

#ifdef __cplusplus
}
#endif
