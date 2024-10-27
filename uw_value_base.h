#pragma once

#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <uchar.h>

// ICU library:
#include <unicode/uchar.h>

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************
 * Low level hash functions
 */

typedef uint64_t  UwType_Hash;

typedef struct {
    uint64_t seed;
    uint64_t see1;
    uint64_t see2;
    uint64_t buffer[6];
    //size_t len;
    int buf_size;
} UwHashContext;

void _uw_hash_init(UwHashContext* ctx /*, size_t len */);
void _uw_hash_uint64(UwHashContext* ctx, uint64_t data);
UwType_Hash _uw_hash_finish(UwHashContext* ctx);

/****************************************************************
 * UwValue
 */

// Integral types
typedef nullptr_t  UwType_Null;
typedef bool       UwType_Bool;
typedef int64_t    UwType_Int;
typedef double     UwType_Float;

static_assert(sizeof(UwType_Int) == sizeof(UwType_Float));

#define UwTypeId_Null    0
#define UwTypeId_Bool    1
#define UwTypeId_Int     2
#define UwTypeId_Float   3
#define UwTypeId_String  4
#define UwTypeId_List    5
#define UwTypeId_Map     6

// forward declaration
struct _UwValue;

// internal string structure
typedef struct {
    uint8_t cap_size:3,      // length and capacity size in bytes minus one
            char_size:2,     // character size in bytes minus one
            block_count:3;   // number of 64-bit blocks for fast comparison
} _UwStringHeader;

typedef _UwStringHeader  _UwString;

// internal list structure
typedef struct {
    size_t length;
    size_t capacity;
    struct _UwValue* items[];
} _UwList;

// internal map structures

struct __UwHashTable;

typedef size_t (*_UwHtGet)(struct __UwHashTable* ht, size_t index);
typedef void   (*_UwHtSet)(struct __UwHashTable* ht, size_t index, size_t value);

typedef struct __UwHashTable {
    uint8_t item_size;   // in bytes
    UwType_Hash hash_bitmask;  // calculated from item_size
    size_t items_used;
    size_t capacity;
    _UwHtGet get_item;  // getter function optimized for specific item size
    _UwHtSet set_item;  // setter function optimized for specific item size
    uint8_t items[];    // items have variable size
} _UwHashTable;

typedef struct {
    _UwList* kv_pairs;        // key-value pairs in the insertion order
    _UwHashTable hash_table;
} _UwMap;

// okay, here it is, at last:

#ifndef UW_TYPE_BITWIDTH
#   define UW_TYPE_BITWIDTH  8
#endif

#define UW_REFCOUNT_BITWIDTH  (64 - UW_TYPE_BITWIDTH)
#define UW_MAX_REFCOUNT       ((1ULL << UW_REFCOUNT_BITWIDTH) - 1)

struct _UwValue {
    uint64_t type_id: UW_TYPE_BITWIDTH,
             refcount: UW_REFCOUNT_BITWIDTH;
    union {
        UwType_Null   null_value;
        UwType_Bool   bool_value;
        UwType_Int    int_value;
        UwType_Float  float_value;
        _UwString*    string_value;
        _UwList*      list_value;
        _UwMap*       map_value;
    };
};

typedef struct _UwValue* UwValuePtr;

// make new reference
#define uw_makeref(v) \
    ({ \
        if (v) { \
            uw_assert((v)->refcount < UW_MAX_REFCOUNT); \
            (v)->refcount++; \
        } \
        (v); \
    })

/****************************************************************
 * C strings
 */

typedef char* CStringPtr;

/****************************************************************
 * Type checking
 */

#define uw_is_null(value)    ((value) && ((value)->type_id == UwTypeId_Null))
#define uw_is_bool(value)    ((value) && ((value)->type_id == UwTypeId_Bool))
#define uw_is_int(value)     ((value) && ((value)->type_id == UwTypeId_Int))
#define uw_is_float(value)   ((value) && ((value)->type_id == UwTypeId_Float))
#define uw_is_string(value)  ((value) && ((value)->type_id == UwTypeId_String))
#define uw_is_list(value)    ((value) && ((value)->type_id == UwTypeId_List))
#define uw_is_map(value)     ((value) && ((value)->type_id == UwTypeId_Map))

