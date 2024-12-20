#include <string.h>

#include "include/uw_base.h"
#include "include/uw_file.h"
#include "include/uw_string_io.h"
#include "src/uw_bool_internal.h"
#include "src/uw_charptr_internal.h"
#include "src/uw_class_internal.h"
#include "src/uw_file_internal.h"
#include "src/uw_float_internal.h"
#include "src/uw_hash_internal.h"
#include "src/uw_int_internal.h"
#include "src/uw_list_internal.h"
#include "src/uw_map_internal.h"
#include "src/uw_null_internal.h"
#include "src/uw_signed_internal.h"
#include "src/uw_status_internal.h"
#include "src/uw_string_internal.h"
#include "src/uw_string_io_internal.h"
#include "src/uw_unsigned_internal.h"

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
    if (_uw_types[v->type_id]->data_optional) {
        // extra_data is optional, not creating
        v->extra_data = nullptr;
        return true;
    }
    return _uw_mandatory_alloc_extra_data(v);
}

bool _uw_mandatory_alloc_extra_data(UwValuePtr v)
{
    UwType* t = _uw_types[v->type_id];
    unsigned memsize = t->data_offset + t->data_size;
    if (memsize) {
        _UwExtraData* extra_data = t->allocator->alloc(memsize);
        if (!extra_data) {
            return false;
        }
        extra_data->refcount = 1;
        v->extra_data = extra_data;
    } else {
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
            t->allocator->free(v->extra_data, memsize);
        } else {
            uw_dump(stderr, v);
            uw_panic("Extra data is allocated, but memsize evaluates to zero");
        }
        v->extra_data = nullptr;
    }
}

static UwResult default_create(UwTypeId type_id, va_list ap)
/*
 * Default implementation of create method for types that have extra data.
 */
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

static UwResult default_clone(UwValuePtr self)
/*
 * Default implementation of clone method.
 */
{
    if (self->extra_data) {
        self->extra_data->refcount++;
    }
    return *self;
}

static void default_destroy(UwValuePtr self)
/*
 * Default implementation of destroy method for types that have extra data.
 */
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
 * Miscellaneous helpers
 */

bool uw_charptr_to_string(UwValuePtr v)
{
    if (!uw_is_charptr(v)) {
        return true;
    }
    UwValue result = _uw_charptr_to_string(v);
    if (uw_ok(&result)) {
        *v = uw_move(&result);
        return true;
    } else {
        uw_destroy(&result);  // make error checker happy
        return false;
    }
}

/****************************************************************
 * Global list of interfaces
 */

bool _uw_registered_interfaces[UW_INTERFACE_CAPACITY] = {
    [UwInterfaceId_Logic]        = true,
    [UwInterfaceId_Arithmetic]   = true,
    [UwInterfaceId_Bitwise]      = true,
    [UwInterfaceId_Comparison]   = true,
    [UwInterfaceId_RandomAccess] = true,
    [UwInterfaceId_String]       = true,
    [UwInterfaceId_List]         = true,
    [UwInterfaceId_File]         = true,
    [UwInterfaceId_FileReader]   = true,
    [UwInterfaceId_FileWriter]   = true,
    [UwInterfaceId_LineReader]   = true
};

int uw_register_interface()
{
    for (int i = 0; i < UW_INTERFACE_CAPACITY; i++) {
        if (!_uw_registered_interfaces[i]) {
            _uw_registered_interfaces[i] = true;
            return i;
        }
    }
    return -1;
}

/****************************************************************
 * Global list of types initialized with built-in types.
 */

static UwType null_type = {
    .id              = UwTypeId_Null,
    .ancestor_id     = UwTypeId_Null,  // no ancestor; Null can't be an ancestor for any type
    .compound        = false,
    .data_optional   = true,
    .name            = "Null",
    .data_offset     = 0,
    .data_size       = 0,
    .allocator       = nullptr,
    ._create         = _uw_null_create,
    ._destroy        = nullptr,
    ._init           = nullptr,
    ._fini           = nullptr,
    ._clone          = nullptr,
    ._hash           = _uw_null_hash,
    ._deepcopy       = nullptr,
    ._dump           = _uw_null_dump,
    ._to_string      = _uw_null_to_string,
    ._is_true        = _uw_null_is_true,
    ._equal_sametype = _uw_null_equal_sametype,
    ._equal          = _uw_null_equal,

    .interfaces = {}
};

