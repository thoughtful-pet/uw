#include <stdarg.h>

#include "include/uw_base.h"
#include "src/uw_bool_internal.h"

bool _uw_init_bool(UwValuePtr self)
{
    return true;
}

void _uw_hash_bool(UwValuePtr self, UwHashContext* ctx)
{
    _uw_hash_uint64(ctx, self->type_id);
    _uw_hash_uint64(ctx, self->bool_value);
}

UwValuePtr _uw_copy_bool(UwValuePtr self)
{
    UwValuePtr value = uw_create_bool();
    if (value) {
        value->bool_value = self->bool_value;
    }
    return value;
}

void _uw_dump_bool(UwValuePtr self, int indent)
{
    _uw_dump_start(self, indent);
    printf("%s\n", self->bool_value? "true" : "false");
}

UwValuePtr _uw_bool_to_string(UwValuePtr self)
{
    // XXX not implemented yet
    return nullptr;
}

bool _uw_bool_is_true(UwValuePtr self)
{
    return self->bool_value;
}

bool _uw_bool_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return self->bool_value == other->bool_value;
}

bool _uw_bool_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        switch (t) {
            case UwTypeId_Bool:  return self->bool_value == other->bool_value;
            case UwTypeId_Int:   return self->bool_value == (UwType_Bool) other->int_value;
            case UwTypeId_Float: return self->bool_value == (UwType_Bool) other->float_value;
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

bool _uw_bool_equal_ctype(UwValuePtr self, UwCType ctype, ...)
{
    bool result = false;
    va_list ap;
    va_start(ap);
    switch (ctype) {
        case uwc_bool:      { bool a = va_arg(ap, int /*bool*/); result = a == self->bool_value; break; }

        case uwc_char:      result = self->bool_value == (UwType_Bool)           (char) va_arg(ap,          int); break;
        case uwc_uchar:     result = self->bool_value == (UwType_Bool)  (unsigned char) va_arg(ap, unsigned int); break;
        case uwc_short:     result = self->bool_value == (UwType_Bool)          (short) va_arg(ap,          int); break;
        case uwc_ushort:    result = self->bool_value == (UwType_Bool) (unsigned short) va_arg(ap, unsigned int); break;

        case uwc_int:       result = self->bool_value == (UwType_Bool) va_arg(ap,                int); break;
        case uwc_uint:      result = self->bool_value == (UwType_Bool) va_arg(ap,       unsigned int); break;
        case uwc_long:      result = self->bool_value == (UwType_Bool) va_arg(ap,               long); break;
        case uwc_ulong:     result = self->bool_value == (UwType_Bool) va_arg(ap,      unsigned long); break;
        case uwc_longlong:  result = self->bool_value == (UwType_Bool) va_arg(ap,          long long); break;
        case uwc_ulonglong: result = self->bool_value == (UwType_Bool) va_arg(ap, unsigned long long); break;  // XXX can't handle all unsigned range
        case uwc_int32:     result = self->bool_value == (UwType_Bool) va_arg(ap,            int32_t); break;
        case uwc_uint32:    result = self->bool_value == (UwType_Bool) va_arg(ap,           uint32_t); break;
        case uwc_int64:     result = self->bool_value == (UwType_Bool) va_arg(ap,            int64_t); break;
        case uwc_uint64:    result = self->bool_value == (UwType_Bool) va_arg(ap,           uint64_t); break;  // XXX can't handle all unsigned range

        case uwc_float:     result = self->bool_value == (UwType_Bool) va_arg(ap, double /*float*/); break;
        case uwc_double:    result = self->bool_value == (UwType_Bool) va_arg(ap, double); break;

        case uwc_value_ptr:
        case uwc_value_makeref: { UwValuePtr other = va_arg(ap, UwValuePtr); result = _uw_bool_equal(self, other); break; }

        default: break;
    }
    va_end(ap);
    return result;
}
