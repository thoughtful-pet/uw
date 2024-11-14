#include "include/uw_c.h"
#include "src/uw_string_io_internal.h"

/****************************************************************
 * Basic interface methods
 */

bool _uw_init_stringio(UwValuePtr self)
{
    return true;
}

void _uw_fini_stringio(UwValuePtr self)
{
    struct _UwStringIO* sio = _uw_get_data_ptr(self, UwTypeId_StringIO, struct _UwStringIO*);
    sio->line_position = 0;
    uw_delete(&sio->pushback);
}

UwValuePtr _uw_copy_stringio(UwValuePtr self)
{
    // XXX not implemented yet
    return nullptr;
}

/****************************************************************
 * LineReader interface methods
 */

bool _uw_stringio_start_read_lines(UwValuePtr self)
{
    struct _UwStringIO* sio = _uw_get_data_ptr(self, UwTypeId_StringIO, struct _UwStringIO*);
    sio->line_position = 0;
    uw_delete(&sio->pushback);
    return true;
}

UwValuePtr _uw_stringio_read_line(UwValuePtr self)
{
    UwValue result = uw_create("");
    if (!result) {
        return nullptr;
    }
    if (!_uw_stringio_read_line_inplace(self, result)) {
        return nullptr;
    }
    return uw_move(&result);
}

bool _uw_stringio_read_line_inplace(UwValuePtr self, UwValuePtr line)
{
    struct _UwStringIO* sio = _uw_get_data_ptr(self, UwTypeId_StringIO, struct _UwStringIO*);

    uw_string_truncate(line, 0);

    if (sio->pushback) {
        if (!uw_string_append(line, sio->pushback)) {
            return false;
        }
        uw_delete(&sio->pushback);
        return true;
    }

    if (!uw_string_index_valid(self, sio->line_position)) {
        return false;
    }

    size_t lf_pos;
    if (!uw_string_indexof(self, '\n', sio->line_position, &lf_pos)) {
        lf_pos = uw_strlen(self) - 1;
    }
    uw_string_append_substring(line, self, sio->line_position, lf_pos + 1);
    sio->line_position = lf_pos + 1;
    return true;
}

bool _uw_stringio_unread_line(UwValuePtr self, UwValuePtr line)
{
    struct _UwStringIO* sio = _uw_get_data_ptr(self, UwTypeId_StringIO, struct _UwStringIO*);

    if (sio->pushback) {
        return false;
    }
    sio->pushback = uw_makeref(line);
    return true;
}

void _uw_stringio_stop_read_lines(UwValuePtr self)
{
    struct _UwStringIO* sio = _uw_get_data_ptr(self, UwTypeId_StringIO, struct _UwStringIO*);
    uw_delete(&sio->pushback);
}

/****************************************************************
 * Shorthand functions
 */

#define CREATE_STRING_IO_IMPL {  \
    UwValuePtr result = _uw_create(UwTypeId_StringIO);  \
    if (result) {  \
        if (!uw_string_append(result, str)) {  \
            uw_delete(&result);  \
        }  \
    }  \
    return result;  \
}

UwValuePtr _uw_create_string_io_u8_wrapper(char* str) CREATE_STRING_IO_IMPL
UwValuePtr _uw_create_string_io_u8(char8_t* str)      CREATE_STRING_IO_IMPL
UwValuePtr _uw_create_string_io_u32(char32_t* str)    CREATE_STRING_IO_IMPL
UwValuePtr _uw_create_string_io_uw(UwValuePtr str)    CREATE_STRING_IO_IMPL

bool uw_start_read_lines(UwValuePtr reader)
{
    UwInterface_LineReader* iface = uw_get_interface(reader, LineReader);
    if (!iface) {
        return false;
    }
    return iface->start(reader);
}

UwValuePtr uw_read_line(UwValuePtr reader)
{
    UwInterface_LineReader* iface = uw_get_interface(reader, LineReader);
    if (!iface) {
        return nullptr;
    }
    return iface->read_line(reader);
}

bool uw_read_line_inplace(UwValuePtr reader, UwValuePtr line)
{
    UwInterface_LineReader* iface = uw_get_interface(reader, LineReader);
    if (!iface) {
        return false;
    }
    return iface->read_line_inplace(reader, line);
}

bool uw_unread_line(UwValuePtr reader, UwValuePtr line)
{
    UwInterface_LineReader* iface = uw_get_interface(reader, LineReader);
    if (!iface) {
        return false;
    }
    return iface->unread_line(reader, line);
}

void uw_stop_read_lines(UwValuePtr reader)
{
    UwInterface_LineReader* iface = uw_get_interface(reader, LineReader);
    if (iface) {
        iface->stop(reader);
    }
}