static UwType bool_type = {
    .id              = UwTypeId_Bool,
    .ancestor_id     = UwTypeId_Null,  // no ancestor
    .compound        = false,
    .data_optional   = true,
    .name            = "Bool",
    .data_offset     = 0,
    .data_size       = 0,
    .allocator       = nullptr,
    ._create         = _uw_bool_create,
    ._destroy        = nullptr,
    ._init           = nullptr,
    ._fini           = nullptr,
    ._clone          = nullptr,
    ._hash           = _uw_bool_hash,
    ._deepcopy       = nullptr,
    ._dump           = _uw_bool_dump,
    ._to_string      = _uw_bool_to_string,
    ._is_true        = _uw_bool_is_true,
    ._equal_sametype = _uw_bool_equal_sametype,
    ._equal          = _uw_bool_equal,

    .interfaces = {
        // [UwInterfaceId_Logic] = &bool_type_logic_interface
    }
};

static UwType int_type = {
    .id              = UwTypeId_Int,
    .ancestor_id     = UwTypeId_Null,  // no ancestor
    .compound        = false,
    .data_optional   = true,
    .name            = "Int",
    .data_offset     = 0,
    .data_size       = 0,
    .allocator       = nullptr,
    ._create         = _uw_int_create,
    ._destroy        = nullptr,
    ._init           = nullptr,
    ._fini           = nullptr,
    ._clone          = nullptr,
    ._hash           = _uw_int_hash,
    ._deepcopy       = nullptr,
    ._dump           = _uw_int_dump,
    ._to_string      = _uw_int_to_string,
    ._is_true        = _uw_int_is_true,
    ._equal_sametype = _uw_int_equal_sametype,
    ._equal          = _uw_int_equal,

    .interfaces = {
        // [UwInterfaceId_Logic]      = &int_type_logic_interface,
        // [UwInterfaceId_Arithmetic] = &int_type_arithmetic_interface,
        // [UwInterfaceId_Bitwise]    = &int_type_bitwise_interface,
        // [UwInterfaceId_Comparison] = &int_type_comparison_interface
    }
};

static UwType signed_type = {
    .id              = UwTypeId_Signed,
    .ancestor_id     = UwTypeId_Int,
    .compound        = false,
    .data_optional   = true,
    .name            = "Signed",
    .data_offset     = 0,
    .data_size       = 0,
    .allocator       = nullptr,
    ._create         = _uw_signed_create,
    ._destroy        = nullptr,
    ._init           = nullptr,
    ._fini           = nullptr,
    ._clone          = nullptr,
    ._hash           = _uw_signed_hash,
    ._deepcopy       = nullptr,
    ._dump           = _uw_signed_dump,
    ._to_string      = _uw_signed_to_string,
    ._is_true        = _uw_signed_is_true,
    ._equal_sametype = _uw_signed_equal_sametype,
    ._equal          = _uw_signed_equal,

    .interfaces = {
        // [UwInterfaceId_Logic]      = &int_type_logic_interface,
        // [UwInterfaceId_Arithmetic] = &int_type_arithmetic_interface,
        // [UwInterfaceId_Bitwise]    = &int_type_bitwise_interface,
        // [UwInterfaceId_Comparison] = &int_type_comparison_interface
    }
};

static UwType unsigned_type = {
    .id              = UwTypeId_Unsigned,
    .ancestor_id     = UwTypeId_Int,
    .compound        = false,
    .data_optional   = true,
    .name            = "Unsigned",
    .data_offset     = 0,
    .data_size       = 0,
    .allocator       = nullptr,
    ._create         = _uw_unsigned_create,
    ._destroy        = nullptr,
    ._init           = nullptr,
    ._fini           = nullptr,
    ._clone          = nullptr,
    ._hash           = _uw_unsigned_hash,
    ._deepcopy       = nullptr,
    ._dump           = _uw_unsigned_dump,
    ._to_string      = _uw_unsigned_to_string,
    ._is_true        = _uw_unsigned_is_true,
    ._equal_sametype = _uw_unsigned_equal_sametype,
    ._equal          = _uw_unsigned_equal,

    .interfaces = {
        // [UwInterfaceId_Logic]      = &int_type_logic_interface,
        // [UwInterfaceId_Arithmetic] = &int_type_arithmetic_interface,
        // [UwInterfaceId_Bitwise]    = &int_type_bitwise_interface,
        // [UwInterfaceId_Comparison] = &int_type_comparison_interface
    }
};

