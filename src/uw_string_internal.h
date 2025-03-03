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

    _UwExtraData  value_data;

    /*
     * variable part:
     *
     * union {
     *     // if str_uint_cap is not set, capacity and length are stored in UwValue
     *     uint8_t str[];
     *
     *     // if str_uint_cap is set
     *     struct {
     *         unsigned capacity;
     *         unsigned length;
     *         uint8_t str[];
     *     };
     *  };
     */
};

/****************************************************************
 * Low level helper functions.
 */

static inline uint8_t _uw_string_char_size(UwValuePtr s)
/*
 * char_size is stored as 0-based whereas all
 * functions use 1-based values.
 */
{
    return s->str_char_size + 1;
}

static inline void _uw_string_set_char_size(UwValuePtr s, uint8_t char_size)
/*
 * char_size is stored as 0-based whereas all
 * functions use 1-based values.
 */
{
    s->str_char_size = char_size - 1;
}

static inline uint8_t* _uw_string_char_ptr(UwValuePtr s, unsigned start_pos)
/*
 * Return address of character in string at `start_pos`.
 */
{
    unsigned offset = _uw_string_char_size(s) * start_pos;
    if (_likely_(s->str_embedded)) {
        return &s->str_1[offset];
    } else {
        uint8_t* ptr = ((uint8_t*) s->extra_data) + sizeof(struct _UwStringExtraData) + offset;
        if (_unlikely_(s->str_uint_cap)) {
            ptr += 2 * sizeof(unsigned);
        }
        return ptr;
    }
}

static char _panic_bad_char_size[] = "Bad char size: %u\n";

static inline unsigned get_embedded_capacity(uint8_t char_size)
{
    _UwValue v;
    switch (char_size) {
        case 1: return _UWC_LENGTH_OF(v.str_1);
        case 2: return _UWC_LENGTH_OF(v.str_2);
        case 3: return _UWC_LENGTH_OF(v.str_3);
        case 4: return _UWC_LENGTH_OF(v.str_4);
        default: uw_panic(_panic_bad_char_size, char_size);
    }
}

static inline unsigned _uw_string_capacity(UwValuePtr s)
{
    if (_likely_(s->str_embedded)) {
        return get_embedded_capacity(_uw_string_char_size(s));
    } else if (_unlikely_(s->str_uint_cap)) {
        return *( (unsigned*) (((uint8_t*) s->extra_data) + sizeof(struct _UwStringExtraData)) );
    } else {
        return s->str_capacity;
    }
}

static inline void _uw_string_set_capacity(UwValuePtr s, unsigned capacity)
{
    if (_unlikely_(s->str_embedded)) {
        // no op
    } else if (_unlikely_(s->str_uint_cap)) {
        *( (unsigned*) (((uint8_t*) s->extra_data) + sizeof(struct _UwStringExtraData)) ) = capacity;
    } else {
        s->str_capacity = capacity;
    }
}

static inline unsigned _uw_string_length(UwValuePtr s)
{
    if (_likely_(s->str_embedded)) {
        return s->str_embedded_length;
    } else if (_unlikely_(s->str_uint_cap)) {
        return *( (unsigned*) (((uint8_t*) s->extra_data) + sizeof(struct _UwStringExtraData) + sizeof(unsigned)) );
    } else {
        return s->str_length;
    }
}

static inline void _uw_string_set_length(UwValuePtr s, unsigned length)
{
    if (_likely_(s->str_embedded)) {
        s->str_embedded_length = length;
    } else if (_unlikely_(s->str_uint_cap)) {
        *( (unsigned*) (((uint8_t*) s->extra_data) + sizeof(struct _UwStringExtraData) + sizeof(unsigned)) ) = length;
    } else {
        s->str_length = length;
    }
}

static inline unsigned _uw_string_inc_length(UwValuePtr s, unsigned increment)
/*
 * Increment length, return previous value.
 */
{
    if (_likely_(s->str_embedded)) {
        uint8_t length = s->str_embedded_length;
        s->str_embedded_length = length + increment;
        return length;
    } else if (_unlikely_(s->str_uint_cap)) {
        unsigned* length_ptr = (unsigned*) (((uint8_t*) s->extra_data) + sizeof(struct _UwStringExtraData) + sizeof(unsigned));
        unsigned length = *length_ptr;
        *length_ptr = length + increment;
        return length;
    } else {
        unsigned length = s->str_length;
        s->str_length = length + increment;
        return length;
    }
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
 * Methods that depend on char_size field.
 */

typedef char32_t (*GetChar)(uint8_t* p);
typedef void     (*PutChar)(uint8_t* p, char32_t c);
typedef void     (*Hash)(uint8_t* self_ptr, unsigned length, UwHashContext* ctx);
typedef uint8_t  (*MaxCharSize)(uint8_t* self_ptr, unsigned length);
typedef bool     (*Equal)(uint8_t* self_ptr, UwValuePtr other, unsigned other_start_pos, unsigned length);
typedef bool     (*EqualCStr)(uint8_t* self_ptr, char* other, unsigned length);
typedef bool     (*EqualUtf8)(uint8_t* self_ptr, char8_t* other, unsigned length);
typedef bool     (*EqualUtf32)(uint8_t* self_ptr, char32_t* other, unsigned length);
typedef void     (*CopyTo)(uint8_t* self_ptr, UwValuePtr dest, unsigned dest_start_pos, unsigned length);
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

static inline StrMethods* get_str_methods(UwValuePtr str)
{
    return &_uws_str_methods[str->str_char_size];
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
