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

void _uw_delete_list(_UwList* list)
{
    for (size_t i = 0; i < list->length; i++) {
        uw_delete_value(&list->items[i]);
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

    UwValueRef a_vref = _uw_list_item_ref(a, 0);
    UwValueRef b_vref = _uw_list_item_ref(b, 0);
    while (n) {
        if (uw_compare(*a_vref, *b_vref) != UW_EQ) {
            return UW_NEQ;
        }
        n--;
        a_vref++;
        b_vref++;
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

    UwValueRef a_vref = _uw_list_item_ref(a, 0);
    UwValueRef b_vref = _uw_list_item_ref(b, 0);
    while (n) {
        if (!uw_equal(*a_vref, *b_vref)) {
            return false;
        }
        n--;
        a_vref++;
        b_vref++;
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
    _uw_list_append_uw(list, &v);
}

void _uw_list_append_bool(UwValuePtr list, UwType_Bool item)
{
    UwValue v = _uw_create_bool(item);
    _uw_list_append_uw(list, &v);
}

void _uw_list_append_int(UwValuePtr list, UwType_Int item)
{
    UwValue v = _uw_create_int(item);
    _uw_list_append_uw(list, &v);
}

void _uw_list_append_float(UwValuePtr list, UwType_Float item)
{
    UwValue v = _uw_create_float(item);
    _uw_list_append_uw(list, &v);
}

void _uw_list_append_u8_wrapper(UwValuePtr list, char* item)
{
    UwValue v = _uw_create_string_u8_wrapper(item);
    _uw_list_append_uw(list, &v);
}

void _uw_list_append_u8(UwValuePtr list, char8_t* item)
{
    UwValue v = _uw_create_string_u8(item);
    _uw_list_append_uw(list, &v);
}

void _uw_list_append_u32(UwValuePtr list, char32_t* item)
{
    UwValue v = _uw_create_string_u32(item);
    _uw_list_append_uw(list, &v);
}

void _uw_list_append_uw(UwValuePtr list, UwValueRef item)
{
    uw_assert_list(list);
    uw_assert((list) != *(item));
    _uw_list_append(&(list)->list_value, (item));
}

void _uw_list_append(_UwList** list_ref, UwValueRef item)
{
    _UwList* list = *list_ref;

    uw_assert(*item);  // item should not be nullptr
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
    list->items[list->length++] = *item;
    *item = nullptr;
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
        return _list->items[_list->length];
    }
}

#ifdef DEBUG

void _uw_dump_list(_UwList* list, int indent)
{
    // continue current line
    printf("%zu items, capacity=%zu\n", list->length, list->capacity);

    indent += 4;
    for (size_t i = 0; i < list->length; i++) {
        _uw_dump_value(list->items[i], indent, "");
    }
}

#endif
