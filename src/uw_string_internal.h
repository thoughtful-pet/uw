#pragma once

/*
 * String internals.
 */

#include "include/uw_base.h"

#ifdef __cplusplus
extern "C" {
#endif

// branch optimization hints
#define _likely_(x)    __builtin_expect(!!(x), 1)
#define _unlikely_(x)  __builtin_expect(!!(x), 0)

struct _UwStringExtraData {
    /*
     * variable-size structure because of struct _UwString
     */
    _UwExtraData     value_data;
    struct _UwString string_data;
};

#define _uw_str_is_embedded(value)  \
    ((value)->str_header.is_embedded)

#define _uw_get_string_ptr(value)  \
    (  \
        _uw_str_is_embedded(value)?  \
            &(value)->str_header  \
        :  \
            &((struct _UwStringExtraData*) ((value)->extra_data))->string_data \
    )

/****************************************************************
 * Safety helpers.
 *
 * cap_size and char_size are stored as 0-based whereas all
 * functions use 1-based values.
 *
 * The following functions should be used to make the code
 * less error-prone, instead of accessing fields directly
 * and forgetting +/- 1.
 */

static inline uint8_t _uw_string_cap_size(struct _UwString* s)
{
    return s->cap_size + 1;
}

static inline void _uw_string_set_cap_size(struct _UwString* s, uint8_t cap_size)
{
    s->cap_size = cap_size - 1;
}

static inline uint8_t _uw_string_char_size(struct _UwString* s)
{
    return s->char_size + 1;
}

static inline void _uw_string_set_char_size(struct _UwString* s, uint8_t char_size)
{
    s->char_size = char_size - 1;
}

/****************************************************************
 * Basic interface methods
 */

UwResult _uw_string_create        (UwTypeId type_id, va_list ap);
void     _uw_string_destroy       (UwValuePtr self);
UwResult _uw_string_clone         (UwValuePtr self);
void     _uw_string_hash          (UwValuePtr self, UwHashContext* ctx);
void     _uw_string_dump          (UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail);
bool     _uw_string_is_true       (UwValuePtr self);
bool     _uw_string_equal_sametype(UwValuePtr self, UwValuePtr other);
bool     _uw_string_equal         (UwValuePtr self, UwValuePtr other);

void _uw_string_dump_data(FILE* fp, UwValuePtr str, int indent);
/*
 * Helper function for _uw_string_dump.
 */

/****************************************************************
 * The string is allocated in blocks.
 * Given that typical allocator's granularity is 16,
 * there's no point to make block size less than that.
 */

#define UWSTRING_BLOCK_SIZE    16

/****************************************************************
 * Methods that depend on cap_size field.
 *
 * These basic methods are required by both string implementation
 * and test suite, that's why a separate file.
 */

typedef unsigned (*CapGetter)(struct _UwString* str);
typedef void     (*CapSetter)(struct _UwString* str, unsigned n);

typedef struct {
    CapGetter   get_length;
    CapSetter   set_length;
    CapGetter   get_capacity;
    CapSetter   set_capacity;
} CapMethods;

extern CapMethods _uws_cap_methods[8];

static inline CapMethods* get_cap_methods(struct _UwString* str)
{
    return &_uws_cap_methods[str->cap_size];
}

static inline uint8_t* cap_data_addr(uint8_t cap_size, struct _UwString* desired_addr)
/*
 * Return aligned address of capacity data
 * that follows 1-byte str header at `desired_addr` which can be unaligned.
 *
 * This function does not dereference `desired_addr` and can be used to calculate
 * address starting from nullptr.
 */
{
    static_assert(
        sizeof(struct _UwString) == 1
    );
    return align_pointer(
        ((uint8_t*) desired_addr) + sizeof(struct _UwString),
        cap_size
    );
}

static inline uint8_t* get_char0_ptr(struct _UwString* desired_addr, uint8_t cap_size, uint8_t char_size)
/*
 * Return address of the first character in string.
 * This function is also used to determine memory size for different cap_size and char_size.
 *
 * This function does not dereference `desired_addr` and can be used to calculate
 * address starting from nullptr.
 */
{
    return align_pointer(
        cap_data_addr(cap_size, desired_addr) + cap_size * 2,  // 2 == length + capacity
        char_size  // align at char_size boundary
    );
}

static inline uint8_t* get_char_ptr(struct _UwString* s, unsigned start_pos)
/*
 * Return address of character in string at `start_pos`.
 */
{
    uint8_t cap_size  = _uw_string_cap_size(s);
    uint8_t char_size = _uw_string_char_size(s);
    return get_char0_ptr(s, cap_size, char_size) + start_pos * char_size;
}

/****************************************************************
 * Methods that depend on char_size field.
 */

typedef char32_t (*GetChar)(uint8_t* p);
typedef void     (*PutChar)(uint8_t* p, char32_t c);
typedef void     (*Hash)(uint8_t* self_ptr, unsigned length, UwHashContext* ctx);
typedef uint8_t  (*MaxCharSize)(uint8_t* self_ptr, unsigned length);
typedef bool     (*Equal)(uint8_t* self_ptr, struct _UwString* other, unsigned other_start_pos, unsigned length);
typedef bool     (*EqualCStr)(uint8_t* self_ptr, char* other, unsigned length);
typedef bool     (*EqualUtf8)(uint8_t* self_ptr, char8_t* other, unsigned length);
typedef bool     (*EqualUtf32)(uint8_t* self_ptr, char32_t* other, unsigned length);
typedef void     (*CopyTo)(uint8_t* self_ptr, struct _UwString* dest, unsigned dest_start_pos, unsigned length);
typedef void     (*CopyToUtf8)(uint8_t* self_ptr, char* dest_ptr, unsigned length);
typedef unsigned (*CopyFromCStr)(uint8_t* self_ptr, char* src_ptr, unsigned length);
typedef unsigned (*CopyFromUtf8)(uint8_t* self_ptr, char8_t* src_ptr, unsigned length);
typedef unsigned (*CopyFromUtf32)(uint8_t* self_ptr, char32_t* src_ptr, unsigned length);

typedef struct {
    GetChar       get_char;
    PutChar       put_char;
    Hash          hash;
    MaxCharSize   max_char_size;
    Equal         equal;
    EqualCStr     equal_cstr;
    EqualUtf8     equal_u8;
    EqualUtf32    equal_u32;
    CopyTo        copy_to;
    CopyToUtf8    copy_to_u8;
    CopyFromCStr  copy_from_cstr;
    CopyFromUtf8  copy_from_utf8;
    CopyFromUtf32 copy_from_utf32;
} StrMethods;

extern StrMethods _uws_str_methods[4];

static inline StrMethods* get_str_methods(struct _UwString* str)
{
    return &_uws_str_methods[str->char_size];
}

/****************************************************************
 * Misc. functions
 */

static inline char32_t read_utf8_char(char8_t** str)
/*
 * Decode UTF-8 character from null-terminated string, update `*str`.
 *
 * Stop decoding if null character is encountered.
 *
 * Return decoded character, null, or 0xFFFFFFFF if UTF-8 sequence is invalid.
 */
{
    char8_t c = **str;
    if (_unlikely_(c == 0)) {
        return 0;
    }
    (*str)++;

    if (c < 0x80) {
        return  c;
    }

    char32_t codepoint = 0;
    char8_t next;

#   define APPEND_NEXT         \
        next = **str;          \
        if (_unlikely_(next == 0)) return 0; \
        if (_unlikely_((next & 0b1100'0000) != 0b1000'0000)) goto bad_utf8; \
        (*str)++;              \
        codepoint <<= 6;       \
        codepoint |= next & 0x3F;

    codepoint = c & 0b0001'1111;
    if ((c & 0b1110'0000) == 0b1100'0000) {
        APPEND_NEXT
    } else if ((c & 0b1111'0000) == 0b1110'0000) {
        APPEND_NEXT
        APPEND_NEXT
    } else if ((c & 0b1111'1000) == 0b1111'0000) {
        APPEND_NEXT
        APPEND_NEXT
        APPEND_NEXT
    } else {
        goto bad_utf8;
    }
    if (_unlikely_(codepoint == 0)) {
        // zero codepoint encoded with 2 or more bytes,
        // make it invalid to avoid mixing up with 1-byte null character
bad_utf8:
        codepoint = 0xFFFFFFFF;
    }
    return codepoint;
#   undef APPEND_NEXT
}

#ifdef __cplusplus
}
#endif
