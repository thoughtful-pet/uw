#pragma once

#include "include/uw_base.h"

#ifdef __cplusplus
extern "C" {
#endif

UwResult _uw_charptr_create        (UwTypeId type_id, va_list ap);
void     _uw_charptr_hash          (UwValuePtr self, UwHashContext* ctx);
void     _uw_charptr_dump          (UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail);
UwResult _uw_charptr_to_string     (UwValuePtr self);
bool     _uw_charptr_is_true       (UwValuePtr self);
bool     _uw_charptr_equal_sametype(UwValuePtr self, UwValuePtr other);
bool     _uw_charptr_equal         (UwValuePtr self, UwValuePtr other);

unsigned _uw_charptr_strlen2(UwValuePtr charptr, uint8_t* char_size);

bool _uw_charptr_equal_string(UwValuePtr charptr, UwValuePtr str);
// comparison helper

#ifdef __cplusplus
}
#endif
