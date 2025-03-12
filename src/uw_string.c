#include <limits.h>

#include <libpussy/dump.h>

#include "include/uw.h"
#include "src/uw_charptr_internal.h"
#include "src/uw_string_internal.h"

// lookup table to validate capacity

#define _header_size  sizeof(_UwString)

static unsigned _max_capacity[4] = {
    UINT_MAX - _header_size,
    (UINT_MAX - _header_size) / 2,
    (UINT_MAX - _header_size) / 3,
    (UINT_MAX - _header_size) / 4
};

/****************************************************************
 * Basic functions
 */

void* uw_string_get_bytes(UwValuePtr str, unsigned* length)
{
    uw_assert_string(str);
    uint8_t char_size = _uw_string_char_size(str);
    *length = char_size * _uw_string_length(str);
    return (void*) _uw_string_char_ptr(str, 0);
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

static inline uint8_t update_char_width(uint8_t width, char32_t c)
/*
 * Set bits in `width` according to char size.
 * Return updated `width`.
 *
 * This function is used to calculate max charactter size in a string.
 * The result is converted to char size by char_width_to_char_size()
 */
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
/*
 * Convert `width` bits produced by update_char_width() to char size.
 */
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

static inline uint8_t calc_cap_size(unsigned capacity)
{
    if (_likely_(capacity < 256)) {
        return 1;
    } else if (_unlikely_(capacity < 65536)) {
        return 2;
    } else {
        return sizeof(unsigned);
    }
}

static unsigned calc_extra_data_size(uint8_t char_size, unsigned desired_capacity, unsigned* real_capacity)
/*
 * Calculate memory size for extra data.
 */
{
    unsigned header_size;
    if (_likely_(desired_capacity <= 256 - offsetof(struct _UwStringExtraData, str.cap8.data))) {
        header_size = offsetof(struct _UwStringExtraData, str.cap8.data);
    } else if (_unlikely_(desired_capacity <= 65536 - offsetof(struct _UwStringExtraData, str.cap16.data))) {
        header_size = offsetof(struct _UwStringExtraData, str.cap16.data);
    } else {
        header_size = offsetof(struct _UwStringExtraData, str.capU.data);
    }
    unsigned size = header_size + char_size * desired_capacity + UWSTRING_BLOCK_SIZE - 1;
    size &= ~(UWSTRING_BLOCK_SIZE - 1);
    if (real_capacity) {
        *real_capacity = (size - header_size) / char_size;
    }
    return size;
}

static inline unsigned get_extra_data_size(UwValuePtr str)
/*
 * Get memory size occupied by extra data.
 */
{
    return calc_extra_data_size(_uw_string_char_size(str), _uw_string_capacity(str), nullptr);
}

static bool make_empty_string(UwValuePtr result, unsigned capacity, uint8_t char_size)
/*
 * Create empty string with desired parameters.
 * Result type_id must be set before calling this function, other fields are assumed undefined.
 * Return false if OOM.
 */
{
    result->str_embedded_length = 0;

    // check if string can be embedded into result

    if (capacity <= get_embedded_capacity(char_size)) {
        result->str_embedded = 1;
        result->str_embedded_char_size = char_size - 1;  // char_size is stored as 0-based
        result->str_4[0] = 0;
        result->str_4[1] = 0;
        result->str_4[2] = 0;
        return true;
    }

    if(capacity > _max_capacity[char_size]) {
        return false;
    }

    result->str_embedded = 0;

    // allocate string

    unsigned real_capacity;
    unsigned memsize = calc_extra_data_size(char_size, capacity, &real_capacity);

    UwType* t = _uw_types[result->type_id];

    result->extra_data = t->allocator->allocate(memsize, true);
    if (!result->extra_data) {
        return false;
    }
    result->extra_data->refcount = 1;

    string_struct(result).cap_size = calc_cap_size(real_capacity);
    string_struct(result).char_size = char_size - 1;  // char_size is stored as 0-based

    _uw_string_set_caplen(result, real_capacity, 0);
    return true;
}

static bool expand_string(UwValuePtr str, unsigned increment, uint8_t new_char_size)
/*
 * Expand string in place, if necessary, replacing `str->extra_data`.
 *
 * If string refcount is greater than 1, always make a copy of `str->extra_data`
 * because the string is about to be updated.
 */
{
    uw_assert_string(str);
    uint8_t char_size = _uw_string_char_size(str);
    if (str->str_embedded) {
        if (new_char_size < char_size) {
            if (increment == 0) {
                // no need to expand
                return true;
            }
            // current char_size is greater than new one, use current as new:
            new_char_size = char_size;
        }
        if (increment > _max_capacity[new_char_size] - str->str_embedded_length) {
            return false;
        }
        unsigned new_length = str->str_embedded_length + increment;
        if (new_length <= get_embedded_capacity(new_char_size)) {
            // no need to expand
            if (new_char_size > char_size) {
                // but need to make existing chars wider
                _UwValue orig_str = *str;
                str->str_embedded_char_size = new_char_size - 1;  // char_size is stored as 0-based
                get_str_methods(&orig_str)->copy_to(
                    _uw_string_char_ptr(&orig_str, 0),
                    str, 0, orig_str.str_embedded_length
                );
            }
            return true;
        }
        goto copy_string;
    }

    if (str->extra_data->refcount > 1) {
        // always make a copy before modification of shared string data
        // XXX handling refcount this way is extremely not thread safe
        str->extra_data->refcount--;
        goto copy_string;

    } else {
        uw_assert(str->extra_data->refcount == 1);

        // refcount is 1, check if string needs expanding

        if (new_char_size > char_size) {
            // copy string if char size needs to increase
            str->extra_data->refcount = 0; // make refcount zero to free original data after copy
            goto copy_string;
        }

        unsigned length = _uw_string_length(str);

        if (increment > _max_capacity[new_char_size] - length) {
            return false;
        }

        unsigned new_length = length + increment;
        unsigned capacity = _uw_string_capacity(str);

        if (new_length <= capacity) {
            // no need to expand
            return true;
        }

        unsigned orig_memsize = get_extra_data_size(str);
        unsigned new_capacity;
        unsigned new_memsize = calc_extra_data_size(char_size, new_length, &new_capacity);

        uint8_t new_cap_size = calc_cap_size(new_capacity);
        if (new_cap_size > string_struct(str).cap_size) {
            // when cap_size changes, reallocating would require data move
            // it's easier and less error-prone to copy string
            str->extra_data->refcount = 0; // make refcount zero to free original data after copy
            goto copy_string;
        }

        // reallocate data

        if (!_uw_types[str->type_id]->allocator->reallocate((void**) &str->extra_data,
                                                            orig_memsize, new_memsize, true, nullptr)) {
            return false;
        }
        _uw_string_set_caplen(str, new_capacity, length);
        return true;
    }

copy_string: {

        _UwValue orig_str = *str;
        unsigned length = _uw_string_length(str);
        unsigned capacity = _uw_string_capacity(str);

        if (increment > _max_capacity[new_char_size] - length) {
            // cannot expand
            // restore refcount of the original string
            if (!str->str_embedded) {
                str->extra_data->refcount++;
            }
            return false;
        }

        unsigned new_capacity = length + increment;
        if (new_capacity < capacity) {
            new_capacity = capacity;
        }

        if (new_char_size < char_size) {
            new_char_size = char_size;
        }

        // allocate string
        if (!make_empty_string(str, new_capacity, new_char_size)) {
            // restore refcount of the original string
            if (!str->str_embedded) {
                str->extra_data->refcount++;
            }
            return false;
        }
        // copy original string to new string
        get_str_methods(&orig_str)->copy_to(_uw_string_char_ptr(&orig_str, 0), str, 0, length);
        _uw_string_set_length(str, length);

        // free saved string if not embedded and reference count is zero
        if (!orig_str.str_embedded && orig_str.extra_data->refcount == 0) {
            _uw_types[orig_str.type_id]->allocator->release((void**) &orig_str.extra_data, get_extra_data_size(&orig_str));
        }
        return true;
    }
}

/****************************************************************
 * Basic interface methods
 */

static UwResult string_create(UwTypeId type_id, va_list ap)
{
    UwValuePtr v = va_arg(ap, UwValuePtr);
    UwValue result = uw_clone(v);  // this converts CharPtr to string
    if (!uw_is_string(&result)) {
        return UwError(UW_ERROR_INCOMPATIBLE_TYPE);
    } else {
        return uw_move(&result);
    }
}

static void string_destroy(UwValuePtr self)
{
    if (self->str_embedded) {
        return;
    }
    if (0 == --self->extra_data->refcount) {

        UwType* t = _uw_types[self->type_id];
        t->allocator->release((void**) &self->extra_data, get_extra_data_size(self));
    }
}

static UwResult string_clone(UwValuePtr self)
{
    UwValue result = *self;
    if (!result.str_embedded) {
        if (result.extra_data) {
            result.extra_data->refcount++;
        }
    }
    return uw_move(&result);
}

static UwResult string_deepcopy(UwValuePtr self)
{
    UwValue result = UwString();
    unsigned length = _uw_string_length(self);
    if (!make_empty_string(&result, length, _uw_string_char_size(self))) {
        return UwOOM();
    }
    get_str_methods(self)->copy_to(_uw_string_char_ptr(self, 0), &result, 0, length);
    _uw_string_set_length(&result, length);
    return uw_move(&result);
}

static void string_hash(UwValuePtr self, UwHashContext* ctx)
{
    // mind maps: the hash should be the same for subtypes, that's why not using self->type_id here
    _uw_hash_uint64(ctx, UwTypeId_String);

    unsigned length = _uw_string_length(self);
    if (length) {
        get_str_methods(self)->hash(_uw_string_char_ptr(self, 0), length, ctx);
    }
}

static void string_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    _uw_string_dump_data(fp, self, next_indent);
}

