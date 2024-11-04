#include "include/uw_base.h"
#include "src/uw_class_internal.h"

UwValuePtr _uw_create_class()
{
    return _uw_alloc_value(UwTypeId_Class);
}

void _uw_destroy_class(UwValuePtr self)
/*
 * Subclass can inherit this basic method,
 * so go down inheritance chain and call destructors of all base classes.
 */
{
    if (!self) {
        return;
    }
    UwType* type = _uw_types[self->type_id];
    uw_assert(type != nullptr);

    while (type->ancestor_id != UwTypeId_Null) {
        type->destroy(self);
        type = _uw_types[type->ancestor_id];
    }

    if (type->id == UwTypeId_Class) {
        // self destruction
        _uw_free_value(self);
    } else {
        type->destroy(self);
    }
}

void _uw_hash_class(UwValuePtr self, UwHashContext* ctx)
/*
 * Basic hashing.
 */
{
    _uw_hash_uint64(ctx, self->type_id);
}

UwValuePtr _uw_copy_class(UwValuePtr self)
/*
 * Bare class cannot be copied, this method must be defined in a subclass.
 */
{
    return nullptr;
}

void _uw_dump_class(UwValuePtr self, int indent)
{
    _uw_dump_start(self, indent);
    putchar('\n');
}

UwValuePtr _uw_class_to_string(UwValuePtr self)
{
    // XXX not implemented yet
    return nullptr;
}

bool _uw_class_is_true(UwValuePtr self)
/*
 * Bare class is always false.
 */
{
    return false;
}

bool _uw_class_equal_sametype(UwValuePtr self, UwValuePtr other)
/*
 * Bare class is not equal to anything.
 */
{
    return false;
}

bool _uw_class_equal(UwValuePtr self, UwValuePtr other)
/*
 * Bare class is not equal to anything.
 */
{
    return false;
}

bool _uw_class_equal_ctype(UwValuePtr self, UwCType ctype, ...)
/*
 * Bare class is not equal to anything.
 */
{
    return false;
}
