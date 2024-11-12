#include <string.h>

#include "include/uw_base.h"
#include "include/uw_file.h"
#include "include/uw_string_io.h"
#include "src/uw_null_internal.h"
#include "src/uw_bool_internal.h"
#include "src/uw_class_internal.h"
#include "src/uw_hash_internal.h"
#include "src/uw_int_internal.h"
#include "src/uw_float_internal.h"
#include "src/uw_string_internal.h"
#include "src/uw_string_io_internal.h"
#include "src/uw_list_internal.h"
#include "src/uw_map_internal.h"
#include "src/uw_file_internal.h"

/****************************************************************
 * Basic functions
 */

UwValuePtr _uw_alloc_value(UwTypeId type_id)
{
    UwType* t = _uw_types[type_id];
    size_t memsize = t->data_offset + t->data_size;
    UwValuePtr value = _uw_allocator.alloc(memsize);
    if (value) {
        memset(value, 0, memsize);
        value->type_id  = type_id;
        value->refcount = 1;
        value->alloc_id = _uw_allocator.id;
    }
    return value;
}

static bool call_init(UwValuePtr value, UwTypeId type_id)
/*
 * Helper function for _uw_create.
 */
{
    UwType* type = _uw_types[type_id];
    uw_assert(type != nullptr);

    if (type->ancestor_id != UwTypeId_Null) {
        if (!call_init(value, type->ancestor_id)) {
            return false;
        }
    }
    UwMethodInit init = _uw_types[type_id]->init;
    uw_assert(init != nullptr);
    if (init(value)) {
        return true;
    }
    // init failed, call fini for already initialized ancestors
    for (;;) {
        type = _uw_types[type->ancestor_id];
        if (type->id == UwTypeId_Null) {
            break;
        }
        type->fini(value);
    }
    return false;
}

