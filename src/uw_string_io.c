#include "include/uw.h"
#include "src/uw_charptr_internal.h"
#include "src/uw_string_internal.h"
#include "src/uw_string_io_internal.h"

/****************************************************************
 * Basic interface methods
 */

UwResult _uw_stringio_init(UwValuePtr self, va_list ap)
{
    UwValuePtr v = va_arg(ap, UwValuePtr);
    UwValue str = uw_clone(v);  // this converts CharPtr to string
    if (!uw_is_string(&str)) {
        return UwError(UW_ERROR_INCOMPATIBLE_TYPE);
    }
    struct _UwStringIO* sio = _uw_get_stringio_ptr(self);
    sio->line = uw_move(&str);
    sio->pushback = UwNull();
    return UwOK();
}

void _uw_stringio_fini(UwValuePtr self)
{
    struct _UwStringIO* sio = _uw_get_stringio_ptr(self);
    uw_destroy(&sio->line);
    uw_destroy(&sio->pushback);
}

UwResult _uw_stringio_deepcopy(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

void _uw_stringio_hash(UwValuePtr self, UwHashContext* ctx)
{
    _uw_hash_uint64(ctx, self->type_id);

    struct _UwStringIO* sio = _uw_get_stringio_ptr(self);
    struct _UwString* s = _uw_get_string_ptr(&sio->line);
    unsigned length = get_cap_methods(s)->get_length(s);
    if (length) {
        get_str_methods(s)->hash(get_char_ptr(s, 0), length, ctx);
    }
}

void _uw_stringio_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    struct _UwStringIO* sio = _uw_get_stringio_ptr(self);

    _uw_dump_start(fp, self, first_indent);
    _uw_string_dump_data(fp, &sio->line, next_indent);

    _uw_print_indent(fp, next_indent);
    fprintf(fp, "Current position: %u\n", sio->line_position);
    if (uw_is_null(&sio->pushback)) {
        fputs("Pushback: none\n", fp);
    }
    else if (!uw_is_string(&sio->pushback)) {
        fputs("WARNING: bad pushback:\n", fp);
        uw_dump(fp, &sio->pushback);
    } else {
        fputs("Pushback:\n", fp);
        _uw_string_dump_data(fp, &sio->pushback, next_indent);
    }
}

UwResult _uw_stringio_to_string(UwValuePtr self)
{
    struct _UwStringIO* sio = _uw_get_stringio_ptr(self);
    return uw_clone(&sio->line);
}

bool _uw_stringio_is_true(UwValuePtr self)
{
    struct _UwStringIO* sio = _uw_get_stringio_ptr(self);
    return uw_is_true(&sio->line);
}

bool _uw_stringio_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    struct _UwStringIO* sio_self  = _uw_get_stringio_ptr(self);
    struct _UwStringIO* sio_other = _uw_get_stringio_ptr(other);

    UwMethodEqual fn_cmp = _uw_types[sio_self->line.type_id]->_equal_sametype;
    return fn_cmp(&sio_self->line, &sio_other->line);
}

bool _uw_stringio_equal(UwValuePtr self, UwValuePtr other)
{
    struct _UwStringIO* sio_self  = _uw_get_stringio_ptr(self);

    UwMethodEqual fn_cmp = _uw_types[sio_self->line.type_id]->_equal;
    return fn_cmp(&sio_self->line, other);
}

/****************************************************************
 * LineReader interface methods
 */

UwResult _uwi_stringio_start_read_lines(UwValuePtr self)
{
    struct _UwStringIO* sio = _uw_get_stringio_ptr(self);
    sio->line_position = 0;
    sio->line_number = 0;
    uw_destroy(&sio->pushback);
    return UwOK();
}

UwResult _uwi_stringio_read_line(UwValuePtr self)
{
    UwValue result = UwString();
    UwValue status = _uwi_stringio_read_line_inplace(self, &result);
    if (uw_error(&status)) {
        return uw_move(&status);
    }
    return uw_move(&result);
}

UwResult _uwi_stringio_read_line_inplace(UwValuePtr self, UwValuePtr line)
{
    struct _UwStringIO* sio = _uw_get_stringio_ptr(self);

    uw_string_truncate(line, 0);

    if (uw_is_string(&sio->pushback)) {
        if (!uw_string_append(line, &sio->pushback)) {
            return UwOOM();
        }
        uw_destroy(&sio->pushback);
        sio->line_number++;
        return UwOK();
    }

    if (!uw_string_index_valid(&sio->line, sio->line_position)) {
        return UwError(UW_ERROR_EOF);
    }

    unsigned lf_pos;
    if (!uw_string_indexof(&sio->line, '\n', sio->line_position, &lf_pos)) {
        lf_pos = uw_strlen(&sio->line) - 1;
    }
    uw_string_append_substring(line, &sio->line, sio->line_position, lf_pos + 1);
    sio->line_position = lf_pos + 1;
    sio->line_number++;
    return UwOK();
}

UwResult _uwi_stringio_unread_line(UwValuePtr self, UwValuePtr line)
{
    struct _UwStringIO* sio = _uw_get_stringio_ptr(self);

    if (uw_is_null(&sio->pushback)) {
        sio->pushback = uw_clone(line);
        sio->line_number--;
        return UwOK();
    } else {
        return UwError(UW_ERROR_PUSHBACK_FAILED);
    }
}

UwResult _uwi_stringio_get_line_number(UwValuePtr self)
{
    return UwUnsigned(_uw_get_stringio_ptr(self)->line_number);
}

UwResult _uwi_stringio_stop_read_lines(UwValuePtr self)
{
    struct _UwStringIO* sio = _uw_get_stringio_ptr(self);
    uw_destroy(&sio->pushback);
    return UwOK();
}
