#include <stdarg.h>
#include <string.h>

#include "include/uw_c.h"
#include "src/uw_list_internal.h"

bool _uw_init_list(UwValuePtr self)
{
    return _uw_alloc_list(
        self->alloc_id,
        _uw_get_list_ptr(self),
        UWLIST_INITIAL_CAPACITY
    );
}

void _uw_fini_list(UwValuePtr self)
{
    _uw_delete_list(self->alloc_id, _uw_get_list_ptr(self));
}

void _uw_list_unbrace(UwValuePtr self)
{
    struct _UwList* list = _uw_get_list_ptr(self);
    unsigned n = list->length;
    UwValuePtr* item_ref = list->items;
    while (n--) {
        _uw_unbrace(item_ref++);
    }
}

void _uw_hash_list(UwValuePtr self, UwHashContext* ctx)
{
    _uw_hash_uint64(ctx, self->type_id);
    struct _UwList* list = _uw_get_list_ptr(self);
    for (unsigned i = 0; i < list->length; i++) {
        UwValuePtr item = list->items[i];
        _uw_call_hash(item, ctx);
    }
}

UwValuePtr _uw_copy_list(UwValuePtr self)
{
    struct _UwList* list = _uw_get_list_ptr(self);
    UwValuePtr result = _uw_alloc_value(UwTypeId_List);
    if (result) {
        struct _UwList* result_list = _uw_get_list_ptr(result);
        if (!_uw_alloc_list(result->alloc_id, result_list, list->length)) {
            goto error;
        }
        // deep copy
        UwValuePtr* src_item_ptr = list->items;
        UwValuePtr* dest_item_ptr = result_list->items;
        for (unsigned i = 0; i < list->length; i++) {
            UwValuePtr new_item = uw_copy(*src_item_ptr++);
            if (!new_item) {
                goto error;
            }
            *dest_item_ptr++ = new_item;
            result_list->length++;
        }
    }
    return result;

error:
    uw_delete(&result);
    return nullptr;
}

void _uw_dump_list(UwValuePtr self, int indent, struct _UwValueChain* prev_compound)
{
    _uw_dump_start(self, indent);

    if (_uw_compound_value_seen(self, prev_compound))
    {
        printf(" already dumped: this");
        for (struct _UwValueChain* prevc = prev_compound; prevc; prevc = prevc->prev) {
            if (self == prevc->value) {
                printf("->this\n");
                return;
            }
            printf("->%p", prevc->value);
        }
        printf("->NULL\n");
        uw_assert(false);
        return;
    }

    struct _UwList* list = _uw_get_list_ptr(self);
    printf("%u items, capacity=%u\n", list->length, list->capacity);

    indent += 4;
    for (unsigned i = 0; i < list->length; i++) {

        UwValuePtr item = list->items[i];
        struct _UwValueChain* prevc;
        struct _UwValueChain this_compound;

        if (item->compound) {
            prevc = &this_compound;
            this_compound.prev = prev_compound;
            this_compound.value = self;
        } else {
            prevc = prev_compound;
        }

        _uw_call_dump(item, indent, prevc);
    }
}

UwValuePtr _uw_list_to_string(UwValuePtr self)
{
    // XXX not implemented yet
    return nullptr;
}

bool _uw_list_is_true(UwValuePtr self)
{
    return _uw_get_list_ptr(self)->length;
}

bool _uw_list_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return _uw_list_eq(_uw_get_list_ptr(self), _uw_get_list_ptr(other));
}

bool _uw_list_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        if (t == UwTypeId_List) {
            return _uw_list_eq(_uw_get_list_ptr(self), _uw_get_list_ptr(other));
        } else {
            // check base class
            t = _uw_types[t]->ancestor_id;
            if (t == UwTypeId_Null) {
                return false;
            }
        }
    }
}

bool _uw_list_equal_ctype(UwValuePtr self, UwCType ctype, ...)
{
    bool result = false;
    va_list ap;
    va_start(ap);
    switch (ctype) {
        case uwc_value_ptr:
        case uwc_value_makeref: {
            UwValuePtr other = va_arg(ap, UwValuePtr);
            result = _uw_list_equal(self, other);
            break;
        }
        default: break;
    }
    va_end(ap);
    return result;
}

static unsigned round_capacity(unsigned capacity)
{
    if (capacity <= UWLIST_INITIAL_CAPACITY) {
        return (capacity + UWLIST_INITIAL_CAPACITY - 1) & ~(UWLIST_INITIAL_CAPACITY - 1);
    } else {
        return (capacity + UWLIST_CAPACITY_INCREMENT - 1) & ~(UWLIST_CAPACITY_INCREMENT - 1);
    }
}

