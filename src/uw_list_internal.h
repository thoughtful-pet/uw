#pragma once

/*
 * List internals.
 */

#include "include/uw_base.h"

#ifdef __cplusplus
extern "C" {
#endif

// the following constants must be power of two:
#define UWLIST_INITIAL_CAPACITY    4
#define UWLIST_CAPACITY_INCREMENT  16

struct _UwList {
    size_t length;
    size_t capacity;
    UwValuePtr* items;
};

#define _uw_get_list_ptr(value)  \
    (  \
        (struct _UwList*) (  \
            ((uint8_t*) (value)) + sizeof(_UwValueBase) \
        )  \
    )

/****************************************************************
 * Basic interface methods
 */

bool       _uw_init_list          (UwValuePtr self);
void       _uw_fini_list          (UwValuePtr self);
void       _uw_hash_list          (UwValuePtr self, UwHashContext* ctx);
UwValuePtr _uw_copy_list          (UwValuePtr self);
void       _uw_dump_list          (UwValuePtr self, int indent);
UwValuePtr _uw_list_to_string     (UwValuePtr self);
bool       _uw_list_is_true       (UwValuePtr self);
bool       _uw_list_equal_sametype(UwValuePtr self, UwValuePtr other);
bool       _uw_list_equal         (UwValuePtr self, UwValuePtr other);
bool       _uw_list_equal_ctype   (UwValuePtr self, UwCType ctype, ...);

/****************************************************************
 * Helpers
 */

static inline size_t _uw_list_length(struct _UwList* list)
{
    return list->length;
}

static inline UwValuePtr _uw_list_item(struct _UwList* list, size_t index)
{
    return list->items[index];
}

static inline UwValuePtr* _uw_list_item_ptr(struct _UwList* list, size_t index)
{
    return &list->items[index];
}

bool _uw_alloc_list(UwAllocId alloc_id, struct _UwList* list, size_t capacity);
/*
 * - allocate list items
 * - set list->length = 0
 * - set list->capacity = rounded capacity
 *
 * Return true if list->items is not nullptr.
 */

void _uw_delete_list(UwAllocId alloc_id, struct _UwList* list);
/*
 * Call destructor for all items and free the list items.
 */

bool _uw_list_eq(struct _UwList* a, struct _UwList* b);
/*
 * Compare for equality.
 */

bool _uw_list_append(UwAllocId alloc_id, struct _UwList* list, UwValuePtr item);
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
