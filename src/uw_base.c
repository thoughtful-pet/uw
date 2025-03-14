#ifndef _GNU_SOURCE
#   define _GNU_SOURCE
#endif

#include <unistd.h>
#include <sys/mman.h>

#include "include/uw_base.h"
#include "include/uw_file.h"
#include "include/uw_string.h"
#include "src/uw_charptr_internal.h"
#include "src/uw_hash_internal.h"
#include "src/uw_list_internal.h"
#include "src/uw_map_internal.h"
#include "src/uw_string_internal.h"

/****************************************************************
 * Basic functions
 */

[[noreturn]]
void uw_panic(char* fmt, ...)
{
    va_list ap;
    va_start(ap);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(1);
}

UwResult _uw_create(UwTypeId type_id, ...)
/*
 * Constructor.
 */
{
    va_list ap;
    va_start(ap);
    UwValue result = _uw_types[type_id]->_create(type_id, ap);
    va_end(ap);
    return uw_move(&result);
}

bool _uw_alloc_extra_data(UwValuePtr v)
{
    UwType* t = _uw_types[v->type_id];
    unsigned memsize = t->data_offset + t->data_size;
    if (memsize) {
        _UwExtraData* extra_data = t->allocator->allocate(memsize, true);
        if (!extra_data) {
            return false;
        }
        extra_data->refcount = 1;
        v->extra_data = extra_data;
    } else {
        // extra_data is optional, not creating
        v->extra_data = nullptr;
    }
    return true;
}

void _uw_free_extra_data(UwValuePtr v)
{
    if (v->extra_data) {
        UwType* t = _uw_types[v->type_id];
        unsigned memsize = t->data_offset + t->data_size;
        if (memsize) {
            t->allocator->release((void**) &v->extra_data, memsize);
        } else {
            uw_dump(stderr, v);
            uw_panic("Extra data is allocated, but memsize evaluates to zero");
        }
    }
}

UwResult _uw_default_create(UwTypeId type_id, va_list ap)
{
    UwValue result = {};
    result.type_id    = type_id;
    result.extra_data = nullptr;
    if (!_uw_alloc_extra_data(&result)) {
        return UwOOM();
    }
    UwMethodInit fn_init = _uw_types[type_id]->_init;
    if (!fn_init) {
        return uw_move(&result);
    }
    UwValue status = fn_init(&result, ap);
    if (uw_ok(&status)) {
        return uw_move(&result);
    } else {
        return uw_move(&status);
    }
}

UwResult _uw_default_clone(UwValuePtr self)
{
    if (self->extra_data) {
        self->extra_data->refcount++;
    }
    return *self;
}

void _uw_default_destroy(UwValuePtr self)
{
    _UwExtraData* extra_data = self->extra_data;

    if (!extra_data) {
        return;
    }
    if (extra_data->refcount) {
        extra_data->refcount--;
    }
    if (extra_data->refcount) {
        return;
    }
    if (_uw_types[self->type_id]->compound) {

        _UwCompoundData* cdata = (_UwCompoundData*) self->extra_data;

        if (cdata->destroying) {
            return;
        }

        if (_uw_is_embraced(cdata)) {

            // we have a parent, check if there is a cyclic reference
            if (!_uw_need_break_cyclic_refs(cdata)) {
                // can't destroy self, because it's still a part of some object
                return;
            }
            // there are breakable cyclic references, okay to destroy
        }
        cdata->destroying = true;
    }
    // finalize value
    _uw_call_fini(self);
    _uw_free_extra_data(self);

    // reset value
    self->type_id = UwTypeId_Null;
}

UwType_Hash uw_hash(UwValuePtr value)
{
    UwHashContext ctx;
    _uw_hash_init(&ctx);
    _uw_call_hash(value, &ctx);
    return _uw_hash_finish(&ctx);
}

UwValuePtr _uw_on_chain(UwValuePtr value, _UwCompoundChain* tail)
{
    while (tail) {
        if (value->extra_data == tail->value->extra_data) {
            return tail->value;
        }
        tail = tail->prev;
    }
    return nullptr;
}

/****************************************************************
 * Dump functions.
 */