UwValuePtr _uw_create(UwTypeId type_id)
{
    UwValuePtr value = _uw_alloc_value(type_id);
    if (value) {
        // initialize value
        // go down inheritance chain and call `init` method bottom->top
        if (call_init(value, type_id)) {
            return  value;
        }
        _uw_free_value(value);
    }
    return nullptr;
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

        // finalize value
        // go down inheritance chain and call `fini` method
        UwType* type = _uw_types[v->type_id];
        for (;;) {
            uw_assert(type != nullptr);
            type->fini(v);
            if (type->ancestor_id == UwTypeId_Null) {
                break;
            }
            type = _uw_types[type->ancestor_id];
        }

        // free value
        _uw_free_value(v);
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

static void _uw_fini_integral(UwValuePtr self)
/*
 * Finalization stub for integral types.
 */
{
}

/****************************************************************
 * Dump functions.
 */

void _uw_dump_start(UwValuePtr value, int indent)
{
    char* type_name = _uw_types[value->type_id]->name;

    _uw_print_indent(indent);
    printf("%p", value);
    if (type_name == nullptr) {
        printf(" BAD TYPE %d", value->type_id);
    } else {
        printf(" %s (type id: %d)", type_name,value->type_id);
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
    if (value == nullptr) {
        _uw_print_indent(indent);
        printf("nullptr\n");
        return;
    }
    UwMethodDump fn_dump = _uw_types[value->type_id]->dump;
    uw_assert(fn_dump != nullptr);
    fn_dump(value, 0);
}

void uw_dump(UwValuePtr value)
{
    _uw_dump(value, 0);
}

/****************************************************************
 * Allocators.
 */

static void* malloc_nofail(size_t nbytes)
{
    void* block = malloc(nbytes);
    if (!block) {
        char msg[64];
        sprintf(msg, "malloc(%zu)", nbytes);
        perror(msg);
        exit(1);
    }
    return block;
}

static void* realloc_nofail(void* block, size_t nbytes)
{
    void* new_block = realloc(block, nbytes);
    if (!new_block) {
        char msg[64];
        sprintf(msg, "realloc(%p, %zu)", block, nbytes);
        perror(msg);
        exit(1);
    }
    return new_block;
}

UwAllocator _uw_allocators[1 << UW_ALLOCATOR_BITWIDTH] = {
    {
        .id      = UW_ALLOCATOR_STD,
        .alloc   = malloc,
        .realloc = realloc,
        .free    = free
    },
    {
        .id      = UW_ALLOCATOR_STD_NOFAIL,
        .alloc   = malloc_nofail,
        .realloc = realloc_nofail,
        .free    = free
    }
};

thread_local UwAllocator _uw_allocator = {
    .id      = UW_ALLOCATOR_STD,
    .alloc   = malloc,
    .realloc = realloc,
    .free    = free
};

void uw_set_allocator(UwAllocId alloc_id)
{
    _uw_allocator = _uw_allocators[alloc_id];
}

/****************************************************************
 * Global list of interfaces initialized with built-in interfaces.
 */

bool _uw_registered_interfaces[1 << UW_INTERFACE_BITWIDTH] = {
    [UwInterfaceId_Logic]        = true,
    [UwInterfaceId_Arithmetic]   = true,
    [UwInterfaceId_Bitwise]      = true,
    [UwInterfaceId_Comparison]   = true,
    [UwInterfaceId_RandomAccess] = true,
    [UwInterfaceId_List]         = true,
    [UwInterfaceId_File]         = true,
    [UwInterfaceId_FileReader]   = true,
    [UwInterfaceId_FileWriter]   = true,
    [UwInterfaceId_LineReader]   = true
};

int uw_register_interface()
{
    for (int i = 0; i < (1 << UW_INTERFACE_BITWIDTH); i++) {
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
    .id             = UwTypeId_Null,
    .ancestor_id    = UwTypeId_Null,  // no ancestor; Null can't be an ancestor for any type
    .name           = "Null",
    .data_size      = 0,
    .data_offset    = sizeof(_UwValueBase),
    .init           = _uw_init_null,
    .fini           = _uw_fini_integral,
    .hash           = _uw_hash_null,
    .copy           = _uw_copy_null,
    .dump           = _uw_dump_null,
    .to_string      = _uw_null_to_string,
    .is_true        = _uw_null_is_true,
    .equal_sametype = _uw_null_equal_sametype,
    .equal          = _uw_null_equal,
    .equal_ctype    = _uw_null_equal_ctype,

    .interfaces = {}
};

static UwType bool_type = {
    .id             = UwTypeId_Bool,
    .ancestor_id    = UwTypeId_Null,  // no ancestor
    .name           = "Bool",
    .data_size      = sizeof(UwType_Bool),
    .data_offset    = sizeof(_UwValueBase),
    .init           = _uw_init_bool,
    .fini           = _uw_fini_integral,
    .hash           = _uw_hash_bool,
    .copy           = _uw_copy_bool,
    .dump           = _uw_dump_bool,
    .to_string      = _uw_bool_to_string,
    .is_true        = _uw_bool_is_true,
    .equal_sametype = _uw_bool_equal_sametype,
    .equal          = _uw_bool_equal,
    .equal_ctype    = _uw_bool_equal_ctype,

    .interfaces = {
        // [UwInterfaceId_Logic] = &bool_type_logic_interface
    }
};

static UwType int_type = {
    .id             = UwTypeId_Int,
    .ancestor_id    = UwTypeId_Null,  // no ancestor
    .name           = "Int",
    .data_size      = sizeof(UwType_Int),
    .data_offset    = sizeof(_UwValueBase),
    .init           = _uw_init_int,
    .fini           = _uw_fini_integral,
    .hash           = _uw_hash_int,
    .copy           = _uw_copy_int,
    .dump           = _uw_dump_int,
    .to_string      = _uw_int_to_string,
    .is_true        = _uw_int_is_true,
    .equal_sametype = _uw_int_equal_sametype,
    .equal          = _uw_int_equal,
    .equal_ctype    = _uw_int_equal_ctype,

    .interfaces = {
        // [UwInterfaceId_Logic]      = &int_type_logic_interface,
        // [UwInterfaceId_Arithmetic] = &int_type_arithmetic_interface,
        // [UwInterfaceId_Bitwise]    = &int_type_bitwise_interface,
        // [UwInterfaceId_Comparison] = &int_type_comparison_interface
    }
};

static UwType float_type = {
    .id             = UwTypeId_Float,
    .ancestor_id    = UwTypeId_Null,  // no ancestor
    .name           = "Float",
    .data_size      = sizeof(UwType_Float),
    .data_offset    = sizeof(_UwValueBase),
    .init           = _uw_init_float,
    .fini           = _uw_fini_integral,
    .hash           = _uw_hash_float,
    .copy           = _uw_copy_float,
    .dump           = _uw_dump_float,
    .to_string      = _uw_float_to_string,
    .is_true        = _uw_float_is_true,
    .equal_sametype = _uw_float_equal_sametype,
    .equal          = _uw_float_equal,
    .equal_ctype    = _uw_float_equal_ctype,

    .interfaces = {
        // [UwInterfaceId_Logic]      = &float_type_logic_interface,
        // [UwInterfaceId_Arithmetic] = &float_type_arithmetic_interface,
        // [UwInterfaceId_Comparison] = &float_type_comparison_interface
    }
};

static UwType string_type = {
    .id             = UwTypeId_String,
    .ancestor_id    = UwTypeId_Null,  // no ancestor
    .name           = "String",
    .data_size      = sizeof(struct _UwString*),
    .data_offset    = sizeof(_UwValueBase),
    .init           = _uw_init_string,
    .fini           = _uw_fini_string,
    .hash           = _uw_hash_string,
    .copy           = _uw_copy_string,
    .dump           = _uw_dump_string,
    .to_string      = _uw_copy_string,  // yes, simply make a copy
    .is_true        = _uw_string_is_true,
    .equal_sametype = _uw_string_equal_sametype,
    .equal          = _uw_string_equal,
    .equal_ctype    = _uw_string_equal_ctype,

    .interfaces = {
        // [UwInterfaceId_RandomAccess] = &string_type_random_access_interface
    }
};

static UwType list_type = {
    .id             = UwTypeId_List,
    .ancestor_id    = UwTypeId_Null,  // no ancestor
    .name           = "List",
    .data_size      = sizeof(struct _UwList),
    .data_offset    = sizeof(_UwValueBase),
    .init           = _uw_init_list,
    .fini           = _uw_fini_list,
    .hash           = _uw_hash_list,
    .copy           = _uw_copy_list,
    .dump           = _uw_dump_list,
    .to_string      = _uw_list_to_string,
    .is_true        = _uw_list_is_true,
    .equal_sametype = _uw_list_equal_sametype,
    .equal          = _uw_list_equal,
    .equal_ctype    = _uw_list_equal_ctype,

    .interfaces = {
        // [UwInterfaceId_RandomAccess] = &list_type_random_access_interface,
        // [UwInterfaceId_List]         = &list_type_list_interface
    }
};

static UwType map_type = {
    .id             = UwTypeId_Map,
    .ancestor_id    = UwTypeId_Null,  // no ancestor
    .name           = "Map",
    .data_size      = sizeof(struct _UwMap),
    .data_offset    = sizeof(_UwValueBase),
    .init           = _uw_init_map,
    .fini           = _uw_fini_map,
    .hash           = _uw_hash_map,
    .copy           = _uw_copy_map,
    .dump           = _uw_dump_map,
    .to_string      = _uw_map_to_string,
    .is_true        = _uw_map_is_true,
    .equal_sametype = _uw_map_equal_sametype,
    .equal          = _uw_map_equal,
    .equal_ctype    = _uw_map_equal_ctype,

    .interfaces = {
        // [UwInterfaceId_RandomAccess] = &map_type_random_access_interface
    }
};

static UwType class_type = {
    .id             = UwTypeId_Class,
    .ancestor_id    = UwTypeId_Null,  // no ancestor
    .name           = "Class",
    .data_size      = 0,
    .data_offset    = sizeof(_UwValueBase),
    .init           = _uw_init_class,
    .fini           = _uw_fini_integral,
    .hash           = _uw_hash_class,
    .copy           = _uw_copy_class,
    .dump           = _uw_dump_class,
    .to_string      = _uw_class_to_string,
    .is_true        = _uw_class_is_true,
    .equal_sametype = _uw_class_equal_sametype,
    .equal          = _uw_class_equal,
    .equal_ctype    = _uw_class_equal_ctype,

    .interfaces = {}
};

static UwInterface_File file_type_file_interface = {
    .open     = _uw_file_open,
    .close    = _uw_file_close,
    .set_fd   = _uw_file_set_fd,
    .get_name = _uw_file_get_name,
    .set_name = _uw_file_set_name
};

static UwInterface_FileReader file_type_file_reader_interface = {
    .read = _uw_file_read
};

static UwInterface_FileWriter file_type_file_writer_interface = {
    .write = _uw_file_write
};

static UwInterface_LineReader file_type_line_reader_interface = {
    .start             = _uw_file_start_read_lines,
    .read_line         = _uw_file_read_line,
    .read_line_inplace = _uw_file_read_line_inplace,
    .unread_line       = _uw_file_unread_line,
    .stop              = _uw_file_stop_read_lines
};

static UwType file_type = {
    .id             = UwTypeId_File,
    .ancestor_id    = UwTypeId_Null,  // no ancestor
    .name           = "File",
    .data_size      = sizeof(struct _UwFile),
    .data_offset    = sizeof(_UwValueBase),
    .init           = _uw_init_file,
    .fini           = _uw_fini_file,
    .hash           = _uw_hash_file,
    .copy           = _uw_copy_file,
    .dump           = _uw_dump_file,
    .to_string      = _uw_file_to_string,
    .is_true        = _uw_file_is_true,
    .equal_sametype = _uw_file_equal_sametype,
    .equal          = _uw_file_equal,
    .equal_ctype    = _uw_file_equal_ctype,

    .interfaces = {
        [UwInterfaceId_File]       = &file_type_file_interface,
        [UwInterfaceId_FileReader] = &file_type_file_reader_interface,
        [UwInterfaceId_FileWriter] = &file_type_file_writer_interface,
        [UwInterfaceId_LineReader] = &file_type_line_reader_interface
    }
};

static UwInterface_LineReader stringio_type_line_reader_interface = {
    .start             = _uw_stringio_start_read_lines,
    .read_line         = _uw_stringio_read_line,
    .read_line_inplace = _uw_stringio_read_line_inplace,
    .unread_line       = _uw_stringio_unread_line,
    .stop              = _uw_stringio_stop_read_lines
};

static UwType stringio_type = {
    .id             = UwTypeId_StringIO,
    .ancestor_id    = UwTypeId_String,  // it's a subclass!
    .name           = "StringIO",
    .data_size      = sizeof(struct _UwStringIO),
    .data_offset    = sizeof(_UwValueBase) + sizeof(struct _UwString*),
    .init           = _uw_init_stringio,
    .fini           = _uw_fini_stringio,
    .hash           = _uw_hash_string,
    .copy           = _uw_copy_stringio,
    .dump           = _uw_dump_string,
    .to_string      = _uw_copy_string,  // yes, simply make a copy of string
    .is_true        = _uw_string_is_true,
    .equal_sametype = _uw_string_equal_sametype,
    .equal          = _uw_string_equal,
    .equal_ctype    = _uw_string_equal_ctype,

    // in a subclass all interfaces of base classes must be in place:
    .interfaces = {
        // [UwInterfaceId_RandomAccess] = &string_type_random_access_interface
        [UwInterfaceId_LineReader] = &stringio_type_line_reader_interface
    }
};

UwType* _uw_types[1 << UW_TYPE_BITWIDTH] = {

    [UwTypeId_Null]   = &null_type,
    [UwTypeId_Bool]   = &bool_type,
    [UwTypeId_Int]    = &int_type,
    [UwTypeId_Float]  = &float_type,
    [UwTypeId_String] = &string_type,
    [UwTypeId_List]   = &list_type,
    [UwTypeId_Map]    = &map_type,
    [UwTypeId_Class]  = &class_type,
    [UwTypeId_File]     = &file_type,
    [UwTypeId_StringIO] = &stringio_type
};


int uw_add_type(UwType* type)
{
    for (int i = 0; i < (1 << UW_TYPE_BITWIDTH); i++) {
        if (_uw_types[i] == nullptr) {
            _uw_types[i] = type;
            return i;
        }
    }
    return -1;
}

int uw_subclass(UwType* type, char* name, UwTypeId ancestor_id, unsigned data_size)
{
    UwType* ancestor = _uw_types[ancestor_id];
    uw_assert(ancestor != nullptr);

    *type = *ancestor;  // copy type structure

    type->ancestor_id = ancestor_id;
    type->name = name;
    type->data_size = data_size;
    type->data_offset = ancestor->data_offset + ancestor->data_size;

    int type_id = uw_add_type(type);
    if (type_id != -1) {
        type->id = (UwTypeId) type_id;
    }
    return type_id;
}
