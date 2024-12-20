#include "include/uw_base.h"
#include "src/uw_struct_internal.h"

UwResult _uw_struct_create(UwTypeId type_id, va_list ap)
{
    _UwValue result = {
        .type_id = UwTypeId_Struct,
        .extra_data = nullptr
    };
    return result;
}

void _uw_struct_hash(UwValuePtr self, UwHashContext* ctx)
/*
 * Basic hashing.
 */
{
    _uw_hash_uint64(ctx, self->type_id);
}

UwResult _uw_struct_deepcopy(UwValuePtr self)
/*
 * Bare struct cannot be copied, this method must be defined in a subtype.
 */
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

void _uw_struct_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    _uw_dump_base_extra_data(fp, self->extra_data);
    fputc('\n', fp);
}

UwResult _uw_struct_to_string(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

bool _uw_struct_is_true(UwValuePtr self)
/*
 * Bare struct is always false.
 */
{
    return false;
}

bool _uw_struct_equal_sametype(UwValuePtr self, UwValuePtr other)
/*
 * Bare struct is not equal to anything.
 */
{
    return false;
}

bool _uw_struct_equal(UwValuePtr self, UwValuePtr other)
/*
 * Bare struct is not equal to anything.
 */
{
    return false;
}