static UwType float_type = {
    .id              = UwTypeId_Float,
    .ancestor_id     = UwTypeId_Null,  // no ancestor
    .compound        = false,
    .data_optional   = true,
    .name            = "Float",
    .data_offset     = 0,
    .data_size       = 0,
    .allocator       = nullptr,
    ._create         = _uw_float_create,
    ._destroy        = nullptr,
    ._init           = nullptr,
    ._fini           = nullptr,
    ._clone          = nullptr,
    ._hash           = _uw_float_hash,
    ._deepcopy       = nullptr,
    ._dump           = _uw_float_dump,
    ._to_string      = _uw_float_to_string,
    ._is_true        = _uw_float_is_true,
    ._equal_sametype = _uw_float_equal_sametype,
    ._equal          = _uw_float_equal,

    .interfaces = {
        // [UwInterfaceId_Logic]      = &float_type_logic_interface,
        // [UwInterfaceId_Arithmetic] = &float_type_arithmetic_interface,
        // [UwInterfaceId_Comparison] = &float_type_comparison_interface
    }
};

static UwType string_type = {
    .id              = UwTypeId_String,
    .ancestor_id     = UwTypeId_Null,  // no ancestor
    .compound        = false,
    .data_optional   = true,
    .name            = "String",
    .data_offset     = offsetof(struct _UwStringExtraData, string_data),
    .data_size       = 0,
    .allocator       = &_uw_default_allocator,
    ._create         = _uw_string_create,
    ._destroy        = _uw_string_destroy,
    ._init           = nullptr,           // custom constructor performs all the initialization
    ._fini           = nullptr,           // there's nothing to finalize, just run custom destructor
    ._clone          = _uw_string_clone,
    ._hash           = _uw_string_hash,
    ._deepcopy       = _uw_string_clone,  // strings are COW so copy is clone
    ._dump           = _uw_string_dump,
    ._to_string      = _uw_string_clone,  // yes, simply make a copy
    ._is_true        = _uw_string_is_true,
    ._equal_sametype = _uw_string_equal_sametype,
    ._equal          = _uw_string_equal,

    .interfaces = {
        // [UwInterfaceId_RandomAccess] = &string_type_random_access_interface
    }
};

static UwType charptr_type = {
    .id              = UwTypeId_CharPtr,
    .ancestor_id     = UwTypeId_Null,  // no ancestor
    .compound        = false,
    .data_optional   = true,
    .name            = "CharPtr",
    .data_offset     = 0,
    .data_size       = 0,
    .allocator       = nullptr,
    ._create         = _uw_charptr_create,
    ._destroy        = nullptr,
    ._init           = nullptr,
    ._fini           = nullptr,
    ._clone          = _uw_charptr_to_string,
    ._hash           = _uw_charptr_hash,
    ._deepcopy       = _uw_charptr_to_string,
    ._dump           = _uw_charptr_dump,
    ._to_string      = _uw_charptr_to_string,
    ._is_true        = _uw_charptr_is_true,
    ._equal_sametype = _uw_charptr_equal_sametype,
    ._equal          = _uw_charptr_equal,

    .interfaces = {}
};

static UwType list_type = {
    .id              = UwTypeId_List,
    .ancestor_id     = UwTypeId_Null,  // no ancestor
    .compound        = true,
    .data_optional   = false,
    .name            = "List",
    .data_offset     = offsetof(struct _UwListExtraData, list_data),
    .data_size       = sizeof(struct _UwList),
    .allocator       = &_uw_default_allocator,
    ._create         = default_create,
    ._destroy        = default_destroy,
    ._init           = _uw_list_init,
    ._fini           = _uw_list_fini,
    ._clone          = default_clone,
    ._hash           = _uw_list_hash,
    ._deepcopy       = _uw_list_deepcopy,
    ._dump           = _uw_list_dump,
    ._to_string      = _uw_list_to_string,
    ._is_true        = _uw_list_is_true,
    ._equal_sametype = _uw_list_equal_sametype,
    ._equal          = _uw_list_equal,

    .interfaces = {
        // [UwInterfaceId_RandomAccess] = &list_type_random_access_interface,
        // [UwInterfaceId_List]         = &list_type_list_interface
    }
};