void _uw_dump_start(FILE* fp, UwValuePtr value, int indent)
{
    char* type_name = _uw_types[value->type_id]->name;

    _uw_print_indent(fp, indent);
    fprintf(fp, "%p", value);
    if (type_name == nullptr) {
        fprintf(fp, " BAD TYPE %d", value->type_id);
    } else {
        fprintf(fp, " %s (type id: %d)", type_name, value->type_id);
    }
}

void _uw_dump_base_extra_data(FILE* fp, _UwExtraData* extra_data)
{
    if (extra_data) {
        fprintf(fp, " extra data %p refcount=%u;", extra_data, extra_data->refcount);
    } else {
        fprintf(fp, " ERROR: extra data is NULL;");
    }
}

void _uw_print_indent(FILE* fp, int indent)
{
    for (int i = 0; i < indent; i++ ) {
        fputc(' ', fp);
    }
}

void _uw_dump(FILE* fp, UwValuePtr value, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    if (value == nullptr) {
        _uw_print_indent(fp, first_indent);
        fprintf(fp, "nullptr\n");
        return;
    }
    UwMethodDump fn_dump = _uw_types[value->type_id]->_dump;
    uw_assert(fn_dump != nullptr);
    fn_dump(value, fp, first_indent, next_indent, tail);
}

void uw_dump(FILE* fp, UwValuePtr value)
{
    _uw_dump(fp, value, 0, 0, nullptr);
}

/****************************************************************
 * Global list of interfaces
 */

static unsigned num_registered_interfaces = 0;

unsigned uw_register_interface()
{
    if (num_registered_interfaces == UINT_MAX) {
        fprintf(stderr, "Cannot define more interfaces than %u\n", UINT_MAX);
        return 0;
    }
    return num_registered_interfaces++;
}

// Miscellaneous interfaces
unsigned UwInterfaceId_File;
unsigned UwInterfaceId_FileReader;
unsigned UwInterfaceId_FileWriter;
unsigned UwInterfaceId_LineReader;

/****************************************************************
 * Null type
 */

static UwResult null_create(UwTypeId type_id, va_list ap)
{
    return UwNull();
}

static void null_hash(UwValuePtr self, UwHashContext* ctx)
{
    _uw_hash_uint64(ctx, UwTypeId_Null);
}

static void null_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    fputc('\n', fp);
}

static UwResult null_to_string(UwValuePtr self)
{
    return UwString_1_12(4, 'n', 'u', 'l', 'l', 0, 0, 0, 0, 0, 0, 0, 0);
}

static bool null_is_true(UwValuePtr self)
{
    return false;
}

static bool null_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return true;
}

static bool null_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        switch (t) {
            case UwTypeId_Null:
                return true;

            case UwTypeId_CharPtr:
            case UwTypeId_Ptr:
                return other->ptr == nullptr;

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

static UwType null_type = {
    .id              = UwTypeId_Null,
    .ancestor_id     = UwTypeId_Null,  // no ancestor; Null can't be an ancestor for any type
    .name            = "Null",
    .allocator       = nullptr,
    .data_offset     = 0,
    .data_size       = 0,
    .compound        = false,
    ._create         = null_create,
    ._destroy        = nullptr,
    ._init           = nullptr,
    ._fini           = nullptr,
    ._clone          = nullptr,
    ._hash           = null_hash,
    ._deepcopy       = nullptr,
    ._dump           = null_dump,
    ._to_string      = null_to_string,
    ._is_true        = null_is_true,
    ._equal_sametype = null_equal_sametype,
    ._equal          = null_equal,

    .num_interfaces  = 0,
    .interfaces      = nullptr
};

/****************************************************************
 * Bool type
 */

static UwResult bool_create(UwTypeId type_id, va_list ap)
{
    return UwBool(false);
}

static void bool_hash(UwValuePtr self, UwHashContext* ctx)
{
    // mind maps: the hash should be the same for subtypes, that's why not using self->type_id here
    _uw_hash_uint64(ctx, UwTypeId_Bool);
    _uw_hash_uint64(ctx, self->bool_value);
}

static void bool_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    fprintf(fp, ": %s\n", self->bool_value? "true" : "false");
}

