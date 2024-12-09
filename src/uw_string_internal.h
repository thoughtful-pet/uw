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

// internal string structure

struct _UwString {
    // variable length structure
    uint8_t cap_size:3,      // length and capacity size in bytes minus one
            char_size:2,     // character size in bytes minus one
            block_count:3;   // number of 64-bit blocks for fast comparison
};

#define _uw_get_string_pptr(value)  \
    (  \
        (struct _UwString**) (  \
            ((uint8_t*) (value)) + sizeof(_UwValueBase) \
        )  \
    )

/****************************************************************
 * Basic interface methods
 */

bool       _uw_init_string          (UwValuePtr self);
void       _uw_fini_string          (UwValuePtr self);
void       _uw_hash_string          (UwValuePtr self, UwHashContext* ctx);
UwValuePtr _uw_copy_string          (UwValuePtr self);
void       _uw_dump_string          (UwValuePtr self, int indent, struct _UwValueChain* prev_compound);
bool       _uw_string_is_true       (UwValuePtr self);
bool       _uw_string_equal_sametype(UwValuePtr self, UwValuePtr other);
bool       _uw_string_equal         (UwValuePtr self, UwValuePtr other);
bool       _uw_string_equal_ctype   (UwValuePtr self, UwCType ctype, ...);

/****************************************************************
 * The string is allocated in 8-byte blocks.
 * One block can hold up to 5 ASCII characters, an average length of English word.
 * Short strings up to 6 blocks can me compared for equality
 * simply comparing 64-bit integers.
 *
 * The first byte contains bit fields. It is followed by length and capacity
 * fields aligned on cap_size boundary.
 * String data follows capacity field and is aligned if char size is 2 or 4.
 */

#define UWSTRING_BLOCK_SIZE    8

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

static inline uint8_t string_header_size(uint8_t cap_size, uint8_t char_size)
{
    if (_unlikely_((char_size - 1) == 2)) {
        return 3 * cap_size;
    } else {
        return (3 * cap_size + char_size - 1) & ~(char_size - 1);
    }
}

static inline uint8_t* get_char_ptr(struct _UwString* str, unsigned start_pos)
{
    struct _UwString s = *str;
    return ((uint8_t*) str) + string_header_size(s.cap_size + 1, s.char_size + 1) + start_pos * (s.char_size + 1);
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

static inline uint8_t _uw_string_char_size(struct _UwString* s)
{
    return s->char_size + 1;
}

#ifdef DEBUG
    bool uw_eq_fast(struct _UwString* a, struct _UwString* b);
#endif

#ifdef __cplusplus
}
#endif
