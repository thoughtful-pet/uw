#pragma once

/*
 * Safe dynamic memory management is based on a couple of definitions:
 *
 *   1. `typename`, e.g. UwValue and CString
 *      This is a shortest name for automatically cleaned variables.
 *      This is actually a pointer that can be either nullptr
 *      or point to an allocated block.
 *
 *      Cleanup handlers are fired at scope exit and
 *      there's always a chance to shoot in the foot assigning
 *      nullptr to such a variable without explicitly freeing
 *      previous value.
 *
 *      caveat: automatically cleaned values is a GNU extension
 *              which, however, is supported in clang as well
 *
 *   2. `typename`Ptr, e.g. UwValuePtr and CStringPtr
 *      Raw pointer for constant arguments and return values.
 *      This is an alias for `typename`, however, it is not
 *      automatically cleaned because that does not work for arguments.
 *
 * uw_move() macro transfers ownership from automatically cleaned
 * variable to a standalone `typename`Ptr
 *
 * uw_makeref() macro increments reference counter
 *
 * uw_destroy() always zeroes pointer to the value, even if decremented
 *              refcount is still above zero.
 *
 * caveat: uw_move() and other macros use a GNU extension
 *         "Statements and Declarations in Expressions"
 *         which, however, is supported in clang as well
 *
 * Tips:
 *
 *   1. Use blocks for autocleaning, especially in loop bodies.
 *      Variables are cleaned only at scope exit, that's not a pure destructor!
 *      Without scopes in loops you'd have to call `uw_delete` on each iteration.
 *
 *   2. Never pass newly created value to a function that increments refcount,
 *      i.e. the following would cause memory leak:
 *
 *      uw_list_append(mylist, uw_create(1));
 *
 *      use a temporary variable instead:
 *
 *      {   // new scope for autocleaning
 *          UwValue v = uw_create(1);
 *          uw_list_append(mylist, v);
 *      }
 *
 *      The only exception is *_va function where uw_ptr designator allows that:
 *
 *      uw_list_append_va(mylist, uw_ptr, uw_create(1), uw_ptr, uw_create(2), -1);
 *
 * Yes, C is weird. C++ could handle this better bit it's weird in its own way.
 *
 * But what if constructors returned values with zero refcount?
 * Then:
 *
 *   1. We'd have to increment refcount manually after assigning to a variable.
 *
 *   2. The memory would still leak:
 *
 *      uw_list_append(mylist, uw_create(1));
 *
 *      because no one would free the value returned by uw_create: cleanup attribute
 *      cannot be applied to them.
 *
 * Yeah, function arguments in C is an ephemeral thing.
 *
 *
 * Notes on generics
 *
 * Generics behave strangely with char* and char8_t*.
 * Clang 16 uses char* version called with either string constant
 * as initializer, but complains if char8_t* choice is not defined.
 *
 * As a workaround, using *_u8_wrapper(char*) functions which treat
 * char* as char8_t* and call *_u8 version.
 *
 * Basically, *_cstr functions should be used for char*.
 *
 * Not checked with neither newer Clang nor GCC.
 *
 */

#include <uw_value_base.h>

#ifdef __cplusplus
extern "C" {
#endif

#define uw_move(var) \
    ({  \
        typeof(var) tmp = (var);  \
        (var) = nullptr;  \
        tmp;  \
    })

// automatically cleaned values
#define UwValue [[ gnu::cleanup(uw_delete) ]] UwValuePtr

// automatically cleaned value
#define CString [[ gnu::cleanup(uw_delete_cstring) ]] CStringPtr

/****************************************************************
 * Type names
 */

#define uw_get_type_name(v) _Generic((v),        \
                  char: _uw_get_type_name_by_id, \
         unsigned char: _uw_get_type_name_by_id, \
                 short: _uw_get_type_name_by_id, \
        unsigned short: _uw_get_type_name_by_id, \
                   int: _uw_get_type_name_by_id, \
          unsigned int: _uw_get_type_name_by_id, \
                  long: _uw_get_type_name_by_id, \
         unsigned long: _uw_get_type_name_by_id, \
             long long: _uw_get_type_name_by_id, \
    unsigned long long: _uw_get_type_name_by_id, \
            UwValuePtr: _uw_get_type_name_from_value  \
    )(v)

/****************************************************************
 * Constructors
 *
 * Notes
 *
 * 1. uw_create('a') creates integer, use uw_create("a") for strings
 *
 * 2. For particular cases there's uw_create_string that accepts
 *    char* and UwValuePtr.
 */

