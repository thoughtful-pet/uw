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
#include <string.h>
#include <uchar.h>

#include <libpussy/allocator.h>

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
typedef uint16_t UwTypeId;

// Integral types
typedef nullptr_t  UwType_Null;
typedef bool       UwType_Bool;
typedef int64_t    UwType_Signed;
typedef uint64_t   UwType_Unsigned;
typedef double     UwType_Float;

#define UW_SIGNED_MAX  0x7fff'ffff'ffff'ffffL

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


typedef struct { uint8_t v[3]; } uint24_t;  // always little-endian

// make sure largest C type fits into 64 bits
static_assert( sizeof(long long) <= sizeof(uint64_t) );

union __UwValue {
    /*
     * 128-bit value
     */

    UwTypeId /* uint16_t */ type_id;

    struct {
        // integral types
        UwTypeId /* uint16_t */ _integral_type_id;
        uint8_t carry;  // for integer arithmetic
        uint8_t  _integral_pagging_1;
        uint32_t _integral_pagging_2;
        union {
            // Integral types
            UwType_Bool     bool_value;
            UwType_Signed   signed_value;
            UwType_Unsigned unsigned_value;
            UwType_Float    float_value;
        };
    };

    struct {
        // charptr and ptr
        UwTypeId /* uint16_t */ _charptr_type_id;
        uint8_t charptr_subtype; // see UW_CHARPTR* constants
        uint8_t  _charptr_padding_1;
        uint32_t _charptr_pagging_2;
        union {
            // C string pointers for UwType_CharPtr
            char*     charptr;
            char8_t*  char8ptr;
            char32_t* char32ptr;

            // void*
            void* ptr;
        };
    };

    struct {
        // values with allocated extra data
        UwTypeId /* uint16_t */ _extradata_type_id;
        uint16_t _extradata_pagging_1;
        uint32_t _extradata_pagging_2;
        _UwExtraData* extra_data;
    };

    struct {
        // status
        UwTypeId /* uint16_t */ _status_type_id;
        int16_t uw_errno;
        uint32_t status_code;
        _UwExtraData* status_data;
    };

    struct {
        // embedded string
        UwTypeId /* uint16_t */ _emb_string_type_id;
        uint8_t str_embedded:1,       // the string data is embedded into UwValue
                str_embedded_char_size:2;
        uint8_t str_embedded_length;  // length of embedded string
        union {
            uint8_t  str_1[12];
            uint16_t str_2[6];
            uint24_t str_3[4];
            uint32_t str_4[3];
        };
    };

    struct {
        // allocated string
        UwTypeId /* uint16_t */ _string_type_id;
        uint8_t _x_str_embedded:1;  // zero for allocated string
        uint8_t _str_padding[5];
        void* string_data;
    };
};

typedef union __UwValue _UwValue;

// make sure _UwValue structure is correct
static_assert( offsetof(_UwValue, charptr_subtype) == 2 );
static_assert( offsetof(_UwValue, status_code)     == 4 );

static_assert( offsetof(_UwValue, bool_value) == 8 );
static_assert( offsetof(_UwValue, charptr)    == 8 );
static_assert( offsetof(_UwValue, extra_data) == 8 );
static_assert( offsetof(_UwValue, status_data) == 8 );
static_assert( offsetof(_UwValue, string_data) == 8 );

static_assert( offsetof(_UwValue, str_embedded_length) == 3 );
static_assert( offsetof(_UwValue, str_1) == 4 );
static_assert( offsetof(_UwValue, str_2) == 4 );
static_assert( offsetof(_UwValue, str_3) == 4 );
static_assert( offsetof(_UwValue, str_4) == 4 );

static_assert( sizeof(_UwValue) == 16 );

typedef _UwValue* UwValuePtr;
typedef _UwValue  UwResult;  // alias for return values

// automatically cleaned value
#define _UW_VALUE_CLEANUP [[ gnu::cleanup(uw_destroy) ]]
#define UwValue _UW_VALUE_CLEANUP _UwValue

// Built-in types
#define UwTypeId_Null        0
#define UwTypeId_Bool        1U
#define UwTypeId_Int         2U  // abstract integer
#define UwTypeId_Signed      3U  // subtype of int, signed integer
#define UwTypeId_Unsigned    4U  // subtype of int, unsigned integer
#define UwTypeId_Float       5U
#define UwTypeId_String      6U
#define UwTypeId_CharPtr     7U  // container for static C strings
#define UwTypeId_List        8U  // always has extra data
#define UwTypeId_Map         9U  // always has extra data
#define UwTypeId_Status     10U  // extra_data is optional
#define UwTypeId_Struct     11U
#define UwTypeId_Ptr        12U  // void*

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
 * Types and interfaces
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
    unsigned interface_id;
    void* interface_methods;
} _UwInterface;