void _uw_string_dump_data(FILE* fp, UwValuePtr str, int indent)
{
    if (str->str_embedded) {
        fprintf(fp, " embedded,");
    } else {
        fprintf(fp, " data=%p, refcount=%u, cap_size=%u, data size=%u, ptr=%p",
                str->extra_data, str->extra_data->refcount, string_struct(str).cap_size,
                get_extra_data_size(str), _uw_string_char_ptr(str, 0));
    }

    unsigned capacity = _uw_string_capacity(str);
    unsigned length = _uw_string_length(str);
    uint8_t char_size = _uw_string_char_size(str);

    fprintf(fp, " length=%u, capacity=%u, char size=%u\n", length, capacity, char_size);
    indent += 4;

    if (length) {
        // print first 80 characters
        _uw_print_indent(fp, indent);
        StrMethods* strmeth = get_str_methods(str);
        for(unsigned i = 0; i < length; i++) {
            _uw_putchar32_utf8(fp, strmeth->get_char(_uw_string_char_ptr(str, i)));
            if (i == 80) {
                fprintf(fp, "...");
                break;
            }
        }
        fputc('\n', fp);

        uint8_t* addr = _uw_string_char_ptr(str, 0);
        dump_hex(fp, indent, addr, length * char_size, (uint8_t*) (((ptrdiff_t) addr) & 15), true, true);
        if (length < capacity) {
            _uw_print_indent(fp, indent);
            fputs("capacity remainder:\n", fp);
            uint8_t* rem_addr = _uw_string_char_ptr(str, length);
            dump_hex(fp, indent, rem_addr, (capacity - length) * char_size,
                     (uint8_t*) ((((ptrdiff_t) addr) & 15) + length * char_size), true, true);
        }
    }
}

static bool string_is_true(UwValuePtr self)
{
    return _uw_string_length(self);
}

static inline bool _uw_string_eq(UwValuePtr a, UwValuePtr b)
{
    if (a == b) {
        return true;
    }
    unsigned a_length = _uw_string_length(a);
    unsigned b_length = _uw_string_length(b);
    if (a_length != b_length) {
        return false;
    }
    if (a_length == 0) {
        return true;
    }
    return get_str_methods(a)->equal(_uw_string_char_ptr(a, 0), b, 0, a_length);
}

static bool string_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return _uw_string_eq(self, other);
}

static bool string_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        switch (t) {
            case UwTypeId_String:
                return _uw_string_eq(self, other);

            case UwTypeId_CharPtr:
                return _uw_charptr_equal_string(other, self);

            default: {
                // check base type
                t = _uw_types[t]->ancestor_id;
                if (t == UwTypeId_Null) {
                    return false;
                }
            }
        }
    }
}

UwType _uw_string_type = {
    .id              = UwTypeId_String,
    .ancestor_id     = UwTypeId_Null,  // no ancestor
    .name            = "String",
    .allocator       = &default_allocator,
    .data_offset     = sizeof(_UwExtraData),
    .data_size       = 0,
    .compound        = false,
    ._create         = string_create,
    ._destroy        = string_destroy,
    ._init           = nullptr,       // custom constructor performs all the initialization
    ._fini           = nullptr,       // there's nothing to finalize, just run custom destructor
    ._clone          = string_clone,
    ._hash           = string_hash,
    ._deepcopy       = string_deepcopy,
    ._dump           = string_dump,
    ._to_string      = string_deepcopy,  // yes, simply make a copy
    ._is_true        = string_is_true,
    ._equal_sametype = string_equal_sametype,
    ._equal          = string_equal,

    .num_interfaces  = 0,
    .interfaces      = nullptr
        // [UwInterfaceId_RandomAccess] = &string_type_random_access_interface
};

/****************************************************************
 * UTF-8/UTF-32 functions
 */

char* uw_char32_to_utf8(char32_t codepoint, char* buffer)
{
    /*
     * U+0000 - U+007F      0xxxxxxx
     * U+0080 - U+07FF      110xxxxx  10xxxxxx
     * U+0800 - U+FFFF      1110xxxx  10xxxxxx  10xxxxxx
     * U+010000 - U+10FFFF  11110xxx  10xxxxxx  10xxxxxx  10xxxxxx
     */
    if (codepoint < 0x80) {
        *buffer++ = (char) codepoint;
        return buffer;
    }
    if (codepoint < 0b1'00000'000000) {
        *buffer++ = (char) (0xC0 | (codepoint >> 6));
        *buffer++ = (char) (0x80 | (codepoint & 0x3F));
        return buffer;
    }
    if (codepoint < 0b1'0000'000000'000000) {
        *buffer++ = (char) (0xE0 | (codepoint >> 12));
        *buffer++ = (char) (0x80 | ((codepoint >> 6) & 0x3F));
        *buffer++ = (char) (0x80 | (codepoint & 0x3F));
        return buffer;
    }
    *buffer++ = (char) (0xF0 | ((codepoint >> 18) & 0x07));
    *buffer++ = (char) (0x80 | ((codepoint >> 12) & 0x3F));
    *buffer++ = (char) (0x80 | ((codepoint >> 6) & 0x3F));
    *buffer++ = (char) (0x80 | (codepoint & 0x3F));
    return buffer;
}

