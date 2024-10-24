#include <string.h>

#include "uw_value.h"

/*
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

// branch optimization hints
#define _likely_(x)    __builtin_expect(!!(x), 1)
#define _unlikely_(x)  __builtin_expect(!!(x), 0)

typedef struct { uint8_t v[3]; } uint24_t;

/****************************************************************
 * misc. helper functions
 */

static inline uint8_t update_char_width(uint8_t width, char32_t c)
{
    if (_unlikely_(c >= 16777216)) {
        return width | 4;
    }
    if (_unlikely_(c >= 65536)) {
        return width | 2;
    }
    if (_unlikely_(c >= 256)) {
        return width | 1;
    }
    return width;
}

static inline uint8_t char_width_to_char_size(uint8_t width)
{
    if (width & 4) {
        return 4;
    }
    if (width & 2) {
        return 3;
    }
    if (width & 1) {
        return 2;
    }
    return 1;
}

size_t u32_strlen(char32_t* str)
{
    size_t length = 0;
    while (_likely_(*str++)) {
        length++;
    }
    return length;
}

size_t u32_strlen2(char32_t* str, uint8_t* char_size)
{
    size_t length = 0;
    uint8_t width = 0;
    char32_t c;
    while (_likely_(c = *str++)) {
        width = update_char_width(width, c);
        length++;
    }
    *char_size = char_width_to_char_size(width);
    return length;
}

char32_t* u32_strchr(char32_t* str, char32_t chr)
{
    char32_t c;
    while (_likely_(c = *str)) {
        if (_unlikely_(c == chr)) {
            return str;
        }
        str++;
    }
    return nullptr;
}

uint8_t u32_char_size(char32_t* str, size_t max_len)
{
    uint8_t width = 0;
    while (max_len--) {
        char32_t c = *str++;
        if (_unlikely_(c == 0)) {
            break;
        }
        width = update_char_width(width, c);
    }
    return char_width_to_char_size(width);
}

/****************************************************************
 * UTF-8 helper functions
 */