static UwType map_type = {
    .id              = UwTypeId_Map,
    .ancestor_id     = UwTypeId_Null,  // no ancestor
    .compound        = true,
    .data_optional   = false,
    .name            = "Map",
    .data_offset     = offsetof(struct _UwMapExtraData, map_data),
    .data_size       = sizeof(struct _UwMap),
    .allocator       = &_uw_default_allocator,
    ._create         = default_create,
    ._destroy        = default_destroy,
    ._init           = _uw_map_init,
    ._fini           = _uw_map_fini,
    ._clone          = default_clone,
    ._hash           = _uw_map_hash,
    ._deepcopy       = _uw_map_deepcopy,
    ._dump           = _uw_map_dump,
    ._to_string      = _uw_map_to_string,
    ._is_true        = _uw_map_is_true,
    ._equal_sametype = _uw_map_equal_sametype,
    ._equal          = _uw_map_equal,

    .interfaces = {
        // [UwInterfaceId_RandomAccess] = &map_type_random_access_interface
    }
};

static UwType status_type = {
    .id              = UwTypeId_Status,
    .ancestor_id     = UwTypeId_Null,  // no ancestor
    .compound        = false,
    .data_optional   = true,
    .name            = "Status",
    .data_offset     = offsetof(struct _UwStatusExtraData, status_desc),  // extra_data is optional and not allocated by default
    .data_size       = sizeof(char*),
    .allocator       = &_uw_default_allocator,
    ._create         = default_create,
    ._destroy        = default_destroy,
    ._init           = nullptr,
    ._fini           = _uw_status_fini,
    ._clone          = default_clone,
    ._hash           = _uw_status_hash,
    ._deepcopy       = _uw_status_deepcopy,
    ._dump           = _uw_status_dump,
    ._to_string      = _uw_status_to_string,
    ._is_true        = _uw_status_is_true,
    ._equal_sametype = _uw_status_equal_sametype,
    ._equal          = _uw_status_equal,

    .interfaces = {}
};

static UwType class_type = {
    .id              = UwTypeId_Class,
    .ancestor_id     = UwTypeId_Null,  // no ancestor
    .compound        = false,
    .data_optional   = false,
    .name            = "Class",
    .data_offset     = 0,
    .data_size       = 0,
    .allocator       = &_uw_default_allocator,
    ._create         = default_create,
    ._destroy        = default_destroy,
    ._init           = nullptr,
    ._fini           = nullptr,
    ._clone          = default_clone,
    ._hash           = _uw_class_hash,
    ._deepcopy       = _uw_class_deepcopy,
    ._dump           = _uw_class_dump,
    ._to_string      = _uw_class_to_string,
    ._is_true        = _uw_class_is_true,
    ._equal_sametype = _uw_class_equal_sametype,
    ._equal          = _uw_class_equal,

    .interfaces = {}
};

static UwInterface_File file_type_file_interface = {
    ._open     = _uwi_file_open,
    ._close    = _uwi_file_close,
    ._set_fd   = _uwi_file_set_fd,
    ._get_name = _uwi_file_get_name,
    ._set_name = _uwi_file_set_name
};

static UwInterface_FileReader file_type_file_reader_interface = {
    ._read = _uwi_file_read
};

static UwInterface_FileWriter file_type_file_writer_interface = {
    ._write = _uwi_file_write
};

static UwInterface_LineReader file_type_line_reader_interface = {
    ._start             = _uwi_file_start_read_lines,
    ._read_line         = _uwi_file_read_line,
    ._read_line_inplace = _uwi_file_read_line_inplace,
    ._get_line_number   = _uwi_file_get_line_number,
    ._unread_line       = _uwi_file_unread_line,
    ._stop              = _uwi_file_stop_read_lines
};

