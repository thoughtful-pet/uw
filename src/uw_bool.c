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

void _uw_dump_bool(UwValuePtr self, int indent, struct _UwValueChain* prev_compound)
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
        if (t == UwTypeId_Bool) {
            return self->bool_value == other->bool_value;
        } else {
            // check base class
            t = _uw_types[t]->ancestor_id;
            if (t == UwTypeId_Null) {
                return false;
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

        case uwc_value_ptr:
        case uwc_value_makeref: { UwValuePtr other = va_arg(ap, UwValuePtr); result = _uw_bool_equal(self, other); break; }

        default: break;
    }
    va_end(ap);
    return result;
}
