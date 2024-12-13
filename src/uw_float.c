#include "include/uw_base.h"
#include "src/uw_float_internal.h"

UwResult _uw_float_create(UwTypeId type_id, va_list ap)
{
    return UwFloat(0.0);
}

void _uw_float_hash(UwValuePtr self, UwHashContext* ctx)
{
    // mind maps: the hash should be the same for subclasses, that's why not using self->type_id here
    _uw_hash_uint64(ctx, UwTypeId_Float);
    _uw_hash_buffer(ctx, &self->float_value, sizeof(self->float_value));
}

void _uw_float_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    fprintf(fp, ": %f\n", self->float_value);
}

UwResult _uw_float_to_string(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
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
            case UwTypeId_Signed:      return self->float_value == (UwType_Float) other->signed_value;
            case UwTypeId_Unsigned: return self->float_value == (UwType_Float) other->unsigned_value;
            case UwTypeId_Float:    return self->float_value == other->float_value;
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
