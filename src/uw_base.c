#include "include/uw_base.h"
#include "src/uw_null_internal.h"
#include "src/uw_bool_internal.h"
#include "src/uw_hash_internal.h"
#include "src/uw_int_internal.h"
#include "src/uw_float_internal.h"
#include "src/uw_string_internal.h"
#include "src/uw_list_internal.h"
#include "src/uw_map_internal.h"

/****************************************************************
 * Basic functions
 */

UwValuePtr _uw_create(UwTypeId type_id)
{
    UwMethodCreate fn = _uw_types[type_id].create;
    uw_assert(fn != nullptr);
    return fn();
}

void uw_delete(UwValuePtr* value)
{
    if (!value) {
        return;
    }

    UwValuePtr v = *value;
    if (!v) {
        return;
    }

    uw_assert(v->refcount);
    if (0 == --v->refcount) {
        _uw_call_destroy(v);
    }
    *value = nullptr;
}


UwType_Hash uw_hash(UwValuePtr value)
{
    UwHashContext ctx;
    _uw_hash_init(&ctx);
    _uw_call_hash(value, &ctx);
    return _uw_hash_finish(&ctx);
}

static void _uw_destroy_integral(UwValuePtr self)
/*
 * Common destructor for integral types.
 */
{
    if (self) {
        _uw_free_value(self);
    }
}

/****************************************************************
 * Dump functions.
 */

void _uw_dump_start(UwValuePtr value, int indent)
{
    char* type_name = _uw_types[value->type_id].name;

    _uw_print_indent(indent);
    printf("%p", value);
    if (type_name == nullptr) {
        printf(" BAD TYPE %d", value->type_id);
    } else {
        printf(" %s (%d)", type_name,value->type_id);
    }
    printf(" refcount=%zu; ", value->refcount);
}

void _uw_print_indent(int indent)
{
    for (int i = 0; i < indent; i++ ) {
        putchar(' ');
    }
}

void _uw_dump(UwValuePtr value, int indent)
{
    UwMethodDump fn_dump = _uw_types[value->type_id].dump;
    uw_assert(fn_dump != nullptr);
    return fn_dump(value, 0);
}

void uw_dump(UwValuePtr value)
{
    _uw_dump(value, 0);
}

/****************************************************************
 * Global list of types initialized with built-in types.
 */

UwType _uw_types[1 << UW_TYPE_BITWIDTH] = {

    [UwTypeId_Null] = {
        .id               = UwTypeId_Null,
        .ancestor_id      = UwTypeId_Null,  // no ancestor; Null can't be an ancestor for any type
        .name             = "Null",
        .create           = _uw_create_null,
        .destroy          = _uw_destroy_integral,
        .hash             = _uw_hash_null,
        .copy             = _uw_copy_null,
        .dump             = _uw_dump_null,
        .to_string        = _uw_null_to_string,
        .is_true          = _uw_null_is_true,
        .equal_sametype   = _uw_null_equal_sametype,
        .equal            = _uw_null_equal,
        .equal_ctype      = _uw_null_equal_ctype
    },

    [UwTypeId_Bool] = {
        .id               = UwTypeId_Bool,
        .ancestor_id      = UwTypeId_Null,  // no ancestor
        .name             = "Bool",
        .create           = _uw_create_bool,
        .destroy          = _uw_destroy_integral,
        .hash             = _uw_hash_bool,
        .copy             = _uw_copy_bool,
        .dump             = _uw_dump_bool,
        .to_string        = _uw_bool_to_string,
        .is_true          = _uw_bool_is_true,
        .equal_sametype   = _uw_bool_equal_sametype,
        .equal            = _uw_bool_equal,
        .equal_ctype      = _uw_bool_equal_ctype
    },

    [UwTypeId_Int] = {
        .id               = UwTypeId_Int,
        .ancestor_id      = UwTypeId_Null,  // no ancestor
        .name             = "Int",
        .create           = _uw_create_int,
        .destroy          = _uw_destroy_integral,
        .hash             = _uw_hash_int,
        .copy             = _uw_copy_int,
        .dump             = _uw_dump_int,
        .to_string        = _uw_int_to_string,
        .is_true          = _uw_int_is_true,
        .equal_sametype   = _uw_int_equal_sametype,
        .equal            = _uw_int_equal,
        .equal_ctype      = _uw_int_equal_ctype
    },

    [UwTypeId_Float] = {
        .id               = UwTypeId_Float,
        .ancestor_id      = UwTypeId_Null,  // no ancestor
        .name             = "Float",
        .create           = _uw_create_float,
        .destroy          = _uw_destroy_integral,
        .hash             = _uw_hash_float,
        .copy             = _uw_copy_float,
        .dump             = _uw_dump_float,
        .to_string        = _uw_float_to_string,
        .is_true          = _uw_float_is_true,
        .equal_sametype   = _uw_float_equal_sametype,
        .equal            = _uw_float_equal,
        .equal_ctype      = _uw_float_equal_ctype
    },

    [UwTypeId_String] = {
        .id               = UwTypeId_String,
        .ancestor_id      = UwTypeId_Null,  // no ancestor
        .name             = "String",
        .create           = _uw_create_string,
        .destroy          = _uw_destroy_string,
        .hash             = _uw_hash_string,
        .copy             = _uw_copy_string,
        .dump             = _uw_dump_string,
        .to_string        = _uw_copy_string,  // yes, simply make a copy
        .is_true          = _uw_string_is_true,
        .equal_sametype   = _uw_string_equal_sametype,
        .equal            = _uw_string_equal,
        .equal_ctype      = _uw_string_equal_ctype
    },

    [UwTypeId_List] = {
        .id               = UwTypeId_List,
        .ancestor_id      = UwTypeId_Null,  // no ancestor
        .name             = "List",
        .create           = _uw_create_list,
        .destroy          = _uw_destroy_list,
        .hash             = _uw_hash_list,
        .copy             = _uw_copy_list,
        .dump             = _uw_dump_list,
        .to_string        = _uw_list_to_string,
        .is_true          = _uw_list_is_true,
        .equal_sametype   = _uw_list_equal_sametype,
        .equal            = _uw_list_equal,
        .equal_ctype      = _uw_list_equal_ctype
    },

    [UwTypeId_Map] = {
        .id               = UwTypeId_Map,
        .ancestor_id      = UwTypeId_Null,  // no ancestor
        .name             = "Map",
        .create           = _uw_create_map,
        .destroy          = _uw_destroy_map,
        .hash             = _uw_hash_map,
        .copy             = _uw_copy_map,
        .dump             = _uw_dump_map,
        .to_string        = _uw_map_to_string,
        .is_true          = _uw_map_is_true,
        .equal_sametype   = _uw_map_equal_sametype,
        .equal            = _uw_map_equal,
        .equal_ctype      = _uw_map_equal_ctype
    }
};
