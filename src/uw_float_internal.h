#pragma once

#include "include/uw_base.h"

#ifdef __cplusplus
extern "C" {
#endif

bool       _uw_init_float          (UwValuePtr self);
void       _uw_hash_float          (UwValuePtr self, UwHashContext* ctx);
UwValuePtr _uw_copy_float          (UwValuePtr self);
void       _uw_dump_float          (UwValuePtr self, int indent);
UwValuePtr _uw_float_to_string     (UwValuePtr self);
bool       _uw_float_is_true       (UwValuePtr self);
bool       _uw_float_equal_sametype(UwValuePtr self, UwValuePtr other);
bool       _uw_float_equal         (UwValuePtr self, UwValuePtr other);
bool       _uw_float_equal_ctype   (UwValuePtr self, UwCType ctype, ...);

#ifdef __cplusplus
}
#endif
