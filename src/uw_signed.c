#include "include/uw_base.h"
#include "src/uw_signed_internal.h"

UwResult _uw_signed_create(UwTypeId type_id, va_list ap)
{
    return UwSigned(0);
}

void _uw_signed_hash(UwValuePtr self, UwHashContext* ctx)
{
    // mind maps: same signed and unsigned integers must have same hash, so
    if (self->signed_value < 0) {
        _uw_hash_uint64(ctx, UwTypeId_Signed);
    } else {
        _uw_hash_uint64(ctx, UwTypeId_Unsigned);
    }
    _uw_hash_uint64(ctx, self->signed_value);
}

void _uw_signed_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    fprintf(fp, ": %lld\n", (long long) self->signed_value);
}

UwResult _uw_signed_to_string(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

bool _uw_signed_is_true(UwValuePtr self)
{
    return self->signed_value;
}

bool _uw_signed_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return self->signed_value == other->signed_value;
}

bool _uw_signed_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        switch (t) {
            case UwTypeId_Signed:
                return self->signed_value == other->signed_value;

            case UwTypeId_Unsigned:
                if (self->signed_value < 0) {
                    return false;
                } else {
                    return self->signed_value == (UwType_Signed) other->unsigned_value;
                }

            case UwTypeId_Float:
                return self->signed_value == other->float_value;

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
