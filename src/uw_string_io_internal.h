#pragma once

#include "include/uw_base.h"

#ifdef __cplusplus
extern "C" {
#endif

struct _UwStringIO {
    // line reader data
    unsigned line_position;
    UwValuePtr pushback;
};

/****************************************************************
 * Basic interface methods
 */

bool       _uw_init_stringio(UwValuePtr self);
void       _uw_fini_stringio(UwValuePtr self);
UwValuePtr _uw_copy_stringio(UwValuePtr self);

/****************************************************************
 * LineReader interface methods
 */

bool       _uw_stringio_start_read_lines (UwValuePtr self);
UwValuePtr _uw_stringio_read_line        (UwValuePtr self);
bool       _uw_stringio_read_line_inplace(UwValuePtr self, UwValuePtr line);
bool       _uw_stringio_unread_line      (UwValuePtr self, UwValuePtr line);
void       _uw_stringio_stop_read_lines  (UwValuePtr self);

#ifdef __cplusplus
}
#endif