typedef struct {
    UwTypeId id;
    UwTypeId ancestor_id;
    char* name;
    Allocator* allocator;
    unsigned data_offset;  // offset of type-specific data
    unsigned data_size;
    bool compound;  // if true, value.extra_data may include other compound values;
                    // this flag is required to work around cyclic references

    // basic interface
    // method names must start with underscore, see note for interfaces
    UwMethodCreate   _create;    // mandatory
    UwMethodDestroy  _destroy;   // optional
    UwMethodInit     _init;      // optional, should be called by create() when extra data is allocated
    UwMethodFini     _fini;      // optional
    UwMethodClone    _clone;     // if set, it is called by uw_clone()
    UwMethodHash     _hash;
    UwMethodDeepCopy _deepcopy;  // XXX how it should work with subtypes is not clear yet
    UwMethodDump     _dump;
    UwMethodToString _to_string;
    UwMethodIsTrue   _is_true;
    UwMethodEqual    _equal_sametype;
    UwMethodEqual    _equal;

    // other interfaces
    unsigned num_interfaces;
    _UwInterface* interfaces;  // a subtype must define all interfaces of base type,
                               // i.e. copy ancestor's interfaces if it does not define anything new
} UwType;

// Built-in interfaces
/*
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
*/

// Miscellaneous interfaces
extern unsigned UwInterfaceId_File;
extern unsigned UwInterfaceId_FileReader;
extern unsigned UwInterfaceId_FileWriter;
extern unsigned UwInterfaceId_LineReader;

unsigned uw_register_interface();
/*
 * Add interface to the first available position in the global list.
 * Return interface id or 0 if the list is full.
 *
 * XXX probably need a parameter, something like interface declaration -- TBD
 *
 * 0 is okay as an error indicator because the table already has basic interfaces.
 */

static inline void* _uw_get_interface(UwType* type, unsigned interface_id)
{
    // XXX okay for now, revise later
    _UwInterface* iface = type->interfaces;
    for (unsigned i = 0, n = type->num_interfaces; i < n; i++) {
        if (iface->interface_id == interface_id) {
            return iface->interface_methods;
        }
        iface++;
    }
    return nullptr;
}

/*
 * The following macros leverage naming scheme where
 * interface structure is named UwInterface_<interface_name>
 * and id is named UwInterfaceId_<interface_name>
 *
 * IMPORTANT: all method names in interfaces must start with underscore.
 * That's because of limitations of ## preprocesor operator which
 * is used in uw_call and other convenience macros.
 *
 * Interface methods should always return UwResult.
 * Simply because if an interface may not exist,
 * the wrapper have to return UwError.
 */
