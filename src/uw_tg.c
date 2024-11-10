/*
 * Type-generic helper functions.
 */

#include "include/uw_c.h"
#include "src/uw_string_internal.h"  // for _uw_copy_string

char* _uw_get_type_name_by_id(uint8_t type_id)
{
    return _uw_types[type_id]->name;
}

char* _uw_get_type_name_from_value(UwValuePtr value)
{
    return _uw_get_type_name_by_id(value->type_id);
}

#define _uw_call_equal_ctype(value, ...)  \
    ({  \
        UwTypeId type_id = (value)->type_id;  \
        UwMethodEqualCType fn_equal_ctype = _uw_types[type_id]->equal_ctype;  \
        uw_assert(fn_equal_ctype != nullptr);  \
        fn_equal_ctype((value), __VA_ARGS__);  \
    })

bool _uw_equal_null      (UwValuePtr a, nullptr_t          b) { return uw_is_null(a); }
bool _uw_equal_bool      (UwValuePtr a, bool               b) { return _uw_call_equal_ctype(a, uwc_bool,   b); }
bool _uw_equal_char      (UwValuePtr a, char               b) { return _uw_call_equal_ctype(a, uwc_char,   b); }
bool _uw_equal_uchar     (UwValuePtr a, unsigned char      b) { return _uw_call_equal_ctype(a, uwc_uchar,  b); }
bool _uw_equal_short     (UwValuePtr a, short              b) { return _uw_call_equal_ctype(a, uwc_short,  b); }
bool _uw_equal_ushort    (UwValuePtr a, unsigned short     b) { return _uw_call_equal_ctype(a, uwc_ushort, b); }
bool _uw_equal_int       (UwValuePtr a, int                b) { return _uw_call_equal_ctype(a, uwc_int,    b); }
bool _uw_equal_uint      (UwValuePtr a, unsigned int       b) { return _uw_call_equal_ctype(a, uwc_uint,   b); }
bool _uw_equal_long      (UwValuePtr a, long               b) { return _uw_call_equal_ctype(a, uwc_long,   b); }
bool _uw_equal_ulong     (UwValuePtr a, unsigned long      b) { return _uw_call_equal_ctype(a, uwc_ulong,  b); }
bool _uw_equal_longlong  (UwValuePtr a, long long          b) { return _uw_call_equal_ctype(a, uwc_longlong,  b); }
bool _uw_equal_ulonglong (UwValuePtr a, unsigned long long b) { return _uw_call_equal_ctype(a, uwc_ulonglong, b); }
bool _uw_equal_float     (UwValuePtr a, float              b) { return _uw_call_equal_ctype(a, uwc_float,  b); }
bool _uw_equal_double    (UwValuePtr a, double             b) { return _uw_call_equal_ctype(a, uwc_double, b); }
bool _uw_equal_uw        (UwValuePtr a, UwValuePtr         b) { return _uw_equal(a, b); }

bool _uw_equal_u8_wrapper(UwValuePtr a, char* b)
{
    return _uw_equal_u8(a, (char8_t*) b);
}

UwValuePtr _uw_create_string_uw(UwValuePtr str)
{
    uw_assert_string(str);
    return _uw_copy_string(str);
}

UwValuePtr _uw_create_string_u8_wrapper(char* initializer)
{
    return _uw_create_string_u8((char8_t*) initializer);
}

bool _uw_substring_eq_u8_wrapper(UwValuePtr a, size_t start_pos, size_t end_pos, char* b)
{
    return _uw_substring_eq_u8(a, start_pos, end_pos, (char8_t*) b);
}

bool _uw_string_append_u8_wrapper(UwValuePtr dest, char* src)
{
    return _uw_string_append_u8(dest, (char8_t*) src);
}

bool _uw_string_append_substring_u8_wrapper(UwValuePtr dest, char* src, size_t src_start_pos, size_t src_end_pos)
{
    return _uw_string_append_substring_u8(dest, (char8_t*) src, src_start_pos, src_end_pos);
}

UwValuePtr _uw_string_join_u8_wrapper(char* separator, UwValuePtr list)
{
    UwValue sep = uw_create(separator);
    if (!sep) {
        return nullptr;
    }
    return _uw_string_join_uw(sep, list);
}

bool _uw_list_append_u8_wrapper(UwValuePtr list, char* item)
{
    UwValue v = _uw_create_string_u8_wrapper(item);
    if (!v) {
        return false;
    }
    return _uw_list_append_uw(list, v);
}

bool _uw_map_has_key_u8_wrapper(UwValuePtr map, char* key)
{
    UwValue k = _uw_create_string_u8_wrapper(key);
    uw_assert(k != nullptr);
    return _uw_map_has_key_uw(map, k);
}

UwValuePtr _uw_map_get_u8_wrapper(UwValuePtr map, char* key)
{
    UwValue k = _uw_create_string_u8_wrapper(key);
    uw_assert(k != nullptr);
    return _uw_map_get_uw(map, k);
}

void _uw_map_del_u8_wrapper(UwValuePtr map, char* key)
{
    UwValue k = _uw_create_string_u8_wrapper(key);
    uw_assert(k != nullptr);
    _uw_map_del_uw(map, k);
}
