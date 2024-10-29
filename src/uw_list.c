#include <stdarg.h>
#include <string.h>

#include "include/uw_c.h"
#include "src/uw_null_internal.h"  // for _uw_create_null
#include "src/uw_list_internal.h"

UwValuePtr _uw_create_list()
{
    UwValuePtr value = _uw_alloc_value();
    if (value) {
        value->type_id = UwTypeId_List;
        value->refcount = 1;
        value->list_value = _uw_alloc_list(UWLIST_INITIAL_CAPACITY);
        if (!value->list_value) {
            _uw_free_value(value);
            value = nullptr;
        }
    }
    return value;
}

void _uw_destroy_list(UwValuePtr self)
{
    if (self) {
        if (self->list_value) {
            _uw_delete_list(self->list_value);
        }
        _uw_free_value(self);
    }
}

void _uw_hash_list(UwValuePtr self, UwHashContext* ctx)
{
    _uw_hash_uint64(ctx, self->type_id);
    struct _UwList* list = self->list_value;
    for (size_t i = 0; i < list->length; i++) {
        UwValuePtr item = list->items[i];
        _uw_call_hash(item, ctx);
    }
}

UwValuePtr _uw_copy_list(UwValuePtr self)
{
    UwValuePtr result = _uw_alloc_value();
    if (result) {
        result->type_id = UwTypeId_List;
        result->refcount = 1;
        result->list_value = _uw_alloc_list((self->list_value->length + UWLIST_CAPACITY_INCREMENT - 1) & ~(UWLIST_CAPACITY_INCREMENT - 1));
        if (!result->list_value) {
            goto error;
        }
        // deep copy
        struct _UwList* list = self->list_value;
        struct _UwList* new_list = result->list_value;
        UwValuePtr* new_item_ptr = new_list->items;
        for (size_t i = 0; i < list->length; i++) {
            UwValuePtr new_item = uw_copy(list->items[i]);
            if (!new_item) {
                goto error;
            }
            *new_item_ptr++ = new_item;
            new_list->length++;
        }
    }
    return result;

error:
    _uw_destroy_list(result);
    return nullptr;
}

void _uw_dump_list(UwValuePtr self, int indent)
{
    _uw_dump_start(self, indent);

    struct _UwList* list = self->list_value;
    printf("%zu items, capacity=%zu\n", list->length, list->capacity);

    indent += 4;
    for (size_t i = 0; i < list->length; i++) {
        _uw_call_dump(list->items[i], indent);
    }
}

UwValuePtr _uw_list_to_string(UwValuePtr self)
{
    // XXX not implemented yet
    return nullptr;
}

bool _uw_list_is_true(UwValuePtr self)
{
    return self->list_value->length;
}

bool _uw_list_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return _uw_list_eq(self->list_value, other->list_value);
}

bool _uw_list_equal(UwValuePtr self, UwValuePtr other)
{
    if (other->type_id == UwTypeId_List) {
        return _uw_list_eq(self->list_value, other->list_value);
    } else {
        return false;
    }
}

bool _uw_list_equal_ctype(UwValuePtr self, UwCType ctype, ...)
{
    bool result = false;
    va_list ap;
    va_start(ap);
    switch (ctype) {
        case uwc_value_ptr:
        case uwc_value: {
            UwValuePtr other = va_arg(ap, UwValuePtr);
            result = _uw_list_equal(self, other);
            break;
        }
        default: break;
    }
    va_end(ap);
    return result;
}

struct _UwList* _uw_alloc_list(size_t capacity)
{
    capacity = (capacity + UWLIST_CAPACITY_INCREMENT - 1) & ~(UWLIST_CAPACITY_INCREMENT - 1);

    struct _UwList* list = malloc(sizeof(struct _UwList) + capacity * sizeof(UwValuePtr));
    if (!list) {
        perror(__func__);
        exit(1);
    }
    list->length = 0;
    list->capacity = capacity;
    return list;
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
    UwValuePtr list = _uw_create_list();
    if (list) {
        if (!uw_list_append_ap(list, ap)) {
            uw_delete(&list);
        }
    }
    return list;
}

void _uw_delete_list(struct _UwList* list)
{
    for (size_t i = 0; i < list->length; i++) {
        uw_delete(&list->items[i]);
    }
    _uw_free_list(list);
}

size_t _uw_list_length(struct _UwList* list)
{
    return list->length;
}

bool _uw_list_eq(struct _UwList* a, struct _UwList* b)
{
    if (a == b) {
        // compare with self
        return true;
    }

    size_t n = a->length;
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
    UwValuePtr v = _uw_create_null();
    if (!v) {
        return false;
    }
    return _uw_list_append(&list->list_value, v);
}