#define uw_create(initializer) _Generic((initializer), \
             nullptr_t: _uw_create_null,       \
                  bool: _uw_create_bool,       \
                  char: _uw_create_int,        \
         unsigned char: _uw_create_int,        \
                 short: _uw_create_int,        \
        unsigned short: _uw_create_int,        \
                   int: _uw_create_int,        \
          unsigned int: _uw_create_int,        \
                  long: _uw_create_int,        \
         unsigned long: _uw_create_int,        \
             long long: _uw_create_int,        \
    unsigned long long: _uw_create_int,        \
                 float: _uw_create_float,      \
                double: _uw_create_float,      \
                 char*: _uw_create_string_u8_wrapper, \
              char8_t*: _uw_create_string_u8,  \
             char32_t*: _uw_create_string_u32, \
            UwValuePtr:  uw_copy               \
    )(initializer)

#define uw_create_string(initializer) _Generic((initializer), \
             char*:  uw_create_string_c,   \
        UwValuePtr: _uw_create_string_uw   \
    )(initializer)

/****************************************************************
 * Comparison functions
 */

/*
 * Full comparison functions.
 * Return one of four constants.
 */
#define uw_compare(a, b) _Generic((b),          \
             nullptr_t: _uw_compare_null,       \
                  bool: _uw_compare_bool,       \
                  char: _uw_compare_char,       \
         unsigned char: _uw_compare_uchar,      \
                 short: _uw_compare_short,      \
        unsigned short: _uw_compare_ushort,     \
                   int: _uw_compare_int,        \
          unsigned int: _uw_compare_uint,       \
                  long: _uw_compare_long,       \
         unsigned long: _uw_compare_ulong,      \
             long long: _uw_compare_longlong,   \
    unsigned long long: _uw_compare_ulonglong,  \
                 float: _uw_compare_float,      \
                double: _uw_compare_double,     \
                 char*: _uw_compare_u8_wrapper, \
              char8_t*: _uw_compare_u8,         \
             char32_t*: _uw_compare_u32,        \
            UwValuePtr: _uw_compare_uw          \
    )((a), (b))

#define uw_equal(a, b) _Generic((b),          \
             nullptr_t: _uw_equal_null,       \
                  bool: _uw_equal_bool,       \
                  char: _uw_equal_char,       \
         unsigned char: _uw_equal_uchar,      \
                 short: _uw_equal_short,      \
        unsigned short: _uw_equal_ushort,     \
                   int: _uw_equal_int,        \
          unsigned int: _uw_equal_uint,       \
                  long: _uw_equal_long,       \
         unsigned long: _uw_equal_ulong,      \
             long long: _uw_equal_longlong,   \
    unsigned long long: _uw_equal_ulonglong,  \
                 float: _uw_equal_float,      \
                double: _uw_equal_double,     \
                 char*: _uw_equal_u8_wrapper, \
              char8_t*: _uw_equal_u8,         \
             char32_t*: _uw_equal_u32,        \
            UwValuePtr: _uw_equal_uw          \
    )((a), (b))

/****************************************************************
 * List functions
 */

#define uw_list_append(list, item) _Generic((item), \
             nullptr_t: _uw_list_append_null,       \
                  bool: _uw_list_append_bool,       \
                  char: _uw_list_append_int,        \
         unsigned char: _uw_list_append_int,        \
                 short: _uw_list_append_int,        \
        unsigned short: _uw_list_append_int,        \
                   int: _uw_list_append_int,        \
          unsigned int: _uw_list_append_int,        \
                  long: _uw_list_append_int,        \
         unsigned long: _uw_list_append_int,        \
             long long: _uw_list_append_int,        \
    unsigned long long: _uw_list_append_int,        \
                 float: _uw_list_append_float,      \
                double: _uw_list_append_float,      \
                 char*: _uw_list_append_u8_wrapper, \
              char8_t*: _uw_list_append_u8,         \
             char32_t*: _uw_list_append_u32,        \
            UwValuePtr: _uw_list_append_uw          \
    )((list), (item))

/****************************************************************
 * Map functions
 */

#define uw_map_has_key(map, key) _Generic((key),    \
             nullptr_t: _uw_map_has_key_null,       \
                  bool: _uw_map_has_key_bool,       \
                  char: _uw_map_has_key_int,        \
         unsigned char: _uw_map_has_key_int,        \
                 short: _uw_map_has_key_int,        \
        unsigned short: _uw_map_has_key_int,        \
                   int: _uw_map_has_key_int,        \
          unsigned int: _uw_map_has_key_int,        \
                  long: _uw_map_has_key_int,        \
         unsigned long: _uw_map_has_key_int,        \
             long long: _uw_map_has_key_int,        \
    unsigned long long: _uw_map_has_key_int,        \
                 float: _uw_map_has_key_float,      \
                double: _uw_map_has_key_float,      \
                 char*: _uw_map_has_key_u8_wrapper, \
              char8_t*: _uw_map_has_key_u8,         \
             char32_t*: _uw_map_has_key_u32,        \
            UwValuePtr: _uw_map_has_key_uw          \
    )((map), (key))

