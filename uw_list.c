#include <string.h>

#include "uw_value.h"

#define UWLIST_INITIAL_CAPACITY    16
#define UWLIST_CAPACITY_INCREMENT  16

_UwList* _uw_alloc_list(size_t capacity)
{
    _UwList* list = malloc(sizeof(_UwList) + capacity * sizeof(UwValuePtr));
    if (!list) {
        perror(__func__);
        exit(1);
    }
    list->length = 0;
    list->capacity = capacity;
    return list;
}

UwValuePtr uw_create_list()
{
    UwValuePtr value = _uw_alloc_value();
    value->type_id = UwTypeId_List;
    value->list_value = _uw_alloc_list(UWLIST_INITIAL_CAPACITY);
    return value;
}

UwValuePtr uw_create_list_va(...)
{
    va_list ap;
    va_start(ap);
    UwValue list = uw_create_list_ap(ap);
    va_end(ap);
    return uw_ptr(list);
}

UwValuePtr uw_create_list_ap(va_list ap)
{
    UwValue list = uw_create_list();
    uw_list_append_ap(list, ap);
    return uw_ptr(list);
}

void _uw_delete_list(_UwList* list)
{
    for (size_t i = 0; i < list->length; i++) {
        uw_delete(&list->items[i]);
    }
    free(list);
}

int _uw_list_cmp(_UwList* a, _UwList* b)
{
    if (a == b) {
        // compare with self
        return UW_EQ;
    }

    size_t n = a->length;
    if (b->length != n) {
        // lists have different lengths
        return UW_NEQ;
    }

    UwValuePtr* a_vptr = a->items;
    UwValuePtr* b_vptr = b->items;
    while (n) {
        if (uw_compare(*a_vptr, *b_vptr) != UW_EQ) {
            return UW_NEQ;
        }
        n--;
        a_vptr++;
        b_vptr++;
    }
    return UW_EQ;
}

bool _uw_list_eq(_UwList* a, _UwList* b)
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
        if (!uw_equal(*a_vptr, *b_vptr)) {
            return false;
        }
        n--;
        a_vptr++;
        b_vptr++;
    }
    return true;
}

void _uw_list_hash(_UwList* list, UwHashContext* ctx)
{
    for (size_t i = 0; i < list->length; i++) {
        _uw_calculate_hash(list->items[i], ctx);
    }
}

void _uw_list_append_null(UwValuePtr list, UwType_Null item)
{
    UwValue v = _uw_create_null(nullptr);
    _uw_list_append_uw(list, v);
}

void _uw_list_append_bool(UwValuePtr list, UwType_Bool item)
{
    UwValue v = _uw_create_bool(item);
    _uw_list_append_uw(list, v);
}

void _uw_list_append_int(UwValuePtr list, UwType_Int item)
{
    UwValue v = _uw_create_int(item);
    _uw_list_append_uw(list, v);
}

void _uw_list_append_float(UwValuePtr list, UwType_Float item)
{
    UwValue v = _uw_create_float(item);
    _uw_list_append_uw(list, v);
}

void _uw_list_append_u8_wrapper(UwValuePtr list, char* item)
{
    UwValue v = _uw_create_string_u8_wrapper(item);
    _uw_list_append_uw(list, v);
}

void _uw_list_append_u8(UwValuePtr list, char8_t* item)
{
    UwValue v = _uw_create_string_u8(item);
    _uw_list_append_uw(list, v);
}

void _uw_list_append_u32(UwValuePtr list, char32_t* item)
{
    UwValue v = _uw_create_string_u32(item);
    _uw_list_append_uw(list, v);
}

void _uw_list_append_uw(UwValuePtr list, UwValuePtr item)
{
    uw_assert_list(list);
    uw_assert(list != item);
    _uw_list_append(&list->list_value, item);
}

void _uw_list_append(_UwList** list_ref, UwValuePtr item)
{
    _UwList* list = *list_ref;

    uw_assert(item);  // item should not be nullptr
    uw_assert(list->length <= list->capacity);

    if (list->length == list->capacity) {
        list->capacity += UWLIST_CAPACITY_INCREMENT;
        *list_ref = realloc(list, sizeof(_UwList) + (list->capacity * sizeof(UwValuePtr)));
        if (!*list_ref) {
            perror(__func__);
            exit(1);
        }
        list = *list_ref;
    }
    list->items[list->length++] = uw_makeref(item);
}

void uw_list_append_va(UwValuePtr list, ...)
{
    va_list ap;
    va_start(ap);
    uw_list_append_ap(list, ap);
    va_end(ap);
}

void uw_list_append_ap(UwValuePtr list, va_list ap)
{
    uw_assert_list(list);

    for (;;) {
        int ctype = va_arg(ap, int);
        if (ctype == -1) {
            break;
        }
        UwValue item = uw_create_from_ctype(ctype, ap);

        _uw_list_append(&(list)->list_value, item);
    }
}

UwValuePtr uw_list_item(UwValuePtr list, ssize_t index)
{
    uw_assert_list(list);

    _UwList* _list = list->list_value;

    if (index < 0) {
        index = _list->length + index;
        uw_assert(index >= 0);
    } else {
        uw_assert(index < _list->length);
    }
    return  uw_makeref(_list->items[index]);
}

UwValuePtr _uw_copy_list(_UwList* list)
{
    UwValuePtr new_list = _uw_alloc_value();
    new_list->type_id = UwTypeId_List;
    new_list->list_value = _uw_alloc_list((list->length + UWLIST_CAPACITY_INCREMENT - 1) & ~(UWLIST_CAPACITY_INCREMENT - 1));

    // deep copy
    UwValuePtr* new_item_ptr = new_list->list_value->items;
    for (size_t i = 0; i < list->length; i++) {
        *new_item_ptr++ = uw_copy(list->items[i]);
    }
    return new_list;
}

UwValuePtr uw_list_pop(UwValuePtr list)
{
    uw_assert_list(list);

    _UwList* _list = list->list_value;
    if (_list->length == 0) {
        return nullptr;
    } else {
        _list->length--;
        return uw_ptr(_list->items[_list->length]);
    }
}

void uw_list_del(UwValuePtr list, size_t start_index, size_t end_index)
{
    uw_assert_list(list);
    _uw_list_del(list->list_value, start_index, end_index);
}

void _uw_list_del(_UwList* list, size_t start_index, size_t end_index)
{
    if (list->length == 0) {
        return;
    }
    if (end_index >= list->length) {
        end_index = list->length - 1;
    }
    if (start_index > end_index) {
        return;
    }

    for (size_t i = start_index; i <= end_index; i++) {
        uw_delete(&list->items[i]);
    }

    size_t tail_len = (list->length - 1 - end_index) * sizeof(UwValuePtr);
    if (tail_len) {
        memmove(&list->items[start_index], &list->items[end_index + 1], tail_len);
    }

    list->length -= (end_index - start_index) + 1;
}

#ifdef DEBUG

void _uw_dump_list(_UwList* list, int indent)
{
    // continue current line
    printf("%zu items, capacity=%zu\n", list->length, list->capacity);

    indent += 4;
    for (size_t i = 0; i < list->length; i++) {
        _uw_dump(list->items[i], indent, "");
    }
}

#endif