bool _uw_alloc_list(UwAllocId alloc_id, struct _UwList* list, unsigned capacity)
{
    list->length = 0;
    list->capacity = round_capacity(capacity);
    list->items = _uw_allocators[alloc_id].alloc(list->capacity * sizeof(UwValuePtr));
    return list->items != nullptr;
}

void _uw_delete_list(UwAllocId alloc_id, struct _UwList* list)
{
    if (list->items) {
        for (unsigned i = 0; i < list->length; i++) {
            UwValuePtr item = list->items[i];
            if (item) {  // can be already destroyed by unbrace
                if (item->compound) {
                    _uw_unbrace(&item);
                } else {
                    uw_delete(&item);
                }
            }
        }
        _uw_allocators[alloc_id].free(list->items);
        list->items = nullptr;
    }
}

UwValuePtr uw_create_list_va(...)
{
    va_list ap;
    va_start(ap);
    UwValuePtr list = uw_create_list_ap(ap);
    va_end(ap);
    return list;
}

UwValuePtr uw_create_list_ap(va_list ap)
{
    UwValuePtr list = uw_create_list();
    if (list) {
        if (!uw_list_append_ap(list, ap)) {
            uw_delete(&list);
        }
    }
    return list;
}

unsigned uw_list_length(UwValuePtr list)
{
    uw_assert_list(list);
    return _uw_list_length(_uw_get_list_ptr(list));
}

bool _uw_list_eq(struct _UwList* a, struct _UwList* b)
{
    unsigned n = a->length;
    if (b->length != n) {
        // lists have different lengths
        return false;
    }

    UwValuePtr* a_vptr = a->items;
    UwValuePtr* b_vptr = b->items;
    while (n) {
        if (!_uw_equal(*a_vptr, *b_vptr)) {
            return false;
        }
        n--;
        a_vptr++;
        b_vptr++;
    }
    return true;
}

bool _uw_list_append_null(UwValuePtr list, UwType_Null item)
{
    uw_assert_list(list);
    UwValuePtr v = uw_create_null();
    if (!v) {
        return false;
    }
    return _uw_list_append(list->alloc_id, _uw_get_list_ptr(list), v);
}

bool _uw_list_append_bool(UwValuePtr list, UwType_Bool item)
{
    uw_assert_list(list);
    UwValuePtr v = _uwc_create_bool(item);
    if (!v) {
        return false;
    }
    return _uw_list_append(list->alloc_id, _uw_get_list_ptr(list), v);
}

bool _uw_list_append_int(UwValuePtr list, UwType_Int item)
{
    uw_assert_list(list);
    UwValuePtr v = _uwc_create_int(item);
    if (!v) {
        return false;
    }
    return _uw_list_append(list->alloc_id, _uw_get_list_ptr(list), v);
}

bool _uw_list_append_float(UwValuePtr list, UwType_Float item)
{
    uw_assert_list(list);
    UwValuePtr v = _uwc_create_float(item);
    if (!v) {
        return false;
    }
    return _uw_list_append(list->alloc_id, _uw_get_list_ptr(list), v);
}

bool _uw_list_append_u8(UwValuePtr list, char8_t* item)
{
    uw_assert_list(list);
    UwValuePtr v = _uw_create_string_u8(item);
    if (!v) {
        return false;
    }
    return _uw_list_append(list->alloc_id, _uw_get_list_ptr(list), v);
}

bool _uw_list_append_u32(UwValuePtr list, char32_t* item)
{
    uw_assert_list(list);
    UwValuePtr v = _uw_create_string_u32(item);
    if (!v) {
        return false;
    }
    return _uw_list_append(list->alloc_id, _uw_get_list_ptr(list), v);
}

bool _uw_list_append_uw(UwValuePtr list, UwValuePtr item)
{
    uw_assert_list(list);

    // use separate variable for proper cleaning on error
    UwValuePtr item_ref = nullptr;
    if (item->compound) {
        item_ref = _uw_embrace(item);
    } else {
        item_ref = uw_makeref(item);
    }

    if (_uw_list_append(list->alloc_id, _uw_get_list_ptr(list), item_ref)) {
        // item is on the list, return leaving refcount intact
        return true;
    }
    // append failed, decrement refcount
    if (item_ref->compound) {
        _uw_unbrace(&item_ref);
    } else {
        uw_delete(&item_ref);
    }
    return false;
}


bool _uw_list_append(UwAllocId alloc_id, struct _UwList* list, UwValuePtr item)
{
    uw_assert(item != nullptr);
    uw_assert(list->length <= list->capacity);

    if (list->length == list->capacity) {
        unsigned new_capacity = round_capacity(list->capacity + UWLIST_CAPACITY_INCREMENT);
        UwValuePtr* new_items = _uw_allocators[alloc_id].realloc(list->items, new_capacity * sizeof(UwValuePtr));
        if (!new_items) {
            return false;
        }
        list->items = new_items;
        list->capacity = new_capacity;
    }
    list->items[list->length++] = item;
    return true;
}