static UwResult bool_to_string(UwValuePtr self)
{
    if (self->bool_value) {
        return UwString_1_12(4, 't', 'r', 'u', 'e', 0, 0, 0, 0, 0, 0, 0, 0);
    } else {
        return UwString_1_12(5, 'f', 'a', 'l', 's', 'e', 0, 0, 0, 0, 0, 0, 0);
    }
}

static bool bool_is_true(UwValuePtr self)
{
    return self->bool_value;
}

static bool bool_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return self->bool_value == other->bool_value;
}

static bool bool_equal(UwValuePtr self, UwValuePtr other)
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

static UwType bool_type = {
    .id              = UwTypeId_Bool,
    .ancestor_id     = UwTypeId_Null,  // no ancestor
    .name            = "Bool",
    .allocator       = nullptr,
    .data_offset     = 0,
    .data_size       = 0,
    .compound        = false,
    ._create         = bool_create,
    ._destroy        = nullptr,
    ._init           = nullptr,
    ._fini           = nullptr,
    ._clone          = nullptr,
    ._hash           = bool_hash,
    ._deepcopy       = nullptr,
    ._dump           = bool_dump,
    ._to_string      = bool_to_string,
    ._is_true        = bool_is_true,
    ._equal_sametype = bool_equal_sametype,
    ._equal          = bool_equal,

    .num_interfaces  = 0,
    .interfaces      = nullptr
        // [UwInterfaceId_Logic] = &bool_type_logic_interface
};

/****************************************************************
 * Abstract Integer type
 */

static UwResult int_create(UwTypeId type_id, va_list ap)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

static void int_hash(UwValuePtr self, UwHashContext* ctx)
{
    _uw_hash_uint64(ctx, UwTypeId_Int);
}

static void int_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    fputs(": abstract\n", fp);
}

static UwResult int_to_string(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

static bool int_is_true(UwValuePtr self)
{
    return self->signed_value;
}

static bool int_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return true;
}

static bool int_equal(UwValuePtr self, UwValuePtr other)
{
    return false;
}

static UwType int_type = {
    .id              = UwTypeId_Int,
    .ancestor_id     = UwTypeId_Null,  // no ancestor
    .name            = "Int",
    .allocator       = nullptr,
    .data_offset     = 0,
    .data_size       = 0,
    .compound        = false,
    ._create         = int_create,
    ._destroy        = nullptr,
    ._init           = nullptr,
    ._fini           = nullptr,
    ._clone          = nullptr,
    ._hash           = int_hash,
    ._deepcopy       = nullptr,
    ._dump           = int_dump,
    ._to_string      = int_to_string,
    ._is_true        = int_is_true,
    ._equal_sametype = int_equal_sametype,
    ._equal          = int_equal,

    .num_interfaces  = 0,
    .interfaces      = nullptr
        // [UwInterfaceId_Logic]      = &int_type_logic_interface,
        // [UwInterfaceId_Arithmetic] = &int_type_arithmetic_interface,
        // [UwInterfaceId_Bitwise]    = &int_type_bitwise_interface,
        // [UwInterfaceId_Comparison] = &int_type_comparison_interface
};

/****************************************************************
 * Signed type
 */

static UwResult signed_create(UwTypeId type_id, va_list ap)
{
    return UwSigned(0);
}

static void signed_hash(UwValuePtr self, UwHashContext* ctx)
{
    // mind maps: same signed and unsigned integers must have same hash, so
    if (self->signed_value < 0) {
        _uw_hash_uint64(ctx, UwTypeId_Signed);
    } else {
        _uw_hash_uint64(ctx, UwTypeId_Unsigned);
    }
    _uw_hash_uint64(ctx, self->signed_value);
}

static void signed_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    fprintf(fp, ": %lld\n", (long long) self->signed_value);
}

static UwResult signed_to_string(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

static bool signed_is_true(UwValuePtr self)
{
    return self->signed_value;
}

static bool signed_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return self->signed_value == other->signed_value;
}

static bool signed_equal(UwValuePtr self, UwValuePtr other)
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

