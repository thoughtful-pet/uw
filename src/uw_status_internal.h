#pragma once

#include "include/uw_base.h"

#ifdef __cplusplus
extern "C" {
#endif

struct _UwStatusExtraData {
    _UwExtraData value_data;
    char* status_desc;  // status description
                        // note: allocated by asprintf
};

UwResult _uw_status_create        (UwTypeId type_id, va_list ap);
void     _uw_status_fini          (UwValuePtr self);
void     _uw_status_hash          (UwValuePtr self, UwHashContext* ctx);
UwResult _uw_status_deepcopy      (UwValuePtr self);
void     _uw_status_dump          (UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail);
UwResult _uw_status_to_string     (UwValuePtr self);
bool     _uw_status_is_true       (UwValuePtr self);
bool     _uw_status_equal_sametype(UwValuePtr self, UwValuePtr other);
bool     _uw_status_equal         (UwValuePtr self, UwValuePtr other);

#ifdef __cplusplus
}
#endif
