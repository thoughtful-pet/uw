#pragma once

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

#define uw_create_map() _uw_create_map()
UwValuePtr _uw_create_map();
UwValuePtr uw_create_map_va(...);
UwValuePtr uw_create_map_ap(va_list ap);

bool uw_map_update(UwValuePtr map, UwValuePtr key, UwValuePtr value);
bool uw_map_update_va(UwValuePtr map, ...);
bool uw_map_update_ap(UwValuePtr map, va_list ap);
/*
 * Insert or assign key-value pair using move semantic.
 *
 * As long as UW values are mutable, the key is copied.
 */

/*
 * Check `key` is in `map`.
 */
bool _uw_map_has_key_null (UwValuePtr map, UwType_Null  key);
bool _uw_map_has_key_bool (UwValuePtr map, UwType_Bool  key);
bool _uw_map_has_key_int  (UwValuePtr map, UwType_Int   key);
bool _uw_map_has_key_float(UwValuePtr map, UwType_Float key);
bool _uw_map_has_key_u8   (UwValuePtr map, char8_t*     key);
bool _uw_map_has_key_u32  (UwValuePtr map, char32_t*    key);
bool _uw_map_has_key_uw   (UwValuePtr map, UwValuePtr   key);

/*
 * Get value by `key`. Return nullptr if `key` is not in `map`.
 */
UwValuePtr _uw_map_get_null (UwValuePtr map, UwType_Null  key);
UwValuePtr _uw_map_get_bool (UwValuePtr map, UwType_Bool  key);
UwValuePtr _uw_map_get_int  (UwValuePtr map, UwType_Int   key);
UwValuePtr _uw_map_get_float(UwValuePtr map, UwType_Float key);
UwValuePtr _uw_map_get_u8   (UwValuePtr map, char8_t*     key);
UwValuePtr _uw_map_get_u32  (UwValuePtr map, char32_t*    key);
UwValuePtr _uw_map_get_uw   (UwValuePtr map, UwValuePtr   key);

/*
 * Delete item from map by `key`.
 */
void _uw_map_del_null (UwValuePtr map, UwType_Null  key);
void _uw_map_del_bool (UwValuePtr map, UwType_Bool  key);
void _uw_map_del_int  (UwValuePtr map, UwType_Int   key);
void _uw_map_del_float(UwValuePtr map, UwType_Float key);
void _uw_map_del_u8   (UwValuePtr map, char8_t*     key);
void _uw_map_del_u32  (UwValuePtr map, char32_t*    key);
void _uw_map_del_uw   (UwValuePtr map, UwValuePtr   key);

size_t uw_map_length(UwValuePtr map);
/*
 * Return the number of items in `map`.
 */

bool uw_map_item(UwValuePtr map, size_t index, UwValuePtr* key, UwValuePtr* value);
/*
 * Get key-value pair from the map.
 * Return true if `index` is valid.
 * It's the caller's responsibility to destroy returned key-value to avoid memory leaks.
 * The simplest way is passing pointers to UwValue.
 */


#ifdef __cplusplus
}
#endif