static UwType signed_type = {
    .id              = UwTypeId_Signed,
    .ancestor_id     = UwTypeId_Int,
    .name            = "Signed",
    .allocator       = nullptr,
    .data_offset     = 0,
    .data_size       = 0,
    .compound        = false,
    ._create         = signed_create,
    ._destroy        = nullptr,
    ._init           = nullptr,
    ._fini           = nullptr,
    ._clone          = nullptr,
    ._hash           = signed_hash,
    ._deepcopy       = nullptr,
    ._dump           = signed_dump,
    ._to_string      = signed_to_string,
    ._is_true        = signed_is_true,
    ._equal_sametype = signed_equal_sametype,
    ._equal          = signed_equal,

    .num_interfaces  = 0,
    .interfaces      = nullptr
        // [UwInterfaceId_Logic]      = &int_type_logic_interface,
        // [UwInterfaceId_Arithmetic] = &int_type_arithmetic_interface,
        // [UwInterfaceId_Bitwise]    = &int_type_bitwise_interface,
        // [UwInterfaceId_Comparison] = &int_type_comparison_interface
};

/****************************************************************
 * Unsigned type
 */

static UwResult unsigned_create(UwTypeId type_id, va_list ap)
{
    return UwUnsigned(0);
}

static void unsigned_hash(UwValuePtr self, UwHashContext* ctx)
{
    // mind maps: same signed and unsigned integers must have same hash,
    // so using UwTypeId_Unsigned, not self->type_id
    _uw_hash_uint64(ctx, UwTypeId_Unsigned);
    _uw_hash_uint64(ctx, self->unsigned_value);
}

static void unsigned_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    fprintf(fp, ": %llu\n", (unsigned long long) self->unsigned_value);
}

static UwResult unsigned_to_string(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

static bool unsigned_is_true(UwValuePtr self)
{
    return self->unsigned_value;
}

static bool unsigned_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return self->unsigned_value == other->unsigned_value;
}

static bool unsigned_equal(UwValuePtr self, UwValuePtr other)
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

static UwType unsigned_type = {
    .id              = UwTypeId_Unsigned,
    .ancestor_id     = UwTypeId_Int,
    .name            = "Unsigned",
    .allocator       = nullptr,
    .data_offset     = 0,
    .data_size       = 0,
    .compound        = false,
    ._create         = unsigned_create,
    ._destroy        = nullptr,
    ._init           = nullptr,
    ._fini           = nullptr,
    ._clone          = nullptr,
    ._hash           = unsigned_hash,
    ._deepcopy       = nullptr,
    ._dump           = unsigned_dump,
    ._to_string      = unsigned_to_string,
    ._is_true        = unsigned_is_true,
    ._equal_sametype = unsigned_equal_sametype,
    ._equal          = unsigned_equal,

    .num_interfaces  = 0,
    .interfaces      = nullptr
        // [UwInterfaceId_Logic]      = &int_type_logic_interface,
        // [UwInterfaceId_Arithmetic] = &int_type_arithmetic_interface,
        // [UwInterfaceId_Bitwise]    = &int_type_bitwise_interface,
        // [UwInterfaceId_Comparison] = &int_type_comparison_interface
};

/****************************************************************
 * Float type
 */

static UwResult float_create(UwTypeId type_id, va_list ap)
{
    return UwFloat(0.0);
}

static void float_hash(UwValuePtr self, UwHashContext* ctx)
{
    // mind maps: the hash should be the same for subtypes, that's why not using self->type_id here
    _uw_hash_uint64(ctx, UwTypeId_Float);
    _uw_hash_buffer(ctx, &self->float_value, sizeof(self->float_value));
}

static void float_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    fprintf(fp, ": %f\n", self->float_value);
}

static UwResult float_to_string(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

static bool float_is_true(UwValuePtr self)
{
    return self->float_value;
}

static bool float_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return self->float_value == other->float_value;
}

