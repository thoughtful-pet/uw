#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************
 * LineReader interface
 */

typedef bool (*UwMethodStartReadLines)(UwValuePtr self);
/*
 * Prepare to read lines.
 *
 * Basically, any value that implements LineReader interface
 * must be prepared to read lines without making this call.
 *
 * Calling this method again should reset line reader.
 */

typedef UwValuePtr (*UwMethodReadLine)(UwValuePtr self);
/*
 * Read next line.
 */

typedef bool (*UwMethodReadLineInPlace)(UwValuePtr self, UwValuePtr line);
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
 *
 * WARNING: when using in conjunction with read_line, do not forget to create
 * new line for the result!
 * That's because unread simply increments refcount and if the same string
 * is used for read_line, its content will be overwritten!
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
    UwMethodStopReadLines   stop;

} UwInterface_LineReader;


/****************************************************************
 * Shorthand functions
 */

UwValuePtr _uw_create_string_io_u8_wrapper(char* str);
UwValuePtr _uw_create_string_io_u8(char8_t* str);
UwValuePtr _uw_create_string_io_u32(char32_t* str);
UwValuePtr _uw_create_string_io_uw(UwValuePtr str);

bool       uw_start_read_lines (UwValuePtr reader);
UwValuePtr uw_read_line        (UwValuePtr reader);
bool       uw_read_line_inplace(UwValuePtr reader, UwValuePtr line);
bool       uw_unread_line      (UwValuePtr reader, UwValuePtr line);
void       uw_stop_read_lines  (UwValuePtr reader);

#ifdef __cplusplus
}
#endif
