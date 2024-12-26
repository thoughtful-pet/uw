#include <limits.h>
#include <string.h>

#include "include/uw.h"
#include "src/uw_charptr_internal.h"
#include "src/uw_string_internal.h"

// lookup table to validate capacity

#define _header_size  \
    (offsetof(struct _UwStringExtraData, string_data) + 8 /* slightly longer than string header */)

static unsigned _max_capacity[8] = {
    UINT_MAX - _header_size,
    (UINT_MAX - _header_size) / 2,
    (UINT_MAX - _header_size) / 3,
    (UINT_MAX - _header_size) / 4,
    0,  // unused
    0,  // unused
    0,  // unused
    (UINT_MAX - _header_size) / 8
};

/****************************************************************
 * Basic functions
 */

void* uw_string_get_bytes(UwValuePtr str, unsigned* length)
{
    uw_assert_string(str);
    struct _UwString* s = _uw_get_string_ptr(str);
    uint8_t char_size = _uw_string_char_size(s);
    CapMethods* capmeth = get_cap_methods(s);
    *length = char_size * capmeth->get_length(s);
    return (void*) get_char_ptr(s, 0);
}

[[noreturn]]
static void panic_bad_cap_size(struct _UwString* str)
{
    uw_panic("Bad length/capacity size: %u\n", _uw_string_cap_size(str));
}

[[noreturn]]
static void panic_bad_char_size(uint8_t char_size)
{
    uw_panic("Bad char size: %u\n", char_size);
}