bool uw_list_append_va(UwValuePtr list, ...)
{
    va_list ap;
    va_start(ap);
    bool result = uw_list_append_ap(list, ap);
    va_end(ap);
    return result;
}

bool uw_list_append_ap(UwValuePtr list, va_list ap)
{
    uw_assert_list(list);

    UwAllocId alloc_id = list->alloc_id;
    unsigned num_appended = 0;
    for (;;) {
        int ctype = va_arg(ap, int);
        if (ctype == -1) {
            break;
        }
        UwValuePtr item = uw_create_from_ctype(ctype, ap);
        if (!item) {
            goto error;
        }
        if (item->compound) {
            _uw_embrace(item);
            item->refcount--;
        } else {
            uw_makeref(item);
        }
        if (!_uw_list_append(alloc_id, _uw_get_list_ptr(list), item)) {
            if (item->compound) {
                _uw_unbrace(&item);
            } else {
                uw_delete(&item);
            }
            goto error;
        }
        num_appended++;
    }
    return true;

error:
    while (num_appended--) {
        // when -Wall enabled, compiler complains, but nothing is wrong here
        // {
        //     UwValue v = uw_list_pop(list);
        // }
        // anyway, have to call destructor manually
        UwValuePtr v = uw_list_pop(list);
        uw_delete(&v);
    }
    return false;
}

UwValuePtr uw_list_item(UwValuePtr self, int index)
{
    uw_assert_list(self);

    struct _UwList* list = _uw_get_list_ptr(self);

    if (index < 0) {
        index = list->length + index;
        uw_assert(index >= 0);
    } else {
        uw_assert(((unsigned) index) < list->length);
    }
    return  uw_makeref(list->items[index]);
}

UwValuePtr uw_list_pop(UwValuePtr self)
{
    uw_assert_list(self);

    struct _UwList* list = _uw_get_list_ptr(self);
    if (list->length == 0) {
        return nullptr;
    } else {
        list->length--;
        UwValuePtr* item_ptr = &list->items[list->length];
        UwValuePtr item = *item_ptr;
        *item_ptr = nullptr;
        if (item->compound) {
            uw_makeref(item);
            _uw_unbrace(&item);
        }
        return item;
    }
}

void uw_list_del(UwValuePtr self, unsigned start_index, unsigned end_index)
{
    uw_assert_list(self);
    _uw_list_del(_uw_get_list_ptr(self), start_index, end_index);
}

void _uw_list_del(struct _UwList* list, unsigned start_index, unsigned end_index)
{
    if (list->length == 0) {
        return;
    }
    if (end_index > list->length) {
        end_index = list->length;
    }
    if (start_index >= end_index) {
        return;
    }

    for (unsigned i = start_index; i < end_index; i++) {
        UwValuePtr item = list->items[i];
        if (item) {  // can be already destroyed by unbrace
            if (item->compound) {
                _uw_unbrace(&item);
            } else {
                uw_delete(&item);
            }
        }
    }

    unsigned new_length = list->length - (end_index - start_index);
    unsigned tail_length = list->length - end_index;
    if (tail_length) {
        memmove(&list->items[start_index], &list->items[end_index], tail_length * sizeof(UwValuePtr));
        memset(&list->items[new_length], 0, (list->length - new_length) * sizeof(UwValuePtr));
    }

    list->length = new_length;
}

UwValuePtr uw_list_slice(UwValuePtr self, unsigned start_index, unsigned end_index)
{
    struct _UwList* list = _uw_get_list_ptr(self);
    unsigned length = _uw_list_length(list);

    if (end_index > length) {
        end_index = length;
    }
    if (start_index >= end_index) {
        // return empty list
        return uw_create_list();
    }
    unsigned slice_len = end_index - start_index;

    UwValuePtr result = _uw_alloc_value(UwTypeId_List);
    if (!result) {
        return nullptr;
    }
    struct _UwList* result_list = _uw_get_list_ptr(result);
    if (!_uw_alloc_list(result->alloc_id, result_list, slice_len)) {
        _uw_free_value(result);
        return nullptr;
    }

    UwValuePtr* src_item_ptr = &list->items[start_index];
    UwValuePtr* dest_item_ptr = result_list->items;
    for (unsigned i = start_index; i < end_index; i++) {
        UwValuePtr item = *src_item_ptr++;
        if (item->compound) {
            _uw_embrace(item);
        } else {
            uw_makeref(item);
        }
        *dest_item_ptr++ = item;
    }
    result_list->length = slice_len;
    return result;
}
