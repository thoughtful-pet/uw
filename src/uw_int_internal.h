#pragma once

#include "include/uw_base.h"

#ifdef __cplusplus
extern "C" {
#endif

UwResult _uw_int_create        (UwTypeId type_id, va_list ap);
void     _uw_int_hash          (UwValuePtr self, UwHashContext* ctx);
void     _uw_int_dump          (UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail);
UwResult _uw_int_to_string     (UwValuePtr self);
bool     _uw_int_is_true       (UwValuePtr self);
bool     _uw_int_equal_sametype(UwValuePtr self, UwValuePtr other);
bool     _uw_int_equal         (UwValuePtr self, UwValuePtr other);

#ifdef __cplusplus
}
#endif