static inline bool read_utf8_buffer(char8_t** ptr, size_t* bytes_remaining, char32_t* codepoint)
/*
 * Decode UTF-8 character from buffer, update `*ptr`.
 *
 * Null charaters are returned as zero codepoints.
 *
 * Return false if UTF-8 sequence is incomplete or `bytes_remaining` is zero.
 * Otherwise return true.
 * If character is invalid, 0xFFFFFFFF is written to the `codepoint`.
 */
{
    size_t remaining = *bytes_remaining;
    if (!remaining) {
        return false;
    }

    char32_t result = 0;
    char8_t next;

    char8_t c = *((*ptr)++);
    remaining--;

#   define APPEND_NEXT         \
        next = **ptr;          \
        if (_unlikely_((next & 0b1100'0000) != 0b1000'0000)) goto bad_utf8; \
        (*ptr)++;              \
        remaining--;  \
        result <<= 6;       \
        result |= next & 0x3F;

    if (c < 0x80) {
        result = c;
        *bytes_remaining = remaining;
    } else {
        result = c & 0b0001'1111;
        if ((c & 0b1110'0000) == 0b1100'0000) {
            if (_unlikely_(!remaining)) return false;
            APPEND_NEXT
            *bytes_remaining = remaining;
        } else if ((c & 0b1111'0000) == 0b1110'0000) {
            if (_unlikely_(remaining < 2)) return false;
            APPEND_NEXT
            APPEND_NEXT
            *bytes_remaining = remaining;
        } else if ((c & 0b1111'1000) == 0b1111'0000) {
            if (_unlikely_(remaining < 3)) return false;
            APPEND_NEXT
            APPEND_NEXT
            APPEND_NEXT
            *bytes_remaining = remaining;
        } else {
            goto bad_utf8;
        }
        if (_unlikely_(codepoint == 0)) {
            // zero codepoint encoded with 2 or more bytes,
            // make it invalid to avoid mixing up with 1-byte null character
bad_utf8:
            result = 0xFFFFFFFF;
        }
    }
    *codepoint = result;
    return true;
#   undef APPEND_NEXT
}

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

size_t utf8_strlen(char8_t* str)
{
    size_t length = 0;
    while(_likely_(*str != 0)) {
        char32_t c = read_utf8_char(&str);
        if (_likely_(c != 0xFFFFFFFF)) {
            length++;
        }
    }
    return length;
}

size_t utf8_strlen2(char8_t* str, uint8_t* char_size)
{
    size_t length = 0;
    uint8_t width = 0;
    while(_likely_(*str != 0)) {
        char32_t c = read_utf8_char(&str);
        if (_likely_(c != 0xFFFFFFFF)) {
            width = update_char_width(width, c);
            length++;
        }
    }
    *char_size = char_width_to_char_size(width);
    return length;
}

size_t utf8_strlen2_buf(char8_t* buffer, size_t* size, uint8_t* char_size)
{
    char8_t* ptr = buffer;
    size_t bytes_remaining = *size;
    size_t length = 0;
    uint8_t width = 0;

    while (_likely_(bytes_remaining)) {
        char32_t c;
        if (_unlikely_(!read_utf8_buffer(&ptr, &bytes_remaining, &c))) {
            break;
        }
        if (_likely_(c != 0xFFFFFFFF)) {
            width = update_char_width(width, c);
            length++;
        }
    }
    *size -= bytes_remaining;

    if (char_size) {
        *char_size = char_width_to_char_size(width);
    }

    return length;
}

uint8_t utf8_char_size(char8_t* str, size_t max_len)
{
    uint8_t width = 0;
    while(_likely_(*str != 0)) {
        char32_t c = read_utf8_char(&str);
        if (_likely_(c != 0xFFFFFFFF)) {
            width = update_char_width(width, c);
        }
    }
    return char_width_to_char_size(width);
}

char8_t* utf8_skip(char8_t* str, size_t n)
{
    while(n--) {
        char32_t c = read_utf8_char(&str);
        if (_unlikely_(*str == 0)) {
            break;
        }
    }
    return str;
}

/****************************************************************
 * Methods that depend on cap_size field.
 */

#include "uw_string_capmeth.c"


/****************************************************************
 * Methods that depend on char_size field.
 */
typedef char32_t (*GetChar)(uint8_t* p);
typedef void     (*PutChar)(uint8_t* p, char32_t c);
typedef void     (*Hash)(uint8_t* self_ptr, size_t length, UwHashContext* ctx);
typedef uint8_t  (*MaxCharSize)(uint8_t* self_ptr, size_t length);
typedef bool     (*Equal)(uint8_t* self_ptr, _UwString* other, size_t other_start_pos, size_t length);
typedef bool     (*EqualCStr)(uint8_t* self_ptr, char* other, size_t length);
typedef bool     (*EqualUtf8)(uint8_t* self_ptr, char8_t* other, size_t length);
typedef bool     (*EqualUtf32)(uint8_t* self_ptr, char32_t* other, size_t length);
typedef void     (*CopyTo)(uint8_t* self_ptr, _UwString* dest, size_t dest_start_pos, size_t length);
typedef void     (*CopyToCStr)(uint8_t* self_ptr, char* dest_ptr, size_t length);
typedef size_t   (*CopyFromCStr)(uint8_t* self_ptr, char* src_ptr, size_t length);
typedef size_t   (*CopyFromUtf8)(uint8_t* self_ptr, char8_t* src_ptr, size_t length);
typedef size_t   (*CopyFromUtf32)(uint8_t* self_ptr, char32_t* src_ptr, size_t length);

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
    CopyToCStr    copy_to_cstr;
    CopyFromCStr  copy_from_cstr;
    CopyFromUtf8  copy_from_utf8;
    CopyFromUtf32 copy_from_utf32;
} StrMethods;

#define STR_CHAR_METHODS_IMPL(type_name)  \
    static char32_t _get_char_##type_name(uint8_t* p)  \
    {  \
        return *((type_name*) p); \
    }  \
    static void _put_char_##type_name(uint8_t* p, char32_t c)  \
    {  \
        *((type_name*) p) = (type_name) c; \
    }

STR_CHAR_METHODS_IMPL(uint8_t)
STR_CHAR_METHODS_IMPL(uint16_t)
STR_CHAR_METHODS_IMPL(uint32_t)

static inline char32_t get_char_uint24_t(uint24_t** p)
{
    uint8_t *byte = (uint8_t*) *p;
    // always little endian
    char32_t c = byte[0] | (byte[1] << 8) | (byte[2] << 16);
    (*p)++;
    return c;
}

static char32_t _get_char_uint24_t(uint8_t* p)
{
    return get_char_uint24_t((uint24_t**) &p);
}

static inline void put_char_uint24_t(uint24_t** p, char32_t c)
{
    uint8_t *byte = (uint8_t*) *p;
    // always little endian
    byte[0] = (uint8_t) c; c >>= 8;
    byte[1] = (uint8_t) c; c >>= 8;
    byte[2] = (uint8_t) c;
    (*p)++;
}

static void _put_char_uint24_t(uint8_t* p, char32_t c)
{
    put_char_uint24_t((uint24_t**) &p, c);
}

/*
 * Implementation of hash methods.
 *
 * Calculate hash as if characters were chat32_t regardless of storage size.
 */

// integral types:

#define STR_HASH_IMPL(type_name)  \
    static void _hash_##type_name(uint8_t* self_ptr, size_t length, UwHashContext* ctx) \
    {  \
        type_name* ptr = (type_name*) self_ptr;  \
        while (_likely_(length--)) {  \
            union {  \
                struct {  \
                    char32_t a;  \
                    char32_t b;  \
                };  \
                uint64_t i64;  \
            } data;  \
            \
            data.a = *ptr++;  \
            \
            if (_unlikely_(0 == length--)) {  \
                data.b = 0;  \
                _uw_hash_uint64(ctx, data.i64);  \
                break;  \
            }  \
            data.b = *ptr++;  \
            _uw_hash_uint64(ctx, data.i64);  \
        }  \
    }

STR_HASH_IMPL(uint8_t)
STR_HASH_IMPL(uint16_t)
STR_HASH_IMPL(uint32_t)

// uint24:

static void _hash_uint24_t(uint8_t* self_ptr, size_t length, UwHashContext* ctx)
{
    while (_likely_(length--)) {
        union {
            struct {
                char32_t a;
                char32_t b;
            };
            uint64_t i64;
        } data;

        data.a = get_char_uint24_t((uint24_t**) &self_ptr);

        if (_unlikely_(0 == length--)) {
            data.b = 0;
            _uw_hash_uint64(ctx, data.i64);
            break;
        }
        data.b = get_char_uint24_t((uint24_t**) &self_ptr);
        _uw_hash_uint64(ctx, data.i64);
    }
}


/*
 * Implementation of max_char_size methods.
 */

static uint8_t _max_char_size_uint8_t(uint8_t* self_ptr, size_t length)
{
    return 1;
}

static uint8_t _max_char_size_uint16_t(uint8_t* self_ptr, size_t length)
{
    uint16_t* ptr = (uint16_t*) self_ptr;
    while (_likely_(length--)) {
        uint16_t c = *ptr++;
        if (_unlikely_(c >= 256)) {
            return 2;
        }
    }
    return 1;
}

static uint8_t _max_char_size_uint24_t(uint8_t* self_ptr, size_t length)
{
    uint24_t* ptr = (uint24_t*) self_ptr;
    while (_likely_(length--)) {
        char32_t c = get_char_uint24_t(&ptr);
        if (_unlikely_(c >= 65536)) {
            return 3;
        } else if (_unlikely_(c >= 256)) {
            return 2;
        }
    }
    return 1;
}

static uint8_t _max_char_size_uint32_t(uint8_t* self_ptr, size_t length)
{
    uint32_t* ptr = (uint32_t*) self_ptr;
    while (_likely_(length--)) {
        uint32_t c = *ptr++;
        if (_unlikely_(c >= 16777216)) {
            return 4;
        } else if (_unlikely_(c >= 65536)) {
            return 3;
        } else if (_unlikely_(c >= 256)) {
            return 2;
        }
    }
    return 1;
}


/*
 * Implementation of equality methods.
 *
 * In helper functions self argument is always uint8_t regardless of char width.
 * That's because get_ptr returns uint8_t*.
 */

// memcmp for the same char size:

#define STR_EQ_MEMCMP_HELPER_IMPL(type_name)  \
    static inline bool eq_##type_name##_##type_name(uint8_t* self_ptr, type_name* other_ptr, size_t length)  \
    {  \
        return memcmp(self_ptr, other_ptr, length * sizeof(type_name)) == 0;  \
    }

STR_EQ_MEMCMP_HELPER_IMPL(uint8_t)
STR_EQ_MEMCMP_HELPER_IMPL(uint16_t)
STR_EQ_MEMCMP_HELPER_IMPL(uint24_t)
STR_EQ_MEMCMP_HELPER_IMPL(uint32_t)

// integral types:

#define STR_EQ_HELPER_IMPL(type_name_self, type_name_other)  \
    static inline bool eq_##type_name_self##_##type_name_other(uint8_t* self_ptr, type_name_other* other_ptr, size_t length)  \
    {  \
        type_name_self* this_ptr = (type_name_self*) self_ptr;  \
        while (_likely_(length--)) {  \
            if (_unlikely_(*this_ptr++ != *other_ptr++)) {  \
                return false;  \
            }  \
        }  \
        return true;  \
    }

STR_EQ_HELPER_IMPL(uint8_t,  uint16_t)
STR_EQ_HELPER_IMPL(uint8_t,  uint32_t)
STR_EQ_HELPER_IMPL(uint16_t, uint8_t)
STR_EQ_HELPER_IMPL(uint16_t, uint32_t)
STR_EQ_HELPER_IMPL(uint32_t, uint8_t)
STR_EQ_HELPER_IMPL(uint32_t, uint16_t)

// uint24_t as self type:

#define STR_EQ_S24_HELPER_IMPL(type_name_other)  \
    static inline bool eq_uint24_t_##type_name_other(uint8_t* self_ptr, type_name_other* other_ptr, size_t length)  \
    {  \
        while (_likely_(length--)) {  \
            if (_unlikely_(get_char_uint24_t((uint24_t**) &self_ptr) != *other_ptr++)) {  \
                return false;  \
            }  \
        }  \
        return true;  \
    }

STR_EQ_S24_HELPER_IMPL(uint8_t)
STR_EQ_S24_HELPER_IMPL(uint16_t)
STR_EQ_S24_HELPER_IMPL(uint32_t)

// uint24_t as other type:

#define STR_EQ_O24_HELPER_IMPL(type_name_self)  \
    static inline bool eq_##type_name_self##_uint24_t(uint8_t* self_ptr, uint24_t* other_ptr, size_t length)  \
    {  \
        type_name_self* this_ptr = (type_name_self*) self_ptr;  \
        while (_likely_(length--)) {  \
            if (_unlikely_(*this_ptr++ != get_char_uint24_t(&other_ptr))) {  \
                return false;  \
            }  \
        }  \
        return true;  \
    }

STR_EQ_O24_HELPER_IMPL(uint8_t)
STR_EQ_O24_HELPER_IMPL(uint16_t)
STR_EQ_O24_HELPER_IMPL(uint32_t)

#define STR_EQ_IMPL(type_name_self)  \
    static bool _eq_##type_name_self(uint8_t* self_ptr, _UwString* other, size_t other_start_pos, size_t length)  \
    {  \
        uint8_t* other_ptr = get_char_ptr(other, other_start_pos);  \
        switch (other->char_size) {  \
            case 0: return eq_##type_name_self##_uint8_t(self_ptr, (uint8_t*) other_ptr, length);  \
            case 1: return eq_##type_name_self##_uint16_t(self_ptr, (uint16_t*) other_ptr, length);  \
            case 2: return eq_##type_name_self##_uint24_t(self_ptr, (uint24_t*) other_ptr, length);  \
            case 3: return eq_##type_name_self##_uint32_t(self_ptr, (uint32_t*) other_ptr, length);  \
            default: return false;  \
        }  \
    }

STR_EQ_IMPL(uint8_t)
STR_EQ_IMPL(uint16_t)
STR_EQ_IMPL(uint24_t)
STR_EQ_IMPL(uint32_t)

// comparison with C/char32_t null-terminated string, integral `self` types

#define STR_EQ_CU32_IMPL(type_name_self, type_name_other)  \
    static bool _eq_##type_name_self##_##type_name_other(uint8_t* self_ptr, type_name_other* other, size_t length)  \
    {  \
        type_name_self* this_ptr = (type_name_self*) self_ptr;  \
        while (_likely_(length--)) {  \
            type_name_other c = *other++;  \
            if (_unlikely_(c == 0)) {  \
                return false;  \
            }  \
            if (_unlikely_(*this_ptr++ != c)) {  \
                return false;  \
            }  \
        }  \
        return *other == 0;  \
    }

STR_EQ_CU32_IMPL(uint8_t,  char)
STR_EQ_CU32_IMPL(uint16_t, char)
STR_EQ_CU32_IMPL(uint32_t, char)
STR_EQ_CU32_IMPL(uint8_t,  char32_t)
STR_EQ_CU32_IMPL(uint16_t, char32_t)
STR_EQ_CU32_IMPL(uint32_t, char32_t)

// comparison with C/char32_t null-terminated string, uint24 self type

#define STR_EQ_CU32_S24_IMPL(type_name_other)  \
    static bool _eq_uint24_t_##type_name_other(uint8_t* self_ptr, type_name_other* other, size_t length)  \
    {  \
        while (_likely_(length--)) {  \
            char c = *other++;  \
            if (_unlikely_(c == 0)) {  \
                return false;  \
            }  \
            if (_unlikely_(get_char_uint24_t((uint24_t**) &self_ptr) != c)) {  \
                return false;  \
            }  \
        }  \
        return *other == 0;  \
    }

STR_EQ_CU32_S24_IMPL(char)
STR_EQ_CU32_S24_IMPL(char32_t)

// Comparison with UTF-8 null-terminated string.
// Specific checking for invalid codepoint looks unnecessary, but wrong char32_t string may
// contain such a value. This should not make comparison successful.

#define STR_EQ_UTF8_IMPL(type_name_self, check_invalid_codepoint)  \
    static bool _eq_##type_name_self##_char8_t(uint8_t* self_ptr, char8_t* other, size_t length)  \
    {  \
        type_name_self* this_ptr = (type_name_self*) self_ptr;  \
        while(_likely_(length--)) {  \
            if (_unlikely_(*other == 0)) {  \
                return false;  \
            }  \
            char32_t codepoint = read_utf8_char(&other);  \
            if (check_invalid_codepoint) { \
                if (_unlikely_(codepoint == 0xFFFFFFFF)) {  \
                    return false;  \
                }  \
            }  \
            if (_unlikely_(*this_ptr++ != codepoint)) {  \
                return false;  \
            }  \
        }  \
        return *other == 0;  \
    }

STR_EQ_UTF8_IMPL(uint8_t,  0)
STR_EQ_UTF8_IMPL(uint16_t, 0)
STR_EQ_UTF8_IMPL(uint32_t, 1)

static bool _eq_uint24_t_char8_t(uint8_t* self_ptr, char8_t* other, size_t length)
{
    while(_likely_(length--)) {
        if (_unlikely_(*other == 0)) {
            return false;
        }
        char32_t codepoint = read_utf8_char(&other);
        if (_unlikely_(get_char_uint24_t((uint24_t**) &self_ptr) != codepoint)) {
            return false;
        }
    }
    return *other == 0;
}

/*
 * Implementation of copy methods.
 *
 * When copying from a source with lesser or equal char size
 * the caller must ensure destination char size is sufficient.
 *
 * In helper functions self argument is always uint8_t regardless of char width.
 * That's because get_ptr returns uint8_t*.
 */

// use memcpy for the same char size:

#define STR_COPY_TO_MEMCPY_HELPER_IMPL(type_name)  \
    static inline void cp_##type_name##_##type_name(uint8_t* self_ptr, type_name* dest_ptr, size_t length)  \
    {  \
        memcpy(dest_ptr, self_ptr, length * sizeof(type_name));  \
    }  \

STR_COPY_TO_MEMCPY_HELPER_IMPL(uint8_t)
STR_COPY_TO_MEMCPY_HELPER_IMPL(uint16_t)
STR_COPY_TO_MEMCPY_HELPER_IMPL(uint24_t)
STR_COPY_TO_MEMCPY_HELPER_IMPL(uint32_t)

// integral types:

#define STR_COPY_TO_HELPER_IMPL(type_name_self, type_name_dest)  \
    static inline void cp_##type_name_self##_##type_name_dest(uint8_t* self_ptr, type_name_dest* dest_ptr, size_t length)  \
    {  \
        type_name_self* src_ptr = (type_name_self*) self_ptr;  \
        while (_likely_(length--)) {  \
            *dest_ptr++ = *src_ptr++;  \
        }  \
    }

STR_COPY_TO_HELPER_IMPL(uint8_t,  uint16_t)
STR_COPY_TO_HELPER_IMPL(uint8_t,  uint32_t)
STR_COPY_TO_HELPER_IMPL(uint16_t, uint8_t)
STR_COPY_TO_HELPER_IMPL(uint16_t, uint32_t)
STR_COPY_TO_HELPER_IMPL(uint32_t, uint8_t)
STR_COPY_TO_HELPER_IMPL(uint32_t, uint16_t)

// uint24_t as source type:

#define STR_COPY_TO_S24_HELPER_IMPL(type_name_dest)  \
    static inline void cp_uint24_t_##type_name_dest(uint8_t* self_ptr, type_name_dest* dest_ptr, size_t length)  \
    {  \
        while (_likely_(length--)) {  \
            *dest_ptr++ = get_char_uint24_t((uint24_t**) &self_ptr);  \
        }  \
    }

STR_COPY_TO_S24_HELPER_IMPL(uint8_t)
STR_COPY_TO_S24_HELPER_IMPL(uint16_t)
STR_COPY_TO_S24_HELPER_IMPL(uint32_t)

// uint24_t as destination type:

#define STR_COPY_TO_D24_HELPER_IMPL(type_name_self)  \
    static inline void cp_##type_name_self##_uint24_t(uint8_t* self_ptr, uint24_t* dest_ptr, size_t length)  \
    {  \
        type_name_self* src_ptr = (type_name_self*) self_ptr;  \
        while (_likely_(length--)) {  \
            put_char_uint24_t(&dest_ptr, *src_ptr++);  \
        }  \
    }

STR_COPY_TO_D24_HELPER_IMPL(uint8_t)
STR_COPY_TO_D24_HELPER_IMPL(uint16_t)
STR_COPY_TO_D24_HELPER_IMPL(uint32_t)

#define STR_COPY_TO_IMPL(type_name_self)  \
    static void _cp_to_##type_name_self(uint8_t* self_ptr, _UwString* dest, size_t dest_start_pos, size_t length)  \
    {  \
        uint8_t* dest_ptr = get_char_ptr(dest, dest_start_pos);  \
        switch (dest->char_size) {  \
            case 0: cp_##type_name_self##_uint8_t(self_ptr, (uint8_t*) dest_ptr, length); return; \
            case 1: cp_##type_name_self##_uint16_t(self_ptr, (uint16_t*) dest_ptr, length); return; \
            case 2: cp_##type_name_self##_uint24_t(self_ptr, (uint24_t*) dest_ptr, length); return; \
            case 3: cp_##type_name_self##_uint32_t(self_ptr, (uint32_t*) dest_ptr, length); return; \
        }  \
    }

STR_COPY_TO_IMPL(uint8_t)
STR_COPY_TO_IMPL(uint16_t)
STR_COPY_TO_IMPL(uint24_t)
STR_COPY_TO_IMPL(uint32_t)

// copy to C-string

static void _cp_to_cstr_uint8_t(uint8_t* self_ptr, char* dest, size_t length)
{
    memcpy(dest, self_ptr, length);
    *(dest + length) = 0;
}

// integral types:

#define CSTR_REPLACEMENT_CHAR  '.'  // as in hex dumps

#define STR_COPY_TO_CSTR_IMPL(type_name_self)  \
    static void _cp_to_cstr_##type_name_self(uint8_t* self_ptr, char* dest, size_t length)  \
    {  \
        type_name_self* src_ptr = (type_name_self*) self_ptr;  \
        while (_likely_(length--)) {  \
            type_name_self c = *src_ptr++;  \
            if (_unlikely_(c >= 256)) {  \
                c = CSTR_REPLACEMENT_CHAR;  \
            }  \
            *dest++ = (char) c;  \
        }  \
        *dest = 0;  \
    }

STR_COPY_TO_CSTR_IMPL(uint16_t)
STR_COPY_TO_CSTR_IMPL(uint32_t)

// uint24_t

static void _cp_to_cstr_uint24_t(uint8_t* self_ptr, char* dest, size_t length)
{
    while (_likely_(length--)) {
        char32_t c = get_char_uint24_t((uint24_t**) &self_ptr);
        if (_unlikely_(c >= 256)) {
            c = CSTR_REPLACEMENT_CHAR;
        }
        *dest++ = (char) c;
    }
    *dest = 0;
}

// copy from C/char32_t string, integral `self` types

#define STR_CP_FROM_CSTR_IMPL(type_name_src, type_name_self)  \
    static size_t _cp_from_##type_name_src##_##type_name_self(uint8_t* self_ptr, type_name_src* src_ptr, size_t length)  \
    {  \
        type_name_self* dest_ptr = (type_name_self*) self_ptr;  \
        size_t chars_copied = 0;  \
        while (_likely_(length--)) {  \
            type_name_src c = *src_ptr++;  \
            if (_unlikely_(c == 0)) {  \
                break;  \
            }  \
            *dest_ptr++ = c;  \
            chars_copied++;  \
        }  \
        return chars_copied;  \
    }

STR_CP_FROM_CSTR_IMPL(char, uint8_t)
STR_CP_FROM_CSTR_IMPL(char, uint16_t)
STR_CP_FROM_CSTR_IMPL(char, uint32_t)
STR_CP_FROM_CSTR_IMPL(char32_t, uint8_t)
STR_CP_FROM_CSTR_IMPL(char32_t, uint16_t)
STR_CP_FROM_CSTR_IMPL(char32_t, uint32_t)

// copy from C/char32_t string, uint24 self

#define STR_CP_FROM_CSTR24_IMPL(type_name_src)  \
    static size_t _cp_from_##type_name_src##_uint24_t(uint8_t* self_ptr, type_name_src* src_ptr, size_t length)  \
    {  \
        size_t chars_copied = 0;  \
        while (_likely_(length--)) {  \
            type_name_src c = *src_ptr++;  \
            if (_unlikely_(c == 0)) {  \
                break;  \
            }  \
            put_char_uint24_t((uint24_t**) &self_ptr, c);  \
            chars_copied++;  \
        }  \
        return chars_copied;  \
    }

STR_CP_FROM_CSTR24_IMPL(char)
STR_CP_FROM_CSTR24_IMPL(char32_t)

// copy from UTF-8_t null-terminated string

#define STR_CP_FROM_U8_IMPL(type_name_self)  \
    static size_t _cp_from_u8_##type_name_self(uint8_t* self_ptr, char8_t* src_ptr, size_t length)  \
    {  \
        type_name_self* dest_ptr = (type_name_self*) self_ptr;  \
        size_t chars_copied = 0;  \
        while (_likely_(*src_ptr != 0 && length--)) {  \
            char32_t c = read_utf8_char(&src_ptr);  \
            if (_likely_(c != 0xFFFFFFFF)) {  \
                *dest_ptr++ = c;  \
                chars_copied++;  \
            }  \
        }  \
        return chars_copied;  \
    }

STR_CP_FROM_U8_IMPL(uint8_t)
STR_CP_FROM_U8_IMPL(uint16_t)
STR_CP_FROM_U8_IMPL(uint32_t)

static size_t _cp_from_u8_uint24_t(uint8_t* self_ptr, uint8_t* src_ptr, size_t length)
{
    size_t chars_copied = 0;
    while (_likely_((*((uint8_t*) src_ptr) == 0) && length--)) {
        char32_t c = read_utf8_char(&src_ptr);
        if (_likely_(c != 0xFFFFFFFF)) {
            put_char_uint24_t((uint24_t**) &self_ptr, c);
            chars_copied++;
        }
    }
    return chars_copied;
}

/*
 * String methods table
 */
static StrMethods str_methods[4] = {
    { _get_char_uint8_t,      _put_char_uint8_t,    _hash_uint8_t,             _max_char_size_uint8_t,
      _eq_uint8_t,            _eq_uint8_t_char,     _eq_uint8_t_char8_t,       _eq_uint8_t_char32_t,
      _cp_to_uint8_t,         _cp_to_cstr_uint8_t,
      _cp_from_char_uint8_t,  _cp_from_u8_uint8_t,  _cp_from_char32_t_uint8_t
    },
    { _get_char_uint16_t,     _put_char_uint16_t,   _hash_uint16_t,            _max_char_size_uint16_t,
      _eq_uint16_t,           _eq_uint16_t_char,    _eq_uint16_t_char8_t,      _eq_uint16_t_char32_t,
      _cp_to_uint16_t,        _cp_to_cstr_uint16_t,
      _cp_from_char_uint16_t, _cp_from_u8_uint16_t, _cp_from_char32_t_uint16_t
    },
    { _get_char_uint24_t,     _put_char_uint24_t,   _hash_uint24_t,            _max_char_size_uint24_t,
      _eq_uint24_t,           _eq_uint24_t_char,    _eq_uint24_t_char8_t,      _eq_uint24_t_char32_t,
      _cp_to_uint24_t,        _cp_to_cstr_uint24_t,
      _cp_from_char_uint24_t, _cp_from_u8_uint24_t, _cp_from_char32_t_uint24_t
    },
    { _get_char_uint32_t,     _put_char_uint32_t,   _hash_uint32_t,            _max_char_size_uint32_t,
      _eq_uint32_t,           _eq_uint32_t_char,    _eq_uint32_t_char8_t,      _eq_uint32_t_char32_t,
      _cp_to_uint32_t,        _cp_to_cstr_uint32_t,
      _cp_from_char_uint32_t, _cp_from_u8_uint32_t, _cp_from_char32_t_uint32_t
    }
};
#define get_str_methods(str) (&str_methods[(str)->char_size])


/****************************************************************
 * String allocation
 */

/*
 * Although cap_size and char_size are stored as 0-based,
 * it's less error-prone for API to use 1-based values.
 * Basically, there's no need to read these bit fields
 * for purposes other than indexing method tables.
 */

static inline uint8_t calc_cap_size(size_t capacity)
{
    if (capacity < 256) {
        return 1;
    } else if (capacity < 65536) {
        return 2;
    } else if (capacity < (1ULL << 32)) {
        return 4;
    } else {
        return 8;
    }
}
static inline uint8_t calc_char_size(char32_t c)
{
    if (c < 256) {
        return 1;
    } else if (c < 65536) {
        return 2;
    } else if (c < 16777216) {
        return 3;
    } else {
        return 4;
    }
}

#define pad(n) (((n) + (UWSTRING_BLOCK_SIZE - 1)) & ~(UWSTRING_BLOCK_SIZE - 1))

static void calc_string_alloc_params(
    size_t*  capacity,      // in-out
    uint8_t  char_size,     // in
    size_t   old_memsize,   // in
    uint8_t* new_cap_size,  // in-out
    size_t*  new_memsize,   // out
    size_t*  num_blocks     // out
)
// XXX too many parameters, refactor
{
    uint8_t real_cap_size = calc_cap_size(*capacity);
    if (*new_cap_size < real_cap_size) {
        *new_cap_size = real_cap_size;
    }

    // rough values, initially:
    *new_memsize = pad(string_header_size(*new_cap_size, char_size) + *capacity * char_size);

    if (*new_memsize < old_memsize) {
        *new_memsize = old_memsize;  // don't shrink
    }
    *num_blocks = *new_memsize / UWSTRING_BLOCK_SIZE;

    // adjust them
    do {
        size_t real_capacity = (*num_blocks * UWSTRING_BLOCK_SIZE - string_header_size(*new_cap_size, char_size)) / char_size;
        real_cap_size = calc_cap_size(real_capacity);
#       ifdef DEBUG
        // honor desired capacity
        if (real_cap_size < *new_cap_size) {
            real_cap_size = *new_cap_size;
        }
#       endif
        if (real_cap_size == *new_cap_size && real_capacity >= *capacity) {
            *capacity = real_capacity;
            break;
        }
        if (real_cap_size == *new_cap_size) {
            (*num_blocks)++;
        } else {
            *new_cap_size = real_cap_size;
        }
    } while (true);
    *new_memsize = *num_blocks * UWSTRING_BLOCK_SIZE;
}

static inline _UwString* do_allocate_string(size_t capacity, uint8_t cap_size, uint8_t char_size)
{
    size_t memsize;
    size_t num_blocks;
    calc_string_alloc_params(&capacity, char_size, 0, &cap_size, &memsize, &num_blocks);

    _UwString* s = malloc(memsize);
    if (!s) {
        perror(__func__);
        exit(1);
    }

    // clear up to first 7 blocks for fast comparison to work
    size_t n;
    if (num_blocks <= 7) {
        n = memsize;
    } else {
        n = 7 * UWSTRING_BLOCK_SIZE;
    }
    memset(s, 0, n);

    s->cap_size = cap_size - 1;
    s->char_size = char_size - 1;
    s->block_count = (num_blocks > 7)? 7 : ((uint8_t) num_blocks - 1);

    CapMethods* capmeth = get_cap_methods(s);
    capmeth->set_length(s, 0);

    // calculate real capacity
    capacity = (num_blocks * UWSTRING_BLOCK_SIZE - string_header_size(cap_size, char_size)) / char_size;

    capmeth->set_capacity(s, capacity);

    return s;
}

#ifdef DEBUG
    static _UwString* allocate_string2(uint8_t cap_size, uint8_t char_size)
    {
        return do_allocate_string(1, cap_size, char_size);
    }
#endif

static _UwString* allocate_string(size_t capacity, uint8_t char_size)
{
    uw_assert(0 < char_size && char_size <= 4);

    uint8_t cap_size;

    if (capacity == 0) {
        // use of 1 initial block gives us the following
        // capacity for char size 1-4:  5, 2, 1, 1
        cap_size = 1;
        capacity = (UWSTRING_BLOCK_SIZE - string_header_size(cap_size, char_size)) / char_size;
    } else {
        cap_size = calc_cap_size(capacity);
    }

    return do_allocate_string(capacity, cap_size, char_size);
}

static inline void clean_tail(_UwString* str, size_t position, size_t capacity, uint8_t char_size)
{
    // clean tail for fast comparison to work
    if (position == capacity) {
        return;
    }
    size_t max_n = UWSTRING_BLOCK_SIZE * 7 - string_header_size(str->cap_size + 1, char_size);
    size_t n = capacity * char_size;
    if (n > max_n) {
        n = max_n;
    }
    size_t offset = position * char_size;
    if (offset >= n) {
        return;
    }
    n -= offset;
    memset(get_char_ptr(str, position), 0, n);
}

static bool reallocate_string(_UwString** str_ref, size_t new_capacity, uint8_t new_char_size)
/*
 * Return true if memory address or cap_size, or char_size have changed.
 *
 * Zero values for `new_*` parameters mean use existing one.
 */
{
    _UwString* s = *str_ref;
    uint8_t cap_size = s->cap_size + 1;
    uint8_t char_size = s->char_size + 1;

    CapMethods* capmeth = get_cap_methods(s);
    size_t length = capmeth->get_length(s);
    size_t capacity = capmeth->get_capacity(s);

    size_t old_memsize = pad(string_header_size(cap_size, char_size) + capacity * char_size);

    if (new_char_size < char_size) {
        // char size may only grow
        new_char_size = char_size;
    }

    uint8_t new_cap_size = 0;
    size_t new_memsize;
    size_t num_blocks;
    calc_string_alloc_params(&new_capacity, new_char_size, old_memsize, &new_cap_size, &new_memsize, &num_blocks);

    _UwString* new_str;
    bool result;

    if (new_char_size == char_size) {
        if (new_cap_size == cap_size) {
            // simply re-allocate existing block
            new_str = realloc(s, new_memsize);
            if (!new_str) {
                perror(__func__);
                exit(1);
            }
            clean_tail(new_str, length, new_capacity, char_size);  // clean new space for fast comparison to work
            new_str->block_count = (num_blocks > 7)? 7 : ((uint8_t) num_blocks - 1);
            capmeth->set_capacity(new_str, new_capacity);
            result = new_str != s;

        } else {
            // cap_size has changed, so we need to move string to the right
            // realloc would also move the data
            // to avoid double move, allocate new string
            result = true;

            new_str = allocate_string(new_capacity, new_char_size);

            get_cap_methods(new_str)->set_length(new_str, length);
            memcpy(get_char_ptr(new_str, 0), get_char_ptr(s, 0), length * char_size);
            free(s);
        }
    } else {
        // char_size has changed!
        // allocate new string and copy characters, expanding them
        result = true;

        new_str = allocate_string(new_capacity, new_char_size);

        get_cap_methods(new_str)->set_length(new_str, length);
        get_str_methods(s)->copy_to(get_char_ptr(s, 0), new_str, 0, length);

        free(s);
    }

    *str_ref = new_str;
    return result;
}

static _UwString* expand(UwValuePtr str, size_t increment, uint8_t new_char_size)
{
    _UwString* s = str->string_value;

    CapMethods* capmeth = get_cap_methods(s);
    StrMethods* strmeth = get_str_methods(s);

    size_t length = capmeth->get_length(s);
    size_t capacity = capmeth->get_capacity(s);
    size_t new_capacity = length + increment;

    if (new_capacity > capacity || new_char_size > s->char_size + 1) {
        // need to reallocate
        reallocate_string(&s, new_capacity, new_char_size);
        str->string_value = s;
    }
    return s;
}


/****************************************************************
 * Constructors and destructor
 */

UwValuePtr uw_create_empty_string(size_t capacity, uint8_t char_size)
{
    UwValuePtr result = _uw_alloc_value();
    result->type_id = UwTypeId_String;
    result->string_value = allocate_string(capacity, char_size);
    return result;
}

#ifdef DEBUG
    UwValuePtr uw_create_empty_string2(uint8_t cap_size, uint8_t char_size)
    {
        UwValuePtr result = _uw_alloc_value();
        result->type_id = UwTypeId_String;
        result->string_value = allocate_string2(cap_size, char_size);
        return result;
    }
#endif

UwValuePtr _uw_create_string_c(char* initializer)
{
    size_t capacity = 0;

    if (initializer && *initializer == 0) {
        initializer = nullptr;
    } else {
        capacity = strlen(initializer);
    }

    UwValuePtr result = uw_create_empty_string(capacity, 1);

    _UwString* s = result->string_value;
    if (initializer) {
        get_str_methods(s)->copy_from_cstr(get_char_ptr(s, 0), initializer, capacity);
        get_cap_methods(s)->set_length(s, capacity);
    }
    return result;
}

UwValuePtr _uw_create_string_u8_wrapper(char* initializer)
{
    return _uw_create_string_u8((char8_t*) initializer);
}

UwValuePtr _uw_create_string_u8(char8_t* initializer)
{
    size_t capacity = 0;
    uint8_t char_size = 1;

    if (initializer && *initializer == 0) {
        initializer = nullptr;
    } else {
        capacity = utf8_strlen2(initializer, &char_size);
    }

    UwValuePtr result = uw_create_empty_string(capacity, char_size);

    _UwString* s = result->string_value;
    if (initializer) {
        get_str_methods(s)->copy_from_utf8(get_char_ptr(s, 0), initializer, capacity);
        get_cap_methods(s)->set_length(s, capacity);
    }
    return result;
}

UwValuePtr _uw_create_string_u32(char32_t* initializer)
{
    size_t capacity = 0;
    uint8_t char_size = 1;

    if (initializer && *initializer == 0) {
        initializer = nullptr;
    } else {
        capacity = u32_strlen2(initializer, &char_size);
    }

    UwValuePtr result = uw_create_empty_string(capacity, char_size);

    _UwString* s = result->string_value;
    if (initializer) {
        get_str_methods(s)->copy_from_utf32(get_char_ptr(s, 0), initializer, capacity);
        get_cap_methods(s)->set_length(s, capacity);
    }
    return result;
}

UwValuePtr _uw_create_string_uw(UwValuePtr str)
{
    // XXX convert values of other types to string!!!
    uw_assert_string(str);

    _UwString* src = str->string_value;
    return _uw_copy_string(src);
}

UwValuePtr _uw_copy_string(_UwString* str)
{
    StrMethods* strmeth = get_str_methods(str);

    size_t capacity = get_cap_methods(str)->get_length(str);
    uint8_t char_size = strmeth->max_char_size(get_char_ptr(str, 0), capacity);

    UwValuePtr result = uw_create_empty_string(capacity, char_size);

    if (capacity) {
        _UwString* s = result->string_value;
        strmeth->copy_to(get_char_ptr(str, 0), s, 0, capacity);
        get_cap_methods(s)->set_length(s, capacity);
    }
    return result;
}

void _uw_delete_string(_UwString* str)
{
    free(str);
}

/****************************************************************
 * String functions
 */

size_t uw_strlen(UwValuePtr str)
{
    uw_assert_string(str);
    _UwString* s = str->string_value;
    return get_cap_methods(s)->get_length(s);
}

int _uw_string_cmp(_UwString* a, _UwString* b)
/*
 * Although this method is useless for unicode (lacks collation),
 * it's a reference implementation of char-by-char processing.
 */
{
    if (a == b) {
        // compare with self
        return UW_EQ;
    }

    size_t n_a = get_cap_methods(a)->get_length(a);
    size_t n_b = get_cap_methods(b)->get_length(b);

    uint8_t* aptr = get_char_ptr(a, 0);
    uint8_t* bptr = get_char_ptr(b, 0);

    StrMethods* strmeth_a = get_str_methods(a);
    StrMethods* strmeth_b = get_str_methods(b);

    while (n_a & n_b) {
        char32_t c_a = strmeth_a->get_char(aptr);
        char32_t c_b = strmeth_b->get_char(bptr);

        if (c_a < c_b) {
            return UW_LESS;
        } else if (c_a > c_b) {
            return UW_GREATER;
        }

        n_a--;
        n_b--;
        aptr += a->char_size + 1;
        bptr += b->char_size + 1;
    }

    // remaining length
    if (n_a < n_b) {
        return UW_LESS;
    } else if (n_a > n_b) {
        return UW_GREATER;
    } else {
        return UW_EQ;
    }
}

// TODO

int _uw_compare_u8(UwValuePtr a, char8_t* b)
{
    return 111;
}

int _uw_compare_u8_wrapper(UwValuePtr a, char* b)
{
    return _uw_compare_u8(a, (char8_t*) b);
}

int  uw_compare_cstr(UwValuePtr a, char*  b)
{
    return 111;
}
int _uw_compare_u32(UwValuePtr a, char32_t* b)
{
    return 111;
}

void _uw_string_hash(_UwString* str, UwHashContext* ctx)
{
    size_t length = get_cap_methods(str)->get_length(str);
    if (length) {
        get_str_methods(str)->hash(get_char_ptr(str, 0), length, ctx);
    }
}

static inline bool _uw_eq_fast(_UwString* a, _UwString* b)
{
    if (*((uint64_t*) a) != *((uint64_t*) b)) {
        return false;
    }
    // cap/char sizes match, length and capacity also match
    uint8_t block_count = a->block_count;
    if (block_count == 0) {
        return true;
    }
    if (block_count == 7) {
        return false;
    }
    // compare remaining blocks
    uint64_t* pa = (uint64_t*) a;
    uint64_t* pb = (uint64_t*) b;
    while(block_count--) {
        pa++;
        pb++;
        if (*pa != *pb) {
            return false;
        }
    }
    return true;
}

#ifdef DEBUG
    bool uw_eq_fast(_UwString* a, _UwString* b)
    {
        return _uw_eq_fast(a, b);
    }
#endif

bool _uw_string_eq(_UwString* a, _UwString* b)
{
    if (a == b) {
        // compare with self
        return true;
    }

    // fast comparison
    if (_uw_eq_fast(a, b)) {
        return true;
    }

    // full comparison
    size_t a_length = get_cap_methods(a)->get_length(a);
    size_t b_length = get_cap_methods(b)->get_length(b);
    if (a_length != b_length) {
        return false;
    }
    if (a_length == 0) {
        return true;
    }
    return get_str_methods(a)->equal(get_char_ptr(a, 0), b, 0, a_length);
}

#define STRING_EQ_IMPL(suffix, type_name_b)  \
{  \
    uw_assert_string(a);  \
    _UwString* astr = a->string_value;  \
    \
    size_t a_length = get_cap_methods(astr)->get_length(astr);  \
    return get_str_methods(astr)->equal_##suffix(get_char_ptr(astr, 0), (type_name_b*) b, a_length);  \
}

bool uw_equal_cstr(UwValuePtr a, char* b)     STRING_EQ_IMPL(cstr, char)
bool _uw_equal_u8 (UwValuePtr a, char8_t* b)  STRING_EQ_IMPL(u8,   char8_t)
bool _uw_equal_u32(UwValuePtr a, char32_t* b) STRING_EQ_IMPL(u32,  char32_t)

bool _uw_equal_u8_wrapper(UwValuePtr a, char* b)
{
    return _uw_equal_u8(a, (char8_t*) b);
}

#define SUBSTRING_EQ_IMPL(suffix, type_name_b)  \
{  \
    uw_assert_string(a);  \
    _UwString* astr = a->string_value;  \
    \
    size_t a_length = get_cap_methods(astr)->get_length(astr);  \
    \
    if (end_pos > a_length) {  \
        end_pos = a_length;  \
    }  \
    if (end_pos < start_pos) {  \
        return false;  \
    }  \
    if (end_pos == start_pos && *b == 0) {  \
        return true;  \
    }  \
    \
    return get_str_methods(astr)->equal_##suffix(get_char_ptr(astr, start_pos), b, end_pos - start_pos);  \
}

bool uw_substring_eq_cstr(UwValuePtr a, size_t start_pos, size_t end_pos, char*     b) SUBSTRING_EQ_IMPL(cstr, char)
bool _uw_substring_eq_u8 (UwValuePtr a, size_t start_pos, size_t end_pos, char8_t*  b) SUBSTRING_EQ_IMPL(u8,   char8_t)
bool _uw_substring_eq_u32(UwValuePtr a, size_t start_pos, size_t end_pos, char32_t* b) SUBSTRING_EQ_IMPL(u32,  char32_t)

bool _uw_substring_eq_u8_wrapper(UwValuePtr a, size_t start_pos, size_t end_pos, char* b)
{
    return _uw_substring_eq_u8(a, start_pos, end_pos, (char8_t*) b);
}

bool _uw_substring_eq_uw(UwValuePtr a, size_t start_pos, size_t end_pos, UwValuePtr b)
{
    uw_assert_string(a);
    _UwString* astr = a->string_value;

    size_t a_length = get_cap_methods(astr)->get_length(astr);

    if (end_pos > a_length) {
        end_pos = a_length;
    }
    if (end_pos < start_pos) {
        return false;
    }

    uw_assert_string(b);
    _UwString* bstr = b->string_value;

    if (end_pos == start_pos && get_cap_methods(bstr)->get_length(bstr) == 0) {
        return true;
    }

    return get_str_methods(astr)->equal(get_char_ptr(astr, start_pos), bstr, 0, end_pos - start_pos);
}

CStringPtr uw_string_to_cstring(UwValuePtr str)
{
    uw_assert_string(str);

    _UwString* s = str->string_value;
    size_t length = get_cap_methods(s)->get_length(s);

    CStringPtr result = malloc(length + 1);
    if (!result) {
        perror(__func__);
        exit(1);
    }
    get_str_methods(s)->copy_to_cstr(get_char_ptr(s, 0), result, length);
    return result;
}

void uw_string_copy_buf(UwValuePtr str, char* buffer)
{
    uw_assert_string(str);

    _UwString* s = str->string_value;
    size_t length = get_cap_methods(s)->get_length(s);
    get_str_methods(s)->copy_to_cstr(get_char_ptr(s, 0), buffer, length);
}

void uw_delete_cstring(CStringPtr* str)
{
    free(*str);
    *str = nullptr;
}

void uw_string_swap(UwValuePtr a, UwValuePtr b)
{
    uw_assert_string(a);
    uw_assert_string(b);

    if (a == b) {
        return;
    }

    _UwString* tmp = a->string_value;
    a->string_value = b->string_value;
    b->string_value = tmp;
}

void uw_string_append_char(UwValuePtr dest, char c)
{
    uw_assert_string(dest);
    _UwString* d = dest->string_value;

    size_t dest_len = get_cap_methods(d)->get_length(d);

    d = expand(dest, 1, 1);

    get_cap_methods(d)->set_length(d, dest_len + 1);
    get_str_methods(d)->put_char(get_char_ptr(d, dest_len), c);
}

void _uw_string_append_c32(UwValuePtr dest, char32_t c)
{
    uw_assert_string(dest);
    _UwString* d = dest->string_value;

    size_t dest_len = get_cap_methods(d)->get_length(d);

    d = expand(dest, 1, calc_char_size(c));

    get_cap_methods(d)->set_length(d, dest_len + 1);
    get_str_methods(d)->put_char(get_char_ptr(d, dest_len), c);
}

static void append_cstr(UwValuePtr dest, char* src, size_t src_len)
{
    uw_assert_string(dest);
    _UwString* d = dest->string_value;
    size_t dest_len = get_cap_methods(d)->get_length(d);

    d = expand(dest, src_len, 1);

    get_cap_methods(d)->set_length(d, src_len + dest_len);
    get_str_methods(d)->copy_from_cstr(get_char_ptr(d, dest_len), src, src_len);
}

void uw_string_append_cstr(UwValuePtr dest, char* src)
{
    append_cstr(dest, src, strlen(src));
}

void uw_string_append_substring_cstr(UwValuePtr dest, char* src, size_t src_start_pos, size_t src_end_pos)
{
    size_t src_len = strlen(src);
    if (src_end_pos > src_len) {
        src_end_pos = src_len;
    }
    if (src_start_pos >= src_end_pos) {
        return;
    }
    src_len = src_end_pos - src_start_pos;
    src += src_start_pos;

    append_cstr(dest, src, src_len);
}

static void append_u8(UwValuePtr dest, char8_t* src, size_t src_len)
{
    uw_assert_string(dest);
    _UwString* d = dest->string_value;

    size_t dest_len = get_cap_methods(d)->get_length(d);

    uint8_t src_char_size;
    if (src_len) {
        // src_len is known, find char size
        src_char_size = utf8_char_size(src, src_len);
    } else {
        // src_len == 0, calculate both in one go
        src_len = utf8_strlen2(src, &src_char_size);
    }

    d = expand(dest, src_len, src_char_size);

    get_cap_methods(d)->set_length(d, src_len + dest_len);
    get_str_methods(d)->copy_from_utf8(get_char_ptr(d, dest_len), src, src_len);
}

void _uw_string_append_u8(UwValuePtr dest, char8_t* src)
{
    append_u8(dest, src, 0);
}

void _uw_string_append_u8_wrapper(UwValuePtr dest, char* src)
{
    _uw_string_append_u8(dest, (char8_t*) src);
}

void _uw_string_append_substring_u8(UwValuePtr dest, char8_t* src, size_t src_start_pos, size_t src_end_pos)
{
    size_t src_len = utf8_strlen(src);  // have to find src_len for bounds checking
    if (src_end_pos > src_len) {
        src_end_pos = src_len;
    }
    if (src_start_pos >= src_end_pos) {
        return;
    }
    src_len = src_end_pos - src_start_pos;
    src = utf8_skip(src, src_start_pos);

    append_u8(dest, src, src_len);
}

void _uw_string_append_substring_u8_wrapper(UwValuePtr dest, char* src, size_t src_start_pos, size_t src_end_pos)
{
    _uw_string_append_substring_u8(dest, (char8_t*) src, src_start_pos, src_end_pos);
}

static void append_u32(UwValuePtr dest, char32_t* src, size_t src_len)
{
    uw_assert_string(dest);
    _UwString* d = dest->string_value;

    size_t dest_len = get_cap_methods(d)->get_length(d);

    uint8_t src_char_size;
    if (src_len) {
        // src_len is known, find char size
        src_char_size = u32_char_size(src, src_len);
    } else {
        // src_len == 0, calculate both in one go
        src_len = u32_strlen2(src, &src_char_size);
    }

    d = expand(dest, src_len, src_char_size);

    get_cap_methods(d)->set_length(d, src_len + dest_len);
    get_str_methods(d)->copy_from_utf32(get_char_ptr(d, dest_len), src, src_len);
}

void _uw_string_append_u32(UwValuePtr dest, char32_t* src)
{
    append_u32(dest, src, 0);
}

void _uw_string_append_substring_u32(UwValuePtr dest, char32_t*  src, size_t src_start_pos, size_t src_end_pos)
{
    uint8_t src_char_size;
    size_t src_len = u32_strlen(src);  // have to find src_len for bounds checking
    if (src_end_pos > src_len) {
        src_end_pos = src_len;
    }
    if (src_start_pos >= src_end_pos) {
        return;
    }
    src_len = src_end_pos - src_start_pos;
    src += src_start_pos;

    append_u32(dest, src, src_len);
}

static void append_uw(UwValuePtr dest, _UwString* src, size_t src_start_pos, size_t src_len)
{
    uw_assert_string(dest);
    _UwString* d = dest->string_value;
    size_t dest_len = get_cap_methods(d)->get_length(d);

    StrMethods* src_strmeth = get_str_methods(src);

    uint8_t src_char_size = src->char_size + 1;
    if (src_char_size > d->char_size + 1) {
        src_char_size = src_strmeth->max_char_size(get_char_ptr(src, src_start_pos), src_len);
    }

    d = expand(dest, src_len, src_char_size);

    get_cap_methods(d)->set_length(d, src_len + dest_len);
    src_strmeth->copy_to(get_char_ptr(src, src_start_pos), d, dest_len, src_len);
}

void _uw_string_append_uw(UwValuePtr dest, UwValuePtr src)
{
    uw_assert_string(src);
    _UwString* s = src->string_value;

    size_t src_len = get_cap_methods(s)->get_length(s);
    append_uw(dest, s, 0, src_len);
}

void _uw_string_append_substring_uw(UwValuePtr dest, UwValuePtr src, size_t src_start_pos, size_t src_end_pos)
{
    uw_assert_string(src);
    _UwString* s = src->string_value;

    size_t src_len = get_cap_methods(s)->get_length(s);
    if (src_end_pos > src_len) {
        src_end_pos = src_len;
    }
    if (src_start_pos >= src_end_pos) {
        return;
    }
    src_len = src_end_pos - src_start_pos;

    append_uw(dest, s, src_start_pos, src_len);
}

size_t uw_string_append_utf8(UwValuePtr dest, char8_t* buffer, size_t size)
{
    uw_assert(size != 0);
    uw_assert_string(dest);
    _UwString* d = dest->string_value;
    size_t dest_len = get_cap_methods(d)->get_length(d);

    uint8_t src_char_size;
    size_t src_len = utf8_strlen2_buf(buffer, &size, &src_char_size);

    if (src_len) {
        d = expand(dest, src_len, src_char_size);

        get_cap_methods(d)->set_length(d, src_len + dest_len);
        get_str_methods(d)->copy_from_utf8(get_char_ptr(d, dest_len), buffer, src_len);
    }

    return size;
}

void _uw_string_insert_many_c32(UwValuePtr str, size_t position, char32_t value, size_t n)
{
    if (n == 0) {
        return;
    }
    uw_assert_string(str);
    _UwString* s = str->string_value;
    size_t len = get_cap_methods(s)->get_length(s);

    if (position > len) {
        return;
    }
    s = expand(str, n, calc_char_size(value));

    uint8_t char_size = s->char_size + 1;
    uint8_t* insertion_ptr = get_char_ptr(s, position);

    if (position < len) {
        memmove(insertion_ptr + n * char_size, insertion_ptr, (len - position) * char_size);
    }
    get_cap_methods(s)->set_length(s, len + n);

    for (size_t i = 0; i < n; i++) {
        get_str_methods(s)->put_char(insertion_ptr, value);
        insertion_ptr += char_size;
    }
}

UwValuePtr uw_string_get_substring(UwValuePtr str, size_t start_pos, size_t end_pos)
{
    uw_assert_string(str);
    _UwString* s = str->string_value;

    CapMethods* capmeth = get_cap_methods(s);
    StrMethods* strmeth = get_str_methods(s);

    size_t length = capmeth->get_length(s);

    if (end_pos > length) {
        end_pos = length;
    }
    if (start_pos >= end_pos) {
        return uw_create_empty_string(0, 1);
    }
    length = end_pos - start_pos;
    uint8_t* src = get_char_ptr(s, start_pos);
    uint8_t char_size = strmeth->max_char_size(src, length);

    UwValuePtr result = uw_create_empty_string(length, char_size);
    _UwString* dest = result->string_value;

    get_cap_methods(dest)->set_length(dest, length);
    strmeth->copy_to(src, dest, 0, length);

    return result;
}

char32_t uw_string_at(UwValuePtr str, size_t position)
{
    uw_assert_string(str);
    _UwString* s = str->string_value;
    size_t length = get_cap_methods(s)->get_length(s);
    if (position < length) {
        return get_str_methods(s)->get_char(get_char_ptr(s, position));
    } else {
        return 0;
    }
}

void uw_string_erase(UwValuePtr str, size_t start_pos, size_t end_pos)
{
    uw_assert_string(str);
    _UwString* s = str->string_value;
    CapMethods* capmeth = get_cap_methods(s);
    size_t length = capmeth->get_length(s);
    uint8_t char_size = s->char_size + 1;

    if (start_pos >= length || start_pos >= end_pos) {
        return;
    }
    if (end_pos >= length) {
        // truncate
        length = start_pos;
    } else {
        size_t tail_len = length - end_pos;
        memmove(get_char_ptr(s, start_pos), get_char_ptr(s, end_pos), tail_len * char_size);
        length -= end_pos - start_pos;
    }
    capmeth->set_length(s, length);
    // clean tail for fast comparison to work
    clean_tail(s, length, capmeth->get_capacity(s), char_size);
}

void uw_string_truncate(UwValuePtr str, size_t position)
{
    uw_assert_string(str);
    _UwString* s = str->string_value;
    CapMethods* capmeth = get_cap_methods(s);

    if (position < capmeth->get_length(s)) {
        capmeth->set_length(s, position);
        // clean tail for fast comparison to work
        clean_tail(s, position, capmeth->get_capacity(s), s->char_size + 1);
    }
}

void uw_string_ltrim(UwValuePtr str)
{
    uw_assert_string(str);
    _UwString* s = str->string_value;
    StrMethods* strmeth = get_str_methods(s);
    size_t len = get_cap_methods(s)->get_length(s);
    uint8_t char_size = s->char_size + 1;

    char8_t* ptr = get_char_ptr(s, 0);
    size_t i = 0;
    while (i < len) {
        char32_t c = strmeth->get_char(ptr);
        if (!uw_char_isspace(c)) {
            break;
        }
        i++;
        ptr += char_size;
    }
    uw_string_erase(str, 0, i);
}

void uw_string_rtrim(UwValuePtr str)
{
    uw_assert_string(str);
    _UwString* s = str->string_value;
    StrMethods* strmeth = get_str_methods(s);
    size_t n = get_cap_methods(s)->get_length(s);
    uint8_t char_size = s->char_size + 1;

    char8_t* ptr = get_char_ptr(s, n);
    while (n) {
        ptr -= char_size;
        char32_t c = strmeth->get_char(ptr);
        if (!uw_char_isspace(c)) {
            break;
        }
        n--;
    }
    uw_string_truncate(str, n);
}

void uw_string_trim(UwValuePtr str)
{
    uw_string_rtrim(str);
    uw_string_ltrim(str);
}

UwValuePtr _uw_string_join_c32(char32_t separator, UwValuePtr list)
{
    char32_t s[2] = {separator, 0};
    UwValue sep = uw_create(s);
    return _uw_string_join_uw(sep, list);
}

UwValuePtr _uw_string_join_u8_wrapper(char* separator, UwValuePtr list)
{
    UwValue sep = uw_create(separator);
    return _uw_string_join_uw(sep, list);
}

UwValuePtr _uw_string_join_u8(char8_t* separator, UwValuePtr list)
{
    UwValue sep = uw_create(separator);
    return _uw_string_join_uw(sep, list);
}

UwValuePtr _uw_string_join_u32(char32_t*  separator, UwValuePtr list)
{
    UwValue sep = uw_create(separator);
    return _uw_string_join_uw(sep, list);
}

UwValuePtr _uw_string_join_uw(UwValuePtr separator, UwValuePtr list)
{
    // XXX skipping non-string values

    size_t num_items = uw_list_length(list);
    size_t separator_len = uw_strlen(separator);
    bool item_added;

    // calculate total length and max char width
    size_t result_len = 0;
    uint8_t max_char_size = uw_string_char_size(separator);
    item_added = false;
    for (size_t i = 0; i < num_items; i++) {
        {   // nested scope for autocleaning item
            UwValue item = uw_list_item(list, i);
            if (uw_is_string(item)) {
                if (item_added) {
                    result_len += separator_len;
                }
                uint8_t char_size = uw_string_char_size(item);
                if (max_char_size < char_size) {
                    max_char_size = char_size;
                }
                result_len += uw_strlen(item);
                item_added = true;
            }
        }
    }
    // join list items
    UwValue result = uw_create_empty_string(result_len, max_char_size);
    item_added = false;
    for (size_t i = 0; i < num_items; i++) {
        {   // nested scope for autocleaning item
            UwValue item = uw_list_item(list, i);
            if (uw_is_string(item)) {
                if (item_added) {
                    uw_string_append(result, separator);
                }
                uw_string_append(result, item);
                item_added = true;
            }
        }
    }
    return uw_move(result);
}

#ifdef DEBUG

static void putchar32_utf8(char32_t codepoint)
{
    /*
     * U+0000 - U+007F      0xxxxxxx
     * U+0080 - U+07FF      110xxxxx  10xxxxxx
     * U+0800 - U+FFFF      1110xxxx  10xxxxxx  10xxxxxx
     * U+010000 - U+10FFFF  11110xxx  10xxxxxx  10xxxxxx  10xxxxxx
     */
    if (codepoint > 0x10FFFF) {
        putchar('.');
        return;
    }
    if (codepoint < 32) {
        putchar('.');
        return;
    }
    if (codepoint < 0x80) {
        putchar((char) codepoint);
        return;
    }
    if (codepoint < 0b1'00000'000000) {
        putchar((char) 0xC0 | (codepoint >> 6));
        putchar((char) 0x80 | (codepoint & 0x3F));
        return;
    }
    if (codepoint < 0b1'0000'000000'000000) {
        putchar((char) 0xE0 | (codepoint >> 12));
        putchar((char) 0x80 | ((codepoint >> 6) & 0x3F));
        putchar((char) 0x80 | (codepoint & 0x3F));
        return;
    }
    putchar((char) 0xF0 | (codepoint >> 18));
    putchar((char) 0x80 | ((codepoint >> 12) & 0x3F));
    putchar((char) 0x80 | ((codepoint >> 6) & 0x3F));
    putchar((char) 0x80 | (codepoint & 0x3F));
}

void _uw_dump_string(_UwString* str, int indent)
{
    CapMethods* capmeth = get_cap_methods(str);
    size_t length = capmeth->get_length(str);

    // continue current line
    printf("%llu chars, capacity=%llu, char size=%u, cap size=%u, block_count=%u\n",
           (unsigned long long) length, (unsigned long long) capmeth->get_capacity(str),
           (unsigned) str->char_size + 1, (unsigned) str->cap_size + 1, (unsigned) str->block_count);
    indent += 4;

    // dump up to 8 blocks
    _uw_print_indent(indent);
    size_t n = 0;
    size_t i = 0;
    uint8_t* bptr = (uint8_t*) str;
    while (n++ <= str->block_count) {
        for (int i = 0; i < UWSTRING_BLOCK_SIZE; i++) {
            printf("%02x ", (unsigned) *bptr++);
        }
        if (n == 4) {
            putchar('\n');
            _uw_print_indent(indent);
        } else {
            putchar(' ');
            putchar(' ');
        }
    }
    if (n != 5) {
        putchar('\n');
    }

    // print first 80 characters
    _uw_print_indent(indent);

    bool print_ellipsis = false;
    if (length > 80) {
        length = 80;
        print_ellipsis = true;
    }

    StrMethods* strmeth = get_str_methods(str);
    for(size_t i = 0; i < length; i++) {
        putchar32_utf8(strmeth->get_char(get_char_ptr(str, i)));
    }
    if (print_ellipsis) {
        printf("...");
    }
    putchar('\n');
}

#endif
