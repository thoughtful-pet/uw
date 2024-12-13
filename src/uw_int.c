#include "include/uw_base.h"
#include "src/uw_int_internal.h"

UwResult _uw_int_create(UwTypeId type_id, va_list ap)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

void _uw_int_hash(UwValuePtr self, UwHashContext* ctx)
{
    _uw_hash_uint64(ctx, UwTypeId_Int);
}

void _uw_int_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    fputs(": abstract\n", fp);
}

UwResult _uw_int_to_string(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

bool _uw_int_is_true(UwValuePtr self)
{
    return self->signed_value;
}

bool _uw_int_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return true;
}

bool _uw_int_equal(UwValuePtr self, UwValuePtr other)
{
    return false;
}
