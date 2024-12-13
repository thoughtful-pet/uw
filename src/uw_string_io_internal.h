#pragma once

#include "include/uw_base.h"

#ifdef __cplusplus
extern "C" {
#endif

struct _UwStringIO {
    // line reader data
    _UwValue line;
    _UwValue pushback;
    unsigned line_number;
    unsigned line_position;
};

struct _UwStringIOExtraData {
    // outline structure to determine correct aligned offset
    _UwExtraData       value_data;
    struct _UwStringIO stringio_data;
};

#define _uw_get_stringio_ptr(value)  \
    (  \
        &((struct _UwStringIOExtraData*) ((value)->extra_data))->stringio_data \
    )

/****************************************************************
 * Basic interface methods
 */

UwResult _uw_stringio_init          (UwValuePtr self, va_list ap);
void     _uw_stringio_fini          (UwValuePtr self);
UwResult _uw_stringio_deepcopy      (UwValuePtr self);
void     _uw_stringio_hash          (UwValuePtr self, UwHashContext* ctx);
void     _uw_stringio_dump          (UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail);
UwResult _uw_stringio_to_string     (UwValuePtr self);
bool     _uw_stringio_is_true       (UwValuePtr self);
bool     _uw_stringio_equal_sametype(UwValuePtr self, UwValuePtr other);
bool     _uw_stringio_equal         (UwValuePtr self, UwValuePtr other);

/****************************************************************
 * LineReader interface methods
 */

UwResult _uwi_stringio_start_read_lines (UwValuePtr self);
UwResult _uwi_stringio_read_line        (UwValuePtr self);
UwResult _uwi_stringio_read_line_inplace(UwValuePtr self, UwValuePtr line);
bool     _uwi_stringio_unread_line      (UwValuePtr self, UwValuePtr line);
unsigned _uwi_stringio_get_line_number  (UwValuePtr self);
void     _uwi_stringio_stop_read_lines  (UwValuePtr self);

#ifdef __cplusplus
}
#endif
