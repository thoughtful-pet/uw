#pragma once

/*
 * Type-generic function.
 *
 * Notes:
 *
 * Although generics is a great thing,
 * they make preprocessor output looks extremely ugly.
 *
 * Note that string literals are weird:
 *
 * char8_t* str = u8"สวัสดี";  --> incompatible pointers
 *
 * but the following is okay:
 *
 * char8_t str[] = u8"สวัสดี";
 *
 * So, when u8 literal is passed to generic, it's a char*, not char8_t*.
 *
 * Generics are also weird: Clang 16 (at least) complains if char8_t*
 * choice is not defined despite of the above.
 *
 * As a workaround, using *_u8_wrapper(char*) functions which treat
 * char* as char8_t* and call *_u8 version.
 *
 * Basically, *_cstr functions should be used for char*.
 *
 * Not checked with neither newer Clang nor GCC.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

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

char* _uw_get_type_name_from_value(UwValuePtr value);
char* _uw_get_type_name_by_id(uint8_t type_id);

/****************************************************************
 * Constructors
 *
 * Note: uw_create('a') creates integer.
 * For strings use uw_create("a") or specific constructors.
 */

#define uw_create(initializer) _Generic((initializer), \
             nullptr_t: _uwc_create_null,       \
                  bool: _uwc_create_bool,       \
                  char: _uwc_create_int,        \
         unsigned char: _uwc_create_int,        \
                 short: _uwc_create_int,        \
        unsigned short: _uwc_create_int,        \
                   int: _uwc_create_int,        \
          unsigned int: _uwc_create_int,        \
                  long: _uwc_create_int,        \
         unsigned long: _uwc_create_int,        \
             long long: _uwc_create_int,        \
    unsigned long long: _uwc_create_int,        \
                 float: _uwc_create_float,      \
                double: _uwc_create_float,      \
                 char*: _uw_create_string_u8_wrapper, \
              char8_t*: _uw_create_string_u8,  \
             char32_t*: _uw_create_string_u32, \
            UwValuePtr:  uw_copy               \
    )(initializer)

UwValuePtr _uwc_create_null(UwType_Null initializer);
UwValuePtr _uw_create_string_u8_wrapper(char* initializer);

/****************************************************************
 * Comparison for equality
 */

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

bool _uw_equal_null      (UwValuePtr a, nullptr_t          b);
bool _uw_equal_bool      (UwValuePtr a, bool               b);
bool _uw_equal_char      (UwValuePtr a, char               b);
bool _uw_equal_uchar     (UwValuePtr a, unsigned char      b);
bool _uw_equal_short     (UwValuePtr a, short              b);
bool _uw_equal_ushort    (UwValuePtr a, unsigned short     b);
bool _uw_equal_int       (UwValuePtr a, int                b);
bool _uw_equal_uint      (UwValuePtr a, unsigned int       b);
bool _uw_equal_long      (UwValuePtr a, long               b);
bool _uw_equal_ulong     (UwValuePtr a, unsigned long      b);
bool _uw_equal_longlong  (UwValuePtr a, long long          b);
bool _uw_equal_ulonglong (UwValuePtr a, unsigned long long b);
bool _uw_equal_float     (UwValuePtr a, float              b);
bool _uw_equal_double    (UwValuePtr a, double             b);
bool _uw_equal_u8_wrapper(UwValuePtr a, char*              b);
bool _uw_equal_u8        (UwValuePtr a, char8_t*           b);
bool _uw_equal_u32       (UwValuePtr a, char32_t*          b);
bool _uw_equal_uw        (UwValuePtr a, UwValuePtr         b);


/****************************************************************
 * String functions
 */

#define uw_create_string(initializer) _Generic((initializer), \
             char*:  uw_create_string_cstr, \
        UwValuePtr: _uw_create_string_uw    \
    )(initializer)

UwValuePtr _uw_create_string_uw (UwValuePtr str);

#define uw_string_append(dest, src) _Generic((src),   \
              char32_t: _uw_string_append_c32,        \
                   int: _uw_string_append_c32,        \
                 char*: _uw_string_append_u8_wrapper, \
              char8_t*: _uw_string_append_u8,         \
             char32_t*: _uw_string_append_u32,        \
            UwValuePtr: _uw_string_append_uw          \
    )((dest), (src))

bool _uw_string_append_u8_wrapper(UwValuePtr dest, char* src);

