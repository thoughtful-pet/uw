#pragma once

#include <ctype.h>

#ifdef UW_WITH_ICU
    // ICU library for character classification:
#   include <unicode/uchar.h>
#endif

#include <uw_ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

UwValuePtr  uw_create_string_cstr(char*      initializer);
UwValuePtr _uw_create_string_u8  (char8_t*   initializer);
UwValuePtr _uw_create_string_u32 (char32_t*  initializer);

UwValuePtr uw_create_empty_string(unsigned capacity, uint8_t char_size);

uint8_t uw_string_char_size(UwValuePtr s);

// check if `index` is within string length
#define uw_string_index_valid(str, index) ((index) < uw_strlen(str))

bool  uw_equal_cstr(UwValuePtr a, char* b);
bool _uw_equal_u8  (UwValuePtr a, char8_t* b);
bool _uw_equal_u32 (UwValuePtr a, char32_t* b);

unsigned uw_strlen(UwValuePtr str);
/*
 * Return length of string.
 */

CStringPtr uw_string_to_cstring(UwValuePtr str);
/*
 * Create C string.
 * XXX decode multibyte ???
 */

void uw_string_copy_buf(UwValuePtr str, char* buffer);
/*
 * Copy string to buffer, appending terminating 0.
 * Use carefully. The caller is responsible to allocate the buffer.
 * Encode multibyte chars to UTF-8.
 */

UwValuePtr uw_string_get_substring(UwValuePtr str, unsigned start_pos, unsigned end_pos);
/*
 * Get substring from `start_pos` to `end_pos`.
 */

bool  uw_substring_eq_cstr(UwValuePtr a, unsigned start_pos, unsigned end_pos, char*      b);
bool _uw_substring_eq_u8  (UwValuePtr a, unsigned start_pos, unsigned end_pos, char8_t*   b);
bool _uw_substring_eq_u32 (UwValuePtr a, unsigned start_pos, unsigned end_pos, char32_t*  b);
bool _uw_substring_eq_uw  (UwValuePtr a, unsigned start_pos, unsigned end_pos, UwValuePtr b);

char32_t uw_string_at(UwValuePtr str, unsigned position);
/*
 * Return character at `position`.
 * If position is beyond end of line return 0.
 */

void uw_string_erase(UwValuePtr str, unsigned start_pos, unsigned end_pos);
/*
 * Erase characters from `start_pos` to `end_pos`.
 */

void uw_string_truncate(UwValuePtr str, unsigned position);
/*
 * Truncate string at given `position`.
 */

bool uw_string_indexof(UwValuePtr str, char32_t chr, unsigned start_pos, unsigned* result);
/*
 * Find first occurence of `chr` in `str` starting from `start_pos`.
 *
 * Return true if character is found and write its position to `result`.
 * `result` can be nullptr if position is not needed and the function
 * is called just to check if `chr` is in `str`.
 */


void uw_string_ltrim(UwValuePtr str);
void uw_string_rtrim(UwValuePtr str);
void uw_string_trim(UwValuePtr str);

void uw_string_lower(UwValuePtr str);
void uw_string_upper(UwValuePtr str);


/*
 * Append functions
 */
bool _uw_string_append_char(UwValuePtr dest, char       c);
bool  uw_string_append_cstr(UwValuePtr dest, char*      src);
bool _uw_string_append_c32 (UwValuePtr dest, char32_t   c);
bool _uw_string_append_u8  (UwValuePtr dest, char8_t*   src);
bool _uw_string_append_u32 (UwValuePtr dest, char32_t*  src);
bool _uw_string_append_uw  (UwValuePtr dest, UwValuePtr src);

bool uw_string_append_utf8(UwValuePtr dest, char8_t* buffer, unsigned size, unsigned* bytes_processed);
/*
 * Append UTF-8-encoded characters from `buffer`.
 * Write the number of bytes processed to `bytes_processed`, which can be less
 * than `size` if buffer ends with incomplete UTF-8 sequence.
 *
 * Return false if out of memory.
 */

bool uw_string_append_buffer(UwValuePtr dest, uint8_t* buffer, unsigned size);
/*
 * Append bytes from `buffer`.
 * `dest` char size must be 1.
 *
 * Return false if out of memory.
 */

/*
 * Insert functions
 * TODO other types
 */
bool _uw_string_insert_many_c32(UwValuePtr str, unsigned position, char32_t value, unsigned n);

/*
 * Append substring functions.
 *
 * Append `src` substring starting from `src_start_pos` to `src_end_pos`.
 */
