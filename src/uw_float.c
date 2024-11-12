#include <stdarg.h>

#include "include/uw_base.h"
#include "src/uw_float_internal.h"

bool _uw_init_float(UwValuePtr self)
{
    self->float_value = 0.0;
    return true;
}

void _uw_hash_float(UwValuePtr self, UwHashContext* ctx)
{
    _uw_hash_uint64(ctx, self->type_id);

    uint64_t v = 0;
    uint8_t* p = (uint8_t*) &self->float_value;
    for (size_t i = 0; i < sizeof(UwType_Float); i++) {
        v <<= 8;
        v += *p++;
    }
    _uw_hash_uint64(ctx, v);
}

UwValuePtr _uw_copy_float(UwValuePtr self)
{
    UwValuePtr value = uw_create_float();
    if (value) {
        value->float_value = self->float_value;
    }
    return value;
}

void _uw_dump_float(UwValuePtr self, int indent, struct _UwValueChain* prev_compound)
{
    _uw_dump_start(self, indent);
    printf("%f\n", self->float_value);
}

UwValuePtr _uw_float_to_string(UwValuePtr self)
{
    // XXX not implemented yet
    return nullptr;
}

bool _uw_float_is_true(UwValuePtr self)
{
    return self->float_value;
}

bool _uw_float_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return self->float_value == other->float_value;
}

bool _uw_float_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        switch (t) {
            case UwTypeId_Int:   return self->float_value == (UwType_Float) other->int_value;
            case UwTypeId_Float: return self->float_value == other->float_value;
            default: {
                // check base class
                t = _uw_types[t]->ancestor_id;
                if (t == UwTypeId_Null) {
                    return false;
                }
            }
        }
    }
}

bool _uw_float_equal_ctype(UwValuePtr self, UwCType ctype, ...)
{
    bool result = false;
    va_list ap;
    va_start(ap);
    switch (ctype) {
        case uwc_char:      result = self->float_value == (UwType_Float)           (char) va_arg(ap,          int); break;
        case uwc_uchar:     result = self->float_value == (UwType_Float)  (unsigned char) va_arg(ap, unsigned int); break;
        case uwc_short:     result = self->float_value == (UwType_Float)          (short) va_arg(ap,          int); break;
        case uwc_ushort:    result = self->float_value == (UwType_Float) (unsigned short) va_arg(ap, unsigned int); break;

        case uwc_int:       result = self->float_value == (UwType_Float) va_arg(ap,                int); break;
        case uwc_uint:      result = self->float_value == (UwType_Float) va_arg(ap,       unsigned int); break;
        case uwc_long:      result = self->float_value == (UwType_Float) va_arg(ap,               long); break;
        case uwc_ulong:     result = self->float_value == (UwType_Float) va_arg(ap,      unsigned long); break;
        case uwc_longlong:  result = self->float_value == (UwType_Float) va_arg(ap,          long long); break;
        case uwc_ulonglong: result = self->float_value == (UwType_Float) va_arg(ap, unsigned long long); break;  // XXX can't handle all unsigned range
        case uwc_int32:     result = self->float_value == (UwType_Float) va_arg(ap,            int32_t); break;
        case uwc_uint32:    result = self->float_value == (UwType_Float) va_arg(ap,           uint32_t); break;
        case uwc_int64:     result = self->float_value == (UwType_Float) va_arg(ap,            int64_t); break;
        case uwc_uint64:    result = self->float_value == (UwType_Float) va_arg(ap,           uint64_t); break;  // XXX can't handle all unsigned range

        case uwc_float:     result = self->float_value == va_arg(ap, double /*float*/); break;
        case uwc_double:    result = self->float_value == va_arg(ap, double); break;

        case uwc_value_ptr:
        case uwc_value_makeref: { UwValuePtr other = va_arg(ap, UwValuePtr); result = _uw_float_equal(self, other); break; }

        default: break;
    }
    va_end(ap);
    return result;
}