#define uw_string_append_char(dest, chr) _Generic((chr), \
                  char: _uw_string_append_char, \
              char32_t: _uw_string_append_c32,  \
                   int: _uw_string_append_c32   \
    )((dest), (chr))

#define uw_string_insert_chars(str, position, chr, n) _Generic((chr), \
              char32_t: _uw_string_insert_many_c32,   \
                   int: _uw_string_insert_many_c32    \
    )((str), (position), (chr), (n))

#define uw_string_append_substring(dest, src, src_start_pos, src_end_pos) _Generic((src), \
                 char*: _uw_string_append_substring_u8_wrapper,  \
              char8_t*: _uw_string_append_substring_u8,          \
             char32_t*: _uw_string_append_substring_u32,         \
            UwValuePtr: _uw_string_append_substring_uw           \
    )((dest), (src), (src_start_pos), (src_end_pos))

bool _uw_string_append_substring_u8_wrapper(UwValuePtr dest, char* src, size_t src_start_pos, size_t src_end_pos);

/*
 * Substring comparison functions.
 *
 * Compare `str_a` from `start_pos` to `end_pos` with `str_b`.
 */
#define uw_substring_eq(a, start_pos, end_pos, b) _Generic((b), \
             char*: _uw_substring_eq_u8_wrapper,  \
          char8_t*: _uw_substring_eq_u8,          \
         char32_t*: _uw_substring_eq_u32,         \
        UwValuePtr: _uw_substring_eq_uw           \
    )((a), (start_pos), (end_pos), (b))

bool _uw_substring_eq_u8_wrapper(UwValuePtr a, size_t start_pos, size_t end_pos, char* b);

#define uw_string_split(str, splitters) _Generic((splitters),  \
              char32_t: _uw_string_split_c32,            \
                   int: _uw_string_split_c32,            \
                 char*: _uw_string_split_any_u8_wrapper, \
              char8_t*: _uw_string_split_any_u8,         \
             char32_t*: _uw_string_split_any_u32,        \
            UwValuePtr: _uw_string_split_any_uw          \
    )((str), (splitters))

UwValuePtr _uw_string_split_any_u8_wrapper(UwValuePtr str, char* splitters);

#define uw_string_split_strict(str, splitter) _Generic((splitter),  \
              char32_t: _uw_string_split_c32,               \
                   int: _uw_string_split_c32,               \
                 char*: _uw_string_split_strict_u8_wrapper, \
              char8_t*: _uw_string_split_strict_u8,         \
             char32_t*: _uw_string_split_strict_u32,        \
            UwValuePtr: _uw_string_split_strict_uw          \
    )((str), (splitter))

UwValuePtr _uw_string_split_strict_u8_wrapper(UwValuePtr str, char* splitter);

#define uw_string_join(separator, list) _Generic((separator), \
              char32_t: _uw_string_join_c32,        \
                   int: _uw_string_join_c32,        \
                 char*: _uw_string_join_u8_wrapper, \
              char8_t*: _uw_string_join_u8,         \
             char32_t*: _uw_string_join_u32,        \
            UwValuePtr: _uw_string_join_uw          \
    )((separator), (list))

UwValuePtr _uw_string_join_u8_wrapper(char* separator, UwValuePtr list);

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

bool _uw_list_append_u8_wrapper(UwValuePtr list, char* item);

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

bool _uw_map_has_key_u8_wrapper(UwValuePtr map, char* key);

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

UwValuePtr _uw_map_get_u8_wrapper(UwValuePtr map, char* key);

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

void _uw_map_del_u8_wrapper(UwValuePtr map, char* key);

/****************************************************************
 * StringIO functions
 */

#define uw_create_string_io(str) _Generic((str), \
             char*: _uw_create_string_io_u8_wrapper,  \
          char8_t*: _uw_create_string_io_u8,          \
         char32_t*: _uw_create_string_io_u32,         \
        UwValuePtr: _uw_create_string_io_uw           \
    )((str))

/****************************************************************
 * File functions
 */

#define uw_file_open(file_name, flags, mode) _Generic((file_name), \
             char*: _uw_file_open_u8_wrapper,  \
          char8_t*: _uw_file_open_u8,          \
         char32_t*: _uw_file_open_u32,         \
        UwValuePtr: _uw_file_open_uw           \
    )((file_name), (flags), (mode))

#ifdef __cplusplus
}
#endif
