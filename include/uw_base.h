#pragma once

/*
 * Configuration:
 *
 * UW_TYPE_BITWIDTH        default: 8
 * UW_ALLOCATOR_BITWIDTH   default: 2
 * UW_INTERFACE_BITWIDTH   default: 8
 *
 * UW_TYPE_CAPACITY        default: 1 << UW_TYPE_BITWIDTH
 * UW_INTERFACE_CAPACITY   default: 1 << UW_INTERFACE_BITWIDTH
 */

#include <limits.h>
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

#ifndef UW_TYPE_CAPACITY
#    define UW_TYPE_CAPACITY  (1 << UW_TYPE_BITWIDTH)
#else
#    if UW_TYPE_CAPACITY > (1 << UW_TYPE_BITWIDTH)
#        undef UW_TYPE_CAPACITY
#        define UW_TYPE_CAPACITY  (1 << UW_TYPE_BITWIDTH)
#    endif
#endif

#ifndef UW_ALLOCATOR_BITWIDTH
#   define UW_ALLOCATOR_BITWIDTH  2
#endif

// remaining bits (minus compound bit) are used for reference count
#define UW_REFCOUNT_BITWIDTH  (64 - UW_TYPE_BITWIDTH - UW_ALLOCATOR_BITWIDTH - 1)
#define UW_MAX_REFCOUNT       ((1ULL << UW_REFCOUNT_BITWIDTH) - 1)

// Crazy idea: Given that UwValuePtr is always aligned,
// last 3 bits could be used for allocator id.
// But we'd have to reset these bits each time using UwValuePtr as pointer.

// type for type id
typedef unsigned _BitInt(UW_TYPE_BITWIDTH) UwTypeId;

// type for allocator id
typedef unsigned _BitInt(UW_ALLOCATOR_BITWIDTH) UwAllocId;

// Integral types
typedef nullptr_t  UwType_Null;
typedef bool       UwType_Bool;
typedef int64_t    UwType_Int;
typedef double     UwType_Float;

typedef uint64_t _UwValueBase;

