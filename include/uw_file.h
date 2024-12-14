#pragma once

#include <stdarg.h>
#include <uchar.h>

#include <fcntl.h>

#ifdef __cplusplus
extern "C" {
#endif

// File errors
#define UW_ERROR_FILE_ALREADY_OPENED  300
#define UW_ERROR_CANNOT_SET_FILENAME  301
#define UW_ERROR_FD_ALREADY_SET       302

/****************************************************************
 * File interface
 */

typedef UwResult (*UwMethodOpenFile)         (UwValuePtr self, UwValuePtr file_name, int flags, mode_t mode);
typedef UwResult (*UwMethodCloseFile)        (UwValuePtr self);
typedef UwResult (*UwMethodSetFileDescriptor)(UwValuePtr self, int fd);
typedef UwResult (*UwMethodGetFileName)      (UwValuePtr self);
typedef UwResult (*UwMethodSetFileName)      (UwValuePtr self, UwValuePtr file_name);

// XXX other fd operation: seek, tell, etc.

typedef struct {
    UwMethodOpenFile          _open;
    UwMethodCloseFile         _close;  // only if opened with `open`, don't close one assigned by `set_fd`, right?
    UwMethodSetFileDescriptor _set_fd;
    UwMethodGetFileName       _get_name;
    UwMethodSetFileName       _set_name;

} UwInterface_File;

/****************************************************************
 * FileReader interface
 */

typedef UwResult (*UwMethodReadFile)(UwValuePtr self, void* buffer, unsigned buffer_size, unsigned* bytes_read);

typedef struct {
    UwMethodReadFile _read;

} UwInterface_FileReader;

/****************************************************************
 * FileWriter interface
 */

typedef UwResult (*UwMethodWriteFile)(UwValuePtr self, void* data, unsigned size, unsigned* bytes_written);

// XXX truncate

typedef struct {
    UwMethodWriteFile _write;

} UwInterface_FileWriter;

/****************************************************************
 * Shorthand functions
 */

static inline UwResult uw_create_file()
{
    return _uw_create(UwTypeId_File);
}

#define uw_file_open(file_name, flags, mode) _Generic((file_name), \
             char*: _uw_file_open_u8_wrapper,  \
          char8_t*: _uw_file_open_u8,          \
         char32_t*: _uw_file_open_u32,         \
        UwValuePtr: _uw_file_open              \
    )((file_name), (flags), (mode))

UwResult _uw_file_open(UwValuePtr file_name, int flags, mode_t mode);

static inline UwResult  uw_file_open_cstr(char*     file_name, int flags, mode_t mode) { __UWDECL_CharPtr  (fname, file_name); return _uw_file_open(&fname, flags, mode); }
static inline UwResult _uw_file_open_u8  (char8_t*  file_name, int flags, mode_t mode) { __UWDECL_Char8Ptr (fname, file_name); return _uw_file_open(&fname, flags, mode); }
static inline UwResult _uw_file_open_u32 (char32_t* file_name, int flags, mode_t mode) { __UWDECL_Char32Ptr(fname, file_name); return _uw_file_open(&fname, flags, mode); }

static inline UwResult _uw_file_open_u8_wrapper(char* file_name, int flags, mode_t mode)
{
    return _uw_file_open_u8((char8_t*) file_name, flags, mode);
}

static inline UwResult uw_file_close   (UwValuePtr file)         { return uw_ifcall(file, File, close); }
static inline UwResult uw_file_set_fd  (UwValuePtr file, int fd) { return uw_ifcall(file, File, set_fd, fd); }
static inline UwResult uw_file_get_name(UwValuePtr file)         { return uw_ifcall(file, File, get_name); }
static inline UwResult uw_file_set_name(UwValuePtr file, UwValuePtr file_name)  { return uw_ifcall(file, File, set_name, file_name); }

static inline UwResult uw_file_read(UwValuePtr file, void* buffer, unsigned buffer_size, unsigned* bytes_read)
{
    return uw_ifcall(file, FileReader, read, buffer, buffer_size, bytes_read);
}

static inline UwResult uw_file_write(UwValuePtr file, void* data, unsigned size, unsigned* bytes_written)
{
    return uw_ifcall(file, FileWriter, write, data, size, bytes_written);
}

#ifdef __cplusplus
}
#endif