bool _uw_list_append_bool(UwValuePtr list, UwType_Bool item)
{
    uw_assert_list(list);
    UwValuePtr v = _uwc_create_bool(item);
    if (!v) {
        return false;
    }
    return _uw_list_append(&list->list_value, v);
}

bool _uw_list_append_int(UwValuePtr list, UwType_Int item)
{
    uw_assert_list(list);
    UwValuePtr v = _uwc_create_int(item);
    if (!v) {
        return false;
    }
    return _uw_list_append(&list->list_value, v);
}

bool _uw_list_append_float(UwValuePtr list, UwType_Float item)
{
    uw_assert_list(list);
    UwValuePtr v = _uwc_create_float(item);
    if (!v) {
        return false;
    }
    return _uw_list_append(&list->list_value, v);
}

bool _uw_list_append_u8(UwValuePtr list, char8_t* item)
{
    uw_assert_list(list);
    UwValuePtr v = _uw_create_string_u8(item);
    if (!v) {
        return false;
    }
    return _uw_list_append(&list->list_value, v);
}

bool _uw_list_append_u32(UwValuePtr list, char32_t* item)
{
    uw_assert_list(list);
    UwValuePtr v = _uw_create_string_u32(item);
    if (!v) {
        return false;
    }
    return _uw_list_append(&list->list_value, v);
}

bool _uw_list_append_uw(UwValuePtr list, UwValuePtr item)
{
    uw_assert_list(list);
    uw_assert(list != item);
    return _uw_list_append(&list->list_value, uw_makeref(item));
}

bool _uw_list_append(struct _UwList** list_ref, UwValuePtr item)
{
    struct _UwList* list = *list_ref;

    uw_assert(item);  // item should not be nullptr
    uw_assert(list->length <= list->capacity);

    if (list->length == list->capacity) {
        size_t new_capacity = list->capacity + UWLIST_CAPACITY_INCREMENT;
        struct _UwList* new_list = realloc(list, sizeof(struct _UwList) + (new_capacity * sizeof(UwValuePtr)));
        if (!new_list) {
            return false;
        }
        list = new_list;
        *list_ref = new_list;
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

    size_t num_appended = 0;
    for (;;) {
        int ctype = va_arg(ap, int);
        if (ctype == -1) {
            break;
        }
        {   // nested scope for autocleaning
            UwValue item = uw_create_from_ctype(ctype, ap);
            if (!item) {
                goto error;
            }
            if (!_uw_list_append(&(list)->list_value, item)) {
                goto error;
            }
            num_appended++;
        }
    }
    return true;

error:
    while (num_appended--) {
        {
            UwValue v = uw_list_pop(list);
        }
    }
    return false;
}

UwValuePtr uw_list_item(UwValuePtr list, ssize_t index)
{
    uw_assert_list(list);

    struct _UwList* _list = list->list_value;

    if (index < 0) {
        index = _list->length + index;
        uw_assert(index >= 0);
    } else {
        uw_assert(index < _list->length);
    }
    return  uw_makeref(_list->items[index]);
}

UwValuePtr uw_list_pop(UwValuePtr list)
{
    uw_assert_list(list);

    struct _UwList* _list = list->list_value;
    if (_list->length == 0) {
        return nullptr;
    } else {
        _list->length--;
        UwValuePtr* item_ptr = &_list->items[_list->length];
        UwValuePtr item = *item_ptr;
        *item_ptr = nullptr;
        return item;
    }
}

void uw_list_del(UwValuePtr list, size_t start_index, size_t end_index)
{
    uw_assert_list(list);
    _uw_list_del(list->list_value, start_index, end_index);
}

void _uw_list_del(struct _UwList* list, size_t start_index, size_t end_index)
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

    for (size_t i = start_index; i < end_index; i++) {
        uw_delete(&list->items[i]);
    }

    size_t tail_len = (list->length - end_index) * sizeof(UwValuePtr);
    if (tail_len) {
        memmove(&list->items[start_index], &list->items[end_index], tail_len);
    }

    list->length -= end_index - start_index;
}

UwValuePtr uw_list_slice(UwValuePtr list, size_t start_index, size_t end_index)
{
    size_t length = uw_list_length(list);

    if (end_index > length) {
        end_index = length;
    }
    if (start_index >= end_index) {
        // return empty list
        return _uw_create_list();
    }
    size_t slice_len = end_index - start_index;

    UwValuePtr result = _uw_alloc_value();
    result->type_id = UwTypeId_List;
    result->refcount = 1;
    result->list_value = _uw_alloc_list(slice_len);

    struct _UwList* _list   = list->list_value;
    struct _UwList* _result = result->list_value;

    for (size_t i = start_index, j = 0; i < end_index; i++, j++) {
        _result->items[j] = uw_makeref(_list->items[i]);
    }
    _result->length = slice_len;
    return result;
}