static inline uint8_t calc_cap_size(unsigned capacity)
{
    if (capacity < 256) {
        return 1;
    } else if (capacity < 65536) {
        return 2;
#if UINT_WIDTH == 32
    } else {
        return 4;
#else
    } else if (capacity < (1ULL << 32)) {
        return 4;
    } else {
        return 8;
#endif
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

static unsigned _calc_string_memsize_helper(struct _UwString* desired_addr,
                                            unsigned desired_capacity,
                                            uint8_t cap_size, uint8_t char_size,
                                            unsigned* real_capacity)
/*
 * Calculate memsize for string with given `char_size`, `cap_size`,  and `desired_capacity`.
 * Return memsize and real capacity.
 *
 * This function does not dereference `desired_addr` and can be used to calculate
 * size starting from nullptr.
 */
{
    // final test, if previous ones failed to return OOM error
    uw_assert(desired_capacity <= _max_capacity[char_size]);

    // get address of the first character in string
    uint8_t* char0_addr = get_char0_ptr(desired_addr, cap_size, char_size);

    // calculate end address
    uint8_t* end_addr = uw_align_ptr(
        char0_addr + char_size * desired_capacity,
        UWSTRING_BLOCK_SIZE
    );
    ptrdiff_t memsize = end_addr - (uint8_t*) desired_addr;
    ptrdiff_t strsize = end_addr - char0_addr;

#   if UINT_MAX < PTRDIFF_MAX
        if (memsize > UINT_MAX) {
            uw_panic("String memory size is too big: %td\n", memsize);
        }
#   endif

    *real_capacity = (unsigned) (strsize / char_size);
    return (unsigned) memsize;
}

static unsigned calc_string_memsize(struct _UwString* desired_addr,
                                    unsigned desired_capacity, uint8_t char_size,
                                    unsigned* real_capacity)
/*
 * Calculate size of memory for string starting from possibly unaligned `desired_addr`.
 *
 * This function does not dereference `desired_addr` and can be used to calculate
 * size starting from nullptr.
 */
{
    // calculate capacity that padded memory block may hold (and memsize, of course)
    uint8_t cap_size = calc_cap_size(desired_capacity);
    unsigned memsize = _calc_string_memsize_helper(
        desired_addr, desired_capacity, cap_size, char_size, real_capacity
    );
    uint8_t real_cap_size = calc_cap_size(*real_capacity);

    // now check if cap_size increased

    if (real_cap_size == cap_size) {
        // no need to increase, real capacity and memsize are final,
        // do nothing here
    }
    else if (real_cap_size < cap_size) {
        // uh oh
        uw_panic("Something went wrong: real cap_size %u must be greater or equal %u\n", real_cap_size, cap_size);

    } else {
        // yes, cap_size should be increased
        // re-calculate
        memsize = _calc_string_memsize_helper(
            desired_addr, *real_capacity, real_cap_size, char_size, real_capacity
        );
    }

    return memsize;
}

static inline unsigned calc_extra_data_size(uint8_t char_size, unsigned desired_capacity, unsigned* real_capacity)
/*
 * Calculate memory size for extra data.
 */
{
    unsigned offset = offsetof(struct _UwStringExtraData, string_data);
    return offset + calc_string_memsize((void*) (ptrdiff_t) offset, desired_capacity, char_size, real_capacity);
}

static inline unsigned get_string_memsize(struct _UwString* s)
/*
 * Get memory size occupied by string.
 */
{
    uint8_t* end_addr = uw_align_ptr(
        get_char_ptr(s, get_cap_methods(s)->get_capacity(s)),
        UWSTRING_BLOCK_SIZE
    );
    ptrdiff_t memsize = end_addr - (uint8_t*) s;

#   if UINT_MAX < PTRDIFF_MAX
        if (memsize > UINT_MAX) {
            uw_panic("String memory size is too big: %td\n", memsize);
        }
#   endif
    return (unsigned) memsize;
}

static inline unsigned get_extra_data_size(UwValuePtr str)
/*
 * Get memory size occupied by extra data.
 */
{
    struct _UwString* s = _uw_get_string_ptr(str);
    return offsetof(struct _UwStringExtraData, string_data) + get_string_memsize(s);
}

static inline unsigned get_embedded_capacity(uint8_t char_size)
{
    static _UwValue v;
    switch (char_size) {
        case 1: return _UWC_LENGTH_OF(v.str_1);
        case 2: return _UWC_LENGTH_OF(v.str_2);
        case 3: return _UWC_LENGTH_OF(v.str_3);
        case 4: return _UWC_LENGTH_OF(v.str_4);
        default: panic_bad_char_size(char_size);
    }
}

static bool make_empty_string(UwValuePtr result, uint8_t cap_size, unsigned capacity, uint8_t char_size)
/*
 * Create empty string with desired parameters.
 * Result type_id must be set before calling this function, other fields are assumed undefined.
 * Return false if OOM.
 */
{
    // check if string can be embedded into result

    if (cap_size == 1) {

        unsigned result_capacity = get_embedded_capacity(char_size);
        if (capacity <= result_capacity) {
            _uw_string_set_cap_size (&result->str_header, 1);
            _uw_string_set_char_size(&result->str_header, char_size);
            result->str_length = 0;
            result->str_capacity = result_capacity;
            result->str_header.is_embedded = 1;
            result->str_4[0] = 0;
            result->str_4[1] = 0;
            result->str_4[2] = 0;
            static_assert(sizeof(result->str_4) == 12);
            return true;
        }
    }

    if(capacity > _max_capacity[char_size]) {
        return false;
    }

    result->str_header.is_embedded = 0;

    // allocate string

    unsigned real_capacity;
    unsigned memsize = calc_extra_data_size(char_size, capacity, &real_capacity);
    uw_assert(capacity <= real_capacity);
    uint8_t real_cap_size = calc_cap_size(real_capacity);
    if (cap_size < real_cap_size) {
        cap_size = real_cap_size;
    }

    UwType* t = _uw_types[result->type_id];

    result->extra_data = t->allocator->alloc(memsize);
    if (!result->extra_data) {
        return false;
    }

    result->extra_data->refcount = 1;

    struct _UwString* s = _uw_get_string_ptr(result);

    _uw_string_set_cap_size (s, cap_size);
    _uw_string_set_char_size(s, char_size);

    CapMethods* capmeth = get_cap_methods(s);
    capmeth->set_length(s, 0);
    capmeth->set_capacity(s, real_capacity);
    return result;
}

static struct _UwString* expand_string(UwValuePtr str, unsigned increment, uint8_t new_char_size)
/*
 * Expand string if necessary, replacing `str->extra_data`.
 *
 * If string refcount is greater than 1, always make a copy because the string is about to be updated.
 *
 * Return pointer to string data
 */
{
    uw_assert_string(str);
    if (_uw_str_is_embedded(str)) {
        struct _UwString* s = _uw_get_string_ptr(str);
        if (new_char_size < _uw_string_char_size(s)) {
            if (increment == 0) {
                return s;
            }
            new_char_size = _uw_string_char_size(s);
        }
        if (increment > _max_capacity[new_char_size] - str->str_length) {
            return nullptr;
        }
        unsigned new_length = str->str_length + increment;
        if (new_length <= get_embedded_capacity(new_char_size)) {
            // no need to expand
            if (new_char_size > _uw_string_char_size(s)) {
                // but need to make existing chars wider
                _UwValue saved_str = *str;
                struct _UwString* s_src   = _uw_get_string_ptr(&saved_str);
                get_str_methods(s_src)->copy_to(get_char_ptr(s_src, 0), s, 0, str->str_length);
            }
            return s;
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

        struct _UwString* s = _uw_get_string_ptr(str);
        uint8_t char_size = _uw_string_char_size(s);
        if (new_char_size > char_size) {
            // copy string if char size needs to increase
            // make refcount zero to free original data after copy
            str->extra_data->refcount = 0;
            goto copy_string;
        }

        CapMethods* capmeth = get_cap_methods(s);
        unsigned length = capmeth->get_length(s);

        if (increment > _max_capacity[new_char_size] - length) {
            return nullptr;
        }

        unsigned new_length = length + increment;
        unsigned capacity = capmeth->get_capacity(s);

        if (new_length <= capacity) {
            // no need to expand
            return s;
        }

        // reallocate data

        unsigned check_capacity;
        unsigned orig_memsize = calc_extra_data_size(char_size, capacity, &check_capacity);
        uw_assert(capacity == check_capacity);

        unsigned new_capacity;
        unsigned new_memsize = calc_extra_data_size(char_size, new_length, &new_capacity);

        _UwExtraData* new_data = _uw_types[str->type_id]->allocator->realloc(str->extra_data, orig_memsize, new_memsize);
        if (!new_data) {
            return nullptr;
        }
        if (new_data != str->extra_data) {
            // memory block was moved
            str->extra_data = new_data;
            s = _uw_get_string_ptr(str);
        }

        uint8_t old_cap_size = calc_cap_size(capacity);
        uint8_t new_cap_size = calc_cap_size(new_capacity);

        if (new_cap_size > old_cap_size) {
            // header size increased, move string to the right
            uint8_t* src_addr  = get_char0_ptr(s, old_cap_size, char_size);
            uint8_t* dest_addr = get_char0_ptr(s, new_cap_size, char_size);
            memmove(dest_addr, src_addr, length * char_size);
        }

        _uw_string_set_cap_size (s, new_cap_size);

        capmeth = get_cap_methods(s);
        capmeth->set_length(s, length);           // keep old length
        capmeth->set_capacity(s, new_capacity);

        return s;
    }

copy_string: {

        // save original string
        _UwValue saved_str = *str;
        struct _UwString* s_src   = _uw_get_string_ptr(&saved_str);

        uint8_t src_char_size = _uw_string_char_size(s_src);
        unsigned length = get_cap_methods(s_src)->get_length(s_src);

        if (increment > _max_capacity[new_char_size] - length) {
            // restore refcount of the original string
            if (!_uw_str_is_embedded(str)) {
                str->extra_data->refcount++;
            }
            return nullptr;
        }

        unsigned new_length = length + increment;

        if (new_char_size < src_char_size) {
            new_char_size = src_char_size;
        }

        // allocate string
        if (!make_empty_string(str, calc_cap_size(new_length), new_length, new_char_size)) {
            // restore refcount of the original string
            if (!_uw_str_is_embedded(str)) {
                str->extra_data->refcount++;
            }
            return nullptr;
        }
        struct _UwString* s_dest  = _uw_get_string_ptr(str);

        // copy saved data to allocated string
        get_str_methods(s_src)->copy_to(get_char_ptr(s_src, 0), s_dest, 0, length);

        // keep old length
        get_cap_methods(s_dest)->set_length(s_dest, length);

        // free saved string if not embedded and reference count is zero
        if (!_uw_str_is_embedded(&saved_str) && saved_str.extra_data->refcount == 0) {

            unsigned capacity = get_cap_methods(s_src)->get_capacity(s_src);
            unsigned cleck_capacity;
            unsigned memsize = calc_extra_data_size(src_char_size, capacity, &cleck_capacity);
            uw_assert(capacity == cleck_capacity);
            _uw_types[saved_str.type_id]->allocator->free(saved_str.extra_data, memsize);
        }
        return s_dest;
    }
}

/****************************************************************
 * Basic interface methods
 */

UwResult _uw_string_create(UwTypeId type_id, va_list ap)
{
    UwValuePtr v = va_arg(ap, UwValuePtr);
    UwValue result = uw_clone(v);  // this converts CharPtr to string
    if (!uw_is_string(&result)) {
        return UwError(UW_ERROR_INCOMPATIBLE_TYPE);
    } else {
        return uw_move(&result);
    }
}

void _uw_string_destroy(UwValuePtr self)
{
    if (_uw_str_is_embedded(self)) {
        return;
    }
    if (0 == --self->extra_data->refcount) {

        UwType* t = _uw_types[self->type_id];
        t->allocator->free(self->extra_data, get_extra_data_size(self));
        self->extra_data = nullptr;
    }
}

UwResult _uw_string_clone(UwValuePtr self)
{
    UwValue result = *self;
    if (!result.str_header.is_embedded) {
        if (result.extra_data) {
            result.extra_data->refcount++;
        }
    }
    return uw_move(&result);
}

void _uw_string_hash(UwValuePtr self, UwHashContext* ctx)
{
    // mind maps: the hash should be the same for subtypes, that's why not using self->type_id here
    _uw_hash_uint64(ctx, UwTypeId_String);

    struct _UwString* s = _uw_get_string_ptr(self);
    unsigned length = get_cap_methods(s)->get_length(s);
    if (length) {
        get_str_methods(s)->hash(get_char_ptr(s, 0), length, ctx);
    }
}

void _uw_string_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    _uw_string_dump_data(fp, self, next_indent);
}

void _uw_string_dump_data(FILE* fp, UwValuePtr str, int indent)
{
    struct _UwString* s;

    if (_uw_str_is_embedded(str)) {
        fprintf(fp, " embedded,");
        s = &str->str_header;
    } else {
        fprintf(fp, " data=%p, refcount=%u,", str->extra_data, str->extra_data->refcount);
        s = _uw_get_string_ptr(str);
    }
    CapMethods* capmeth = get_cap_methods(s);
    unsigned length = capmeth->get_length(s);

    fprintf(fp, " %u chars, capacity=%u, char size=%u, cap size=%u\n",
            length, capmeth->get_capacity(s),
            _uw_string_char_size(s), _uw_string_cap_size(s));
    indent += 4;

    if (length) {
        // print first 80 characters
        _uw_print_indent(fp, indent);
        StrMethods* strmeth = get_str_methods(s);
        for(unsigned i = 0; i < length; i++) {
            _uw_putchar32_utf8(fp, strmeth->get_char(get_char_ptr(s, i)));
            if (i == 80) {
                fprintf(fp, "...");
                break;
            }
        }
        fputc('\n', fp);
    }
}

bool _uw_string_is_true(UwValuePtr self)
{
    struct _UwString* str = _uw_get_string_ptr(self);
    CapMethods* capmeth = get_cap_methods(str);
    return capmeth->get_length(str);
}

static inline bool _uw_string_eq(struct _UwString* a, struct _UwString* b)
{
    if (a == b) {
        return true;
    }
    unsigned a_length = get_cap_methods(a)->get_length(a);
    unsigned b_length = get_cap_methods(b)->get_length(b);
    if (a_length != b_length) {
        return false;
    }
    if (a_length == 0) {
        return true;
    }
    return get_str_methods(a)->equal(get_char_ptr(a, 0), b, 0, a_length);
}

bool _uw_string_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return _uw_string_eq(_uw_get_string_ptr(self), _uw_get_string_ptr(other));
}

bool _uw_string_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        switch (t) {
            case UwTypeId_String:
                return _uw_string_eq(_uw_get_string_ptr(self), _uw_get_string_ptr(other));

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
    struct _UwString* s = _uw_get_string_ptr(str);
    uint8_t char_size = _uw_string_char_size(s);

    StrMethods* strmeth = get_str_methods(s);
    char8_t* ptr = get_char_ptr(s, 0);

    unsigned length = 0;
    unsigned n = get_cap_methods(s)->get_length(s);
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
 * Methods that depend on cap_size field.
 */

#define CAP_DATA_ADDR(typename, str)  \
    ((typename*) (  \
        cap_data_addr(sizeof(typename), (str))  \
    ))

#define CAP_METHODS_IMPL(typename)  \
    static inline unsigned get_length_##typename(struct _UwString* str)  \
    {  \
        return CAP_DATA_ADDR(typename, str)[0];  \
    }  \
    static unsigned _get_length_##typename(struct _UwString* str)  \
    {  \
        return get_length_##typename(str);  \
    }  \
    static inline void set_length_##typename(struct _UwString* str, unsigned n)  \
    {  \
        CAP_DATA_ADDR(typename, str)[0] = (typename) n;  \
    }  \
    static void _set_length_##typename(struct _UwString* str, unsigned n)  \
    {  \
        set_length_##typename(str, n);  \
    }  \
    static inline unsigned get_capacity_##typename(struct _UwString* str)  \
    {  \
        return CAP_DATA_ADDR(typename, str)[1];  \
    }  \
    static unsigned _get_capacity_##typename(struct _UwString* str)  \
    {  \
        return get_capacity_##typename(str);  \
    }  \
    static inline void set_capacity_##typename(struct _UwString* str, unsigned n)  \
    {  \
        CAP_DATA_ADDR(typename, str)[1] = (typename) n;  \
    }  \
    static void _set_capacity_##typename(struct _UwString* str, unsigned n)  \
    {  \
        set_capacity_##typename(str, n);  \
    }

CAP_METHODS_IMPL(uint8_t)
CAP_METHODS_IMPL(uint16_t)
CAP_METHODS_IMPL(uint32_t)

#if UINT_WIDTH > 32
    CAP_METHODS_IMPL(uint64_t)
#endif

static unsigned _get_length_x(struct _UwString* str)              { panic_bad_cap_size(str); }
static void     _set_length_x(struct _UwString* str, unsigned n)  { panic_bad_cap_size(str); }
#define _get_capacity_x  _get_length_x
#define _set_capacity_x  _set_length_x

CapMethods _uws_cap_methods[] = {
    { _get_length_uint8_t,  _set_length_uint8_t,  _get_capacity_uint8_t,  _set_capacity_uint8_t  },
    { _get_length_uint16_t, _set_length_uint16_t, _get_capacity_uint16_t, _set_capacity_uint16_t },
    { _get_length_x,        _set_length_x,        _get_capacity_x,        _set_capacity_x        },
    { _get_length_uint32_t, _set_length_uint32_t, _get_capacity_uint32_t, _set_capacity_uint32_t },
    { _get_length_x,        _set_length_x,        _get_capacity_x,        _set_capacity_x        },
    { _get_length_x,        _set_length_x,        _get_capacity_x,        _set_capacity_x        },
    { _get_length_x,        _set_length_x,        _get_capacity_x,        _set_capacity_x        },
#if UINT_WIDTH > 32
    { _get_length_uint64_t, _set_length_uint64_t, _get_capacity_uint64_t, _set_capacity_uint64_t }
#else
    { _get_length_x,        _set_length_x,        _get_capacity_x,        _set_capacity_x        }
#endif
};

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
    static bool _eq_##type_name_self(uint8_t* self_ptr, struct _UwString* other, unsigned other_start_pos, unsigned length)  \
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
    static void _cp_to_##type_name_self(uint8_t* self_ptr, struct _UwString* dest, unsigned dest_start_pos, unsigned length)  \
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

    if (!make_empty_string(&result, calc_cap_size(capacity), capacity, char_size)) {
        return UwOOM();
    }
    return result;
}

#ifdef DEBUG

UwResult uw_create_empty_string2(uint8_t cap_size, uint8_t char_size)
{
    // using not autocleaned variable here, no uw_move necessary on exit
    _UWDECL_String(result);

    if (!make_empty_string(&result, cap_size, 0, char_size)) {
        return UwOOM();
    }
    return result;
}

#endif

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

    if (!make_empty_string(&result, calc_cap_size(length), length, 1)) {
        return UwOOM();
    }
    if (initializer) {
        struct _UwString* s = _uw_get_string_ptr(&result);
        get_str_methods(s)->copy_from_cstr(get_char_ptr(s, 0), initializer, length);
        get_cap_methods(s)->set_length(s, length);
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

    if (!make_empty_string(&result, calc_cap_size(length), length, char_size)) {
        return UwOOM();
    }
    if (initializer) {
        struct _UwString* s = _uw_get_string_ptr(&result);
        get_str_methods(s)->copy_from_utf8(get_char_ptr(s, 0), initializer, length);
        get_cap_methods(s)->set_length(s, length);
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

    if (!make_empty_string(&result, calc_cap_size(length), length, char_size)) {
        return UwOOM();
    }
    if (initializer) {
        struct _UwString* s = _uw_get_string_ptr(&result);
        get_str_methods(s)->copy_from_utf32(get_char_ptr(s, 0), initializer, length);
        get_cap_methods(s)->set_length(s, length);
    }
    return result;
}

/****************************************************************
 * String functions
 */

uint8_t uw_string_char_size(UwValuePtr s)
{
    uw_assert_string(s);
    return _uw_string_char_size(_uw_get_string_ptr(s));
}

unsigned uw_strlen(UwValuePtr str)
{
    uw_assert_string(str);
    struct _UwString* s = _uw_get_string_ptr(str);
    return get_cap_methods(s)->get_length(s);
}

#define STRING_EQ_IMPL(suffix, type_name_b)  \
{  \
    uw_assert_string(a);  \
    struct _UwString* astr = _uw_get_string_ptr(a);  \
    \
    unsigned a_length = get_cap_methods(astr)->get_length(astr);  \
    return get_str_methods(astr)->equal_##suffix(get_char_ptr(astr, 0), (type_name_b*) b, a_length);  \
}

bool  uw_equal_cstr(UwValuePtr a, char* b)     STRING_EQ_IMPL(cstr, char)
bool _uw_equal_u8  (UwValuePtr a, char8_t* b)  STRING_EQ_IMPL(u8,   char8_t)
bool _uw_equal_u32 (UwValuePtr a, char32_t* b) STRING_EQ_IMPL(u32,  char32_t)

#define SUBSTRING_EQ_IMPL(suffix, type_name_b)  \
{  \
    uw_assert_string(a);  \
    struct _UwString* astr = _uw_get_string_ptr(a);  \
    \
    unsigned a_length = get_cap_methods(astr)->get_length(astr);  \
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

bool  uw_substring_eq_cstr(UwValuePtr a, unsigned start_pos, unsigned end_pos, char*     b) SUBSTRING_EQ_IMPL(cstr, char)
bool _uw_substring_eq_u8  (UwValuePtr a, unsigned start_pos, unsigned end_pos, char8_t*  b) SUBSTRING_EQ_IMPL(u8,   char8_t)
bool _uw_substring_eq_u32 (UwValuePtr a, unsigned start_pos, unsigned end_pos, char32_t* b) SUBSTRING_EQ_IMPL(u32,  char32_t)

bool _uw_substring_eq(UwValuePtr a, unsigned start_pos, unsigned end_pos, UwValuePtr b)
{
    uw_assert_string(a);
    struct _UwString* astr = _uw_get_string_ptr(a);

    unsigned a_length = get_cap_methods(astr)->get_length(astr);

    if (end_pos > a_length) {
        end_pos = a_length;
    }
    if (end_pos < start_pos) {
        return false;
    }

    uw_assert_string(b);
    struct _UwString* bstr = _uw_get_string_ptr(b);

    if (end_pos == start_pos && get_cap_methods(bstr)->get_length(bstr) == 0) {
        return true;
    }

    return get_str_methods(astr)->equal(get_char_ptr(astr, start_pos), bstr, 0, end_pos - start_pos);
}

CStringPtr uw_string_to_cstring(UwValuePtr str)
{
    uw_assert_string(str);

    struct _UwString* s = _uw_get_string_ptr(str);
    unsigned length = get_cap_methods(s)->get_length(s);

    CStringPtr result = malloc(length + 1);
    if (!result) {
        return nullptr;
    }
    get_str_methods(s)->copy_to_u8(get_char_ptr(s, 0), result, length);
    return result;
}

void uw_string_copy_buf(UwValuePtr str, char* buffer)
{
    uw_assert_string(str);

    struct _UwString* s = _uw_get_string_ptr(str);
    unsigned length = get_cap_methods(s)->get_length(s);
    get_str_methods(s)->copy_to_u8(get_char_ptr(s, 0), buffer, length);
}

void uw_destroy_cstring(CStringPtr* str)
{
    free(*str);
    *str = nullptr;
}

bool _uw_string_append_char(UwValuePtr dest, char c)
{
    struct _UwString* sd = expand_string(dest, 1, 1);
    if (!sd) {
        return false;
    }
    CapMethods* capmeth = get_cap_methods(sd);
    unsigned length = capmeth->get_length(sd);
    get_str_methods(sd)->put_char(get_char_ptr(sd, length), (unsigned char) c);
    capmeth->set_length(sd, length + 1);
    return true;
}

bool _uw_string_append_c32(UwValuePtr dest, char32_t c)
{
    struct _UwString* sd = expand_string(dest, 1, calc_char_size(c));
    if (!sd) {
        return false;
    }
    CapMethods* capmeth = get_cap_methods(sd);
    unsigned length = capmeth->get_length(sd);
    get_str_methods(sd)->put_char(get_char_ptr(sd, length), c);
    capmeth->set_length(sd, length + 1);
    return true;
}

static bool append_cstr(UwValuePtr dest, char* src, unsigned src_len)
{
    struct _UwString* sd = expand_string(dest, src_len, 1);
    if (!sd) {
        return false;
    }
    CapMethods* capmeth = get_cap_methods(sd);
    unsigned dest_len = capmeth->get_length(sd);
    get_str_methods(sd)->copy_from_cstr(get_char_ptr(sd, dest_len), src, src_len);
    capmeth->set_length(sd, src_len + dest_len);
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
    struct _UwString* sd = expand_string(dest, src_len, src_char_size);
    if (!sd) {
        return false;
    }
    CapMethods* capmeth = get_cap_methods(sd);
    unsigned dest_len = capmeth->get_length(sd);
    get_str_methods(sd)->copy_from_utf8(get_char_ptr(sd, dest_len), src, src_len);
    capmeth->set_length(sd, src_len + dest_len);
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
    struct _UwString* sd = expand_string(dest, src_len, src_char_size);
    if (!sd) {
        return false;
    }
    CapMethods* capmeth = get_cap_methods(sd);
    unsigned dest_len = capmeth->get_length(sd);
    get_str_methods(sd)->copy_from_utf32(get_char_ptr(sd, dest_len), src, src_len);
    capmeth->set_length(sd, src_len + dest_len);
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

static bool append_string(UwValuePtr dest, struct _UwString* src, unsigned src_start_pos, unsigned src_len)
{
    struct _UwString* sd = expand_string(dest, src_len, _uw_string_char_size(src));
    if (!sd) {
        return false;
    }
    CapMethods* capmeth = get_cap_methods(sd);
    unsigned dest_len = capmeth->get_length(sd);
    get_str_methods(src)->copy_to(get_char_ptr(src, src_start_pos), sd, dest_len, src_len);
    capmeth->set_length(sd, src_len + dest_len);
    return true;
}

bool _uw_string_append(UwValuePtr dest, UwValuePtr src)
{
    uw_assert_string(src);
    struct _UwString* s = _uw_get_string_ptr(src);

    unsigned src_len = get_cap_methods(s)->get_length(s);
    return append_string(dest, s, 0, src_len);
}

bool _uw_string_append_substring(UwValuePtr dest, UwValuePtr src, unsigned src_start_pos, unsigned src_end_pos)
{
    uw_assert_string(src);
    struct _UwString* s = _uw_get_string_ptr(src);

    unsigned src_len = get_cap_methods(s)->get_length(s);
    if (src_end_pos > src_len) {
        src_end_pos = src_len;
    }
    if (src_start_pos >= src_end_pos) {
        return true;
    }
    src_len = src_end_pos - src_start_pos;

    return append_string(dest, s, src_start_pos, src_len);
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
        struct _UwString* sd = expand_string(dest, src_len, src_char_size);
        if (!sd) {
            return false;
        }
        CapMethods* capmeth = get_cap_methods(sd);
        unsigned dest_len = capmeth->get_length(sd);
        get_str_methods(sd)->copy_from_utf8(get_char_ptr(sd, dest_len), buffer, src_len);
        capmeth->set_length(sd, src_len + dest_len);
    }
    return true;
}

bool uw_string_append_buffer(UwValuePtr dest, uint8_t* buffer, unsigned size)
{
    if (size == 0) {
        return true;
    }
    uw_assert(uw_string_char_size(dest) == 1);

    struct _UwString* sd = expand_string(dest, size, 1);
    if (!sd) {
        return false;
    }
    CapMethods* capmeth = get_cap_methods(sd);
    StrMethods* strmeth = get_str_methods(sd);
    unsigned dest_len = capmeth->get_length(sd);
    capmeth->set_length(sd, dest_len + size);
    uint8_t* ptr = get_char_ptr(sd, dest_len);
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

    struct _UwString* s = expand_string(str, n, calc_char_size(chr));
    if (!s) {
        return false;
    }
    CapMethods* capmeth = get_cap_methods(s);
    StrMethods* strmeth = get_str_methods(s);
    unsigned len = capmeth->get_length(s);
    uint8_t char_size = _uw_string_char_size(s);
    uint8_t* insertion_ptr = get_char_ptr(s, position);

    if (position < len) {
        memmove(insertion_ptr + n * char_size, insertion_ptr, (len - position) * char_size);
    }
    for (unsigned i = 0; i < n; i++) {
        strmeth->put_char(insertion_ptr, chr);
        insertion_ptr += char_size;
    }
    capmeth->set_length(s, len + n);
    return true;
}

UwResult uw_string_get_substring(UwValuePtr str, unsigned start_pos, unsigned end_pos)
{
    uw_assert_string(str);
    struct _UwString* s = _uw_get_string_ptr(str);

    CapMethods* capmeth = get_cap_methods(s);
    StrMethods* strmeth = get_str_methods(s);

    unsigned length = capmeth->get_length(s);

    if (end_pos > length) {
        end_pos = length;
    }
    if (start_pos >= end_pos) {
        return uw_create_empty_string(0, 1);
    }
    length = end_pos - start_pos;
    uint8_t* src = get_char_ptr(s, start_pos);
    uint8_t char_size = strmeth->max_char_size(src, length);

    UwValue result = uw_create_empty_string(length, char_size);
    if (uw_ok(&result)) {
        struct _UwString* dest = _uw_get_string_ptr(&result);

        get_cap_methods(dest)->set_length(dest, length);
        strmeth->copy_to(src, dest, 0, length);
    }
    return result;
}

char32_t uw_string_at(UwValuePtr str, unsigned position)
{
    uw_assert_string(str);
    struct _UwString* s = _uw_get_string_ptr(str);
    unsigned length = get_cap_methods(s)->get_length(s);
    if (position < length) {
        return get_str_methods(s)->get_char(get_char_ptr(s, position));
    } else {
        return 0;
    }
}

bool uw_string_erase(UwValuePtr str, unsigned start_pos, unsigned end_pos)
{
    if (start_pos >= uw_strlen(str) || start_pos >= end_pos) {
        return true;
    }
    struct _UwString* s = expand_string(str, 0, 0);  // make copy if refcount > 1
    if (!s) {
        return false;
    }
    CapMethods* capmeth = get_cap_methods(s);
    unsigned length = capmeth->get_length(s);
    uint8_t char_size = _uw_string_char_size(s);

    if (end_pos >= length) {
        // truncate
        length = start_pos;
    } else {
        unsigned tail_len = length - end_pos;
        memmove(get_char_ptr(s, start_pos), get_char_ptr(s, end_pos), tail_len * char_size);
        length -= end_pos - start_pos;
    }
    capmeth->set_length(s, length);
    return true;
}

bool uw_string_truncate(UwValuePtr str, unsigned position)
{
    if (position >= uw_strlen(str)) {
        return true;
    }
    struct _UwString* s = expand_string(str, 0, 0);  // make copy if refcount > 1
    if (!s) {
        return false;
    }
    get_cap_methods(s)->set_length(s, position);
    return true;
}

bool uw_string_indexof(UwValuePtr str, char32_t chr, unsigned start_pos, unsigned* result)
{
    uw_assert_string(str);
    struct _UwString* s = _uw_get_string_ptr(str);
    CapMethods* capmeth = get_cap_methods(s);
    StrMethods* strmeth = get_str_methods(s);

    unsigned length = capmeth->get_length(s);
    uint8_t* ptr = get_char_ptr(s, start_pos);

    for (unsigned i = start_pos; i < length; i++) {
        char32_t codepoint = strmeth->get_char(ptr);
        if (codepoint == chr) {
            if (result) {
                *result = i;
            }
            return true;
        }
        ptr += _uw_string_char_size(s);
    }
    return false;
}

bool uw_string_ltrim(UwValuePtr str)
{
    uw_assert_string(str);
    struct _UwString* s = _uw_get_string_ptr(str);
    StrMethods* strmeth = get_str_methods(s);
    unsigned len = get_cap_methods(s)->get_length(s);
    uint8_t char_size = _uw_string_char_size(s);

    char8_t* ptr = get_char_ptr(s, 0);
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
    struct _UwString* s = _uw_get_string_ptr(str);
    StrMethods* strmeth = get_str_methods(s);
    unsigned n = get_cap_methods(s)->get_length(s);
    uint8_t char_size = _uw_string_char_size(s);

    char8_t* ptr = get_char_ptr(s, n);
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
    struct _UwString* s = expand_string(str, 0, 0);  // make copy if refcount > 1
    if (!s) {
        return false;
    }
    StrMethods* strmeth = get_str_methods(s);
    unsigned n = get_cap_methods(s)->get_length(s);
    uint8_t char_size = _uw_string_char_size(s);

    char8_t* ptr = get_char_ptr(s, 0);
    while (n) {
        strmeth->put_char(ptr, uw_char_lower(strmeth->get_char(ptr)));
        ptr += char_size;
        n--;
    }
    return true;
}

bool uw_string_upper(UwValuePtr str)
{
    struct _UwString* s = expand_string(str, 0, 0);  // make copy if refcount > 1
    if (!s) {
        return false;
    }
    StrMethods* strmeth = get_str_methods(s);
    unsigned n = get_cap_methods(s)->get_length(s);
    uint8_t char_size = _uw_string_char_size(s);

    char8_t* ptr = get_char_ptr(s, 0);
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

    struct _UwString* s = _uw_get_string_ptr(str);
    StrMethods* strmeth = get_str_methods(s);

    unsigned len = get_cap_methods(s)->get_length(s);
    uint8_t char_size = _uw_string_char_size(s);

    UwValue result = UwList();
    if (uw_error(&result)) {
        return uw_move(&result);
    }

    char8_t* ptr = get_char_ptr(s, 0);
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
                struct _UwString* subs = _uw_get_string_ptr(&substr);
                strmeth->copy_to(start, subs, 0, substr_len);
                get_cap_methods(subs)->set_length(subs, substr_len);
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
            struct _UwString* subs = _uw_get_string_ptr(&substr);
            strmeth->copy_to(start, subs, 0, substr_len);
            get_cap_methods(subs)->set_length(subs, substr_len);
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
            return _uw_charptr_to_string(&item);
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
