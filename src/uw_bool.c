#include "include/uw_base.h"
#include "include/uw_string.h"
#include "src/uw_bool_internal.h"

UwResult _uw_bool_create(UwTypeId type_id, va_list ap)
{
    return UwBool(false);
}

void _uw_bool_hash(UwValuePtr self, UwHashContext* ctx)
{
    // mind maps: the hash should be the same for subtypes, that's why not using self->type_id here
    _uw_hash_uint64(ctx, UwTypeId_Bool);
    _uw_hash_uint64(ctx, self->bool_value);
}

void _uw_bool_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    fprintf(fp, ": %s\n", self->bool_value? "true" : "false");
}

UwResult _uw_bool_to_string(UwValuePtr self)
{
    if (self->bool_value) {
        return UwString_1_12(4, 't', 'r', 'u', 'e', 0, 0, 0, 0, 0, 0, 0, 0);
    } else {
        return UwString_1_12(5, 'f', 'a', 'l', 's', 'e', 0, 0, 0, 0, 0, 0, 0);
    }
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
            case UwTypeId_Bool:
                return self->bool_value == other->bool_value;

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
