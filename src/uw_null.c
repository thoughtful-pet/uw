#include <stdarg.h>

#include "include/uw_base.h"
#include "src/uw_null_internal.h"

UwValuePtr _uw_create_null()
{
    UwValuePtr value = _uw_alloc_value();
    if (value) {
        value->type_id = UwTypeId_Null;
        value->refcount = 1;
        value->null_value = nullptr;
    }
    return value;
}

void _uw_hash_null(UwValuePtr self, UwHashContext* ctx)
{
    _uw_hash_uint64(ctx, self->type_id);
}

UwValuePtr _uw_copy_null(UwValuePtr self)
{
    return _uw_create_null();
}

void _uw_dump_null(UwValuePtr self, int indent)
{
    _uw_dump_start(self, indent);
    putchar('\n');
}

UwValuePtr _uw_null_to_string(UwValuePtr self)
{
    // XXX not implemented yet
    return nullptr;
}

bool _uw_null_is_true(UwValuePtr self)
{
    return false;
}

bool _uw_null_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return true;
}

bool _uw_null_equal(UwValuePtr self, UwValuePtr other)
{
    return uw_is_null(other);
}

bool _uw_null_equal_ctype(UwValuePtr self, UwCType ctype, ...)
{
    bool result = false;
    va_list ap;
    va_start(ap);
    switch (ctype) {
        case uwc_nullptr:   result = true; break;
        case uwc_value_ptr:
        case uwc_value: {
            UwValuePtr other = va_arg(ap, UwValuePtr);
            result = uw_is_null(other);
            break;
        }
        default: break;
    }
    va_end(ap);
    return result;
}
