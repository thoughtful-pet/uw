#include "include/uw_base.h"
#include "src/uw_unsigned_internal.h"

UwResult _uw_unsigned_create(UwTypeId type_id, va_list ap)
{
    return UwUnsigned(0);
}

void _uw_unsigned_hash(UwValuePtr self, UwHashContext* ctx)
{
    // mind maps: same signed and unsigned integers must have same hash,
    // so using UwTypeId_Unsigned, not self->type_id
    _uw_hash_uint64(ctx, UwTypeId_Unsigned);
    _uw_hash_uint64(ctx, self->unsigned_value);
}

void _uw_unsigned_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    fprintf(fp, ": %llu\n", (unsigned long long) self->unsigned_value);
}

UwResult _uw_unsigned_to_string(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

bool _uw_unsigned_is_true(UwValuePtr self)
{
    return self->unsigned_value;
}

bool _uw_unsigned_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return self->unsigned_value == other->unsigned_value;
}

bool _uw_unsigned_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        switch (t) {
            case UwTypeId_Signed:
                if (other->signed_value < 0) {
                    return false;
                } else {
                    return ((UwType_Signed) self->unsigned_value) == other->signed_value;
                }

            case UwTypeId_Unsigned:
                return self->unsigned_value == other->unsigned_value;

            case UwTypeId_Float:
                return self->unsigned_value == other->float_value;

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