static bool float_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        switch (t) {
            case UwTypeId_Signed:   return self->float_value == (UwType_Float) other->signed_value;
            case UwTypeId_Unsigned: return self->float_value == (UwType_Float) other->unsigned_value;
            case UwTypeId_Float:    return self->float_value == other->float_value;
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

static UwType float_type = {
    .id              = UwTypeId_Float,
    .ancestor_id     = UwTypeId_Null,  // no ancestor
    .name            = "Float",
    .allocator       = nullptr,
    .data_offset     = 0,
    .data_size       = 0,
    .compound        = false,
    ._create         = float_create,
    ._destroy        = nullptr,
    ._init           = nullptr,
    ._fini           = nullptr,
    ._clone          = nullptr,
    ._hash           = float_hash,
    ._deepcopy       = nullptr,
    ._dump           = float_dump,
    ._to_string      = float_to_string,
    ._is_true        = float_is_true,
    ._equal_sametype = float_equal_sametype,
    ._equal          = float_equal,

    .num_interfaces  = 0,
    .interfaces      = nullptr
        // [UwInterfaceId_Logic]      = &float_type_logic_interface,
        // [UwInterfaceId_Arithmetic] = &float_type_arithmetic_interface,
        // [UwInterfaceId_Comparison] = &float_type_comparison_interface
};

/****************************************************************
 * Abstract structure type.
 */

static UwResult struct_create(UwTypeId type_id, va_list ap)
{
    _UwValue result = {
        ._extradata_type_id = UwTypeId_Struct,
        .extra_data = nullptr
    };
    return result;
}

static void struct_hash(UwValuePtr self, UwHashContext* ctx)
/*
 * Basic hashing.
 */
{
    _uw_hash_uint64(ctx, self->type_id);
}

static UwResult struct_deepcopy(UwValuePtr self)
/*
 * Bare struct cannot be copied, this method must be defined in a subtype.
 */
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

static void struct_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    _uw_dump_base_extra_data(fp, self->extra_data);
    fputc('\n', fp);
}

static UwResult struct_to_string(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

static bool struct_is_true(UwValuePtr self)
/*
 * Bare struct is always false.
 */
{
    return false;
}

static bool struct_equal_sametype(UwValuePtr self, UwValuePtr other)
/*
 * Bare struct is not equal to anything.
 */
{
    return false;
}

static bool struct_equal(UwValuePtr self, UwValuePtr other)
/*
 * Bare struct is not equal to anything.
 */
{
    return false;
}

static UwType struct_type = {
    .id              = UwTypeId_Struct,
    .ancestor_id     = UwTypeId_Null,  // no ancestor
    .name            = "Struct",
    .allocator       = &default_allocator,
    .data_offset     = 0,
    .data_size       = 0,
    .compound        = false,
    ._create         = struct_create,
    ._destroy        = _uw_default_destroy,
    ._init           = nullptr,
    ._fini           = nullptr,
    ._clone          = _uw_default_clone,
    ._hash           = struct_hash,
    ._deepcopy       = struct_deepcopy,
    ._dump           = struct_dump,
    ._to_string      = struct_to_string,
    ._is_true        = struct_is_true,
    ._equal_sametype = struct_equal_sametype,
    ._equal          = struct_equal,

    .num_interfaces  = 0,
    .interfaces      = nullptr
};


/****************************************************************
 * Pointer type
 */

static UwResult ptr_create(UwTypeId type_id, va_list ap)
{
    return UwPtr(nullptr);
}

static void ptr_hash(UwValuePtr self, UwHashContext* ctx)
{
    // mind maps: the hash should be the same for subtypes, that's why not using self->type_id here
    _uw_hash_uint64(ctx, UwTypeId_Ptr);
    _uw_hash_buffer(ctx, &self->ptr, sizeof(self->ptr));
}

static void ptr_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    fprintf(fp, ": %p\n", self->ptr);
}

static UwResult ptr_to_string(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

static bool ptr_is_true(UwValuePtr self)
{
    return self->ptr;
}

static bool ptr_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return self->ptr == other->ptr;
}

