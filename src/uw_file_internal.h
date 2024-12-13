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
    _UwValue name;

    // line reader data
    // XXX okay for now, revise later
    char8_t* buffer;
    unsigned position;  // in the buffer
    unsigned data_size; // in the buffer
    char8_t  partial_utf8[4];  // UTF-8 sequence may span adjacent reads
    unsigned partial_utf8_len;
    _UwValue pushback;  // for unread_line
    unsigned line_number;
};

struct _UwFileExtraData {
    // outline structure to determine correct aligned offset
    _UwExtraData   value_data;
    struct _UwFile file_data;
};

#define _uw_get_file_ptr(value)  \
    (  \
        &((struct _UwFileExtraData*) ((value)->extra_data))->file_data \
    )

/****************************************************************
 * Basic interface methods
 */

UwResult _uw_file_init          (UwValuePtr self, va_list ap);
void     _uw_file_fini          (UwValuePtr self);
void     _uw_file_hash          (UwValuePtr self, UwHashContext* ctx);
UwResult _uw_file_deepcopy      (UwValuePtr self);
void     _uw_file_dump          (UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail);
UwResult _uw_file_to_string     (UwValuePtr self);
bool     _uw_file_is_true       (UwValuePtr self);
bool     _uw_file_equal_sametype(UwValuePtr self, UwValuePtr other);
bool     _uw_file_equal         (UwValuePtr self, UwValuePtr other);

/****************************************************************
 * File interface methods
 */

UwResult _uwi_file_open    (UwValuePtr self, UwValuePtr file_name, int flags, mode_t mode);
void     _uwi_file_close   (UwValuePtr self);
bool     _uwi_file_set_fd  (UwValuePtr self, int fd);
UwResult _uwi_file_get_name(UwValuePtr self);
bool     _uwi_file_set_name(UwValuePtr self, UwValuePtr file_name);

/****************************************************************
 * FileReader interface methods
 */

UwResult _uwi_file_read(UwValuePtr self, void* buffer, unsigned buffer_size, unsigned* bytes_read);

/****************************************************************
 * FileWriter interface methods
 */

UwResult _uwi_file_write(UwValuePtr self, void* data, unsigned size, unsigned* bytes_written);

/****************************************************************
 * LineReader interface methods
 */

UwResult _uwi_file_start_read_lines (UwValuePtr self);
UwResult _uwi_file_read_line        (UwValuePtr self);
UwResult _uwi_file_read_line_inplace(UwValuePtr self, UwValuePtr line);
bool     _uwi_file_unread_line      (UwValuePtr self, UwValuePtr line);
unsigned _uwi_file_get_line_number  (UwValuePtr self);
void     _uwi_file_stop_read_lines  (UwValuePtr self);

#ifdef __cplusplus
}
#endif
