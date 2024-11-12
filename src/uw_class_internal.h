#pragma once

#include "include/uw_base.h"

#ifdef __cplusplus
extern "C" {
#endif

bool       _uw_init_class          (UwValuePtr self);
void       _uw_hash_class          (UwValuePtr self, UwHashContext* ctx);
UwValuePtr _uw_copy_class          (UwValuePtr self);
void       _uw_dump_class          (UwValuePtr self, int indent, struct _UwValueChain* prev_compound);
UwValuePtr _uw_class_to_string     (UwValuePtr self);
bool       _uw_class_is_true       (UwValuePtr self);
bool       _uw_class_equal_sametype(UwValuePtr self, UwValuePtr other);
bool       _uw_class_equal         (UwValuePtr self, UwValuePtr other);
bool       _uw_class_equal_ctype   (UwValuePtr self, UwCType ctype, ...);

#ifdef __cplusplus
}
#endif
