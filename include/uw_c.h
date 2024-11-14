#pragma once

/*
 * Include file for pure C.
 */

#include <uw_base.h>
#include <uw_list.h>
#include <uw_map.h>
#include <uw_string.h>
#include <uw_ctype.h>
#include <uw_file.h>
#include <uw_string_io.h>
#include <uw_tg.h>

// automatically cleaned value
#define UwValue [[ gnu::cleanup(uw_delete) ]] UwValuePtr

// automatically cleaned value
#define CString [[ gnu::cleanup(uw_delete_cstring) ]] CStringPtr

/*
 * Although C++ could be a better choice, modern C provides
 * a couple of amazing features:
 *
 * 1. generics
 * 2. cleanup attribute for variables
 *
 * Generics allow to mimic overloaded functions, i.e.
 *
 *   uw_list_append(1);
 *   uw_list_append("wow");
 *
 * Cleanup attribute facilitates dynamic memory management.
 * However, it's not as good as destructors in C++ because
 * cleanup handlers are fired only at scope exit and
 * there's always a chance to shoot in the foot assigning
 * new value to autocleaned variable without explicitly freeing
 * previous value.
 *
 * UW library uses a couple of definitions:
 *
 *   1. `typename`Ptr, e.g. UwValuePtr and CStringPtr
 *      This is the basic definition of raw pointers
 *      that can be used for constant arguments and return values.
 *
 *   2. `typename`, e.g. UwValue and CString
 *      This is a type name for automatically cleaned variables.
 *
 * The general rule is to use autocleaned variables in small scopes,
 * especially if such variables are used in a loop body:
 *
 *  for (unsigned i = 0, n = uw_list_length(mylist); i < n; i++) {
 *      {
 *          UwValue item = uw_list_item(mylist, i);
 *
 *          // process item
 *
 *          // without nested scope we'd have to explicitly
 *          // call uw_delete(&item)
 *      }
 *  }
 *
 * Always initialize autocleaned variable for cleanup function
 * to work properly:
 *
 *   {
 *       UwValue myvar1 = uw_create(123);
 *       UwValue myvar2 = nullptr;  // maybe create value later
 *
 *       // do the job
 *   }
 *
 * When returning a value from an autocleaned variable,
 * use uw_move() function that transfers ownership to the basic
 * `typename`Ptr, i.e.
 *
 *   UwValuePtr myfunc()
 *   {
 *       UwValue result = uw_create(false);
 *
 *       // do the job
 *
 *       return uw_move(result);
 *   }
 *
 * Never pass newly created value to a function that increments refcount,
 * i.e. the following would cause memory leak:
 *
 *   uw_list_append(mylist, uw_create(1));
 *
 * Use a temporary variable instead:
 *
 *   {
 *       UwValue v = uw_create(1);
 *       uw_list_append(mylist, v);
 *   }
 *
 * The only exception is *_va functions where uw_value_ptr designator allows that:
 *
 *   uw_list_append_va(mylist, uw_value_ptr, uw_create(1), uw_value_ptr, uw_create(2), -1);
 *
 * For more examples see how references are handled in `src/uw_map.c` when calling
 * internal `update_map` function.
 *
 *
 * Yes, C is weird. C++ could handle this better, but it's weird in its own way.
 */