bool  uw_string_append_substring_cstr(UwValuePtr dest, char*      src, unsigned src_start_pos, unsigned src_end_pos);
bool _uw_string_append_substring_u8  (UwValuePtr dest, char8_t*   src, unsigned src_start_pos, unsigned src_end_pos);
bool _uw_string_append_substring_u32 (UwValuePtr dest, char32_t*  src, unsigned src_start_pos, unsigned src_end_pos);
bool _uw_string_append_substring_uw  (UwValuePtr dest, UwValuePtr src, unsigned src_start_pos, unsigned src_end_pos);

/*
 * Split functions.
 * Return list of strings.
 */
UwValuePtr uw_string_split(UwValuePtr str);  // split by spaces

UwValuePtr _uw_string_split_c32(UwValuePtr str, char32_t splitter);

UwValuePtr  uw_string_split_any_cstr(UwValuePtr str, char*      splitters);
UwValuePtr _uw_string_split_any_u8  (UwValuePtr str, char8_t*   splitters);
UwValuePtr _uw_string_split_any_u32 (UwValuePtr str, char32_t*  splitters);
UwValuePtr _uw_string_split_any_uw  (UwValuePtr str, UwValuePtr splitters);

UwValuePtr  uw_string_split_strict_cstr(UwValuePtr str, char*      splitter);
UwValuePtr _uw_string_split_strict_u8  (UwValuePtr str, char8_t*   splitter);
UwValuePtr _uw_string_split_strict_u32 (UwValuePtr str, char32_t*  splitter);
UwValuePtr _uw_string_split_strict_uw  (UwValuePtr str, UwValuePtr splitter);

/*
 * Join list items. Return string value.
 */
UwValuePtr _uw_string_join_c32(char32_t   separator, UwValuePtr list);
UwValuePtr _uw_string_join_u8 (char8_t*   separator, UwValuePtr list);
UwValuePtr _uw_string_join_u32(char32_t*  separator, UwValuePtr list);
UwValuePtr _uw_string_join_uw (UwValuePtr separator, UwValuePtr list);

/*
 * Miiscellaneous helper functions.
 */

unsigned uw_strlen_in_utf8(UwValuePtr str);
/*
 * Return length of str as if was encoded in UTF-8.
 */

char* uw_char32_to_utf8(char32_t codepoint, char* buffer);
/*
 * Write up to 4 characters to buffer.
 * Return pointer to the next position in buffer.
 */

void* uw_string_data_ptr(UwValuePtr str);
/*
 * Return pointer to internal string data.
 * The function is intended for file I/O operations.
 */

void _uw_putchar32_utf8(char32_t codepoint);

unsigned utf8_strlen(char8_t* str);
/*
 * Count the number of codepoints in UTF8-encoded string.
 */

unsigned utf8_strlen2(char8_t* str, uint8_t* char_size);
/*
 * Count the number of codepoints in UTF8-encoded string
 * and find max char size.
 */

unsigned utf8_strlen2_buf(char8_t* buffer, unsigned* size, uint8_t* char_size);
/*
 * Count the number of codepoints in the buffer and find max char size.
 *
 * Null characters are allowed! They are counted as zero codepoints.
 *
 * Return the number of codepoints.
 * Write the number of processed bytes back to `size`.
 * This number can be less than original `size` if buffer ends with
 * incomplete sequence.
 */

char8_t* utf8_skip(char8_t* str, unsigned n);
/*
 * Skip `n` characters, return pointer to `n`th char.
 */

unsigned u32_strlen(char32_t* str);
/*
 * Find length of null-terminated `str`.
 */

unsigned u32_strlen2(char32_t* str, uint8_t* char_size);
/*
 * Find both length of null-terminated `str` and max char size in one go.
 */

char32_t* u32_strchr(char32_t* str, char32_t chr);
/*
 * Find the first occurrence of `chr` in the null-terminated `str`.
 */

uint8_t u32_char_size(char32_t* str, unsigned max_len);
/*
 * Find the maximal size of character in `str`, up to `max_len` or null terminator.
 */

#ifdef UW_WITH_ICU
#   define uw_char_lower(c)  u_tolower(c)
#   define uw_char_upper(c)  u_toupper(c)
#else
#   define uw_char_lower(c)  tolower(c)
#   define uw_char_upper(c)  toupper(c)
#endif

/****************************************************************
 * Character classification functions
 */

#ifdef UW_WITH_ICU
#   define uw_char_isspace(c)  u_isspace(c)
#else
#   define uw_char_isspace(c)  isspace(c)
#endif
/*
 * Return true if `c` is a whitespace character.
 */

#define uw_char_isdigit(c)  isdigit(c)
/*
 * Return true if `c` is an ASCII digit.
 * Do not consider any other unicode digits because this function
 * is used in conjunction with standard C library (string to number conversion)
 * that does not support unicode character classification.
 */

/****************************************************************
 * Debug functions
 */

#ifdef DEBUG
    UwValuePtr uw_create_empty_string2(uint8_t cap_size, uint8_t char_size);
#endif

#ifdef __cplusplus
}
#endif
