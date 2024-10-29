#pragma once

/*
 * List internals.
 */

#include "include/uw_base.h"

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************
 * Basic interface methods
 */

UwValuePtr _uw_create_list        ();  // this prototype is duplicated in uw_list.h
void       _uw_destroy_list       (UwValuePtr self);
void       _uw_hash_list          (UwValuePtr self, UwHashContext* ctx);
UwValuePtr _uw_copy_list          (UwValuePtr self);
void       _uw_dump_list          (UwValuePtr self, int indent);
UwValuePtr _uw_list_to_string     (UwValuePtr self);
bool       _uw_list_is_true       (UwValuePtr self);
bool       _uw_list_equal_sametype(UwValuePtr self, UwValuePtr other);
bool       _uw_list_equal         (UwValuePtr self, UwValuePtr other);
bool       _uw_list_equal_ctype   (UwValuePtr self, UwCType ctype, ...);

/****************************************************************
 * The list is a variable-length structure
 */

struct _UwList {
    size_t length;
    size_t capacity;
    struct _UwValue* items[];
};

/****************************************************************
 * Allocator
 */

// the following constants must be power of two:
#define UWLIST_INITIAL_CAPACITY    16
#define UWLIST_CAPACITY_INCREMENT  16

struct _UwList* _uw_alloc_list(size_t capacity);
#define _uw_free_list(list)  free(list)

/****************************************************************
 * Helpers
 */

void _uw_delete_list(struct _UwList* list);
/*
 * Call destructor for all items and free the list itself.
 */

bool _uw_list_eq(struct _UwList* a, struct _UwList* b);
/*
 * Compare for equality.
 */

bool _uw_list_append(struct _UwList** list_ref, UwValuePtr item);
/*
 * Append an item to the list.
 * This function does not increment item's refcount!
 */

void _uw_list_del(struct _UwList* list, size_t start_index, size_t end_index);
/*
 * Delete items from list.
 */

#ifdef __cplusplus
}
#endif
