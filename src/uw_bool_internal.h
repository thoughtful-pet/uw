#pragma once

#include "include/uw_base.h"

#ifdef __cplusplus
extern "C" {
#endif

UwValuePtr _uw_create_bool        ();
void       _uw_hash_bool          (UwValuePtr self, UwHashContext* ctx);
UwValuePtr _uw_copy_bool          (UwValuePtr self);
void       _uw_dump_bool          (UwValuePtr self, int indent);
UwValuePtr _uw_bool_to_string     (UwValuePtr self);
bool       _uw_bool_is_true       (UwValuePtr self);
bool       _uw_bool_equal_sametype(UwValuePtr self, UwValuePtr other);
bool       _uw_bool_equal         (UwValuePtr self, UwValuePtr other);
bool       _uw_bool_equal_ctype   (UwValuePtr self, UwCType ctype, ...);

#ifdef __cplusplus
}
#endif
