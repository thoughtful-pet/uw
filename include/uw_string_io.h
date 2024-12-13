#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************
 * LineReader interface
 */

typedef UwResult (*UwMethodStartReadLines)(UwValuePtr self);
/*
 * Prepare to read lines.
 *
 * Basically, any value that implements LineReader interface
 * must be prepared to read lines without making this call.
 *
 * Calling this method again should reset line reader.
 */

typedef UwResult (*UwMethodReadLine)(UwValuePtr self);
/*
 * Read next line.
 */

typedef UwResult (*UwMethodReadLineInPlace)(UwValuePtr self, UwValuePtr line);
/*
 * Truncate line and read next line into it.
 * Return true if read some data, false if error or eof.
 *
 * Rationale: avoid frequent memory allocations that would take place
 * if we returned line by line.
 * On the contrary, existing line is a pre-allocated buffer for the next one.
 */

typedef bool (*UwMethodUnreadLine)(UwValuePtr self, UwValuePtr line);
/*
 * Push line back to the reader.
 * Only one pushback is guaranteed.
 * Return false if pushback buffer is full.
 */

typedef unsigned (*UwMethodGetLineNumber)(UwValuePtr self);
/*
 * Return current line number, 1-based.
 */

typedef void (*UwMethodStopReadLines)(UwValuePtr self);
/*
 * Free internal buffer.
 */

typedef struct {
    UwMethodStartReadLines  start;
    UwMethodReadLine        read_line;
    UwMethodReadLineInPlace read_line_inplace;
    UwMethodUnreadLine      unread_line;
    UwMethodGetLineNumber   get_line_number;
    UwMethodStopReadLines   stop;

} UwInterface_LineReader;


/****************************************************************
 * Shorthand constructors
 */

#define uw_create_string_io(str) _Generic((str), \
             char*: _uw_create_string_io_u8_wrapper,  \
          char8_t*: _uw_create_string_io_u8,          \
         char32_t*: _uw_create_string_io_u32,         \
        UwValuePtr: _uw_create_string_io              \
    )((str))

static inline UwResult  uw_create_string_io_cstr(char* str)      { __UWDECL_CharPtr  (v, str); return _uw_create(UwTypeId_StringIO, &v); }
static inline UwResult _uw_create_string_io_u8  (char8_t* str)   { __UWDECL_Char8Ptr (v, str); return _uw_create(UwTypeId_StringIO, &v); }
static inline UwResult _uw_create_string_io_u32 (char32_t* str)  { __UWDECL_Char32Ptr(v, str); return _uw_create(UwTypeId_StringIO, &v); }
static inline UwResult _uw_create_string_io     (UwValuePtr str) { return _uw_create(UwTypeId_StringIO, str); }

static inline UwResult _uw_create_string_io_u8_wrapper(char* str)
{
    return _uw_create_string_io_u8((char8_t*) str);
}

/****************************************************************
 * Interface shorthand methods
 */

UwResult uw_start_read_lines (UwValuePtr reader);
UwResult uw_read_line        (UwValuePtr reader);
UwResult uw_read_line_inplace(UwValuePtr reader, UwValuePtr line);
bool     uw_unread_line      (UwValuePtr reader, UwValuePtr line);
unsigned uw_get_line_number  (UwValuePtr reader);
void     uw_stop_read_lines  (UwValuePtr reader);

#ifdef __cplusplus
}
#endif
