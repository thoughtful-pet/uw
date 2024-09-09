/****************************************************************
 * Methods that depend on cap_size field.
 *
 * These basic methods are required by both string implementation
 * and test suite, that's why a separate file.
 */

typedef size_t (*CapGetter)(_UwString* str);
typedef void   (*CapSetter)(_UwString* str, size_t n);

#define CAP_METHODS_IMPL(typename)  \
    static inline size_t get_length_##typename(_UwString* str)  \
    {  \
        typename* data = (typename*) str;  \
        return data[1];  \
    }  \
    static size_t _get_length_##typename(_UwString* str)  \
    {  \
        return get_length_##typename(str);  \
    }  \
    static inline void set_length_##typename(_UwString* str, size_t n)  \
    {  \
        typename* data = (typename*) str;  \
        data[1] = (typename) n;  \
    }  \
    static void _set_length_##typename(_UwString* str, size_t n)  \
    {  \
        return set_length_##typename(str, n);  \
    }  \
    static inline size_t get_capacity_##typename(_UwString* str)  \
    {  \
        typename* data = (typename*) str;  \
        return data[2];  \
    }  \
    static size_t _get_capacity_##typename(_UwString* str)  \
    {  \
        return get_capacity_##typename(str);  \
    }  \
    static inline void set_capacity_##typename(_UwString* str, size_t n)  \
    {  \
        typename* data = (typename*) str;  \
        data[2] = (typename) n;  \
    }  \
    static void _set_capacity_##typename(_UwString* str, size_t n)  \
    {  \
        return set_capacity_##typename(str, n);  \
    }

CAP_METHODS_IMPL(uint8_t)
CAP_METHODS_IMPL(uint16_t)
CAP_METHODS_IMPL(uint32_t)
CAP_METHODS_IMPL(uint64_t)

[[noreturn]]
static void bad_cap_size(_UwString* str)
{
    fprintf(stderr, "Bad size of string length/capacity fields %u\n", str->cap_size);
    exit(1);
}
static size_t   _get_length_x(_UwString* str)              { bad_cap_size(str); }
static void     _set_length_x(_UwString* str, size_t n)    { bad_cap_size(str); }
#define _get_capacity_x  _get_length_x
#define _set_capacity_x  _set_length_x

typedef struct {
    CapGetter   get_length;
    CapSetter   set_length;
    CapGetter   get_capacity;
    CapSetter   set_capacity;
} CapMethods;

static CapMethods cap_methods[8] = {
    { _get_length_uint8_t,  _set_length_uint8_t,  _get_capacity_uint8_t,  _set_capacity_uint8_t  },
    { _get_length_uint16_t, _set_length_uint16_t, _get_capacity_uint16_t, _set_capacity_uint16_t },
    { _get_length_x,        _set_length_x,        _get_capacity_x,        _set_capacity_x        },
    { _get_length_uint32_t, _set_length_uint32_t, _get_capacity_uint32_t, _set_capacity_uint32_t },
    { _get_length_x,        _set_length_x,        _get_capacity_x,        _set_capacity_x        },
    { _get_length_x,        _set_length_x,        _get_capacity_x,        _set_capacity_x        },
    { _get_length_x,        _set_length_x,        _get_capacity_x,        _set_capacity_x        },
    { _get_length_uint64_t, _set_length_uint64_t, _get_capacity_uint64_t, _set_capacity_uint64_t }
};
#define get_cap_methods(str) (&cap_methods[(str)->cap_size])

#define string_header_size(cap_size, char_size)  \
    ((_unlikely_(((char_size) - 1) == 2))?  \
        (3 * (cap_size))  \
        :  \
        ((3 * (cap_size) + (char_size) - 1) & ~((char_size) - 1))  \
    )

#define get_char_ptr(str, start_pos)  \
    ({ \
        _UwStringHeader h = *(str);  \
        uint8_t* ptr = ((uint8_t*) (str)) + string_header_size(h.cap_size + 1, h.char_size + 1) + start_pos * (h.char_size + 1);  \
        ptr;  \
    })