/*
 * Get value by `key`. Return nullptr if `key` is not in `map`.
 *
 * It's the caller's responsibility to destroy returned value to avoid memory leaks.
 * The simplest way is assigning it to UwValue.
 */
#define uw_map_get(map, key) _Generic((key),    \
             nullptr_t: _uw_map_get_null,       \
                  bool: _uw_map_get_bool,       \
                  char: _uw_map_get_int,        \
         unsigned char: _uw_map_get_int,        \
                 short: _uw_map_get_int,        \
        unsigned short: _uw_map_get_int,        \
                   int: _uw_map_get_int,        \
          unsigned int: _uw_map_get_int,        \
                  long: _uw_map_get_int,        \
         unsigned long: _uw_map_get_int,        \
             long long: _uw_map_get_int,        \
    unsigned long long: _uw_map_get_int,        \
                 float: _uw_map_get_float,      \
                double: _uw_map_get_float,      \
                 char*: _uw_map_get_u8_wrapper, \
              char8_t*: _uw_map_get_u8,         \
             char32_t*: _uw_map_get_u32,        \
            UwValuePtr: _uw_map_get_uw          \
    )((map), (key))

/*
 * Delete item from map by `key`.
 */
#define uw_map_del(map, key) _Generic((key),    \
             nullptr_t: _uw_map_del_null,       \
                  bool: _uw_map_del_bool,       \
                  char: _uw_map_del_int,        \
         unsigned char: _uw_map_del_int,        \
                 short: _uw_map_del_int,        \
        unsigned short: _uw_map_del_int,        \
                   int: _uw_map_del_int,        \
          unsigned int: _uw_map_del_int,        \
                  long: _uw_map_del_int,        \
         unsigned long: _uw_map_del_int,        \
             long long: _uw_map_del_int,        \
    unsigned long long: _uw_map_del_int,        \
                 float: _uw_map_del_float,      \
                double: _uw_map_del_float,      \
                 char*: _uw_map_del_u8_wrapper, \
              char8_t*: _uw_map_del_u8,         \
             char32_t*: _uw_map_del_u32,        \
            UwValuePtr: _uw_map_del_uw          \
    )((map), (key))

/****************************************************************
 * String functions
 */

#define uw_string_append(dest, src) _Generic((src),   \
              char32_t: _uw_string_append_c32,        \
                   int: _uw_string_append_c32,        \
                 char*: _uw_string_append_u8_wrapper, \
              char8_t*: _uw_string_append_u8,         \
             char32_t*: _uw_string_append_u32,        \
            UwValuePtr: _uw_string_append_uw          \
    )((dest), (src))

#define uw_string_insert_chars(str, position, value, n) _Generic((value), \
              char32_t: _uw_string_insert_many_c32,   \
                   int: _uw_string_insert_many_c32    \
    )((str), (position), (value), (n))

#define uw_string_append_substring(dest, src, src_start_pos, src_end_pos) _Generic((src), \
                 char*: _uw_string_append_substring_u8_wrapper,  \
              char8_t*: _uw_string_append_substring_u8,          \
             char32_t*: _uw_string_append_substring_u32,         \
            UwValuePtr: _uw_string_append_substring_uw           \
    )((dest), (src), (src_start_pos), (src_end_pos))

#define uw_substring_eq(a, start_pos, end_pos, b) _Generic((b), \
             char*: _uw_substring_eq_u8_wrapper,  \
          char8_t*: _uw_substring_eq_u8,          \
         char32_t*: _uw_substring_eq_u32,         \
        UwValuePtr: _uw_substring_eq_uw           \
    )((a), (start_pos), (end_pos), (b))

#define uw_string_split(str, splitters) _Generic((splitters),  \
              char32_t: _uw_string_split_c32,            \
                   int: _uw_string_split_c32,            \
                 char*: _uw_string_split_any_u8_wrapper, \
              char8_t*: _uw_string_split_any_u8,         \
             char32_t*: _uw_string_split_any_u32,        \
            UwValuePtr: _uw_string_split_any_uw          \
    )((str), (splitters))

#define uw_string_split_strict(str, splitter) _Generic((splitter),  \
              char32_t: _uw_string_split_c32,               \
                   int: _uw_string_split_c32,               \
                 char*: _uw_string_split_strict_u8_wrapper, \
              char8_t*: _uw_string_split_strict_u8,         \
             char32_t*: _uw_string_split_strict_u32,        \
            UwValuePtr: _uw_string_split_strict_uw          \
    )((str), (splitter))

#define uw_string_join(separator, list) _Generic((separator), \
              char32_t: _uw_string_join_c32,        \
                   int: _uw_string_join_c32,        \
                 char*: _uw_string_join_u8_wrapper, \
              char8_t*: _uw_string_join_u8,         \
             char32_t*: _uw_string_join_u32,        \
            UwValuePtr: _uw_string_join_uw          \
    )((separator), (list))

#ifdef __cplusplus
}
#endif
