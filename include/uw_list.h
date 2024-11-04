#pragma once

#include <stdarg.h>
#include <uchar.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline UwValuePtr uw_create_list()
{
    return _uw_create(UwTypeId_List);
}
UwValuePtr uw_create_list_va(...);
UwValuePtr uw_create_list_ap(va_list ap);

size_t uw_list_length(UwValuePtr list);

bool _uw_list_append_null (UwValuePtr list, UwType_Null  item);
bool _uw_list_append_bool (UwValuePtr list, UwType_Bool  item);
bool _uw_list_append_int  (UwValuePtr list, UwType_Int   item);
bool _uw_list_append_float(UwValuePtr list, UwType_Float item);
bool _uw_list_append_u8   (UwValuePtr list, char8_t*     item);
bool _uw_list_append_u32  (UwValuePtr list, char32_t*    item);
bool _uw_list_append_uw   (UwValuePtr list, UwValuePtr   item);

bool uw_list_append_va(UwValuePtr list, ...);
bool uw_list_append_ap(UwValuePtr list, va_list ap);

UwValuePtr uw_list_item(UwValuePtr list, ssize_t index);
/*
 * Return reference to list item. Negative indexes are allowed: -1 last item.
 * It's the caller's responsibility to destroy returned value to avoid memory leaks.
 * The simplest way is assigning it to an UwValue.
 */

UwValuePtr uw_list_pop(UwValuePtr list);
/*
 * Extract last item from the list.
 * It's the caller's responsibility to destroy returned value to avoid memory leaks.
 * The simplest way is assigning it to an UwValue.
 */

void uw_list_del(UwValuePtr list, size_t start_index, size_t end_index);
/*
 * Delete items from list.
 * `end_index` is exclusive, i.e. the number of items to delete equals to end_index - start_index..
 */

UwValuePtr uw_list_slice(UwValuePtr list, size_t start_index, size_t end_index);
/*
 * Return shallow copy of the given range of list.
 */

#ifdef __cplusplus
}
#endif
