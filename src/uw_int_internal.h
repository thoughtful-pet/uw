#pragma once

#include "include/uw_value_base.h"

#ifdef __cplusplus
extern "C" {
#endif

UwValuePtr _uw_create_int        ();
void       _uw_hash_int          (UwValuePtr self, UwHashContext* ctx);
UwValuePtr _uw_copy_int          (UwValuePtr self);
void       _uw_dump_int          (UwValuePtr self, int indent);
UwValuePtr _uw_int_to_string     (UwValuePtr self);
bool       _uw_int_is_true       (UwValuePtr self);
bool       _uw_int_equal_sametype(UwValuePtr self, UwValuePtr other);
bool       _uw_int_equal         (UwValuePtr self, UwValuePtr other);
bool       _uw_int_equal_ctype   (UwValuePtr self, UwCType ctype, ...);

#ifdef __cplusplus
}
#endif
