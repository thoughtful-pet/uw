#include "include/uw_base.h"
#include "include/uw_string.h"
#include "src/uw_null_internal.h"

UwResult _uw_null_create(UwTypeId type_id, va_list ap)
{
    return UwNull();
}

void _uw_null_hash(UwValuePtr self, UwHashContext* ctx)
{
    _uw_hash_uint64(ctx, UwTypeId_Null);
}

void _uw_null_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    fputc('\n', fp);
}

UwResult _uw_null_to_string(UwValuePtr self)
{
    return UwString_1_12(4, 'n', 'u', 'l', 'l', 0, 0, 0, 0, 0, 0, 0, 0);
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
    UwTypeId t = other->type_id;
    for (;;) {
        switch (t) {
            case UwTypeId_Null:
                return true;

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
