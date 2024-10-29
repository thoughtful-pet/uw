#include <stdarg.h>

#include "include/uw_value_base.h"
#include "include/uw_ctype.h"
#include "include/uw_string.h"
#include "src/uw_bool_internal.h"
#include "src/uw_int_internal.h"
#include "src/uw_float_internal.h"
#include "src/uw_null_internal.h"
#include "src/uw_null_internal.h"

UwValuePtr uw_create_from_ctype(UwCType ctype, va_list args)
{
    switch (ctype) {
        case uwc_nullptr:   va_arg(args, void*); return _uw_create_null();  // XXX something I don't get about nullptr_t in va_arg

        case uwc_bool:      return _uwc_create_bool ((bool) va_arg(args, int /*bool*/));

        case uwc_char:      return _uwc_create_int  (               va_arg(args, int /*char*/));
        case uwc_uchar:     return _uwc_create_int  ((unsigned int) va_arg(args, unsigned int /*char*/));
        case uwc_short:     return _uwc_create_int  (               va_arg(args, int /*short*/));
        case uwc_ushort:    return _uwc_create_int  ((unsigned int) va_arg(args, unsigned int /*short*/));
        case uwc_int:       return _uwc_create_int  (               va_arg(args, int));
        case uwc_uint:      return _uwc_create_int  ((unsigned int) va_arg(args, unsigned int));
        case uwc_long:      return _uwc_create_int  (               va_arg(args, long));
        case uwc_ulong:     return _uwc_create_int  ((unsigned int) va_arg(args, unsigned long));
        case uwc_longlong:  return _uwc_create_int  (               va_arg(args, long long));
        case uwc_ulonglong: return _uwc_create_int  (               va_arg(args, unsigned long long));  // XXX can't handle all unsigned range
        case uwc_int32:     return _uwc_create_int  (               va_arg(args, int32_t));
        case uwc_uint32:    return _uwc_create_int  ((unsigned int) va_arg(args, uint32_t));
        case uwc_int64:     return _uwc_create_int  (               va_arg(args, int64_t));
        case uwc_uint64:    return _uwc_create_int  (               va_arg(args, uint64_t));  // XXX can't handle all unsigned range

        case uwc_float:     return _uwc_create_float(va_arg(args, double /*float*/));
        case uwc_double:    return _uwc_create_float(va_arg(args, double));

        case uwc_charptr:   return _uw_create_string_c  (va_arg(args, char*));
        case uwc_char8ptr:  return _uw_create_string_u8 (va_arg(args, char8_t*));
        case uwc_char32ptr: return _uw_create_string_u32(va_arg(args, char32_t*));

        case uwc_value_ptr: return va_arg(args, UwValuePtr);
        case uwc_value:     { UwValuePtr v = va_arg(args, UwValuePtr); return uw_makeref(v); }
        default:
            // panic
            fprintf(stderr, "%s: bad C type identifier %d\n", __func__, ctype);
            exit(1);
    }
}

UwValuePtr _uwc_create_null(nullptr_t dummy)
{
    return _uw_create_null();
}

UwValuePtr _uwc_create_bool(UwType_Bool initializer)
{
    UwValuePtr value = _uw_create_bool();
    value->bool_value = initializer;
    return value;
}

UwValuePtr _uwc_create_int(UwType_Int initializer)
{
    UwValuePtr value = _uw_create_int();
    value->int_value = initializer;
    return value;
}

UwValuePtr _uwc_create_float(UwType_Float initializer)
{
    UwValuePtr value = _uw_create_float();
    value->float_value = initializer;
    return value;
}
