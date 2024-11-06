#pragma once

#include <stdarg.h>
#include <uchar.h>

#include <fcntl.h>

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************
 * File interface
 */

typedef bool (*UwMethodOpenFile)(UwValuePtr self, char8_t* name, int flags, mode_t mode);
typedef void (*UwMethodCloseFile)(UwValuePtr self);
typedef bool (*UwMethodSetFileDescriptor)(UwValuePtr self, int fd);
typedef bool (*UwMethodSetFileName)(UwValuePtr self, char8_t* name);

// XXX other fd operation: seek, tell, etc.

typedef struct {
    UwMethodOpenFile          open;
    UwMethodCloseFile         close;  // only if opened with `open`, don't close one assigned by `set_fd`, right?
    UwMethodSetFileDescriptor set_fd;
    UwMethodSetFileName       set_name;

} UwInterface_File;

/****************************************************************
 * FileReader interface
 */

typedef ssize_t (*UwMethodReadFile)(UwValuePtr self, void* buffer, size_t buffer_size);

typedef struct {
    UwMethodReadFile read;

} UwInterface_FileReader;

/****************************************************************
 * FileWriter interface
 */

typedef ssize_t (*UwMethodWriteFile)(UwValuePtr self, void* data, size_t size);

// XXX truncate

typedef struct {
    UwMethodWriteFile write;

} UwInterface_FileWriter;

/****************************************************************
 * Shorthand functions
 */

static inline UwValuePtr uw_create_file()
{
    return _uw_create(UwTypeId_File);
}

UwValuePtr uw_file_open    (char8_t* name, int flags, mode_t mode);
void       uw_file_close   (UwValuePtr file);
bool       uw_file_set_fd  (UwValuePtr file, int fd);
bool       uw_file_set_name(UwValuePtr file, char8_t* name);
ssize_t    uw_file_read    (UwValuePtr file, void* buffer, size_t buffer_size);
ssize_t    uw_file_write   (UwValuePtr file, void* data, size_t size);

#ifdef __cplusplus
}
#endif
