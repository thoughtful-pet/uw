#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <uchar.h>

#ifdef __cplusplus
extern "C" {
#endif

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

/****************************************************************
 * UwValue
 */

// Type bit width defines the maximum number of types
// and the size of _uw_types array.
#ifndef UW_TYPE_BITWIDTH
#   define UW_TYPE_BITWIDTH  8
#endif

#ifndef UW_ALLOCATOR_BITWIDTH
#   define UW_ALLOCATOR_BITWIDTH  2
#endif

// remaining bits are used for reference count
#define UW_REFCOUNT_BITWIDTH  (64 - UW_TYPE_BITWIDTH - UW_ALLOCATOR_BITWIDTH)
#define UW_MAX_REFCOUNT       ((1ULL << UW_REFCOUNT_BITWIDTH) - 1)

// type for type id
typedef unsigned _BitInt(UW_TYPE_BITWIDTH) UwTypeId;

// type for allocator id
typedef unsigned _BitInt(UW_ALLOCATOR_BITWIDTH) UwAllocId;

// forward declarations
struct _UwValue;
struct _UwString;
struct _UwList;
struct _UwMap;

// Integral types
typedef nullptr_t  UwType_Null;
typedef bool       UwType_Bool;
typedef int64_t    UwType_Int;
typedef double     UwType_Float;

struct _UwValue {
    uint64_t type_id: UW_TYPE_BITWIDTH,
             refcount: UW_REFCOUNT_BITWIDTH,
             alloc_id: UW_ALLOCATOR_BITWIDTH;
    union {
        UwType_Null  null_value;
        UwType_Bool  bool_value;
        UwType_Int   int_value;
        UwType_Float float_value;
        struct _UwString* string_value;
        struct _UwList*   list_value;
        struct _UwMap*    map_value;
    };
};

typedef struct _UwValue* UwValuePtr;

static inline UwValuePtr uw_makeref(UwValuePtr v)
/*
 * make new reference
 */
{
    if (v) {
        uw_assert(v->refcount < UW_MAX_REFCOUNT);
        v->refcount++;
    }
    return v;
}

static inline UwValuePtr uw_move(UwValuePtr* v)
/*
 * "Move" value to another variable or out of the function
 * (i.e. return a value and reset autocleaned variable)
 */
{
    UwValuePtr tmp = *v;
    *v = nullptr;
    return tmp;
}

/****************************************************************
 * Allocators
 */

typedef void* (*UwAlloc)  (size_t nbytes);
typedef void* (*UwRealloc)(void* block, size_t nbytes);
typedef void  (*UwFree)   (void* block);

typedef struct {
    UwAllocId id;
    UwAlloc   alloc;
    UwRealloc realloc;
    UwFree    free;
} UwAllocator;

#define UW_ALLOCATOR_STD         0  // built-in based on malloc/realloc/free
#define UW_ALLOCATOR_STD_NOFAIL  1  // built-in based on malloc/realloc/free, exit on OOM

extern UwAllocator _uw_allocators[1 << UW_ALLOCATOR_BITWIDTH];
/*
 * Global list of allocators initialized with built-in ones.
 */

extern thread_local UwAllocator _uw_allocator;
/*
 * Current allocator.
 */

void uw_set_allocator(UwAllocId alloc_id);
/*
 * Change current allocator.
 */

/*
 * Helper functions
 */

static inline UwValuePtr _uw_alloc_value(UwTypeId type_id)
{
    UwValuePtr value = _uw_allocator.alloc(sizeof(struct _UwValue));
    if (value) {
        value->type_id  = type_id;
        value->refcount = 1;
    }
    return value;
}

static inline void _uw_free_value(UwValuePtr value)
{
    _uw_allocators[value->alloc_id].free(value);
}

/****************************************************************
 * Hash function
 */

typedef uint64_t  UwType_Hash;

struct _UwHashContext;
typedef struct _UwHashContext UwHashContext;

void _uw_hash_uint64(UwHashContext* ctx, uint64_t data);

/****************************************************************
 * Types
 */

// C type identifiers

// These identifiers are used to provide C type info
// to such methods as equal_ctype and variadic functions.

typedef enum {
    uwc_nullptr   =  0,  // nullptr_t
    uwc_bool      =  1,  // bool
    uwc_char      =  2,  // char
    uwc_uchar     =  3,  // unsigned char
    uwc_short     =  4,  // short
    uwc_ushort    =  5,  // unsigned short
    uwc_int       =  6,  // int
    uwc_uint      =  7,  // unsigned int
    uwc_long      =  8,  // long
    uwc_ulong     =  9,  // unsigned long
    uwc_longlong  = 10,  // long long
    uwc_ulonglong = 11,  // unsigned long long
    uwc_int32     = 12,  // int32_t
    uwc_uint32    = 13,  // uint32_t
    uwc_int64     = 14,  // int64_t
    uwc_uint64    = 15,  // uint64_t
    uwc_float     = 16,  // float
    uwc_double    = 17,  // double
    uwc_charptr   = 18,  // char*
    uwc_char8ptr  = 19,  // char8_t*
    uwc_char32ptr = 20,  // char32_t*
    uwc_value_ptr = 21,  // UwValuePtr
    uwc_value     = 22   // reserved for autocleaned UwValue
} UwCType;

// predefined types
#define UwTypeId_Null    0
#define UwTypeId_Bool    1
#define UwTypeId_Int     2
#define UwTypeId_Float   3
#define UwTypeId_String  4
#define UwTypeId_List    5
#define UwTypeId_Map     6

// type checking
#define uw_is_null(value)    ((value) && ((value)->type_id == UwTypeId_Null))
#define uw_is_bool(value)    ((value) && ((value)->type_id == UwTypeId_Bool))
#define uw_is_int(value)     ((value) && ((value)->type_id == UwTypeId_Int))
#define uw_is_float(value)   ((value) && ((value)->type_id == UwTypeId_Float))
#define uw_is_string(value)  ((value) && ((value)->type_id == UwTypeId_String))
#define uw_is_list(value)    ((value) && ((value)->type_id == UwTypeId_List))
#define uw_is_map(value)     ((value) && ((value)->type_id == UwTypeId_Map))

#define uw_assert_null(value)   uw_assert(uw_is_null  (value))
#define uw_assert_bool(value)   uw_assert(uw_is_bool  (value))
#define uw_assert_int(value)    uw_assert(uw_is_int   (value))
#define uw_assert_float(value)  uw_assert(uw_is_float (value))
#define uw_assert_string(value) uw_assert(uw_is_string(value))
#define uw_assert_list(value)   uw_assert(uw_is_list  (value))
#define uw_assert_map(value)    uw_assert(uw_is_map   (value))

// function types for the basic interface
typedef UwValuePtr (*UwMethodCreate)    ();
typedef void       (*UwMethodDestroy)   (UwValuePtr self);
typedef void       (*UwMethodHash)      (UwValuePtr self, UwHashContext* ctx);
typedef UwValuePtr (*UwMethodCopy)      (UwValuePtr self);
typedef void       (*UwMethodDump)      (UwValuePtr self, int indent);
typedef UwValuePtr (*UwMethodToString)  (UwValuePtr self);
typedef bool       (*UwMethodIsTrue)    (UwValuePtr self);
typedef bool       (*UwMethodEqual)     (UwValuePtr self, UwValuePtr other);
typedef bool       (*UwMethodEqualCType)(UwValuePtr self, UwCType ctype, ...);

typedef struct {
    UwTypeId id;
    UwTypeId ancestor_id;

    char* name;

    // basic interface
    UwMethodCreate     create;
    UwMethodDestroy    destroy;
    UwMethodHash       hash;
    UwMethodCopy       copy;
    UwMethodDump       dump;
    UwMethodToString   to_string;
    UwMethodIsTrue     is_true;
    UwMethodEqual      equal_sametype;
    UwMethodEqual      equal;
    UwMethodEqualCType equal_ctype;
} UwType;

extern UwType _uw_types[1 << UW_TYPE_BITWIDTH];
/*
 * Global list of types initialized with built-in types.
 */

/****************************************************************
 * Basic functions
 */

UwValuePtr _uw_create(UwTypeId type_id);
/*
 * Basic constructor.
 */

void uw_delete(UwValuePtr* value);
/*
 * Destructor.
 */

UwType_Hash uw_hash(UwValuePtr value);
/*
 * Calculate hash of value.
 */

static inline UwValuePtr uw_copy(UwValuePtr value)
{
    UwMethodCopy fn_copy = _uw_types[value->type_id].copy;
    uw_assert(fn_copy != nullptr);
    return fn_copy(value);
}

/****************************************************************
 * Helper functions
 */

static inline void _uw_call_destroy(UwValuePtr value)
{
    UwTypeId type_id = value->type_id;
    UwMethodDestroy fn_destroy = _uw_types[type_id].destroy;
    uw_assert(fn_destroy != nullptr);
    fn_destroy(value);
}

static inline void _uw_call_hash(UwValuePtr value, UwHashContext* ctx)
{
    UwTypeId type_id = value->type_id;
    UwMethodHash fn_hash = _uw_types[type_id].hash;
    uw_assert(fn_hash != nullptr);
    fn_hash(value, ctx);
}

static inline void _uw_call_dump(UwValuePtr value, int indent)
{
    UwTypeId type_id = value->type_id;
    UwMethodDump fn_dump = _uw_types[type_id].dump;
    uw_assert(fn_dump != nullptr);
    fn_dump(value, indent);
}

static inline bool _uw_equal(UwValuePtr a, UwValuePtr b)
/*
 * Compare for equality.
 */
{
    if (a == b) {
        // compare with self
        return true;
    }
    UwType* t = &_uw_types[a->type_id];
    UwMethodEqual cmp;
    if (a->type_id == b->type_id) {
        cmp = t->equal_sametype;
    } else {
        cmp = t->equal;
    }
    return cmp(a, b);
}

/****************************************************************
 * Debug functions
 */

void _uw_print_indent(int indent);
void _uw_dump_start(UwValuePtr value, int indent);
void _uw_dump(UwValuePtr value, int indent);

void uw_dump(UwValuePtr value);

#ifdef __cplusplus
}
#endif
