#pragma once

/*
 * C types support.
 */

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

UwValuePtr _uwc_create_null (nullptr_t    dummy);
UwValuePtr _uwc_create_bool (UwType_Bool  initializer);
UwValuePtr _uwc_create_int  (UwType_Int   initializer);
UwValuePtr _uwc_create_float(UwType_Float initializer);

UwValuePtr uw_create_from_ctype(UwCType ctype, va_list args);
/*
 * Helper function for variadic constructors.
 * Create UwValue from C type returned by va_arg(args).
 * See C type identifiers.
 * For uw_value return reference to the orginal value using uw_makeref.
 * For uw_ptr return the orginal value as is.
 */

/****************************************************************
 * C strings
 */

typedef char* CStringPtr;

// somewhat ugly macro to define a local variable initialized with a copy of uw string:
#define UW_CSTRING_LOCAL(variable_name, uw_str) \
    char variable_name[uw_strlen(uw_str) + 1]; \
    uw_string_copy_buf((uw_str), variable_name)

void uw_delete_cstring(CStringPtr* str);

#ifdef __cplusplus
}
#endif