#define uw_get_interface(value, interface_name)  \
    (  \
        (UwInterface_##interface_name *) \
            _uw_get_interface(_uw_types[(value)->type_id], UwInterfaceId_##interface_name)  \
    )

// type checking
#define uw_is_null(value)      uw_is_subtype((value), UwTypeId_Null)
#define uw_is_bool(value)      uw_is_subtype((value), UwTypeId_Bool)
#define uw_is_int(value)       uw_is_subtype((value), UwTypeId_Int)
#define uw_is_signed(value)    uw_is_subtype((value), UwTypeId_Signed)
#define uw_is_unsigned(value)  uw_is_subtype((value), UwTypeId_Unsigned)
#define uw_is_float(value)     uw_is_subtype((value), UwTypeId_Float)
#define uw_is_string(value)    uw_is_subtype((value), UwTypeId_String)
#define uw_is_charptr(value)   uw_is_subtype((value), UwTypeId_CharPtr)
#define uw_is_list(value)      uw_is_subtype((value), UwTypeId_List)
#define uw_is_map(value)       uw_is_subtype((value), UwTypeId_Map)
#define uw_is_status(value)    uw_is_subtype((value), UwTypeId_Status)
#define uw_is_struct(value)    uw_is_subtype((value), UwTypeId_Struct)
#define uw_is_file(value)      uw_is_subtype((value), UwTypeId_File)
#define uw_is_stringio(value)  uw_is_subtype((value), UwTypeId_StringIO)
#define uw_is_ptr(value)       uw_is_subtype((value), UwTypeId_Ptr)

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
#define uw_assert_struct(value)    uw_assert(uw_is_struct  (value))
#define uw_assert_file(value)      uw_assert(uw_is_file    (value))
#define uw_assert_stringio(value)  uw_assert(uw_is_stringio(value))
#define uw_assert_ptr(value)       uw_assert(uw_is_ptr     (value))

extern UwType** _uw_types;
/*
 * Global list of types initialized with built-in types.
 */

UwTypeId uw_add_type(UwType* type);
/*
 * Add type to the first available position in the global list.
 *
 * All fields of `type` must be initialized.
 *
 * Return new type id or 0 (UwTypeId_Null) if the list is full.
 *
 * Null type cannot be an ancestor for new type,
 * so it's okay to use UwTypeId_Null as error indicator.
 */

UwTypeId uw_subtype(UwType* type, char* name, UwTypeId ancestor_id, unsigned data_size);
/*
 * `type` and `name` should point to a static storage.
 *
 * Initialize `type` with ancestor's type, calculate data_offset, set data_size
 * and other essential fields, and then add `type` to the global list using `uw_add_type`.
 *
 * The caller should alter basic methods and set supported interfaces after
 * calling this function.
 *
 * Null type cannot be an ancestor for new type.
 *
 * Return new type id or 0 (UwTypeId_Null) if the list is full.
 */

static inline bool uw_is_subtype(UwValuePtr value, UwTypeId type_id)
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

#define UW_SUCCESS                    0
#define UW_STATUS_VA_END              1  // used as a terminator for variadic arguments
#define UW_ERROR_ERRNO                2
#define UW_ERROR_OOM                  3
#define UW_ERROR_NOT_IMPLEMENTED      4
#define UW_ERROR_INCOMPATIBLE_TYPE    5
#define UW_ERROR_NO_INTERFACE         6
#define UW_ERROR_EOF                  7
#define UW_ERROR_INDEX_OUT_OF_RANGE   8

// list errors
#define UW_ERROR_POP_FROM_EMPTY_LIST  9

// map errors
#define UW_ERROR_KEY_NOT_FOUND        10

// File errors
#define UW_ERROR_FILE_ALREADY_OPENED  11
#define UW_ERROR_CANNOT_SET_FILENAME  12
#define UW_ERROR_FD_ALREADY_SET       13

// StringIO errors
#define UW_ERROR_PUSHBACK_FAILED      14

uint16_t uw_define_status(char* status);
/*
 * Define status in the global table.
 * Return status code or UW_ERROR_OOM
 *
 * This function should be called from the very beginning of main() function
 * or from constructors that are called before main().
 */

char* uw_status_str(uint16_t status_code);
/*
 * Get status string by status code.
 */

static inline bool uw_ok(UwValuePtr status)
{
    if (!status) {
        return false;
    }
    if (!uw_is_status(status)) {
        // any other type means okay
        return true;
    }
    return status->status_code == UW_SUCCESS;
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
    return status->status_code == UW_ERROR_EOF;
}

static inline bool uw_va_end(UwValuePtr status)
{
    if (!status) {
        return false;
    }
    if (!uw_is_status(status)) {
        return false;
    }
    return status->status_code == UW_STATUS_VA_END;
}

UwResult uw_status_desc(UwValuePtr status);
/*
 * Get status description.
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
        ._integral_type_id = UwTypeId_Bool,  \
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
        ._integral_type_id = UwTypeId_Signed,  \
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
        ._integral_type_id = UwTypeId_Unsigned,  \
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
        ._integral_type_id = UwTypeId_Float,  \
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
        ._emb_string_type_id = UwTypeId_String,  \
        .str_embedded = 1  \
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
        ._charptr_type_id = UwTypeId_CharPtr,  \
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
        ._charptr_type_id = UwTypeId_CharPtr,  \
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
        ._charptr_type_id = UwTypeId_CharPtr,  \
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

#define __UWDECL_Ptr(name, initializer)  \
    /* declare Ptr variable */  \
    _UwValue name = {  \
        ._charptr_type_id = UwTypeId_Ptr,  \
        .ptr = (initializer)  \
    }

#define UWDECL_Ptr(name, initializer)  _UW_VALUE_CLEANUP __UWDECL_Ptr((name), (initializer))

#define UwPtr(initializer)  \
    /* make Ptr rvalue */  \
    ({  \
        __UWDECL_Ptr(v, (initializer));  \
        v;  \
    })

// Status declarations and rvalues

#define __UWDECL_Status(name, _status_code)  \
    /* declare Status variable */  \
    _UwValue name = {  \
        ._status_type_id = UwTypeId_Status,  \
        .status_code = _status_code  \
    }

#define UWDECL_Status(name, _status_code)  \
    _UW_VALUE_CLEANUP __UWDECL_Status((name), (_status_code))

#define UwStatus(_status_code)  \
    /* make Status rvalue */  \
    ({  \
        __UWDECL_Status(status, (_status_code));  \
        status;  \
    })

#define UwOK()  \
    /* make success rvalue */  \
    ({  \
        __UWDECL_Status(status, UW_SUCCESS);  \
        status;  \
    })

#define UwError(code)  \
    /* make Status rvalue */  \
    ({  \
        __UWDECL_Status(status, (code));  \
        status;  \
    })

#define UwOOM()  \
    /* make UW_ERROR_OOM rvalue */  \
    ({  \
        __UWDECL_Status(status, UW_ERROR_OOM);  \
        status;  \
    })

#define UwVaEnd()  \
    /* make VA_END rvalue */  \
    ({  \
        __UWDECL_Status(status, UW_STATUS_VA_END);  \
        status;  \
    })

#define UwErrorNoInterface(value, interface_name)  \
    ({  \
        __UWDECL_Status(status, UW_ERROR_NO_INTERFACE);  \
        _uw_set_status_desc(&status,  \
                            "Value of type %s provides no UwInterface_%s",  \
                            _uw_types[(value)->type_id]->name,  \
                            #interface_name  \
        );  \
        status;  \
    })

#define __UWDECL_Errno(name, _errno)  \
    /* declare errno Status variable */  \
    _UwValue name = {  \
        ._status_type_id = UwTypeId_Status,  \
        .status_code = UW_ERROR_ERRNO,  \
        .uw_errno = _errno  \
    }

#define UWDECL_Errno(name, _errno)  _UW_VALUE_CLEANUP __UWDECL_Errno((name), (_errno))

#define UwErrno(_errno)  \
    /* make errno Status rvalue */  \
    ({  \
        __UWDECL_Errno(status, _errno);  \
        status;  \
    })

// List and Map can't be declared in-place, defining rvalues only

#define UwList(...)  _uw_create(UwTypeId_List __VA_OPT__(,) __VA_ARGS__, UwVaEnd())
/*
 * List constructor arguments are list items.
 */

#define UwMap(...)   _uw_create(UwTypeId_Map  __VA_OPT__(,) __VA_ARGS__, UwVaEnd())
/*
 * Map constructor arguments are key-value pairs.
 */


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

#define uw_call_void(v, method_name, ...)  \
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

#define uw_ifcall(v, interface_name, method_name, ...)  \
    /* call interface method */  \
    ({  \
        UwInterface_##interface_name* iface =  \
            _uw_get_interface(_uw_types[(v)->type_id], UwInterfaceId_##interface_name);  \
        iface?  \
            iface->_##method_name((v) __VA_OPT__(,) __VA_ARGS__)  \
        :  \
            UwErrorNoInterface((v), interface_name);  \
    })

#define uw_ifsuper(self, iface, method_name, ...)  \
    /* call super method of interface */  \
    ({  \
        UwInterface_##interface_name* iface =  \
            _uw_get_interface(_uw_types[(self)->type_id]->ancestor_id, UwInterfaceId_##interface_name);  \
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
        uw_call_void(v, destroy);
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
    if (memcmp(a, b, sizeof(_UwValue)) == 0) {
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
    uw_strcopy_buf((uw_str), variable_name)

void uw_destroy_cstring(CStringPtr* str);

/****************************************************************
 * Helper functions and macros
 */

static inline void _uw_call_fini(UwValuePtr value)
{
    uw_call_void(value, fini);
}

static inline void _uw_call_hash(UwValuePtr value, UwHashContext* ctx)
{
    UwMethodHash fn_hash = _uw_types[value->type_id]->_hash;
    fn_hash(value, ctx);
}

UwResult _uw_default_create(UwTypeId type_id, va_list ap);
/*
 * Default implementation of create method for types that have extra data.
 */

void _uw_default_destroy(UwValuePtr self);
/*
 * Default implementation of destroy method for types that have extra data.
 */

UwResult _uw_default_clone(UwValuePtr self);
/*
 * Default implementation of clone method.
 */

bool _uw_alloc_extra_data(UwValuePtr v);
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

static inline void* _uw_get_data_ptr(UwValuePtr v, UwTypeId type_id)
{
    if (v->extra_data) {
        UwType* t = _uw_types[type_id];
        return (void*) (
            ((uint8_t*) v->extra_data) + t->data_offset
        );
    } else {
        return nullptr;
    }
}

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