static UwType file_type = {
    .id              = UwTypeId_File,
    .ancestor_id     = UwTypeId_Null,  // no ancestor
    .compound        = false,
    .data_optional   = false,
    .name            = "File",
    .data_offset     = offsetof(struct _UwFileExtraData, file_data),
    .data_size       = sizeof(struct _UwFile),
    .allocator       = &_uw_default_allocator,
    ._create         = default_create,
    ._destroy        = default_destroy,
    ._init           = _uw_file_init,
    ._fini           = _uw_file_fini,
    ._clone          = default_clone,
    ._hash           = _uw_file_hash,
    ._deepcopy       = _uw_file_deepcopy,
    ._dump           = _uw_file_dump,
    ._to_string      = _uw_file_to_string,
    ._is_true        = _uw_file_is_true,
    ._equal_sametype = _uw_file_equal_sametype,
    ._equal          = _uw_file_equal,

    .interfaces = {
        [UwInterfaceId_File]       = &file_type_file_interface,
        [UwInterfaceId_FileReader] = &file_type_file_reader_interface,
        [UwInterfaceId_FileWriter] = &file_type_file_writer_interface,
        [UwInterfaceId_LineReader] = &file_type_line_reader_interface
    }
};

static UwInterface_LineReader stringio_type_line_reader_interface = {
    ._start             = _uwi_stringio_start_read_lines,
    ._read_line         = _uwi_stringio_read_line,
    ._read_line_inplace = _uwi_stringio_read_line_inplace,
    ._get_line_number   = _uwi_stringio_get_line_number,
    ._unread_line       = _uwi_stringio_unread_line,
    ._stop              = _uwi_stringio_stop_read_lines
};

static UwType stringio_type = {
    .id              = UwTypeId_StringIO,
    .ancestor_id     = UwTypeId_Null,  // no ancestor
    .compound        = false,
    .data_optional   = false,
    .name            = "StringIO",
    .data_offset     = offsetof(struct _UwStringIOExtraData, stringio_data),
    .data_size       = sizeof(struct _UwStringIO),
    .allocator       = &_uw_default_allocator,
    ._create         = default_create,
    ._destroy        = default_destroy,
    ._init           = _uw_stringio_init,
    ._fini           = _uw_stringio_fini,
    ._clone          = default_clone,
    ._hash           = _uw_stringio_hash,
    ._deepcopy       = _uw_stringio_deepcopy,
    ._dump           = _uw_stringio_dump,
    ._to_string      = _uw_stringio_to_string,
    ._is_true        = _uw_stringio_is_true,
    ._equal_sametype = _uw_stringio_equal_sametype,
    ._equal          = _uw_stringio_equal,

    // in a subclass all interfaces of base classes must be in place:
    .interfaces = {
        // [UwInterfaceId_RandomAccess] = &string_type_random_access_interface
        [UwInterfaceId_LineReader] = &stringio_type_line_reader_interface
    }
};

UwType* _uw_types[UW_TYPE_CAPACITY] = {

    [UwTypeId_Null]     = &null_type,
    [UwTypeId_Bool]     = &bool_type,
    [UwTypeId_Int]      = &int_type,
    [UwTypeId_Signed]   = &signed_type,
    [UwTypeId_Unsigned] = &unsigned_type,
    [UwTypeId_Float]    = &float_type,
    [UwTypeId_String]   = &string_type,
    [UwTypeId_CharPtr]  = &charptr_type,
    [UwTypeId_List]     = &list_type,
    [UwTypeId_Map]      = &map_type,
    [UwTypeId_Status]   = &status_type,
    [UwTypeId_Class]    = &class_type,
    [UwTypeId_File]     = &file_type,
    [UwTypeId_StringIO] = &stringio_type
};


UwTypeId uw_add_type(UwType* type)
{
    UwTypeId i = 0;
    do {
        if (_uw_types[i] == nullptr) {
            _uw_types[i] = type;
            return i;
        }
        i++;
#if UW_TYPE_CAPACITY <= _UW_TYPE_MAX
    } while (i < UW_TYPE_CAPACITY);
#else
    } while (i != 0);
#endif
    return UwTypeId_Null;
}

UwTypeId uw_subclass(UwType* type, char* name, UwTypeId ancestor_id, unsigned data_size)
{
    // don't allow subclassing Null, this probably indicates an error
    uw_assert(ancestor_id != UwTypeId_Null);

    UwType* ancestor = _uw_types[ancestor_id];
    uw_assert(ancestor != nullptr);

    *type = *ancestor;  // copy type structure

    type->ancestor_id = ancestor_id;
    type->name = name;
    type->data_offset = ancestor->data_offset + ancestor->data_size;
    type->data_size = data_size;

    UwTypeId type_id = uw_add_type(type);
    if (type_id != UwTypeId_Null) {
        type->id = (UwTypeId) type_id;
    }
    return type_id;
}