/****************************************************************
 * Type names
 */

char* _uw_get_type_name_from_value(UwValuePtr value);
char* _uw_get_type_name_by_id(uint8_t type_id);

/****************************************************************
 * Assertions
 *
 * Unlike assertions from standard library they cannot be
 * turned off and can be used for input parameters validation.
 */

#define uw_assert(condition) \
    ({  \
        if (!(condition)) {  \
            fprintf(stderr, "UW assertion failed at %s:%s:%d: " #condition "\n", __FILE__, __func__, __LINE__);  \
            exit(1);  \
        }  \
    })

#define uw_assert_null(value)   uw_assert(uw_is_null  (value))
#define uw_assert_bool(value)   uw_assert(uw_is_bool  (value))
#define uw_assert_int(value)    uw_assert(uw_is_int   (value))
#define uw_assert_float(value)  uw_assert(uw_is_float (value))
#define uw_assert_string(value) uw_assert(uw_is_string(value))
#define uw_assert_list(value)   uw_assert(uw_is_list  (value))
#define uw_assert_map(value)    uw_assert(uw_is_map   (value))

/****************************************************************
 * C type identifiers
 *
 * These identifiers are used to provide type info
 * to variadic functions.
 */

#define uw_nullptr    0  // nullptr_t
#define uw_bool       1  // bool
#define uw_char       2  // char
#define uw_uchar      3  // unsigned char
#define uw_short      4  // short
#define uw_ushort     5  // unsigned short
#define uw_int        6  // int
#define uw_uint       7  // unsigned int
#define uw_long       8  // long
#define uw_ulong      9  // unsigned long
#define uw_longlong  10  // long long
#define uw_ulonglong 11  // unsigned long long
#define uw_int32     12  // int32_t
#define uw_uint32    13  // uint32_t
#define uw_int64     14  // int64_t
#define uw_uint64    15  // uint64_t
#define uw_float     16  // float
#define uw_double    17  // double
#define uw_charptr   18  // char*
#define uw_char8ptr  19  // char8_t*
#define uw_char32ptr 20  // char32_t*
#define uw_value     21  // UwValue
#define uw_ptr       22  // UwValuePtr

/****************************************************************
 * Constructors
 */

#define _uw_alloc_value() \
    ({ \
        UwValuePtr value = malloc(sizeof(struct _UwValue)); \
        if (!value) { \
            perror(__func__); \
            exit(1); \
        } \
        value->refcount = 1; \
        value; \
    })

UwValuePtr _uw_create_null (nullptr_t    dummy);
UwValuePtr _uw_create_bool (UwType_Bool  initializer);
UwValuePtr _uw_create_int  (UwType_Int   initializer);
UwValuePtr _uw_create_float(UwType_Float initializer);

UwValuePtr uw_create_empty_string(size_t capacity, uint8_t char_size);

#ifdef DEBUG
    UwValuePtr uw_create_empty_string2(uint8_t cap_size, uint8_t char_size);
#endif

UwValuePtr  uw_create_string_c         (char*      initializer);
UwValuePtr _uw_create_string_u8_wrapper(char*      initializer);
UwValuePtr _uw_create_string_u8        (char8_t*   initializer);
UwValuePtr _uw_create_string_u32       (char32_t*  initializer);
UwValuePtr _uw_create_string_uw        (UwValuePtr str);  // XXX convert value of other UW types to string?

UwValuePtr uw_create_list();
UwValuePtr uw_create_list_va(...);
UwValuePtr uw_create_list_ap(va_list ap);
UwValuePtr uw_create_map();
UwValuePtr uw_create_map_va(...);
UwValuePtr uw_create_map_ap(va_list ap);

_UwList* _uw_alloc_list(size_t capacity);  // used by lists and maps

UwValuePtr  uw_copy       (UwValuePtr value);
UwValuePtr _uw_copy_string(_UwString* str);
UwValuePtr _uw_copy_list  (_UwList*   list);
UwValuePtr _uw_copy_map   (_UwMap*    map);

UwValuePtr uw_create_from_ctype(int ctype, va_list args);
/*
 * Helper function for variadic constructors.
 * Create UwValue from C type returned by va_arg(args).
 * See C type identifiers.
 * For uw_value return reference to the orginal value using uw_makeref.
 * For uw_ptr return the orginal value as is.
 */

/****************************************************************
 * Destructors
 */

void uw_delete(UwValuePtr* value);
void uw_delete_cstring(CStringPtr* str);

// helper functions for uw_delete
void _uw_delete_string(_UwString* str);
void _uw_delete_list  (_UwList*   list);
void _uw_delete_map   (_UwMap*    map);


/****************************************************************
 * Hash functions
 */

UwType_Hash uw_hash(UwValuePtr value);
/*
 * Calculate hash of value.
 */

void _uw_calculate_hash(UwValuePtr value, UwHashContext* ctx);
void _uw_string_hash   (_UwString* str,   UwHashContext* ctx);
void _uw_list_hash     (_UwList*   list,  UwHashContext* ctx);
#define _uw_map_hash(map, ctx)  _uw_list_hash((map)->kv_pairs, (ctx))
/*
 * Helper functions for uw_hash for complex types.
 */

/****************************************************************
 * Comparison functions
 *
 * XXX collation is not implemented for strings
 */

#define UW_EQ       0
#define UW_LESS    -1
#define UW_GREATER  1
#define UW_NEQ      2  // not comparable types or greater/less comparison is not possible

int _uw_compare_null      (UwValuePtr a, nullptr_t          b);
int _uw_compare_bool      (UwValuePtr a, bool               b);
int _uw_compare_char      (UwValuePtr a, char               b);
int _uw_compare_uchar     (UwValuePtr a, unsigned char      b);
int _uw_compare_short     (UwValuePtr a, short              b);
int _uw_compare_ushort    (UwValuePtr a, unsigned short     b);
int _uw_compare_int       (UwValuePtr a, int                b);
int _uw_compare_uint      (UwValuePtr a, unsigned int       b);
int _uw_compare_long      (UwValuePtr a, long               b);
int _uw_compare_ulong     (UwValuePtr a, unsigned long      b);
int _uw_compare_longlong  (UwValuePtr a, long long          b);
int _uw_compare_ulonglong (UwValuePtr a, unsigned long long b);
int _uw_compare_float     (UwValuePtr a, float              b);
int _uw_compare_double    (UwValuePtr a, double             b);
int  uw_compare_cstr      (UwValuePtr a, char*              b);  // can't be used in generic
int _uw_compare_u8_wrapper(UwValuePtr a, char*              b);
int _uw_compare_u8        (UwValuePtr a, char8_t*           b);
int _uw_compare_u32       (UwValuePtr a, char32_t*          b);
int _uw_compare_uw        (UwValuePtr a, UwValuePtr         b);

int _uw_string_cmp(_UwString* a, _UwString* b);
int _uw_list_cmp  (_UwList*   a, _UwList*   b);
int _uw_map_cmp   (_UwMap*    a, _UwMap*    b);

/*
 * Simplified comparison for equality.
 */
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
bool  uw_equal_cstr      (UwValuePtr a, char*              b);  // can't be used in generic
bool _uw_equal_u8_wrapper(UwValuePtr a, char*              b);
bool _uw_equal_u8        (UwValuePtr a, char8_t*           b);
bool _uw_equal_u32       (UwValuePtr a, char32_t*          b);
bool _uw_equal_uw        (UwValuePtr a, UwValuePtr         b);

bool _uw_string_eq(_UwString* a, _UwString* b);
bool _uw_list_eq  (_UwList*   a, _UwList*   b);
bool _uw_map_eq   (_UwMap*    a, _UwMap*    b);

/****************************************************************
 * List functions
 */

#define _uw_list_length(list) ((list)->length)

//size_t uw_list_length(UwValuePtr list);
#define uw_list_length(list) \
    ({ uw_assert_list(list); _uw_list_length((list)->list_value); })

void _uw_list_append_null      (UwValuePtr list, UwType_Null  item);
void _uw_list_append_bool      (UwValuePtr list, UwType_Bool  item);
void _uw_list_append_int       (UwValuePtr list, UwType_Int   item);
void _uw_list_append_float     (UwValuePtr list, UwType_Float item);
void _uw_list_append_u8_wrapper(UwValuePtr list, char*        item);
void _uw_list_append_u8        (UwValuePtr list, char8_t*     item);
void _uw_list_append_u32       (UwValuePtr list, char32_t*    item);
void _uw_list_append_uw        (UwValuePtr list, UwValuePtr   item);

void _uw_list_append(_UwList** list_ref, UwValuePtr item);
/*
 * Internal append function, also used by map implementation.
 */

void uw_list_append_va(UwValuePtr list, ...);
void uw_list_append_ap(UwValuePtr list, va_list ap);

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

void _uw_list_del(_UwList* list, size_t start_index, size_t end_index);
// internal function

UwValuePtr uw_list_slice(UwValuePtr list, size_t start_index, size_t end_index);
/*
 * Return shallow copy of the given range of list.
 */

/****************************************************************
 * Map functions
 */

void uw_map_update(UwValuePtr map, UwValuePtr key, UwValuePtr value);
void uw_map_update_va(UwValuePtr map, ...);
void uw_map_update_ap(UwValuePtr map, va_list ap);
/*
 * Insert or assign key-value pair using move semantic.
 */

/*
 * Check `key` is in `map`.
 */
bool _uw_map_has_key_null      (UwValuePtr map, UwType_Null  key);
bool _uw_map_has_key_bool      (UwValuePtr map, UwType_Bool  key);
bool _uw_map_has_key_int       (UwValuePtr map, UwType_Int   key);
bool _uw_map_has_key_float     (UwValuePtr map, UwType_Float key);
bool _uw_map_has_key_u8_wrapper(UwValuePtr map, char*        key);
bool _uw_map_has_key_u8        (UwValuePtr map, char8_t*     key);
bool _uw_map_has_key_u32       (UwValuePtr map, char32_t*    key);
bool _uw_map_has_key_uw        (UwValuePtr map, UwValuePtr   key);

/*
 * Get value by `key`. Return nullptr if `key` is not in `map`.
 */
UwValuePtr _uw_map_get_null      (UwValuePtr map, UwType_Null  key);
UwValuePtr _uw_map_get_bool      (UwValuePtr map, UwType_Bool  key);
UwValuePtr _uw_map_get_int       (UwValuePtr map, UwType_Int   key);
UwValuePtr _uw_map_get_float     (UwValuePtr map, UwType_Float key);
UwValuePtr _uw_map_get_u8_wrapper(UwValuePtr map, char*        key);
UwValuePtr _uw_map_get_u8        (UwValuePtr map, char8_t*     key);
UwValuePtr _uw_map_get_u32       (UwValuePtr map, char32_t*    key);
UwValuePtr _uw_map_get_uw        (UwValuePtr map, UwValuePtr   key);

/*
 * Delete item from map by `key`.
 */
void _uw_map_del_null      (UwValuePtr map, UwType_Null  key);
void _uw_map_del_bool      (UwValuePtr map, UwType_Bool  key);
void _uw_map_del_int       (UwValuePtr map, UwType_Int   key);
void _uw_map_del_float     (UwValuePtr map, UwType_Float key);
void _uw_map_del_u8_wrapper(UwValuePtr map, char*        key);
void _uw_map_del_u8        (UwValuePtr map, char8_t*     key);
void _uw_map_del_u32       (UwValuePtr map, char32_t*    key);
void _uw_map_del_uw        (UwValuePtr map, UwValuePtr   key);

#define uw_map_length(map) \
    ({ uw_assert_map(map); _uw_list_length((map)->map_value->kv_pairs) >> 1; })

bool uw_map_item(UwValuePtr map, size_t index, UwValuePtr* key, UwValuePtr* value);
/*
 * Get key-value pair from the map.
 * Return true if `index` is valid.
 * It's the caller's responsibility to destroy returned key-value to avoid memory leaks.
 * The simplest way is passing pointers to UwValue.
 */

/****************************************************************
 * String functions
 */

#define uw_string_char_size(s)  ((s)->string_value->char_size + 1)

size_t uw_strlen(UwValuePtr str);
/*
 * Return length of string.
 */

CStringPtr uw_string_to_cstring(UwValuePtr str);
/*
 * Create C string.
 * XXX decode multibyte ???
 */

void uw_string_copy_buf(UwValuePtr str, char* buffer);
/*
 * Copy string to buffer, appending terminating 0.
 * Use carefully. The caller is responsible to allocate the buffer.
 * XXX decode multibyte ???
 */

// somewhat ugly macro to define a local variable initialized with a copy of uw string:
#define UW_CSTRING_LOCAL(variable, str) \
    char variable[uw_strlen(str) + 1]; \
    uw_string_copy_buf((str), variable)

void uw_string_swap(UwValuePtr a, UwValuePtr b);
/*
 * Swap internal structures.
 *
 * XXX todo same for lists, maps and integral types?
 */

/*
 * Append functions
 */
void  uw_string_append_char      (UwValuePtr dest, char       c);    // can't be used in generic
void  uw_string_append_cstr      (UwValuePtr dest, char*      src);  // can't be used in generic
void _uw_string_append_c32       (UwValuePtr dest, char32_t   c);
void _uw_string_append_u8_wrapper(UwValuePtr dest, char*      src);
void _uw_string_append_u8        (UwValuePtr dest, char8_t*   src);
void _uw_string_append_u32       (UwValuePtr dest, char32_t*  src);
void _uw_string_append_uw        (UwValuePtr dest, UwValuePtr src);

/*
 * Insert functions
 * TODO other types
 */
void _uw_string_insert_many_c32(UwValuePtr str, size_t position, char32_t value, size_t n);

/*
 * Append substring functions.
 *
 * Append `src` substring starting from `src_start_pos` to `src_end_pos`.
 */
void  uw_string_append_substring_cstr      (UwValuePtr dest, char*      src, size_t src_start_pos, size_t src_end_pos);  // can't be used in generic
void _uw_string_append_substring_u8_wrapper(UwValuePtr dest, char*      src, size_t src_start_pos, size_t src_end_pos);
void _uw_string_append_substring_u8        (UwValuePtr dest, char8_t*   src, size_t src_start_pos, size_t src_end_pos);
void _uw_string_append_substring_u32       (UwValuePtr dest, char32_t*  src, size_t src_start_pos, size_t src_end_pos);
void _uw_string_append_substring_uw        (UwValuePtr dest, UwValuePtr src, size_t src_start_pos, size_t src_end_pos);

size_t uw_string_append_utf8(UwValuePtr dest, char8_t* buffer, size_t size);
/*
 * Append UTF-8-encoded characters from `buffer`.
 * Return the number of bytes processed, which can be less than `size`
 * if buffer ends with incomplete UTF-8 sequence.
 */

UwValuePtr uw_string_get_substring(UwValuePtr str, size_t start_pos, size_t end_pos);
/*
 * Get substring from `start_pos` to `end_pos`.
 */

/*
 * Substring comparison functions.
 *
 * Compare `str_a` from `start_pos` to `end_pos` with `str_b`.
 */
bool  uw_substring_eq_cstr      (UwValuePtr a, size_t start_pos, size_t end_pos, char*      b);  // can't be used in generic
bool _uw_substring_eq_u8_wrapper(UwValuePtr a, size_t start_pos, size_t end_pos, char*      b);
bool _uw_substring_eq_u8        (UwValuePtr a, size_t start_pos, size_t end_pos, char8_t*   b);
bool _uw_substring_eq_u32       (UwValuePtr a, size_t start_pos, size_t end_pos, char32_t*  b);
bool _uw_substring_eq_uw        (UwValuePtr a, size_t start_pos, size_t end_pos, UwValuePtr b);

char32_t uw_string_at(UwValuePtr str, size_t position);
/*
 * Return character at `position`.
 * If position is beyond end of line return 0.
 */

//bool uw_string_index_valid(UwValuePtr str, size_t index);
#define uw_string_index_valid(str, index) ((index) < uw_strlen(str))
/*
 * Return true if `index` is within string length.
 */

//bool string_indexof(UwValuePtr str, char32_t chr, size_t start_pos, size_t* result);
/*
 * Find first occurence of `chr`.
 */

void uw_string_erase(UwValuePtr str, size_t start_pos, size_t end_pos);
/*
 * Erase characters from `start_pos` to `end_pos`.
 */

void uw_string_truncate(UwValuePtr str, size_t position);
/*
 * Truncate string at given `position`.
 */

void uw_string_ltrim(UwValuePtr str);
void uw_string_rtrim(UwValuePtr str);
void uw_string_trim(UwValuePtr str);

#ifdef DEBUG
    bool uw_eq_fast(_UwString* a, _UwString* b);
#endif

/*
 * Split functions.
 * Return list of strings.
 */
UwValuePtr uw_string_split(UwValuePtr str);  // split by spaces

UwValuePtr _uw_string_split_c32(UwValuePtr str, char32_t splitter);

UwValuePtr  uw_string_split_any_cstr      (UwValuePtr str, char*      splitters);  // can't be used in generic
UwValuePtr _uw_string_split_any_u8_wrapper(UwValuePtr str, char*      splitters);
UwValuePtr _uw_string_split_any_u8        (UwValuePtr str, char8_t*   splitters);
UwValuePtr _uw_string_split_any_u32       (UwValuePtr str, char32_t*  splitters);
UwValuePtr _uw_string_split_any_uw        (UwValuePtr str, UwValuePtr splitters);

UwValuePtr  uw_string_split_strict_cstr      (UwValuePtr str, char*      splitter);  // can't be used in generic
UwValuePtr _uw_string_split_strict_u8_wrapper(UwValuePtr str, char*      splitter);
UwValuePtr _uw_string_split_strict_u8        (UwValuePtr str, char8_t*   splitter);
UwValuePtr _uw_string_split_strict_u32       (UwValuePtr str, char32_t*  splitter);
UwValuePtr _uw_string_split_strict_uw        (UwValuePtr str, UwValuePtr splitter);

/*
 * Join list items. Return string value.
 */
UwValuePtr _uw_string_join_c32       (char32_t   separator, UwValuePtr list);
UwValuePtr _uw_string_join_u8_wrapper(char*      separator, UwValuePtr list);
UwValuePtr _uw_string_join_u8        (char8_t*   separator, UwValuePtr list);
UwValuePtr _uw_string_join_u32       (char32_t*  separator, UwValuePtr list);
UwValuePtr _uw_string_join_uw        (UwValuePtr separator, UwValuePtr list);

/*
 * Miiscellaneous helper functions.
 */

size_t utf8_strlen(char8_t* str);
/*
 * Count the number of codepoints in UTF8-encoded string.
 */

size_t utf8_strlen2(char8_t* str, uint8_t* char_size);
/*
 * Count the number of codepoints in UTF8-encoded string
 * and find max char size.
 */

size_t utf8_strlen2_buf(char8_t* buffer, size_t* size, uint8_t* char_size);
/*
 * Count the number of codepoints in the buffer and find max char size.
 *
 * Null characters are allowed! They are counted as zero codepoints.
 *
 * Return the number of codepoints.
 * Write the number of processed bytes back to `size`.
 * This number can be less than original `size` if buffer ends with
 * incomplete sequence.
 */

char8_t* utf8_skip(char8_t* str, size_t n);
/*
 * Skip `n` characters, return pointer to `n`th char.
 */

size_t u32_strlen(char32_t* str);
/*
 * Find length of null-terminated `str`.
 */

size_t u32_strlen2(char32_t* str, uint8_t* char_size);
/*
 * Find both length of null-terminated `str` and max char size in one go.
 */

char32_t* u32_strchr(char32_t* str, char32_t chr);
/*
 * Find the first occurrence of `chr` in the null-terminated `str`.
 */

uint8_t u32_char_size(char32_t* str, size_t max_len);
/*
 * Find the maximal size of character in `str`, up to `max_len` or null terminator.
 */

/****************************************************************
 * Character classification functions
 */

#define uw_char_isspace(c)  u_isspace(c)
/*
 * Return true if `c` is a whitespace character.
 */

#define uw_char_isdigit(c)  ('0' <= (c) && (c) <= '9')
/*
 * Return true if `c` is an ASCII digit.
 * Do not consider any other unicode digits because this function
 * is used in conjunction with standard C library which does
 * not support unicode character classification.
 */

/****************************************************************
 * Debug functions
 */

#ifdef DEBUG

void  uw_dump(UwValuePtr value);
void _uw_dump(UwValuePtr value, int indent, char* label);
void _uw_dump_string(_UwString* str,   int indent);
void _uw_dump_list  (_UwList*   list,  int indent);
void _uw_dump_map   (_UwMap*    map,   int indent);

void _uw_print_indent(int indent);

#endif

#ifdef __cplusplus
}
#endif
