#pragma once

#include "include/uw_base.h"
#include "include/uw_file.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LINE_READER_BUFFER_SIZE  4096  // typical filesystem block size

struct _UwFile {
    int fd;               // file descriptor
    bool is_external_fd;  // fd is set by `set_fd` and should not be closed
    int error;            // errno, set by `open`
    UwValuePtr name;

    // line reader data
    // XXX okay for now, revise later
    char8_t* buffer;
    size_t   position;  // in the buffer
    size_t   data_size; // in the buffer
    char8_t  partial_utf8[4];  // UTF-8 sequence may span adjacent reads
    unsigned partial_utf8_len;
    UwValuePtr pushback;  // for unread_line
};

/****************************************************************
 * Basic interface methods
 */

bool       _uw_init_file          (UwValuePtr self);
void       _uw_fini_file          (UwValuePtr self);
void       _uw_hash_file          (UwValuePtr self, UwHashContext* ctx);
UwValuePtr _uw_copy_file          (UwValuePtr self);
void       _uw_dump_file          (UwValuePtr self, int indent);
UwValuePtr _uw_file_to_string     (UwValuePtr self);
bool       _uw_file_is_true       (UwValuePtr self);
bool       _uw_file_equal_sametype(UwValuePtr self, UwValuePtr other);
bool       _uw_file_equal         (UwValuePtr self, UwValuePtr other);
bool       _uw_file_equal_ctype   (UwValuePtr self, UwCType ctype, ...);

/****************************************************************
 * File interface methods
 */

// Rationale for using char8_t* for file names:
// 1. if it was UwValuePtr, we'd have to copy it for immutability
// 2. in C it's mainly in UTF-8 encoding, creating UwValue from it
//    and passing to methods would involve unnecessary duplicate copying
// Maybe this will be changed in future, but for now it's a better approach.

bool _uw_file_open    (UwValuePtr self, char8_t* name, int flags, mode_t mode);
void _uw_file_close   (UwValuePtr self);
bool _uw_file_set_fd  (UwValuePtr self, int fd);
bool _uw_file_set_name(UwValuePtr self, char8_t* name);

/****************************************************************
 * FileReader interface methods
 */

ssize_t _uw_file_read(UwValuePtr self, void* buffer, size_t buffer_size);

/****************************************************************
 * FileWriter interface methods
 */

ssize_t _uw_file_write(UwValuePtr self, void* data, size_t size);

/****************************************************************
 * LineReader interface methods
 */

bool       _uw_file_start_read_lines (UwValuePtr self);
UwValuePtr _uw_file_read_line        (UwValuePtr self);
bool       _uw_file_read_line_inplace(UwValuePtr self, UwValuePtr line);
bool       _uw_file_unread_line      (UwValuePtr self, UwValuePtr line);
void       _uw_file_stop_read_lines  (UwValuePtr self);

#ifdef __cplusplus
}
#endif
