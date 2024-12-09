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

typedef bool       (*UwMethodOpenFile)         (UwValuePtr self, UwValuePtr file_name, int flags, mode_t mode);
typedef void       (*UwMethodCloseFile)        (UwValuePtr self);
typedef bool       (*UwMethodSetFileDescriptor)(UwValuePtr self, int fd);
typedef UwValuePtr (*UwMethodGetFileName)      (UwValuePtr self);
typedef bool       (*UwMethodSetFileName)      (UwValuePtr self, UwValuePtr file_name);

// XXX other fd operation: seek, tell, etc.

typedef struct {
    UwMethodOpenFile          open;
    UwMethodCloseFile         close;  // only if opened with `open`, don't close one assigned by `set_fd`, right?
    UwMethodSetFileDescriptor set_fd;
    UwMethodGetFileName       get_name;
    UwMethodSetFileName       set_name;

} UwInterface_File;

/****************************************************************
 * FileReader interface
 */

typedef bool (*UwMethodReadFile)(UwValuePtr self, void* buffer, unsigned buffer_size, unsigned* bytes_read);

typedef struct {
    UwMethodReadFile read;

} UwInterface_FileReader;

/****************************************************************
 * FileWriter interface
 */

typedef bool (*UwMethodWriteFile)(UwValuePtr self, void* data, unsigned size, unsigned* bytes_written);

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

UwValuePtr  uw_file_open_cstr      (char*      file_name, int flags, mode_t mode);
UwValuePtr _uw_file_open_u8_wrapper(char*      file_name, int flags, mode_t mode);
UwValuePtr _uw_file_open_u8        (char8_t*   file_name, int flags, mode_t mode);
UwValuePtr _uw_file_open_u32       (char32_t*  file_name, int flags, mode_t mode);
UwValuePtr _uw_file_open_uw        (UwValuePtr file_name, int flags, mode_t mode);

void       uw_file_close   (UwValuePtr file);
bool       uw_file_set_fd  (UwValuePtr file, int fd);
UwValuePtr uw_file_get_name(UwValuePtr file);
bool       uw_file_set_name(UwValuePtr file, UwValuePtr file_name);
bool       uw_file_read    (UwValuePtr file, void* buffer, unsigned buffer_size, unsigned* bytes_read);
bool       uw_file_write   (UwValuePtr file, void* data, unsigned size, unsigned* bytes_written);

#ifdef __cplusplus
}
#endif
