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
    UwValuePtr items;
    unsigned length;
    unsigned capacity;
};

struct _UwListExtraData {
    // outline structure to determine correct aligned offset
    _UwCompoundData value_data;
    struct _UwList  list_data;
};

#define _uw_get_list_ptr(value)  \
    (  \
        &((struct _UwListExtraData*) ((value)->extra_data))->list_data \
    )

/****************************************************************
 * Basic interface methods
 */

void     _uw_list_destroy       (UwValuePtr self);
UwResult _uw_list_init          (UwValuePtr self, va_list ap);
void     _uw_list_fini          (UwValuePtr self);
UwResult _uw_list_clone         (UwValuePtr self);
void     _uw_list_hash          (UwValuePtr self, UwHashContext* ctx);
UwResult _uw_list_deepcopy      (UwValuePtr self);
void     _uw_list_dump          (UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail);
UwResult _uw_list_to_string     (UwValuePtr self);
bool     _uw_list_is_true       (UwValuePtr self);
bool     _uw_list_equal_sametype(UwValuePtr self, UwValuePtr other);
bool     _uw_list_equal         (UwValuePtr self, UwValuePtr other);

/****************************************************************
 * Helpers
 */

static inline unsigned _uw_list_length(struct _UwList* list)
{
    return list->length;
}

static inline unsigned _uw_list_capacity(struct _UwList* list)
{
    return list->capacity;
}

static inline UwValuePtr _uw_list_item(struct _UwList* list, unsigned index)
{
    return &list->items[index];
}

bool _uw_alloc_list(UwTypeId type_id, struct _UwList* list, unsigned capacity);
/*
 * - allocate list items
 * - set list->length = 0
 * - set list->capacity = rounded capacity
 *
 * Return true if list->items is not nullptr.
 */

bool _uw_list_resize(UwTypeId type_id, struct _UwList* list, unsigned desired_capacity);
/*
 * Reallocate list.
 */

void _uw_destroy_list(UwTypeId type_id, struct _UwList* list, _UwCompoundData* parent_cdata);
/*
 * Call destructor for all items and free the list items.
 * For compound values call _uw_abandon before the destructor.
 */

bool _uw_list_eq(struct _UwList* a, struct _UwList* b);
/*
 * Compare for equality.
 */

bool _uw_list_append_item(UwTypeId type_id, struct _UwList* list, UwValuePtr item, UwValuePtr parent);
/*
 * Append: move `item` on the list using uw_move() and call _uw_embrace(parent, item)
 */

UwResult _uw_list_pop(struct _UwList* list);
/*
 * Pop item from list.
 */

void _uw_list_del(struct _UwList* list, unsigned start_index, unsigned end_index);
/*
 * Delete items from list.
 */

#ifdef __cplusplus
}
#endif
