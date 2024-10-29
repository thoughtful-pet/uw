#include <stdarg.h>

#include "include/uw_value_base.h"
#include "src/uw_int_internal.h"

UwValuePtr _uw_create_int()
{
    UwValuePtr value = _uw_alloc_value();
    if (value) {
        value->type_id = UwTypeId_Int;
        value->refcount = 1;
        value->int_value = 0;
    }
    return value;
}

void _uw_hash_int(UwValuePtr self, UwHashContext* ctx)
{
    _uw_hash_uint64(ctx, self->type_id);
    _uw_hash_uint64(ctx, self->int_value);
}

UwValuePtr _uw_copy_int(UwValuePtr self)
{
    UwValuePtr value = _uw_create_int();
    if (value) {
        value->int_value = self->int_value;
    }
    return value;
}

void _uw_dump_int(UwValuePtr self, int indent)
{
    _uw_dump_start(self, indent);
    printf("%lld\n", (long long) self->int_value);
}

UwValuePtr _uw_int_to_string(UwValuePtr self)
{
    // XXX not implemented yet
    return nullptr;
}

bool _uw_int_is_true(UwValuePtr self)
{
    return self->int_value;
}

bool _uw_int_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return self->int_value == other->int_value;
}

bool _uw_int_equal(UwValuePtr self, UwValuePtr other)
{
    switch (other->type_id) {
        case UwTypeId_Bool:  return ((UwType_Bool)  self->int_value) == other->bool_value;
        case UwTypeId_Int:   return                 self->int_value  == other->int_value;
        case UwTypeId_Float: return ((UwType_Float) self->int_value) == other->float_value;
        default:             return false;
    }
}

bool _uw_int_equal_ctype(UwValuePtr self, UwCType ctype, ...)
{
    bool result = false;
    va_list ap;
    va_start(ap);
    switch (ctype) {
        case uwc_bool:      { bool a = va_arg(ap, int /*bool*/), b = self->int_value; result = a == b; break; }

        case uwc_char:      result = self->int_value ==           (char) va_arg(ap,          int); break;
        case uwc_uchar:     result = self->int_value ==  (unsigned char) va_arg(ap, unsigned int); break;
        case uwc_short:     result = self->int_value ==          (short) va_arg(ap,          int); break;
        case uwc_ushort:    result = self->int_value == (unsigned short) va_arg(ap, unsigned int); break;

        case uwc_int:       result = self->int_value == va_arg(ap,                int); break;
        case uwc_uint:      result = self->int_value == va_arg(ap,       unsigned int); break;
        case uwc_long:      result = self->int_value == va_arg(ap,               long); break;
        case uwc_ulong:     result = self->int_value == va_arg(ap,      unsigned long); break;
        case uwc_longlong:  result = self->int_value == va_arg(ap,          long long); break;
        case uwc_ulonglong: result = self->int_value == va_arg(ap, unsigned long long); break;  // XXX can't handle all unsigned range
        case uwc_int32:     result = self->int_value == va_arg(ap,            int32_t); break;
        case uwc_uint32:    result = self->int_value == va_arg(ap,           uint32_t); break;
        case uwc_int64:     result = self->int_value == va_arg(ap,            int64_t); break;
        case uwc_uint64:    result = self->int_value == va_arg(ap,           uint64_t); break;  // XXX can't handle all unsigned range

        case uwc_float:     result = self->int_value == (UwType_Int) va_arg(ap, double /*float*/); break;
        case uwc_double:    result = self->int_value == (UwType_Int) va_arg(ap, double); break;

        case uwc_value_ptr:
        case uwc_value:     { UwValuePtr other = va_arg(ap, UwValuePtr); result = _uw_int_equal(self, other); break; }

        default: break;
    }
    va_end(ap);
    return result;
}
