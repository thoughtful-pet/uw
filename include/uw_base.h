#pragma once

/*
 * Configuration:
 *
 * UW_TYPE_CAPACITY        default: 1 << (sizeof(UwTypeId) * 8)
 * UW_INTERFACE_CAPACITY   default: 256
 */

#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <uchar.h>

#ifdef __cplusplus
extern "C" {
#endif


#define _UWC_LENGTH_OF(array)  (sizeof(array) / sizeof((array)[0]))  // get array length

/****************************************************************
 * Assertions and panic
 *
 * Unlike assertions from standard library they cannot be
 * turned off and can be used for input parameters validation.
 */

#define uw_assert(condition) \
    ({  \
        if (!(condition)) {  \
            uw_panic("UW assertion failed at %s:%s:%d: " #condition "\n", __FILE__, __func__, __LINE__);  \
        }  \
    })

[[noreturn]]
void uw_panic(char* fmt, ...);

/****************************************************************
 * UwValue
 */

// Type for type id.
typedef uint8_t UwTypeId;

#ifndef UW_TYPE_CAPACITY
#    define UW_TYPE_CAPACITY  (1 << (sizeof(UwTypeId) * 8))
#else
#    if (UW_TYPE_CAPACITY == 0) || (UW_TYPE_CAPACITY > (1 << (sizeof(UwTypeId) * 8)))
#        undef UW_TYPE_CAPACITY
#        define UW_TYPE_CAPACITY  (1 << (sizeof(UwTypeId) * 8))
#    endif
#endif

// Integral types
typedef nullptr_t  UwType_Null;
typedef bool       UwType_Bool;
typedef int64_t    UwType_Signed;
typedef uint64_t   UwType_Unsigned;
typedef double     UwType_Float;

typedef struct {
    /*
     * Extra data for UwValue.
     * This structure is variable size.
     */
    unsigned refcount;
} _UwExtraData;

/*
 * Extra data for compound UwValue is a bit more complicated.
 *
 * Traversing cyclic references requires the list of parents with reference counts.
 * Parent reference count is necessary for cases when, say, a list contains items
 * pointing to the same other list.
 */

#define UW_PARENTS_CHUNK_SIZE  4

typedef struct {
    /*
     * Pointers and refcounts on the list of parents are allocated in chunks
     * because alignment for pointer and integer can be different.
     */
    struct __UwCompoundData* parents[UW_PARENTS_CHUNK_SIZE];
    unsigned parents_refcount[UW_PARENTS_CHUNK_SIZE];
} _UwParentsChunk;

// a couple of forward declarations just for less verbose pointer types to self
struct __UwCompoundData;
typedef struct __UwCompoundData _UwCompoundData;

struct __UwCompoundData {
    /*
     * Extra data for compound UwValue.
     * This structure is capable to hold two pointers without allocating a list.
     * This structure is variable size.
     */
    unsigned refcount;
    bool destroying;
    struct {
        union {
            ptrdiff_t using_parents_list: 1;    // given that pointers are aligned, we use least significant bit for a flag
            _UwCompoundData* parents[2];        // using_parents_list == 0, using pointers as is
            struct {
                _UwParentsChunk* parents_list;  // using_parents_list == 1, this points to the list of other parents
                unsigned num_parents_chunks;
            };
        };
        unsigned parents_refcount[2];
    };
};


struct _UwString {
    // Internal string structure.
    // Header:
    uint8_t cap_size:3,      // length and capacity size in bytes minus one
            char_size:2,     // character size in bytes minus one
            is_embedded:1;   // the string data is embedded into UwValue
    // Length and capacity follow the header and are aligned accordingly.
    // Characters follow capacity and are aligned accordingly, except 24-bit characters.
};

typedef struct { uint8_t v[3]; } uint24_t;  // always little-endian

// XXX UwString macros depend on uint8_t, can't be bigger without revising this library
static_assert( sizeof(UwTypeId) == sizeof(uint8_t) );

// make sure largest C type fits into 64 bits
static_assert( sizeof(long long) <= sizeof(uint64_t) );

union __UwValue {
    /*
     * 128-bit value
     */
    // UwValue
    struct {
        union {
            uint64_t first64bits;
            struct {
                UwTypeId type_id;
                union {
                    uint8_t charptr_subtype; // see UW_CHARPTR* constants
                    uint8_t status_class;    // see UWSC_* constants
                    uint8_t carry;           // for integer arithmetic
                };
                union {
                    uint16_t status_code;  // for UWSC_DEFAULT status class
                    int16_t  uw_errno;     // errno for UWSC_ERRNO status class
                };
            };
        };
        union {
            uint64_t second64bits;

            // Integral types
            UwType_Bool     bool_value;
            UwType_Signed   signed_value;
            UwType_Unsigned unsigned_value;
            UwType_Float    float_value;

            // External data
            _UwExtraData* extra_data;

            // C string pointers for UwType_CharPtr
            char*     charptr;
            char8_t*  char8ptr;
            char32_t* char32ptr;
        };
    };
    // embedded string
    struct {
        UwTypeId _type_id;
        struct _UwString str_header;
        // cap_size for embedded strings is always 1 (stored as 0)
        uint8_t str_length;
        uint8_t str_capacity;
        union {
            uint8_t  str_1[12];
            uint16_t str_2[6];
            uint24_t str_3[4];
            uint32_t str_4[3];
        };
    };
};

typedef union __UwValue _UwValue;

// make sure _UwValue has correct size
static_assert( sizeof(_UwValue) == 16 );

typedef _UwValue* UwValuePtr;
typedef _UwValue  UwResult;  // alias for return values

// automatically cleaned value
#define _UW_VALUE_CLEANUP [[ gnu::cleanup(uw_destroy) ]]
#define UwValue _UW_VALUE_CLEANUP _UwValue

// Built-in types
#define UwTypeId_Null         0
#define UwTypeId_Bool         1
#define UwTypeId_Int          2  // abstract integer
#define UwTypeId_Signed       3  // subclass of int, signed integer
#define UwTypeId_Unsigned     4  // subclass of int, unsigned integer
#define UwTypeId_Float        5
#define UwTypeId_String       6
#define UwTypeId_CharPtr      7  // container for static C strings
#define UwTypeId_List         8  // always has extra data
#define UwTypeId_Map          9  // always has extra data
#define UwTypeId_Status      10  // extra_data is optional
#define UwTypeId_Class       11  // Struct ???
#define UwTypeId_File        12
#define UwTypeId_StringIO    13

// char* sub-types
#define UW_CHARPTR    0
#define UW_CHAR8PTR   1
#define UW_CHAR32PTR  2

/****************************************************************
 * Hash functions
 */

typedef uint64_t  UwType_Hash;

struct _UwHashContext;
typedef struct _UwHashContext UwHashContext;

void _uw_hash_uint64(UwHashContext* ctx, uint64_t data);
void _uw_hash_buffer(UwHashContext* ctx, void* buffer, size_t length);
void _uw_hash_string(UwHashContext* ctx, char* str);
void _uw_hash_string32(UwHashContext* ctx, char32_t* str);

/****************************************************************
 * Interfaces
 *
 * IMPORTANT: all method names in interfaces must start with underscore.
 * That's because of limitations of ## preprocesor operator which
 * is used in uw_call, uw_super, and other convenience macros.
 *
 * Interface methods should always return UwResult.
 * Simply because if an interface may not exist,
 * the wrapper have to return UwError.
 */

#ifndef UW_INTERFACE_CAPACITY
#    define UW_INTERFACE_CAPACITY  256
#else
#    if UW_INTERFACE_CAPACITY == 0
#        undef UW_INTERFACE_CAPACITY
#        define UW_INTERFACE_CAPACITY  256
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
#define UwInterfaceId_String        5
    // TBD substring, truncate, trim, append_substring, etc
#define UwInterfaceId_List          6
    // string supports this interface
    // UwMethodAppend
    // UwMethodSlice
    // UwMethodDeleteRange

#define UwInterfaceId_File          7
#define UwInterfaceId_FileReader    8
#define UwInterfaceId_FileWriter    9

#define UwInterfaceId_LineReader   10

/****************************************************************
 * Types
 */

struct __UwCompoundChain {
    /*
     * Compound values may contain cyclic references.
     * This structure along with function `_uw_on_chain`
     * helps to detect them.
     * See dump implementation for list and map values.
     */
    struct __UwCompoundChain* prev;
    UwValuePtr value;
};

typedef struct __UwCompoundChain _UwCompoundChain;

typedef void* (*UwAlloc)  (unsigned nbytes);
typedef void* (*UwRealloc)(void* block, unsigned old_nbytes, unsigned new_nbytes);
typedef void  (*UwFree)   (void* block, unsigned nbytes);

typedef struct {
    UwAlloc   alloc;    // always cleans allocated block
    UwRealloc realloc;  // cleans extra space when block is extended
    UwFree    free;
} UwAllocator;

extern UwAllocator _uw_std_allocator;      // uses stdlib malloc/realloc/free
extern UwAllocator _uw_nofail_allocator;   // same as std, panic when malloc/realloc return nullptr
extern UwAllocator _uw_debug_allocator;    // detects memory damage
extern UwAllocator _uw_default_allocator;  // initialized with std allocator, all built-in types
                                           // point to this allocator;
                                           // can be replaced if necessary with user's allocator
extern UwAllocator _uw_pchunks_allocator;  // for parents chunks to track cyclic references; uses std_allocator by default

extern bool     _uw_allocator_verbose;     // for debug allocator
extern unsigned _uw_blocks_allocated;      // for debug allocator

// Function types for the basic interface.
// The basic interface is embedded into UwType structure.
typedef UwResult (*UwMethodCreate)  (UwTypeId type_id, va_list ap);
typedef void     (*UwMethodDestroy) (UwValuePtr self);
typedef UwResult (*UwMethodInit)    (UwValuePtr self, va_list ap);
typedef void     (*UwMethodFini)    (UwValuePtr self);
typedef UwResult (*UwMethodClone)   (UwValuePtr self);
typedef void     (*UwMethodHash)    (UwValuePtr self, UwHashContext* ctx);
typedef UwResult (*UwMethodDeepCopy)(UwValuePtr self);
typedef void     (*UwMethodDump)    (UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail);
typedef UwResult (*UwMethodToString)(UwValuePtr self);
typedef bool     (*UwMethodIsTrue)  (UwValuePtr self);
typedef bool     (*UwMethodEqual)   (UwValuePtr self, UwValuePtr other);

typedef struct {
    UwTypeId id;
    UwTypeId ancestor_id;

    bool compound;  // if true, value.extra_data may include other compound values;
                    // this flag is required to work around cyclic references

    bool data_optional;  // if true, extra_data is optional

    char* name;

    unsigned data_offset;
    unsigned data_size;
    UwAllocator* allocator;

    // basic interface
    // method names must start with underscore, see note for interfaces
    UwMethodCreate   _create;    // mandatory
    UwMethodDestroy  _destroy;   // optional
    UwMethodInit     _init;      // optional, should be called by create() when extra data is allocated
    UwMethodFini     _fini;      // optional
    UwMethodClone    _clone;     // if set, it is called by uw_clone()
    UwMethodHash     _hash;
    UwMethodDeepCopy _deepcopy;  // XXX how it should work with subclasses is not clear yet
    UwMethodDump     _dump;
    UwMethodToString _to_string;
    UwMethodIsTrue   _is_true;
    UwMethodEqual    _equal_sametype;
    UwMethodEqual    _equal;

    // extra interfaces
    void* interfaces[UW_INTERFACE_CAPACITY];

} UwType;

// type checking
#define uw_is_null(value)      uw_is_subclass((value), UwTypeId_Null)
#define uw_is_bool(value)      uw_is_subclass((value), UwTypeId_Bool)
#define uw_is_int(value)       uw_is_subclass((value), UwTypeId_Int)
#define uw_is_signed(value)    uw_is_subclass((value), UwTypeId_Signed)
#define uw_is_unsigned(value)  uw_is_subclass((value), UwTypeId_Unsigned)
#define uw_is_float(value)     uw_is_subclass((value), UwTypeId_Float)
#define uw_is_string(value)    uw_is_subclass((value), UwTypeId_String)
#define uw_is_charptr(value)   uw_is_subclass((value), UwTypeId_CharPtr)
#define uw_is_list(value)      uw_is_subclass((value), UwTypeId_List)
#define uw_is_map(value)       uw_is_subclass((value), UwTypeId_Map)
#define uw_is_status(value)    uw_is_subclass((value), UwTypeId_Status)
#define uw_is_class(value)     uw_is_subclass((value), UwTypeId_Class)
#define uw_is_file(value)      uw_is_subclass((value), UwTypeId_File)
#define uw_is_stringio(value)  uw_is_subclass((value), UwTypeId_StringIO)

#define uw_assert_null(value)      uw_assert(uw_is_null    (value))
#define uw_assert_bool(value)      uw_assert(uw_is_bool    (value))
#define uw_assert_int(value)       uw_assert(uw_is_int     (value))
#define uw_assert_signed(value)    uw_assert(uw_is_signed  (value))
#define uw_assert_unsigned(value)  uw_assert(uw_is_unsigned(value))
#define uw_assert_float(value)     uw_assert(uw_is_float   (value))
#define uw_assert_string(value)    uw_assert(uw_is_string  (value))
#define uw_assert_charptr(value)   uw_assert(uw_is_charptr (value))
#define uw_assert_list(value)      uw_assert(uw_is_list    (value))
#define uw_assert_map(value)       uw_assert(uw_is_map     (value))
#define uw_assert_status(value)    uw_assert(uw_is_status  (value))
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

static inline char* _uw_get_type_name_by_id     (uint8_t type_id)  { return _uw_types[type_id]->name; }
static inline char* _uw_get_type_name_from_value(UwValuePtr value) { return _uw_types[value->type_id]->name; }

/****************************************************************
 * Statuses
 */

// Classes of status codes
#define UWSC_DEFAULT  0  // value contains status_code, and optionally _UwStatusExtraData
#define UWSC_ERRNO    1  // value contains errno only

// Status codes for UWSC_DEFAULT class
#define UW_SUCCESS                 0
#define UW_STATUS_VA_END           1  // used as a terminator for variadic arguments
#define UW_ERROR_OOM               2
#define UW_ERROR_NOT_IMPLEMENTED   3
#define UW_ERROR_INCOMPATIBLE_TYPE 4
#define UW_ERROR_NO_INTERFACE      5
#define UW_ERROR_EOF               6
#define UW_ERROR_GONE              7

static inline bool uw_ok(UwValuePtr status)
{
    if (!status) {
        return false;
    }
    if (!uw_is_status(status)) {
        // any other type means okay
        return true;
    }
    switch (status->status_class) {
        case UWSC_DEFAULT: return status->status_code == UW_SUCCESS;
        case UWSC_ERRNO:   return status->uw_errno == 0;
        default: return false;
    }
}

static inline bool uw_error(UwValuePtr status)
{
    return !uw_ok(status);
}

static inline bool uw_eof(UwValuePtr status)
{
    if (!status) {
        return false;
    }
    if (!uw_is_status(status)) {
        return false;
    }
    return status->status_class == UWSC_DEFAULT && status->status_code == UW_ERROR_EOF;
}

char* uw_status_desc(UwValuePtr status);
/*
 * Get status description.
 * If `status` has no description return a pointer to static string.
 * If `status` is nullptr, return a pointer to static "null" string.
 * If `status` is not a status, return a pointer to static "bad status" string.
 *
 * XXX the result should be used within the lifetime of `status` value
 * XXX need to replace this function with a safer one, maybe CString-based
 */

void _uw_set_status_desc(UwValuePtr status, char* fmt, ...);
void _uw_set_status_desc_ap(UwValuePtr status, char* fmt, va_list ap);
/*
 * Set description for status.
 * If out of memory assign UW_ERROR_OOM to status.
 */

/****************************************************************
 * Constructors
 */

/*
 * In-place declarations and rvalues
 *
 * __UWDECL_* macros define values that are not automatically cleaned. Use carefully.
 *
 * UWDECL_* macros define automatically cleaned values. Okay to use for local variables.
 *
 * Uw<Typename> macros define rvalues that should be destroyed either explicitly
 * or automatically, by assigning them to an automatically cleaned variable
 * or passing to UwList(), UwMap() or other uw_*_va function that takes care of its arguments.
 */

#define __UWDECL_Null(name)  \
    /* declare Null variable */  \
    _UwValue name = {  \
        .type_id = UwTypeId_Null  \
    }

#define UWDECL_Null(name)  _UW_VALUE_CLEANUP __UWDECL_Null(name)

#define UwNull()  \
    /* make Null rvalue */  \
    ({  \
        __UWDECL_Null(v);  \
        v;  \
    })

#define __UWDECL_Bool(name, initializer)  \
    /* declare Bool variable */  \
    _UwValue name = {  \
        .type_id = UwTypeId_Bool,  \
        .bool_value = (initializer)  \
    }

#define UWDECL_Bool(name, initializer)  _UW_VALUE_CLEANUP __UWDECL_Bool((name), (initializer))

#define UwBool(initializer)  \
    /* make Bool rvalue */  \
    ({  \
        __UWDECL_Bool(v, (initializer));  \
        v;  \
    })

#define __UWDECL_Signed(name, initializer)  \
    /* declare Signed variable */  \
    _UwValue name = {  \
        .type_id = UwTypeId_Signed,  \
        .signed_value = (initializer),  \
    }

#define UWDECL_Signed(name, initializer)  _UW_VALUE_CLEANUP __UWDECL_Signed((name), (initializer))

#define UwSigned(initializer)  \
    /* make Signed rvalue */  \
    ({  \
        __UWDECL_Signed(v, (initializer));  \
        v;  \
    })

#define __UWDECL_Unsigned(name, initializer)  \
    /* declare Unsigned variable */  \
    _UwValue name = {  \
        .type_id = UwTypeId_Unsigned,  \
        .unsigned_value = (initializer)  \
    }

#define UWDECL_Unsigned(name, initializer)  _UW_VALUE_CLEANUP __UWDECL_Unsigned((name), (initializer))

#define UwUnsigned(initializer)  \
    /* make Unsigned rvalue */  \
    ({  \
        __UWDECL_Unsigned(v, (initializer));  \
        v;  \
    })

#define __UWDECL_Float(name, initializer)  \
    /* declare Float variable */  \
    _UwValue name = {  \
        .type_id = UwTypeId_Float,  \
        .float_value = (initializer)  \
    }

#define UWDECL_Float(name, initializer)  _UW_VALUE_CLEANUP __UWDECL_Float((name), (initializer))

#define UwFloat(initializer)  \
    /* make Float rvalue */  \
    ({  \
        __UWDECL_Float(v, (initializer));  \
        v;  \
    })

// The very basic string declaration and rvalue, see uw_string.h for more macros:

#define __UWDECL_String(name)  \
    /* declare empty String variable */  \
    _UwValue name = {  \
        ._type_id = UwTypeId_String,  \
        { .is_embedded = 1 },  \
        .str_capacity = _UWC_LENGTH_OF(name.str_1)  \
    }

#define UWDECL_String(name)  _UW_VALUE_CLEANUP __UWDECL_String(name)

#define UwString()  \
    /* make empty String rvalue */  \
    ({  \
        __UWDECL_String(v);  \
        v;  \
    })

#define __UWDECL_CharPtr(name, initializer)  \
    /* declare CharPtr variable */  \
    _UwValue name = {  \
        .type_id = UwTypeId_CharPtr,  \
        .charptr_subtype = UW_CHARPTR,  \
        .charptr = (initializer)  \
    }

#define UWDECL_CharPtr(name, initializer)  _UW_VALUE_CLEANUP __UWDECL_CharPtr((name), (initializer))

#define UwCharPtr(initializer)  \
    /* make CharPtr rvalue */  \
    ({  \
        __UWDECL_CharPtr(v, (initializer));  \
        v;  \
    })

#define __UWDECL_Char8Ptr(name, initializer)  \
    /* declare Char8Ptr variable */  \
    _UwValue name = {  \
        .type_id = UwTypeId_CharPtr,  \
        .charptr_subtype = UW_CHAR8PTR,  \
        .char8ptr = (char8_t*) (initializer)  \
    }

#define UWDECL_Char8Ptr(name, initializer)  _UW_VALUE_CLEANUP __UWDECL_Char8Ptr((name), (initializer))

#define UwChar8Ptr(initializer)  \
    /* make Char8Ptr rvalue */  \
    ({  \
        __UWDECL_Char8Ptr(v, (initializer));  \
        v;  \
    })

#define __UWDECL_Char32Ptr(name, initializer)  \
    /* declare Char32Ptr variable */  \
    _UwValue name = {  \
        .type_id = UwTypeId_CharPtr,  \
        .charptr_subtype = UW_CHAR32PTR,  \
        .char32ptr = (initializer)  \
    }

#define UWDECL_Char32Ptr(name, initializer)  _UW_VALUE_CLEANUP __UWDECL_Char32Ptr((name), (initializer))

#define UwChar32Ptr(initializer)  \
    /* make Char32Ptr rvalue */  \
    ({  \
        __UWDECL_Char32Ptr(v, (initializer));  \
        v;  \
    })

// Status declarations and rvalues

#define __UWDECL_Status(name, _status_class, _status_code)  \
    /* declare Status variable */  \
    _UwValue name = {  \
        .type_id = UwTypeId_Status,  \
        .status_class = _status_class,  \
        .status_code = _status_code  \
    }

#define UWDECL_Status(name, _status_class, _status_code)  \
    _UW_VALUE_CLEANUP __UWDECL_Status((name), (_status_class), (_status_code))

#define UwStatus(_status_class, _status_code)  \
    /* make Status rvalue */  \
    ({  \
        __UWDECL_Status(status, (_status_class), (_status_code));  \
        status;  \
    })

#define UwOK()  \
    /* make success rvalue */  \
    ({  \
        __UWDECL_Status(status, UWSC_DEFAULT, UW_SUCCESS);  \
        status;  \
    })

#define UwError(code)  \
    /* make Status rvalue of UWSC_DEFAULT class */  \
    ({  \
        __UWDECL_Status(status, UWSC_DEFAULT, (code));  \
        status;  \
    })

#define UwOOM()  \
    /* make UW_ERROR_OOM rvalue */  \
    ({  \
        __UWDECL_Status(status, UWSC_DEFAULT, UW_ERROR_OOM);  \
        status;  \
    })

#define UwVaEnd()  \
    /* make VA_END rvalue */  \
    ({  \
        __UWDECL_Status(status, UWSC_DEFAULT, UW_STATUS_VA_END);  \
        status;  \
    })

#define UwErrorNoInterface(value, interface_name)  \
    ({  \
        __UWDECL_Status(status, UWSC_DEFAULT, UW_ERROR_NO_INTERFACE);  \
        _uw_set_status_desc(&status,  \
                            "Value of type %s provides no UwInterface_%s",  \
                            _uw_types[(value)->type_id]->name,  \
                            #interface_name  \
        );  \
        status;  \
    })

#define __UWDECL_Errno(name, _errno)  \
    /* declare Status variable of UWSC_ERRNO class */  \
    _UwValue name = {  \
        .type_id = UwTypeId_Status,  \
        .status_class = UWSC_ERRNO,  \
        .uw_errno = _errno  \
    }

#define UWDECL_Errno(name, _errno)  _UW_VALUE_CLEANUP __UWDECL_Errno((name), (_errno))

#define UwErrno(_errno)  \
    /* make Status rvalue of UWSC_ERRNO class */  \
    ({  \
        __UWDECL_Errno(status, _errno);  \
        status;  \
    })

// List and Map can't be declared in-place, defining rvalues only

#define UwList(...)  _uw_create(UwTypeId_List __VA_OPT__(,) __VA_ARGS__, UwVaEnd())
#define UwMap(...)   _uw_create(UwTypeId_Map  __VA_OPT__(,) __VA_ARGS__, UwVaEnd())


UwResult _uw_create(UwTypeId type_id, ...);
UwResult _uw_create_ap(UwTypeId type_id, va_list ap);
/*
 * Basic constructor.
 *
 * Integral types are usually reated in-place with shorthand Uw* macros.
 * Nevertheless, they have a constructor which does not expect any arguments
 * and returns default value.
 *
 * String ans StringIO constructors expects UwValuePtr, one of the following types:
 *   - String: returns cloned string
 *   - CharPtr: returns an UW string created from char ptr.
 *
 * CharPtr constructor expects two arguments:
 *   - UW_CHARPTR* constant, unsigned
 *   - pointer to C string
 *
 * Status constructor expects at least three arguments:
 *   - status class: unsigned
 *   - status_code: unsigned
 *   - description format string: can be nullptr
 *   - arguments for the format string
 *
 * List and Map types expect items and key-value pairs respectively
 * as UwValues (not pointers), terminated by UwVaEnd().
 *
 * What custom type constructors expect is up to the user.
 */

#define uw_create(initializer) _Generic((initializer), \
             nullptr_t: _uwc_create_null,       \
                  bool: _uwc_create_bool,       \
                  char: _uwc_create_int,        \
         unsigned char: _uwc_create_unsigned,   \
                 short: _uwc_create_int,        \
        unsigned short: _uwc_create_unsigned,   \
                   int: _uwc_create_int,        \
          unsigned int: _uwc_create_unsigned,   \
                  long: _uwc_create_int,        \
         unsigned long: _uwc_create_unsigned,   \
             long long: _uwc_create_int,        \
    unsigned long long: _uwc_create_unsigned,   \
                 float: _uwc_create_float,      \
                double: _uwc_create_float,      \
                 char*: _uwc_create_string_u8_wrapper, \
              char8_t*: _uw_create_string_u8,   \
             char32_t*: _uw_create_string_u32,  \
            UwValuePtr:  uw_clone               \
    )(initializer)
/*
 * Type-generic constructor.
 *
 * Notes:
 *
 * uw_create('a') creates integer.
 * For strings use uw_create("a") or specific constructors.
 *
 * Although generics is a great thing,
 * they make preprocessor output looks extremely ugly.
 *
 * String literals are weird:
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

static inline UwResult _uwc_create_null    (UwType_Null     initializer) { return UwNull    ();            }
static inline UwResult _uwc_create_bool    (UwType_Bool     initializer) { return UwBool    (initializer); }
static inline UwResult _uwc_create_int     (UwType_Signed   initializer) { return UwSigned  (initializer); }
static inline UwResult _uwc_create_unsigned(UwType_Unsigned initializer) { return UwUnsigned(initializer); }
static inline UwResult _uwc_create_float   (UwType_Float    initializer) { return UwFloat   (initializer); }

// a couple of prototypes from uw_string.h:
UwResult _uw_create_string_u8 (char8_t*  initializer);
UwResult _uw_create_string_u32(char32_t* initializer);

static inline UwResult _uwc_create_string_u8_wrapper(char* initializer)
{
    return _uw_create_string_u8((char8_t*) initializer);
}

/****************************************************************
 * Method invocation macros
 */

#define uw_void_call(v, method_name, ...)  \
    /* call basic interface method that returns void */  \
    {  \
        typeof(_uw_types[(v)->type_id]->_##method_name) meth = _uw_types[(v)->type_id]->_##method_name;  \
        if (meth) {  \
            meth((v) __VA_OPT__(,) __VA_ARGS__);  \
        }  \
    }

#define uw_call(default_result, v, method_name, ...)  \
    /* call basic interface method; if method is nullptr, return default_result */  \
    ({  \
        typeof(_uw_types[(v)->type_id]->_##method_name) meth = _uw_types[(v)->type_id]->_##method_name;  \
        (meth)?  \
            meth((v) __VA_OPT__(,) __VA_ARGS__)  \
        :  \
            (default_result);  \
    })

#define uw_void_super(self, method_name, ...)  \
    /* call super method of the basic interface that returns void */  \
    {  \
        UwTypeId ancestor_id = _uw_types[(self)->type_id]->ancestor_id;  \
        typeof(_uw_types[ancestor_id]->_##method_name) super_meth = _uw_types[ancestor_id]->_##method_name;  \
        if (super_meth) {  \
            super_meth((self) __VA_OPT__(,) __VA_ARGS__);  \
        }  \
    }

#define uw_super(default_result, self, method_name, ...)  \
    /* call super method of the basic interface; if method is nullptr, return default_result */  \
    ({  \
        UwTypeId ancestor_id = _uw_types[(self)->type_id]->ancestor_id;  \
        typeof(_uw_types[ancestor_id]->_##method_name) super_meth = _uw_types[ancestor_id]->_##method_name;  \
        (super_meth)?  \
            super_meth((self) __VA_OPT__(,) __VA_ARGS__)  \
        :  \
            (default_result)  \
    })

#define uw_ifcall(v, interface_name, method_name, ...)  \
    /* call interface method */  \
    ({  \
        UwInterface_##interface_name* iface =  \
            _uw_types[(v)->type_id]->interfaces[UwInterfaceId_##interface_name];  \
        iface?  \
            iface->_##method_name((v) __VA_OPT__(,) __VA_ARGS__)  \
        :  \
            UwErrorNoInterface((v), interface_name);  \
    })

#define uw_ifsuper(self, iface, method_name, ...)  \
    /* call super method of interface */  \
    ({  \
        UwTypeId ancestor_id = _uw_types[(self)->type_id]->ancestor_id;  \
        UwInterface_##interface_name* iface =  \
            _uw_types[ancestor_id->type_id]->interfaces[UwInterfaceId_##interface_name];  \
        iface?  \
            iface->_##method_name((self) __VA_OPT__(,) __VA_ARGS__)  \
        :  \
            UwErrorNoInterface((self), interface_name);  \
    })

/****************************************************************
 * Basic methods
 *
 * Most of them are inline wrappers around method invocation
 * macros.
 *
 * Using inline functions to avoid duplicate evaluation of args
 * when, say, uw_destroy(vptr++) is called.
 */

static inline void uw_destroy(UwValuePtr v)
/*
 * Destroy value: call destructor and make `v` Null.
 */
{
    if (v->type_id != UwTypeId_Null) {
        uw_void_call(v, destroy);
        v->type_id = UwTypeId_Null;
    }
}

static inline UwResult uw_clone(UwValuePtr v)
/*
 * Clone value.
 */
{
    return uw_call(*v, v, clone);
}

static inline UwResult uw_move(UwValuePtr v)
/*
 * "Move" value to another variable or out of the function
 * (i.e. return a value and reset autocleaned variable)
 */
{
    _UwValue tmp = *v;
    v->type_id = UwTypeId_Null;
    return tmp;
}

UwType_Hash uw_hash(UwValuePtr value);
/*
 * Calculate hash of value.
 */

static inline UwResult uw_deepcopy(UwValuePtr v)
{
    return uw_call(*v, v, deepcopy);
}

static inline bool uw_is_true(UwValuePtr v)
{
    return uw_call(false, v, is_true);
}

static inline bool uw_is_compound(UwValuePtr value)
{
    return _uw_types[value->type_id]->compound;
}

static inline UwResult uw_to_string(UwValuePtr v)
{
    return uw_call(UwString(), v, to_string);
}

/****************************************************************
 * Compare for equality.
 */

static inline bool _uw_equal(UwValuePtr a, UwValuePtr b)
{
    if (a == b) {
        // compare with self
        return true;
    }
    if (a->first64bits == b->first64bits && a->second64bits == b->second64bits) {
        // quick comparison
        return true;
    }
    UwType* t = _uw_types[a->type_id];
    UwMethodEqual cmp;
    if (a->type_id == b->type_id) {
        cmp = t->_equal_sametype;
    } else {
        cmp = t->_equal;
    }
    return cmp(a, b);
}

#define uw_equal(a, b) _Generic((b),           \
             nullptr_t: _uwc_equal_null,       \
                  bool: _uwc_equal_bool,       \
                  char: _uwc_equal_char,       \
         unsigned char: _uwc_equal_uchar,      \
                 short: _uwc_equal_short,      \
        unsigned short: _uwc_equal_ushort,     \
                   int: _uwc_equal_int,        \
          unsigned int: _uwc_equal_uint,       \
                  long: _uwc_equal_long,       \
         unsigned long: _uwc_equal_ulong,      \
             long long: _uwc_equal_longlong,   \
    unsigned long long: _uwc_equal_ulonglong,  \
                 float: _uwc_equal_float,      \
                double: _uwc_equal_double,     \
                 char*: _uwc_equal_u8_wrapper, \
              char8_t*: _uwc_equal_u8,         \
             char32_t*: _uwc_equal_u32,        \
            UwValuePtr: _uw_equal              \
    )((a), (b))
/*
 * Type-generic compare for equality.
 */

static inline bool _uwc_equal_null      (UwValuePtr a, nullptr_t          b) { return uw_is_null(a); }
static inline bool _uwc_equal_bool      (UwValuePtr a, bool               b) { __UWDECL_Bool     (v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_char      (UwValuePtr a, char               b) { __UWDECL_Signed   (v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_uchar     (UwValuePtr a, unsigned char      b) { __UWDECL_Unsigned (v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_short     (UwValuePtr a, short              b) { __UWDECL_Signed   (v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_ushort    (UwValuePtr a, unsigned short     b) { __UWDECL_Unsigned (v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_int       (UwValuePtr a, int                b) { __UWDECL_Signed   (v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_uint      (UwValuePtr a, unsigned int       b) { __UWDECL_Unsigned (v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_long      (UwValuePtr a, long               b) { __UWDECL_Signed   (v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_ulong     (UwValuePtr a, unsigned long      b) { __UWDECL_Unsigned (v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_longlong  (UwValuePtr a, long long          b) { __UWDECL_Signed   (v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_ulonglong (UwValuePtr a, unsigned long long b) { __UWDECL_Unsigned (v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_float     (UwValuePtr a, float              b) { __UWDECL_Float    (v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_double    (UwValuePtr a, double             b) { __UWDECL_Float    (v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_u8        (UwValuePtr a, char8_t*           b) { __UWDECL_Char8Ptr (v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_u32       (UwValuePtr a, char32_t*          b) { __UWDECL_Char32Ptr(v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_u8_wrapper(UwValuePtr a, char*              b) { return _uwc_equal_u8(a, (char8_t*) b); }

/****************************************************************
 * C strings
 */

typedef char* CStringPtr;

// automatically cleaned value
#define CString [[ gnu::cleanup(uw_destroy_cstring) ]] CStringPtr

// somewhat ugly macro to define a local variable initialized with a copy of uw string:
#define UW_CSTRING_LOCAL(variable_name, uw_str) \
    char variable_name[uw_strlen_in_utf8(uw_str) + 1]; \
    uw_string_copy_buf((uw_str), variable_name)

void uw_destroy_cstring(CStringPtr* str);

/****************************************************************
 * Helper functions and macros
 */

// XXX this macro is not suitable for subclasses because the base class data_offset is zero;
//     need a better way to define structures
#define _uw_get_data_ptr(value, type_id, type_name_ptr)  \
    (  \
        (type_name_ptr) (  \
            ((uint8_t*) ((value)->extra_data)) + _uw_types[type_id]->data_offset \
        )  \
    )

static inline unsigned uw_align(unsigned n, unsigned boundary)
/*
 * boundary must be power of two
 */
{
    boundary--;
    return (n + boundary) & ~boundary;
}

static inline uint8_t* uw_align_ptr(void* p, unsigned boundary)
/*
 * boundary must be power of two
 */
{
    boundary--;
    return (uint8_t*) (
        (((ptrdiff_t) p) + boundary) & ~(ptrdiff_t) boundary
    );
}

static inline void _uw_call_fini(UwValuePtr value)
{
    uw_void_call(value, fini);
}

static inline void _uw_call_hash(UwValuePtr value, UwHashContext* ctx)
{
    UwMethodHash fn_hash = _uw_types[value->type_id]->_hash;
    fn_hash(value, ctx);
}

bool _uw_alloc_extra_data(UwValuePtr v);
bool _uw_mandatory_alloc_extra_data(UwValuePtr v);
/*
 * Helper functions for custom constructors.
 * Alocate extra data for value `v`.
 * Set v->extra_data->refcount = 1
 */

void _uw_free_extra_data(UwValuePtr v);
/*
 * Helper function for custom destructors.
 * Free extra data and make v->extra_data = nullptr.
 */

#define _uw_init_compound_data(data)
//void _uw_init_compound_data(_UwCompoundData* cdata);
/*
 * Initialize list of parents.
 * XXX nothing to init, the data already contains zeroes and that's what we need.
 */

void _uw_fini_compound_data(_UwCompoundData* cdata);
/*
 * Deallocate list of parents.
 */

bool _uw_adopt(_UwCompoundData* parent, _UwCompoundData* child);
/*
 * Add parent to child's parents or increment
 * parents_refcount if added already.
 *
 * Decrement child refcount.
 *
 * Return false if OOM.
 */

bool _uw_abandon(_UwCompoundData* parent, _UwCompoundData* child);
/*
 * Decrement parents_refcount in child's list of parents and when it reaches zero
 * remove parent from child's parents and return true.
 *
 * If child still refers to parent, return false.
 */

bool _uw_is_embraced(_UwCompoundData* cdata);
/*
 * Return true if data is embraced by some parent.
 */

bool _uw_need_break_cyclic_refs(_UwCompoundData* cdata);
/*
 * Check if all parents have zero refcount and there are cyclic references.
 */

static inline bool _uw_embrace(UwValuePtr parent, UwValuePtr child)
{
    if (_uw_types[child->type_id]->compound) {
        return _uw_adopt((_UwCompoundData*) parent->extra_data, (_UwCompoundData*) child->extra_data);
    } else {
        return true;
    }
}

UwValuePtr _uw_on_chain(UwValuePtr value, _UwCompoundChain* tail);
/*
 * Check if value extra_data is on the chain.
 */

bool uw_charptr_to_string(UwValuePtr v);
/*
 * If `v` is CharPtr, convert it to String in place.
 * Return false if OOM.
 */

/****************************************************************
 * Dump functions
 */

void _uw_print_indent(FILE* fp, int indent);
void _uw_dump_start(FILE* fp, UwValuePtr value, int indent);
void _uw_dump_base_extra_data(FILE* fp, _UwExtraData* extra_data);
void _uw_dump_compound_data(FILE* fp, _UwCompoundData* cdata, int indent);
void _uw_dump(FILE* fp, UwValuePtr value, int first_indent, int next_indent, _UwCompoundChain* tail);

void uw_dump(FILE* fp, UwValuePtr value);

static inline void _uw_call_dump(FILE* fp, UwValuePtr value, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    UwMethodDump fn_dump = _uw_types[value->type_id]->_dump;
    fn_dump(value, fp, first_indent, next_indent, tail);
}

#ifdef __cplusplus
}
#endif