static bool ptr_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        switch (t) {
            case UwTypeId_Null:
                return self->ptr == nullptr;

            case UwTypeId_Ptr:
                return self->ptr == other->ptr;

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

static UwType ptr_type = {
    .id              = UwTypeId_Ptr,
    .ancestor_id     = UwTypeId_Null,  // no ancestor
    .name            = "Ptr",
    .allocator       = nullptr,
    .data_offset     = 0,
    .data_size       = 0,
    .compound        = false,
    ._create         = ptr_create,
    ._destroy        = nullptr,
    ._init           = nullptr,
    ._fini           = nullptr,
    ._clone          = nullptr,
    ._hash           = ptr_hash,
    ._deepcopy       = ptr_to_string,
    ._dump           = ptr_dump,
    ._to_string      = ptr_to_string,
    ._is_true        = ptr_is_true,
    ._equal_sametype = ptr_equal_sametype,
    ._equal          = ptr_equal,

    .num_interfaces  = 0,
    .interfaces      = nullptr
};

/****************************************************************
 * Global list of types initialized with built-in types.
 */

extern UwType _uw_map_type;     // defined in uw_map.c
extern UwType _uw_status_type;  // defined in uw_status.c

static UwType* basic_types[] = {
    [UwTypeId_Null]     = &null_type,
    [UwTypeId_Bool]     = &bool_type,
    [UwTypeId_Int]      = &int_type,
    [UwTypeId_Signed]   = &signed_type,
    [UwTypeId_Unsigned] = &unsigned_type,
    [UwTypeId_Float]    = &float_type,
    [UwTypeId_String]   = &_uw_string_type,
    [UwTypeId_CharPtr]  = &_uw_charptr_type,
    [UwTypeId_List]     = &_uw_list_type,
    [UwTypeId_Map]      = &_uw_map_type,
    [UwTypeId_Status]   = &_uw_status_type,
    [UwTypeId_Struct]   = &struct_type,
    [UwTypeId_Ptr]      = &ptr_type
};

UwType** _uw_types = nullptr;
static size_t uw_types_capacity = 0;
static UwTypeId num_uw_types = 0;

[[ gnu::constructor ]]
static void init_uw_types()
{
    if (_uw_types) {
        return;
    }

    size_t page_size = sysconf(_SC_PAGE_SIZE);
    size_t memsize = (sizeof(basic_types) + page_size - 1) & ~(page_size - 1);
    uw_types_capacity = memsize / sizeof(char*);

    _uw_types = mmap(NULL, memsize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (_uw_types == MAP_FAILED) {
        perror("mmap");
        abort();
    }

    num_uw_types = _UWC_LENGTH_OF(basic_types);
    for(UwTypeId i = 0; i < num_uw_types; i++) {
        UwType* t = basic_types[i];
        if (!t) {
            fprintf(stderr, "Type %u is not defined\n", i);
            abort();
        }
        _uw_types[i] = t;
    }
}

UwTypeId uw_add_type(UwType* type)
{
    // the order constructor are called is undefined, make sure _uw_types is initialized
    init_uw_types();

    if (num_uw_types == ((1 << 8 * sizeof(UwTypeId)) - 1)) {
        fprintf(stderr, "Cannot define more types than %u\n", num_uw_types);
        return UwTypeId_Null;
    }
    if (num_uw_types == uw_types_capacity) {
        size_t page_size = sysconf(_SC_PAGE_SIZE);
        size_t old_memsize = (uw_types_capacity * sizeof(UwType*) + page_size - 1) & ~(page_size - 1);
        size_t new_memsize = ((uw_types_capacity + 1) * sizeof(UwType*) + page_size - 1) & ~(page_size - 1);

        UwType** new_uw_types = mremap(_uw_types, old_memsize, new_memsize, MREMAP_MAYMOVE);
        if (new_uw_types == MAP_FAILED) {
            perror("mremap");
            return UwTypeId_Null;
        }
        _uw_types = new_uw_types;
        uw_types_capacity = new_memsize / sizeof(char*);
    }
    UwTypeId type_id = num_uw_types++;
    type->id = type_id;
    _uw_types[type_id] = type;
    return type_id;
}

UwTypeId uw_subtype(UwType* type, char* name, UwTypeId ancestor_id, unsigned data_size)
{
    // the order constructor are called is undefined, make sure _uw_types is initialized
    init_uw_types();

    uw_assert(ancestor_id != UwTypeId_Null);

    UwType* ancestor = _uw_types[ancestor_id];

    *type = *ancestor;  // copy type structure

    type->ancestor_id = ancestor_id;
    type->name = name;
    type->data_offset = ancestor->data_offset + ancestor->data_size;
    type->data_size = data_size;

    return uw_add_type(type);
}