static inline bool read_utf8_buffer(char8_t** ptr, unsigned* bytes_remaining, char32_t* codepoint)
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
    unsigned remaining = *bytes_remaining;
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

unsigned utf8_strlen(char8_t* str)
{
    unsigned length = 0;
    while(_likely_(*str != 0)) {
        char32_t c = read_utf8_char(&str);
        if (_likely_(c != 0xFFFFFFFF)) {
            length++;
        }
    }
    return length;
}

unsigned utf8_strlen2(char8_t* str, uint8_t* char_size)
{
    unsigned length = 0;
    uint8_t  width = 0;
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

unsigned utf8_strlen2_buf(char8_t* buffer, unsigned* size, uint8_t* char_size)
{
    char8_t* ptr = buffer;
    unsigned bytes_remaining = *size;
    unsigned length = 0;
    uint8_t  width = 0;

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

uint8_t utf8_char_size(char8_t* str, unsigned max_len)
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

char8_t* utf8_skip(char8_t* str, unsigned n)
{
    while(n--) {
        read_utf8_char(&str);
        if (_unlikely_(*str == 0)) {
            break;
        }
    }
    return str;
}

unsigned uw_strlen_in_utf8(UwValuePtr str)
{
    uw_assert_string(str);
    uint8_t char_size = _uw_string_char_size(str);

    StrMethods* strmeth = get_str_methods(str);
    char8_t* ptr = _uw_string_char_ptr(str, 0);

    unsigned length = 0;
    unsigned n = _uw_string_length(str);
    while (n) {
        char32_t c = strmeth->get_char(ptr);
        if (c < 0x80) {
            length++;
        } else if (c < 0b1'00000'000000) {
            length += 2;
        } else if (c < 0b1'0000'000000'000000) {
            length += 3;
        } else {
            length += 4;
        }
        ptr += char_size;
        n--;
    }
    return length;
}

void _uw_putchar32_utf8(FILE* fp, char32_t codepoint)
{
    char buffer[5];
    char* start = buffer;
    char* end = uw_char32_to_utf8(codepoint, buffer);
    while (start < end) {
        fputc(*start++, fp);
    }
}

unsigned u32_strlen(char32_t* str)
{
    unsigned length = 0;
    while (_likely_(*str++)) {
        length++;
    }
    return length;
}

unsigned u32_strlen2(char32_t* str, uint8_t* char_size)
{
    unsigned length = 0;
    uint8_t width = 0;
    char32_t c;
    while (_likely_(c = *str++)) {
        width = update_char_width(width, c);
        length++;
    }
    *char_size = char_width_to_char_size(width);
    return length;
}

int u32_strcmp(char32_t* a, char32_t* b)
{
    if (a == b) {
        return 0;
    }
    for (;;) {
        char32_t ca = *a++;
        char32_t cb = *b++;
        if (ca < cb) {
            return -1;
        } else if (ca > cb) {
            return 1;
        } else if (ca == 0) {
            return 0;
        }
    }
}

int u32_strcmp_cstr(char32_t* a, char* b)
{
    for (;;) {
        char32_t ca = *a++;
        unsigned char cb = *b++;
        if (ca < cb) {
            return -1;
        } else if (ca > cb) {
            return 1;
        } else if (ca == 0) {
            return 0;
        }
    }
}

int u32_strcmp_u8(char32_t* a, char8_t* b)
{
    for (;;) {
        char32_t ca = *a++;
        char32_t cb = read_utf8_char(&b);
        if (ca < cb) {
            return -1;
        } else if (ca > cb) {
            return 1;
        } else if (ca == 0) {
            return 0;
        }
    }
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

uint8_t u32_char_size(char32_t* str, unsigned max_len)
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
 * Methods that depend on char_size field.
 */

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
    static void _hash_##type_name(uint8_t* self_ptr, unsigned length, UwHashContext* ctx) \
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

static void _hash_uint24_t(uint8_t* self_ptr, unsigned length, UwHashContext* ctx)
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

static uint8_t _max_char_size_uint8_t(uint8_t* self_ptr, unsigned length)
{
    return 1;
}

static uint8_t _max_char_size_uint16_t(uint8_t* self_ptr, unsigned length)
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

static uint8_t _max_char_size_uint24_t(uint8_t* self_ptr, unsigned length)
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

static uint8_t _max_char_size_uint32_t(uint8_t* self_ptr, unsigned length)
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
    static inline bool eq_##type_name##_##type_name(uint8_t* self_ptr, type_name* other_ptr, unsigned length)  \
    {  \
        return memcmp(self_ptr, other_ptr, length * sizeof(type_name)) == 0;  \
    }

STR_EQ_MEMCMP_HELPER_IMPL(uint8_t)
STR_EQ_MEMCMP_HELPER_IMPL(uint16_t)
STR_EQ_MEMCMP_HELPER_IMPL(uint24_t)
STR_EQ_MEMCMP_HELPER_IMPL(uint32_t)

// integral types:

#define STR_EQ_HELPER_IMPL(type_name_self, type_name_other)  \
    static inline bool eq_##type_name_self##_##type_name_other(uint8_t* self_ptr, type_name_other* other_ptr, unsigned length)  \
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
    static inline bool eq_uint24_t_##type_name_other(uint8_t* self_ptr, type_name_other* other_ptr, unsigned length)  \
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
    static inline bool eq_##type_name_self##_uint24_t(uint8_t* self_ptr, uint24_t* other_ptr, unsigned length)  \
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
    static bool _eq_##type_name_self(uint8_t* self_ptr, UwValuePtr other, unsigned other_start_pos, unsigned length)  \
    {  \
        uint8_t* other_ptr = _uw_string_char_ptr(other, other_start_pos);  \
        switch (_uw_string_char_size(other)) {  \
            case 1: return eq_##type_name_self##_uint8_t(self_ptr, (uint8_t*) other_ptr, length);  \
            case 2: return eq_##type_name_self##_uint16_t(self_ptr, (uint16_t*) other_ptr, length);  \
            case 3: return eq_##type_name_self##_uint24_t(self_ptr, (uint24_t*) other_ptr, length);  \
            case 4: return eq_##type_name_self##_uint32_t(self_ptr, (uint32_t*) other_ptr, length);  \
            default: return false;  \
        }  \
    }

STR_EQ_IMPL(uint8_t)
STR_EQ_IMPL(uint16_t)
STR_EQ_IMPL(uint24_t)
STR_EQ_IMPL(uint32_t)

// comparison with C/char32_t null-terminated string, integral `self` types

#define STR_EQ_CU32_IMPL(type_name_self, type_name_other)  \
    static bool _eq_##type_name_self##_##type_name_other(uint8_t* self_ptr, type_name_other* other, unsigned length)  \
    {  \
        type_name_self* this_ptr = (type_name_self*) self_ptr;  \
        while (_likely_(length--)) {  \
            type_name_other c = *other++;  \
            if (_unlikely_(c == 0)) {  \
                return false;  \
            }  \
            if (_unlikely_(((type_name_other) (*this_ptr++)) != c)) {  \
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
    static bool _eq_uint24_t_##type_name_other(uint8_t* self_ptr, type_name_other* other, unsigned length)  \
    {  \
        while (_likely_(length--)) {  \
            type_name_other c = *other++;  \
            if (_unlikely_(c == 0)) {  \
                return false;  \
            }  \
            if (_unlikely_(((type_name_other) get_char_uint24_t((uint24_t**) &self_ptr)) != c)) {  \
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
    static bool _eq_##type_name_self##_char8_t(uint8_t* self_ptr, char8_t* other, unsigned length)  \
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

static bool _eq_uint24_t_char8_t(uint8_t* self_ptr, char8_t* other, unsigned length)
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
    static inline void cp_##type_name##_##type_name(uint8_t* self_ptr, type_name* dest_ptr, unsigned length)  \
    {  \
        memcpy(dest_ptr, self_ptr, length * sizeof(type_name));  \
    }  \

STR_COPY_TO_MEMCPY_HELPER_IMPL(uint8_t)
STR_COPY_TO_MEMCPY_HELPER_IMPL(uint16_t)
STR_COPY_TO_MEMCPY_HELPER_IMPL(uint24_t)
STR_COPY_TO_MEMCPY_HELPER_IMPL(uint32_t)

// integral types:

#define STR_COPY_TO_HELPER_IMPL(type_name_self, type_name_dest)  \
    static inline void cp_##type_name_self##_##type_name_dest(uint8_t* self_ptr, type_name_dest* dest_ptr, unsigned length)  \
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
    static inline void cp_uint24_t_##type_name_dest(uint8_t* self_ptr, type_name_dest* dest_ptr, unsigned length)  \
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
    static inline void cp_##type_name_self##_uint24_t(uint8_t* self_ptr, uint24_t* dest_ptr, unsigned length)  \
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
    static void _cp_to_##type_name_self(uint8_t* self_ptr, UwValuePtr dest, unsigned dest_start_pos, unsigned length)  \
    {  \
        uint8_t* dest_ptr = _uw_string_char_ptr(dest, dest_start_pos);  \
        switch (_uw_string_char_size(dest)) {  \
            case 1: cp_##type_name_self##_uint8_t(self_ptr, (uint8_t*) dest_ptr, length); return; \
            case 2: cp_##type_name_self##_uint16_t(self_ptr, (uint16_t*) dest_ptr, length); return; \
            case 3: cp_##type_name_self##_uint24_t(self_ptr, (uint24_t*) dest_ptr, length); return; \
            case 4: cp_##type_name_self##_uint32_t(self_ptr, (uint32_t*) dest_ptr, length); return; \
        }  \
    }

STR_COPY_TO_IMPL(uint8_t)
STR_COPY_TO_IMPL(uint16_t)
STR_COPY_TO_IMPL(uint24_t)
STR_COPY_TO_IMPL(uint32_t)

// copy to C-string

static void _cp_to_u8_uint8_t(uint8_t* self_ptr, char* dest, unsigned length)
{
    memcpy(dest, self_ptr, length);
    *(dest + length) = 0;
}

// integral types:

#define STR_COPY_TO_U8_IMPL(type_name_self)  \
    static void _cp_to_u8_##type_name_self(uint8_t* self_ptr, char* dest, unsigned length)  \
    {  \
        type_name_self* src_ptr = (type_name_self*) self_ptr;  \
        while (_likely_(length--)) {  \
            dest = uw_char32_to_utf8(*src_ptr++, dest); \
        }  \
        *dest = 0;  \
    }

STR_COPY_TO_U8_IMPL(uint16_t)
STR_COPY_TO_U8_IMPL(uint32_t)

// uint24_t

static void _cp_to_u8_uint24_t(uint8_t* self_ptr, char* dest, unsigned length)
{
    while (_likely_(length--)) {
        char32_t c = get_char_uint24_t((uint24_t**) &self_ptr);
        dest = uw_char32_to_utf8(c, dest);
    }
    *dest = 0;
}

// copy from C/char32_t string, integral `self` types

#define STR_CP_FROM_CSTR_IMPL(type_name_src, type_name_self)  \
    static unsigned _cp_from_##type_name_src##_##type_name_self(uint8_t* self_ptr, type_name_src* src_ptr, unsigned length)  \
    {  \
        type_name_self* dest_ptr = (type_name_self*) self_ptr;  \
        unsigned chars_copied = 0;  \
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
    static unsigned _cp_from_##type_name_src##_uint24_t(uint8_t* self_ptr, type_name_src* src_ptr, unsigned length)  \
    {  \
        unsigned chars_copied = 0;  \
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
    static unsigned _cp_from_u8_##type_name_self(uint8_t* self_ptr, char8_t* src_ptr, unsigned length)  \
    {  \
        type_name_self* dest_ptr = (type_name_self*) self_ptr;  \
        unsigned chars_copied = 0;  \
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

static unsigned _cp_from_u8_uint24_t(uint8_t* self_ptr, uint8_t* src_ptr, unsigned length)
{
    unsigned chars_copied = 0;
    while (_likely_((*((uint8_t*) src_ptr)) && length--)) {
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
StrMethods _uws_str_methods[4] = {
    { _get_char_uint8_t,      _put_char_uint8_t,    _hash_uint8_t,             _max_char_size_uint8_t,
      _eq_uint8_t,            _eq_uint8_t_char,     _eq_uint8_t_char8_t,       _eq_uint8_t_char32_t,
      _cp_to_uint8_t,         _cp_to_u8_uint8_t,
      _cp_from_char_uint8_t,  _cp_from_u8_uint8_t,  _cp_from_char32_t_uint8_t
    },
    { _get_char_uint16_t,     _put_char_uint16_t,   _hash_uint16_t,            _max_char_size_uint16_t,
      _eq_uint16_t,           _eq_uint16_t_char,    _eq_uint16_t_char8_t,      _eq_uint16_t_char32_t,
      _cp_to_uint16_t,        _cp_to_u8_uint16_t,
      _cp_from_char_uint16_t, _cp_from_u8_uint16_t, _cp_from_char32_t_uint16_t
    },
    { _get_char_uint24_t,     _put_char_uint24_t,   _hash_uint24_t,            _max_char_size_uint24_t,
      _eq_uint24_t,           _eq_uint24_t_char,    _eq_uint24_t_char8_t,      _eq_uint24_t_char32_t,
      _cp_to_uint24_t,        _cp_to_u8_uint24_t,
      _cp_from_char_uint24_t, _cp_from_u8_uint24_t, _cp_from_char32_t_uint24_t
    },
    { _get_char_uint32_t,     _put_char_uint32_t,   _hash_uint32_t,            _max_char_size_uint32_t,
      _eq_uint32_t,           _eq_uint32_t_char,    _eq_uint32_t_char8_t,      _eq_uint32_t_char32_t,
      _cp_to_uint32_t,        _cp_to_u8_uint32_t,
      _cp_from_char_uint32_t, _cp_from_u8_uint32_t, _cp_from_char32_t_uint32_t
    }
};


/****************************************************************
 * Constructors
 */

UwResult uw_create_empty_string(unsigned capacity, uint8_t char_size)
{
    // using not autocleaned variable here, no uw_move necessary on exit
    __UWDECL_String(result);

    if (!make_empty_string(&result, capacity, char_size)) {
        return UwOOM();
    }
    return result;
}

UwResult uw_create_string_cstr(char* initializer)
{
    unsigned length = 0;

    if (initializer && *initializer == 0) {
        initializer = nullptr;
    } else {
        length = strlen(initializer);
    }

    // using not autocleaned variable here, no uw_move necessary on exit
    __UWDECL_String(result);

    if (!make_empty_string(&result, length, 1)) {
        return UwOOM();
    }
    if (initializer) {
        get_str_methods(&result)->copy_from_cstr(_uw_string_char_ptr(&result, 0), initializer, length);
        _uw_string_set_length(&result, length);
    }
    return result;
}

UwResult _uw_create_string_u8(char8_t* initializer)
{
    unsigned length = 0;
    uint8_t char_size = 1;

    if (initializer && *initializer == 0) {
        initializer = nullptr;
    } else {
        length = utf8_strlen2(initializer, &char_size);
    }

    // using not autocleaned variable here, no uw_move necessary on exit
    __UWDECL_String(result);

    if (!make_empty_string(&result, length, char_size)) {
        return UwOOM();
    }
    if (initializer) {
        get_str_methods(&result)->copy_from_utf8(_uw_string_char_ptr(&result, 0), initializer, length);
        _uw_string_set_length(&result, length);
    }
    return result;
}

UwResult _uw_create_string_u32(char32_t* initializer)
{
    unsigned length = 0;
    uint8_t char_size = 1;

    if (initializer && *initializer == 0) {
        initializer = nullptr;
    } else {
        length = u32_strlen2(initializer, &char_size);
    }

    // using not autocleaned variable here, no uw_move necessary on exit
    __UWDECL_String(result);

    if (!make_empty_string(&result, length, char_size)) {
        return UwOOM();
    }
    if (initializer) {
        get_str_methods(&result)->copy_from_utf32(_uw_string_char_ptr(&result, 0), initializer, length);
        _uw_string_set_length(&result, length);
    }
    return result;
}

/****************************************************************
 * String functions
 */

uint8_t uw_string_char_size(UwValuePtr str)
{
    uw_assert_string(str);
    return _uw_string_char_size(str);
}

unsigned uw_strlen(UwValuePtr str)
{
    uw_assert_string(str);
    return _uw_string_length(str);
}

#define STRING_EQ_IMPL(suffix, type_name_b)  \
{  \
    uw_assert_string(a);  \
    return get_str_methods(a)->equal_##suffix(_uw_string_char_ptr(a, 0), (type_name_b*) b, _uw_string_length(a));  \
}

bool  uw_equal_cstr(UwValuePtr a, char* b)     STRING_EQ_IMPL(cstr, char)
bool _uw_equal_u8  (UwValuePtr a, char8_t* b)  STRING_EQ_IMPL(u8,   char8_t)
bool _uw_equal_u32 (UwValuePtr a, char32_t* b) STRING_EQ_IMPL(u32,  char32_t)

#define SUBSTRING_EQ_IMPL(suffix, type_name_b)  \
{  \
    uw_assert_string(a);  \
    unsigned a_length = _uw_string_length(a);  \
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
    return get_str_methods(a)->equal_##suffix(_uw_string_char_ptr(a, start_pos), b, end_pos - start_pos);  \
}

bool  uw_substring_eq_cstr(UwValuePtr a, unsigned start_pos, unsigned end_pos, char*     b) SUBSTRING_EQ_IMPL(cstr, char)
bool _uw_substring_eq_u8  (UwValuePtr a, unsigned start_pos, unsigned end_pos, char8_t*  b) SUBSTRING_EQ_IMPL(u8,   char8_t)
bool _uw_substring_eq_u32 (UwValuePtr a, unsigned start_pos, unsigned end_pos, char32_t* b) SUBSTRING_EQ_IMPL(u32,  char32_t)

bool _uw_substring_eq(UwValuePtr a, unsigned start_pos, unsigned end_pos, UwValuePtr b)
{
    uw_assert_string(a);
    unsigned a_length = _uw_string_length(a);

    if (end_pos > a_length) {
        end_pos = a_length;
    }
    if (end_pos < start_pos) {
        return false;
    }

    uw_assert_string(b);

    if (end_pos == start_pos && _uw_string_length(b) == 0) {
        return true;
    }

    return get_str_methods(a)->equal(_uw_string_char_ptr(a, start_pos), b, 0, end_pos - start_pos);
}

CStringPtr uw_string_to_cstring(UwValuePtr str)
{
    uw_assert_string(str);
    unsigned length = _uw_string_length(str);

    CStringPtr result = malloc(length + 1);
    if (!result) {
        return nullptr;
    }
    get_str_methods(str)->copy_to_u8(_uw_string_char_ptr(str, 0), result, length);
    return result;
}

void uw_string_copy_buf(UwValuePtr str, char* buffer)
{
    uw_assert_string(str);
    unsigned length = _uw_string_length(str);
    get_str_methods(str)->copy_to_u8(_uw_string_char_ptr(str, 0), buffer, length);
}

void uw_string_substr_buf(UwValuePtr str, unsigned start_pos, unsigned end_pos, char* buffer)
{
    uw_assert_string(str);
    unsigned length = _uw_string_length(str);
    if (end_pos >= length) {
        end_pos = length;
    }
    if (end_pos <= start_pos) {
        *buffer = 0;
        return;
    }
    get_str_methods(str)->copy_to_u8(_uw_string_char_ptr(str, start_pos), buffer, end_pos - start_pos);
}

void uw_destroy_cstring(CStringPtr* str)
{
    free(*str);
    *str = nullptr;
}

bool _uw_string_append_char(UwValuePtr dest, char c)
{
    if (!expand_string(dest, 1, 1)) {
        return false;
    }
    unsigned length = _uw_string_inc_length(dest, 1);
    get_str_methods(dest)->put_char(_uw_string_char_ptr(dest, length), (unsigned char) c);
    return true;
}

bool _uw_string_append_c32(UwValuePtr dest, char32_t c)
{
    if (!expand_string(dest, 1, calc_char_size(c))) {
        return false;
    }
    unsigned length = _uw_string_inc_length(dest, 1);
    get_str_methods(dest)->put_char(_uw_string_char_ptr(dest, length), c);
    return true;
}

static bool append_cstr(UwValuePtr dest, char* src, unsigned src_len)
{
    if (!expand_string(dest, src_len, 1)) {
        return false;
    }
    unsigned dest_length = _uw_string_inc_length(dest, src_len);
    get_str_methods(dest)->copy_from_cstr(_uw_string_char_ptr(dest, dest_length), src, src_len);
    return true;
}

bool uw_string_append_cstr(UwValuePtr dest, char* src)
{
    return append_cstr(dest, src, strlen(src));
}

bool uw_string_append_substring_cstr(UwValuePtr dest, char* src, unsigned src_start_pos, unsigned src_end_pos)
{
    unsigned src_len = strlen(src);
    if (src_end_pos > src_len) {
        src_end_pos = src_len;
    }
    if (src_start_pos >= src_end_pos) {
        return true;
    }
    src_len = src_end_pos - src_start_pos;
    src += src_start_pos;

    return append_cstr(dest, src, src_len);
}

static bool append_u8(UwValuePtr dest, char8_t* src, unsigned src_len, uint8_t src_char_size)
/*
 * `src_len` contains the number of codepoints, not the number of bytes in `src`
 */
{
    if (!expand_string(dest, src_len, src_char_size)) {
        return false;
    }
    unsigned dest_length = _uw_string_inc_length(dest, src_len);
    get_str_methods(dest)->copy_from_utf8(_uw_string_char_ptr(dest, dest_length), src, src_len);
    return true;
}

bool _uw_string_append_u8(UwValuePtr dest, char8_t* src)
{
    uint8_t src_char_size;
    unsigned src_len = utf8_strlen2(src, &src_char_size);
    return append_u8(dest, src, src_len, src_char_size);
}

bool _uw_string_append_substring_u8(UwValuePtr dest, char8_t* src, unsigned src_start_pos, unsigned src_end_pos)
{
    uint8_t src_char_size;
    unsigned src_len = utf8_strlen2(src, &src_char_size);
    if (src_end_pos > src_len) {
        src_end_pos = src_len;
    }
    if (src_start_pos >= src_end_pos) {
        return true;
    }
    src_len = src_end_pos - src_start_pos;
    src = utf8_skip(src, src_start_pos);

    return append_u8(dest, src, src_len, src_char_size);
}

static bool append_u32(UwValuePtr dest, char32_t* src, unsigned src_len, uint8_t src_char_size)
{
    if (!expand_string(dest, src_len, src_char_size)) {
        return false;
    }
    unsigned dest_length = _uw_string_inc_length(dest, src_len);
    get_str_methods(dest)->copy_from_utf32(_uw_string_char_ptr(dest, dest_length), src, src_len);
    return true;
}

bool _uw_string_append_u32(UwValuePtr dest, char32_t* src)
{
    uint8_t src_char_size;
    unsigned src_len = u32_strlen2(src, &src_char_size);
    return append_u32(dest, src, src_len, src_char_size);
}

bool _uw_string_append_substring_u32(UwValuePtr dest, char32_t*  src, unsigned src_start_pos, unsigned src_end_pos)
{
    uint8_t src_char_size;
    unsigned src_len = u32_strlen2(src, &src_char_size);
    if (src_end_pos > src_len) {
        src_end_pos = src_len;
    }
    if (src_start_pos >= src_end_pos) {
        return true;
    }
    src_len = src_end_pos - src_start_pos;
    src += src_start_pos;

    return append_u32(dest, src, src_len, src_char_size);
}

static bool append_string(UwValuePtr dest, UwValuePtr src, unsigned src_start_pos, unsigned src_len)
{
    if (!expand_string(dest, src_len, _uw_string_char_size(src))) {
        return false;
    }
    unsigned dest_length = _uw_string_inc_length(dest, src_len);
    get_str_methods(src)->copy_to(_uw_string_char_ptr(src, src_start_pos), dest, dest_length, src_len);
    return true;
}

bool _uw_string_append(UwValuePtr dest, UwValuePtr src)
{
    uw_assert_string(src);
    unsigned src_len = _uw_string_length(src);
    return append_string(dest, src, 0, src_len);
}

bool _uw_string_append_substring(UwValuePtr dest, UwValuePtr src, unsigned src_start_pos, unsigned src_end_pos)
{
    uw_assert_string(src);

    unsigned src_len = _uw_string_length(src);
    if (src_end_pos > src_len) {
        src_end_pos = src_len;
    }
    if (src_start_pos >= src_end_pos) {
        return true;
    }
    src_len = src_end_pos - src_start_pos;

    return append_string(dest, src, src_start_pos, src_len);
}

bool uw_string_append_utf8(UwValuePtr dest, char8_t* buffer, unsigned size, unsigned* bytes_processed)
{
    if (size == 0) {
        return true;
    }
    uint8_t src_char_size;
    *bytes_processed = size;
    unsigned src_len = utf8_strlen2_buf(buffer, bytes_processed, &src_char_size);

    if (src_len) {
        if (!expand_string(dest, src_len, src_char_size)) {
            return false;
        }
        unsigned dest_length = _uw_string_inc_length(dest, src_len);
        get_str_methods(dest)->copy_from_utf8(_uw_string_char_ptr(dest, dest_length), buffer, src_len);
    }
    return true;
}

bool uw_string_append_buffer(UwValuePtr dest, uint8_t* buffer, unsigned size)
{
    if (size == 0) {
        return true;
    }
    uw_assert(uw_string_char_size(dest) == 1);

    if (!expand_string(dest, size, 1)) {
        return false;
    }
    StrMethods* strmeth = get_str_methods(dest);
    unsigned dest_length = _uw_string_inc_length(dest, size);
    uint8_t* ptr = _uw_string_char_ptr(dest, dest_length);
    while (size--) {
        strmeth->put_char(ptr++, *buffer++);
    }
    return true;
}

bool _uw_string_insert_many_c32(UwValuePtr str, unsigned position, char32_t chr, unsigned n)
{
    if (n == 0) {
        return true;
    }
    uw_assert(position <= uw_strlen(str));

    if (!expand_string(str, n, calc_char_size(chr))) {
        return false;
    }
    StrMethods* strmeth = get_str_methods(str);
    unsigned len = _uw_string_inc_length(str, n);
    uint8_t char_size = _uw_string_char_size(str);
    uint8_t* insertion_ptr = _uw_string_char_ptr(str, position);

    if (position < len) {
        memmove(insertion_ptr + n * char_size, insertion_ptr, (len - position) * char_size);
    }
    for (unsigned i = 0; i < n; i++) {
        strmeth->put_char(insertion_ptr, chr);
        insertion_ptr += char_size;
    }
    return true;
}

UwResult uw_string_get_substring(UwValuePtr str, unsigned start_pos, unsigned end_pos)
{
    uw_assert_string(str);
    StrMethods* strmeth = get_str_methods(str);

    unsigned length = _uw_string_length(str);

    if (end_pos > length) {
        end_pos = length;
    }
    if (start_pos >= end_pos) {
        return uw_create_empty_string(0, 1);
    }
    length = end_pos - start_pos;
    uint8_t* src = _uw_string_char_ptr(str, start_pos);
    uint8_t char_size = strmeth->max_char_size(src, length);

    UwValue result = uw_create_empty_string(length, char_size);
    if (uw_ok(&result)) {
        strmeth->copy_to(src, &result, 0, length);
        _uw_string_set_length(&result, length);
    }
    return result;
}

char32_t uw_string_at(UwValuePtr str, unsigned position)
{
    uw_assert_string(str);
    unsigned length = _uw_string_length(str);
    if (position < length) {
        return get_str_methods(str)->get_char(_uw_string_char_ptr(str, position));
    } else {
        return 0;
    }
}

bool uw_string_erase(UwValuePtr str, unsigned start_pos, unsigned end_pos)
{
    if (start_pos >= uw_strlen(str) || start_pos >= end_pos) {
        return true;
    }
    if (!expand_string(str, 0, 0)) {  // make copy if refcount > 1
        return false;
    }
    unsigned length = _uw_string_length(str);
    uint8_t char_size = _uw_string_char_size(str);

    if (end_pos >= length) {
        // truncate
        length = start_pos;
    } else {
        unsigned tail_len = length - end_pos;
        memmove(_uw_string_char_ptr(str, start_pos), _uw_string_char_ptr(str, end_pos), tail_len * char_size);
        length -= end_pos - start_pos;
    }
    _uw_string_set_length(str, length);
    return true;
}

bool uw_string_truncate(UwValuePtr str, unsigned position)
{
    if (position >= uw_strlen(str)) {
        return true;
    }
    if (!expand_string(str, 0, 0)) {  // make copy if refcount > 1
        return false;
    }
    _uw_string_set_length(str, position);
    return true;
}

bool uw_string_indexof(UwValuePtr str, char32_t chr, unsigned start_pos, unsigned* result)
{
    uw_assert_string(str);
    StrMethods* strmeth = get_str_methods(str);

    unsigned length = _uw_string_length(str);
    uint8_t* ptr = _uw_string_char_ptr(str, start_pos);
    uint8_t char_size = _uw_string_char_size(str);

    for (unsigned i = start_pos; i < length; i++) {
        char32_t codepoint = strmeth->get_char(ptr);
        if (codepoint == chr) {
            if (result) {
                *result = i;
            }
            return true;
        }
        ptr += char_size;
    }
    return false;
}

bool uw_string_ltrim(UwValuePtr str)
{
    uw_assert_string(str);
    StrMethods* strmeth = get_str_methods(str);
    unsigned len = _uw_string_length(str);
    uint8_t char_size = _uw_string_char_size(str);

    char8_t* ptr = _uw_string_char_ptr(str, 0);
    unsigned i = 0;
    while (i < len) {
        char32_t c = strmeth->get_char(ptr);
        if (!uw_char_isspace(c)) {
            break;
        }
        i++;
        ptr += char_size;
    }
    return uw_string_erase(str, 0, i);
}

bool uw_string_rtrim(UwValuePtr str)
{
    uw_assert_string(str);
    StrMethods* strmeth = get_str_methods(str);
    unsigned n = _uw_string_length(str);
    uint8_t char_size = _uw_string_char_size(str);

    char8_t* ptr = _uw_string_char_ptr(str, n);
    while (n) {
        ptr -= char_size;
        char32_t c = strmeth->get_char(ptr);
        if (!uw_char_isspace(c)) {
            break;
        }
        n--;
    }
    return uw_string_truncate(str, n);
}

bool uw_string_trim(UwValuePtr str)
{
    return uw_string_rtrim(str) && uw_string_ltrim(str);
}

bool uw_string_lower(UwValuePtr str)
{
    if (!expand_string(str, 0, 0)) {  // make copy if refcount > 1
        return false;
    }
    StrMethods* strmeth = get_str_methods(str);
    unsigned n = _uw_string_length(str);
    uint8_t char_size = _uw_string_char_size(str);

    char8_t* ptr = _uw_string_char_ptr(str, 0);
    while (n) {
        strmeth->put_char(ptr, uw_char_lower(strmeth->get_char(ptr)));
        ptr += char_size;
        n--;
    }
    return true;
}

bool uw_string_upper(UwValuePtr str)
{
    if (!expand_string(str, 0, 0)) {  // make copy if refcount > 1
        return false;
    }
    StrMethods* strmeth = get_str_methods(str);
    unsigned n = _uw_string_length(str);
    uint8_t char_size = _uw_string_char_size(str);

    char8_t* ptr = _uw_string_char_ptr(str, 0);
    while (n) {
        strmeth->put_char(ptr, uw_char_upper(strmeth->get_char(ptr)));
        ptr += char_size;
        n--;
    }
    return true;
}

UwResult uw_string_split_chr(UwValuePtr str, char32_t splitter)
{
    uw_assert_string(str);
    StrMethods* strmeth = get_str_methods(str);

    unsigned len = _uw_string_length(str);
    uint8_t char_size = _uw_string_char_size(str);

    UwValue result = UwList();
    if (uw_error(&result)) {
        return uw_move(&result);
    }

    char8_t* ptr = _uw_string_char_ptr(str, 0);
    char8_t* start = ptr;
    unsigned i = 0;
    unsigned start_i = 0;
    uint8_t substr_width = 0;

    while (i < len) {
        char32_t c = strmeth->get_char(ptr);
        if (c == splitter) {
            // create substring
            unsigned substr_len = i - start_i;
            UwValue substr = uw_create_empty_string(substr_len, char_width_to_char_size(substr_width));
            if (uw_error(&substr)) {
                return uw_move(&substr);
            }
            if (substr_len) {
                strmeth->copy_to(start, &substr, 0, substr_len);
                _uw_string_set_length(&substr, substr_len);
            }
            if (!uw_list_append(&result, &substr)) {
                return UwOOM();
            }

            start_i = i + 1;
            start = ptr + char_size;
            substr_width = 0;
        }
        i++;
        ptr += char_size;
        substr_width = update_char_width(substr_width, c);
    }
    // create final substring
    {
        unsigned substr_len = i - start_i;
        UwValue substr = uw_create_empty_string(substr_len, char_width_to_char_size(substr_width));
        if (uw_error(&substr)) {
            return uw_move(&substr);
        }
        if (substr_len) {
            strmeth->copy_to(start, &substr, 0, substr_len);
            _uw_string_set_length(&substr, substr_len);
        }
        if (!uw_list_append(&result, &substr)) {
            return UwOOM();
        }
    }
    return uw_move(&result);
}

UwResult _uw_string_join_c32(char32_t separator, UwValuePtr list)
{
    char32_t s[2] = {separator, 0};
    UwValue sep = UwChar32Ptr(s);
    return _uw_string_join(&sep, list);
}

UwResult _uw_string_join_u8(char8_t* separator, UwValuePtr list)
{
    UwValue sep = UwChar8Ptr(separator);
    return _uw_string_join(&sep, list);
}

UwResult _uw_string_join_u32(char32_t*  separator, UwValuePtr list)
{
    UwValue sep = UwChar32Ptr(separator);
    return _uw_string_join(&sep, list);
}

static bool append_charptr(UwValuePtr dest, UwValuePtr charptr, unsigned len, uint8_t char_size)
{
    switch (charptr->charptr_subtype) {
        case UW_CHARPTR:   return append_cstr(dest, charptr->charptr,   len);
        case UW_CHAR8PTR:  return append_u8  (dest, charptr->char8ptr,  len, char_size);
        case UW_CHAR32PTR: return append_u32 (dest, charptr->char32ptr, len, char_size);
        default: return true;  // XXX the caller must ensure charptr_subtype is correct
    }
}

UwResult _uw_string_join(UwValuePtr separator, UwValuePtr list)
{
    unsigned num_items = uw_list_length(list);
    if (num_items == 0) {
        return UwString();
    }
    if (num_items == 1) {
        UwValue item = uw_list_item(list, 0);
        if (uw_is_string(&item)) {
            return uw_move(&item);
        } if (uw_is_charptr(&item)) {
            return uw_charptr_to_string(&item);
        } else {
            // XXX skipping non-string values
            return UwString();
        }
    }

    uint8_t max_char_size;
    unsigned separator_len;
    bool separator_is_string = uw_is_string(separator);

    if (separator_is_string) {
        max_char_size = uw_string_char_size(separator);
        separator_len = uw_strlen(separator);
    } if (uw_is_charptr(separator)) {
        separator_len = _uw_charptr_strlen2(separator, &max_char_size);
    } else {
        UwValue error = UwError(UW_ERROR_INCOMPATIBLE_TYPE);
        _uw_set_status_desc(&error, "Bad separator type for uw_string_join: %u, %s",
                            separator->type_id, uw_get_type_name(separator->type_id));
        return uw_move(&error);
    }

    // calculate total length and max char width of string items
    unsigned num_charptrs = 0;
    unsigned result_len = 0;
    for (unsigned i = 0; i < num_items; i++) {
        {   // nested scope for autocleaning item
            UwValue item = uw_list_item(list, i);
            if (uw_is_string(&item)) {
                if (i) {
                    result_len += separator_len;
                }
                uint8_t char_size = uw_string_char_size(&item);
                if (max_char_size < char_size) {
                    max_char_size = char_size;
                }
                result_len += uw_strlen(&item);
            } else if (uw_is_charptr(&item)) {
                if (i) {
                    result_len += separator_len;
                }
                num_charptrs++;
            }
            // XXX skipping non-string values
        }
    }

    // can allocate array for CharPtr now
    unsigned charptr_len[num_charptrs + 1];

    if (num_charptrs) {
        // need one more pass to get lengths and char sizes of CharPtr items
        unsigned charptr_index = 0;
        for (unsigned i = 0; i < num_items; i++) {
            {   // nested scope for autocleaning item
                UwValue item = uw_list_item(list, i);
                if (uw_is_charptr(&item)) {
                    uint8_t char_size;
                    unsigned len = _uw_charptr_strlen2(&item, &char_size);

                    charptr_len[charptr_index++] = len;

                    result_len += len;
                    if (max_char_size < char_size) {
                        max_char_size = char_size;
                    }
                }
            }
        }
    }

    // join list items
    UwValue result = uw_create_empty_string(result_len, max_char_size);
    if (uw_error(&result)) {
        return uw_move(&result);
    }
    unsigned charptr_index = 0;
    for (unsigned i = 0; i < num_items; i++) {
        {   // nested scope for autocleaning item
            UwValue item = uw_list_item(list, i);
            bool is_string = uw_is_string(&item);
            if (is_string || uw_is_charptr(&item)) {
                if (i) {
                    if (separator_is_string) {
                        if (!_uw_string_append(&result, separator)) {
                            return UwOOM();
                        }
                    } else {
                        if (!append_charptr(&result, separator, separator_len, max_char_size)) {
                            return UwOOM();
                        }
                    }
                }
                if (is_string) {
                    if (!_uw_string_append(&result, &item)) {
                        return UwOOM();
                    }
                } else {
                    if (!append_charptr(&result, &item, charptr_len[charptr_index], max_char_size)) {
                        return UwOOM();
                    }
                    charptr_index++;
                }
            }
            // XXX skipping non-string values
        }
    }
    return uw_move(&result);
}

UwResult _uw_strcat_va(...)
{
    va_list ap;
    va_start(ap);
    UwValue result = uw_strcat_ap(ap);
    va_end(ap);
    return uw_move(&result);
}

static void consume_args(va_list ap)
{
    for (;;) {
        {
            UwValue arg = va_arg(ap, _UwValue);
            if (uw_va_end(&arg)) {
                break;
            }
        }
    }
}

UwResult uw_strcat_ap(va_list ap)
{
    // default error is OOM unless some arg is a status
    UwValue error = UwOOM();

    // count the number of args, check their types,
    // calculate total length and max char width
    unsigned result_len = 0;
    uint8_t max_char_size = 1;
    unsigned num_charptrs = 0;
    va_list temp_ap;
    va_copy(temp_ap, ap);
    for (unsigned arg_no = 0;;) {
        // arg is not auto-cleaned here because we don't consume it yet
        _UwValue arg = va_arg(temp_ap, _UwValue);
        arg_no++;
        if (uw_is_status(&arg)) {
            if (uw_va_end(&arg)) {
                break;
            }
            uw_destroy(&error);
            error = uw_clone(&arg);
            va_end(temp_ap);
            consume_args(ap);
            return uw_move(&error);
        }
        if (uw_is_string(&arg)) {
            result_len += uw_strlen(&arg);
            uint8_t char_size = uw_string_char_size(&arg);
            if (max_char_size < char_size) {
                max_char_size = char_size;
            }
        } else if (uw_is_charptr(&arg)) {
            num_charptrs++;
        } else {
            uw_destroy(&error);
            error = UwError(UW_ERROR_INCOMPATIBLE_TYPE);
            _uw_set_status_desc(&error, "Bad argument %u type for uw_strcat: %u, %s",
                                arg_no, arg.type_id, uw_get_type_name(arg.type_id));
            va_end(temp_ap);
            consume_args(ap);
            return uw_move(&error);
        }
    }
    va_end(temp_ap);

    // can allocate array for CharPtr now
    unsigned charptr_len[num_charptrs + 1];

    if (num_charptrs) {
        // need one more pass to get lengths and char sizes of CharPtr items
        unsigned charptr_index = 0;
        va_copy(temp_ap, ap);
        for (;;) {
            // arg is not auto-cleaned here because we don't consume it yet
            _UwValue arg = va_arg(temp_ap, _UwValue);
            if (uw_va_end(&arg)) {
                break;
            }
            if (uw_is_charptr(&arg)) {
                num_charptrs++;
                uint8_t char_size;
                unsigned len = _uw_charptr_strlen2(&arg, &char_size);

                charptr_len[charptr_index++] = len;

                result_len += len;
                if (max_char_size < char_size) {
                    max_char_size = char_size;
                }
            }
        }
        va_end(temp_ap);
    }

    if (result_len == 0) {
        return UwString();
    }

    // allocate resulting string
    UwValue str = uw_create_empty_string(result_len, max_char_size);

    // concatenate
    unsigned charptr_index = 0;
    for (;;) {
        {
            UwValue arg = va_arg(ap, _UwValue);
            if (uw_va_end(&arg)) {
                return uw_move(&str);
            }
            if (uw_is_string(&arg)) {
                if (!_uw_string_append(&str, &arg)) {
                    break;
                }
            } else if (uw_is_charptr(&arg)) {
                if (!append_charptr(&str, &arg, charptr_len[charptr_index], max_char_size)) {
                    break;
                }
                charptr_index++;
            }
        }
    }
    consume_args(ap);
    return uw_move(&error);
}
