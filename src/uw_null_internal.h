#pragma once

#include "include/uw_base.h"

#ifdef __cplusplus
extern "C" {
#endif

bool       _uw_init_null          (UwValuePtr self);
void       _uw_hash_null          (UwValuePtr self, UwHashContext* ctx);
UwValuePtr _uw_copy_null          (UwValuePtr self);
void       _uw_dump_null          (UwValuePtr self, int indent);
UwValuePtr _uw_null_to_string     (UwValuePtr self);
bool       _uw_null_is_true       (UwValuePtr self);
bool       _uw_null_equal_sametype(UwValuePtr self, UwValuePtr other);
bool       _uw_null_equal         (UwValuePtr self, UwValuePtr other);
bool       _uw_null_equal_ctype   (UwValuePtr self, UwCType ctype, ...);

#ifdef __cplusplus
}
#endif