struct _UwValue {
    _UwValueBase type_id: UW_TYPE_BITWIDTH,
                 refcount: UW_REFCOUNT_BITWIDTH,
                 compound: 1,
                 alloc_id: UW_ALLOCATOR_BITWIDTH;
    // this is a variable part defined only for convenience of integral types,
    // actual size of this structure can be as small as _UwValueBase,
    // e.g. null values do not include this:
    union {
        UwType_Bool  bool_value;
        UwType_Int   int_value;
        UwType_Float float_value;
        uint64_t     embrace_count;  // for compound values
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

static inline UwValuePtr _uw_embrace(UwValuePtr v)
/*
 * Helper function for compound objects.
 *
 * If v is a compound value, increase embrace count.
 */
{
    uw_assert(v);
    uw_assert(v->compound);
    v->embrace_count++;
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

typedef void* (*UwAlloc)  (unsigned nbytes);
typedef void* (*UwRealloc)(void* block, unsigned nbytes);
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

UwValuePtr _uw_alloc_value(UwTypeId type_id);
/*
 * Allocate storage for value using current allocator.
 * The storage is not initialized and may contain garbage.
 */

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
 * C type identifiers
 */

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
    uwc_value_ptr = 21,  // UwValuePtr without incrementing refcount
    uwc_value_makeref = 22   // increment refcount, used to pass autocleaned UwValue
} UwCType;

/****************************************************************
 * Interfaces
 */

#ifndef UW_INTERFACE_BITWIDTH
#   define UW_INTERFACE_BITWIDTH  8
#endif

#ifndef UW_INTERFACE_CAPACITY
#    define UW_INTERFACE_CAPACITY  (1 << UW_INTERFACE_BITWIDTH)
#else
#    if UW_INTERFACE_CAPACITY > (1 << UW_INTERFACE_BITWIDTH)
#        undef UW_INTERFACE_CAPACITY
#        define UW_INTERFACE_CAPACITY  (1 << UW_INTERFACE_BITWIDTH)
#    endif
#endif

extern bool _uw_registered_interfaces[UW_INTERFACE_CAPACITY];
/*
 * Global list of registered interfaces.
 * Its purpose is simply track assigned ids.
 *
 * Not using bit array because static initialization
 * of such array is a nightmare in C.
 */

int uw_register_interface();
/*
 * Add interface to the first available position in the global list.
 * Return interface id or -1 if the list is full.
 */

/*
 * The following macro leverages naming scheme where
 * interface structure is named UwInterface_<interface_name>
 * and id is named UwInterfaceId_<interface_name>
 */
#define uw_get_interface(value, interface_name)  \
    (  \
        (UwInterface_##interface_name *) \
            _uw_types[(value)->type_id]->interfaces[UwInterfaceId_##interface_name]  \
    )

// Built-in interfaces
// TBD, TODO
#define UwInterfaceId_Logic         0
    // TBD
#define UwInterfaceId_Arithmetic    1
    // TBD
#define UwInterfaceId_Bitwise       2
    // TBD
#define UwInterfaceId_Comparison    3
    // UwMethodCompare  -- compare_sametype, compare;
#define UwInterfaceId_RandomAccess  4
    // UwMethodLength
    // UwMethodGetItem     (by index for arrays/strings or by key for maps)
    // UwMethodSetItem     (by index for arrays/strings or by key for maps)
    // UwMethodDeleteItem  (by index for arrays/strings or by key for maps)
    // UwMethodPopItem -- necessary here? it's just delete_item(length - 1)
#define UwInterfaceId_List          5
    // string supports this interface
    // UwMethodAppend
    // UwMethodSlice
    // UwMethodDeleteRange

#define UwInterfaceId_File          6
#define UwInterfaceId_FileReader    7
#define UwInterfaceId_FileWriter    8

#define UwInterfaceId_LineReader    9

/****************************************************************
 * Types
 */

struct _UwValueChain {
    /*
     * Compound values may contain circular references.
     * This structure along with function `_uw_compound_value_seen`
     * helps to track them.
     * See dump implementation for list and map values.
     */
    struct _UwValueChain* prev;
    UwValuePtr value;
};

// Function types for the basic interface.
// The basic interface is embedded into UwType structure.
typedef bool       (*UwMethodInit)      (UwValuePtr self);
typedef void       (*UwMethodFini)      (UwValuePtr self);
typedef void       (*UwMethodUnbrace)   (UwValuePtr self);
typedef void       (*UwMethodHash)      (UwValuePtr self, UwHashContext* ctx);
typedef UwValuePtr (*UwMethodCopy)      (UwValuePtr self);
typedef void       (*UwMethodDump)      (UwValuePtr self, int indent, struct _UwValueChain* prev_compound);
typedef UwValuePtr (*UwMethodToString)  (UwValuePtr self);
typedef bool       (*UwMethodIsTrue)    (UwValuePtr self);
typedef bool       (*UwMethodEqual)     (UwValuePtr self, UwValuePtr other);
typedef bool       (*UwMethodEqualCType)(UwValuePtr self, UwCType ctype, ...);

typedef struct {
    UwTypeId id;
    UwTypeId ancestor_id;

    char* name;

    unsigned data_size;
    unsigned data_offset;

    // basic interface
    UwMethodInit       init;
    UwMethodFini       fini;
    UwMethodFini       unbrace;  // if set, the value is compound
    UwMethodHash       hash;
    UwMethodCopy       copy;
    UwMethodDump       dump;
    UwMethodToString   to_string;
    UwMethodIsTrue     is_true;
    UwMethodEqual      equal_sametype;
    UwMethodEqual      equal;
    UwMethodEqualCType equal_ctype;

    // extra interfaces
    void* interfaces[UW_INTERFACE_CAPACITY];

} UwType;

// Built-in types
#define UwTypeId_Null       0
#define UwTypeId_Bool       1
#define UwTypeId_Int        2
#define UwTypeId_Float      3
#define UwTypeId_String     4
#define UwTypeId_List       5
#define UwTypeId_Map        6
#define UwTypeId_Class      7
#define UwTypeId_File       8
#define UwTypeId_StringIO   9

// type checking
#define uw_is_null(value)      uw_is_subclass((value), UwTypeId_Null)
#define uw_is_bool(value)      uw_is_subclass((value), UwTypeId_Bool)
#define uw_is_int(value)       uw_is_subclass((value), UwTypeId_Int)
#define uw_is_float(value)     uw_is_subclass((value), UwTypeId_Float)
#define uw_is_string(value)    uw_is_subclass((value), UwTypeId_String)
#define uw_is_list(value)      uw_is_subclass((value), UwTypeId_List)
#define uw_is_map(value)       uw_is_subclass((value), UwTypeId_Map)
#define uw_is_class(value)     uw_is_subclass((value), UwTypeId_Class)
#define uw_is_file(value)      uw_is_subclass((value), UwTypeId_File)
#define uw_is_stringio(value)  uw_is_subclass((value), UwTypeId_StringIO)

#define uw_assert_null(value)      uw_assert(uw_is_null    (value))
#define uw_assert_bool(value)      uw_assert(uw_is_bool    (value))
#define uw_assert_int(value)       uw_assert(uw_is_int     (value))
#define uw_assert_float(value)     uw_assert(uw_is_float   (value))
#define uw_assert_string(value)    uw_assert(uw_is_string  (value))
#define uw_assert_list(value)      uw_assert(uw_is_list    (value))
#define uw_assert_map(value)       uw_assert(uw_is_map     (value))
#define uw_assert_class(value)     uw_assert(uw_is_class   (value))
#define uw_assert_file(value)      uw_assert(uw_is_file    (value))
#define uw_assert_stringio(value)  uw_assert(uw_is_stringio(value))

extern UwType* _uw_types[UW_TYPE_CAPACITY];
/*
 * Global list of types initialized with built-in types.
 */

int uw_add_type(UwType* type);
/*
 * Add type to the first available position in the global list.
 *
 * All fields of `type` must be initialized.
 *
 * Return type id or -1 if the list is full.
 */

int uw_subclass(UwType* type, char* name, UwTypeId ancestor_id, unsigned data_size);
/*
 * `type` and `name` should point to a static storage.
 *
 * Initialize `type` with ancestor's type, calculate data_offset, set data_size
 * and other essential fields, and then add `type` to the global list using `uw_add_type`.
 *
 * The caller should alter basic methods and set supported interfaces after
 * calling this function.
 *
 * Return subclassed type id or -1 if the list is full.
 */

static inline bool uw_is_subclass(UwValuePtr value, UwTypeId type_id)
{
    if (!value) {
        return false;
    }
    UwTypeId t = value->type_id;
    for (;;) {
        if (t == type_id) {
            return true;
        }
        t = _uw_types[t]->ancestor_id;
        if (t == UwTypeId_Null) {
            return false;
        }
    }
}

/****************************************************************
 * Basic functions
 */

UwValuePtr _uw_create(UwTypeId type_id);
/*
 * Basic constructor.
 */

void _uw_destroy(UwValuePtr value);
/*
 * Helper function for uw_delete and uw_unbrace.
 */

void uw_delete(UwValuePtr* value_ref);
/*
 * Destructor.
 */

bool _uw_unbrace(UwValuePtr* value_ref);
/*
 * Helper function for `fini` and similar (e.g. pop from list) methods of compound values.
 *
 * `value_ref` points to a value embraced by compound one.
 * If that value is compound, decrease embrace count and if it reaches zero,
 * destroy the value and return true.
 */

UwType_Hash uw_hash(UwValuePtr value);
/*
 * Calculate hash of value.
 */

static inline UwValuePtr uw_copy(UwValuePtr value)
{
    UwMethodCopy fn_copy = _uw_types[value->type_id]->copy;
    uw_assert(fn_copy != nullptr);
    return fn_copy(value);
}

#define _uw_get_data_ptr(value, type_id, type_name_ptr)  \
    (  \
        (type_name_ptr) (  \
            ((uint8_t*) (value)) + _uw_types[type_id]->data_offset \
        )  \
    )

/****************************************************************
 * Helper functions
 */

static inline UwValuePtr uw_create_null()
{
    return _uw_create(UwTypeId_Null);
}

static inline UwValuePtr uw_create_bool()
{
    return _uw_create(UwTypeId_Bool);
}

static inline UwValuePtr uw_create_int()
{
    return _uw_create(UwTypeId_Int);
}

static inline UwValuePtr uw_create_float()
{
    return _uw_create(UwTypeId_Float);
}

static inline void _uw_call_hash(UwValuePtr value, UwHashContext* ctx)
{
    UwTypeId type_id = value->type_id;
    UwMethodHash fn_hash = _uw_types[type_id]->hash;
    uw_assert(fn_hash != nullptr);
    fn_hash(value, ctx);
}

static inline void _uw_call_dump(UwValuePtr value, int indent, struct _UwValueChain* prev_compound)
{
    UwTypeId type_id = value->type_id;
    UwMethodDump fn_dump = _uw_types[type_id]->dump;
    uw_assert(fn_dump != nullptr);
    fn_dump(value, indent, prev_compound);
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
    if (a == nullptr || b == nullptr) {
        return false;
    }
    UwType* t = _uw_types[a->type_id];
    UwMethodEqual cmp;
    if (a->type_id == b->type_id) {
        cmp = t->equal_sametype;
    } else {
        cmp = t->equal;
    }
    return cmp(a, b);
}

bool _uw_compound_value_seen(UwValuePtr value, struct _UwValueChain* prev_compound);
/*
 * Check if value is on the chain.
 */

/****************************************************************
 * Debug functions
 */

void _uw_print_indent(int indent);
void _uw_dump_start(UwValuePtr value, int indent);
void _uw_dump(UwValuePtr value, int indent, struct _UwValueChain* prev_compound);

void uw_dump(UwValuePtr value);

#ifdef __cplusplus
}
#endif
