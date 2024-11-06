#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "include/uw_c.h"
#include "src/uw_file_internal.h"

/****************************************************************
 * Basic interface methods
 */

bool _uw_init_file(UwValuePtr self)
{
    struct _UwFile* f = _uw_get_data_ptr(self, UwTypeId_File, struct _UwFile*);
    f->fd = -1;
    f->is_external_fd = false;
    f->error = 0;
    f->name = uw_create_null();
    if (!f->name) {
        return false;
    }

    // init line reader
    f->buffer = nullptr;
    f->pushback = nullptr;

    return true;
}

void _uw_fini_file(UwValuePtr self)
{
    _uw_file_close(self);
}

void _uw_hash_file(UwValuePtr self, UwHashContext* ctx)
{
    // it's not a hash of entire file content!

    struct _UwFile* f = _uw_get_data_ptr(self, UwTypeId_File, struct _UwFile*);

    _uw_hash_uint64(ctx, self->type_id);

    // XXX
    _uw_call_hash(f->name, ctx);
    _uw_hash_uint64(ctx, f->fd);
    _uw_hash_uint64(ctx, f->is_external_fd);
}

UwValuePtr _uw_copy_file(UwValuePtr self)
{
    // XXX duplicate fd?
    return nullptr;
}

void _uw_dump_file(UwValuePtr self, int indent)
{
    struct _UwFile* f = _uw_get_data_ptr(self, UwTypeId_File, struct _UwFile*);

    _uw_dump_start(self, indent);

    if (uw_is_string(f->name)) {
        UW_CSTRING_LOCAL(file_name, f->name);
        printf("name: %s ", file_name);
    } else {
        printf("name: Null ");
    }
    printf("fd: %d", f->fd);
    if (f->is_external_fd) {
        printf(" (external)");
    }
    putchar('\n');
}

UwValuePtr _uw_file_to_string(UwValuePtr self)
{
    // XXX not implemented yet
    return nullptr;
}

bool _uw_file_is_true(UwValuePtr self)
{
    // XXX
    return false;
}

bool _uw_file_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    // XXX
    return false;
}

bool _uw_file_equal(UwValuePtr self, UwValuePtr other)
{
    // XXX
    return false;
}

bool _uw_file_equal_ctype(UwValuePtr self, UwCType ctype, ...)
{
    // XXX
    return false;
}

/****************************************************************
 * File interface methods
 */

bool _uw_file_open(UwValuePtr self, char8_t* name, int flags, mode_t mode)
{
    struct _UwFile* f = _uw_get_data_ptr(self, UwTypeId_File, struct _UwFile*);

    if (f->fd != -1) {
        // already opened
        // XXX how to indicate this kind of error?
        return false;
    }

    // prepare filename
    UwValuePtr file_name = uw_create(name);
    if (!file_name) {
        return false;
    }

    // open file
    do {
        f->fd = open((char*) name, flags, mode);
    } while (f->fd == -1 && errno == EINTR);

    if (f->fd == -1) {
        f->error = errno;
        uw_delete(&file_name);
        return false;
    }

    // set file name
    uw_delete(&f->name);
    f->name = file_name;
    file_name = nullptr;

    f->is_external_fd = false;

    return true;
}

void _uw_file_close(UwValuePtr self)
{
    struct _UwFile* f = _uw_get_data_ptr(self, UwTypeId_File, struct _UwFile*);

    if (f->fd != -1 && !f->is_external_fd) {
        close(f->fd);
    }
    f->fd = -1;
    f->error = 0;
    uw_delete(&f->name);

    if (f->buffer) {
        free(f->buffer);
        f->buffer = nullptr;
    }
    uw_delete(&f->pushback);
}

bool _uw_file_set_fd(UwValuePtr self, int fd)
{
    struct _UwFile* f = _uw_get_data_ptr(self, UwTypeId_File, struct _UwFile*);

    if (f->fd != -1) {
        // fd already set
        return false;
    }
    f->fd = fd;
    f->is_external_fd = true;
    return true;
}

bool _uw_file_set_name(UwValuePtr self, char8_t* name)
{
    struct _UwFile* f = _uw_get_data_ptr(self, UwTypeId_File, struct _UwFile*);

    if (f->fd != -1 && !f->is_external_fd) {
        // not an externally set fd
        return false;
    }

    // prepare filename
    UwValuePtr file_name = uw_create(name);
    if (!file_name) {
        return false;
    }

    // set file name
    uw_delete(&f->name);
    f->name = file_name;
    file_name = nullptr;

    return true;
}

/****************************************************************
 * FileReader interface methods
 */

ssize_t _uw_file_read(UwValuePtr self, void* buffer, size_t buffer_size)
{
    struct _UwFile* f = _uw_get_data_ptr(self, UwTypeId_File, struct _UwFile*);

    ssize_t result;
    do {
        result = read(f->fd, buffer, buffer_size);
    } while (result < 0 && errno == EINTR);

    return result;
}

/****************************************************************
 * FileWriter interface methods
 */

ssize_t _uw_file_write(UwValuePtr self, void* data, size_t size)
{
    struct _UwFile* f = _uw_get_data_ptr(self, UwTypeId_File, struct _UwFile*);

    ssize_t result;
    do {
        result = write(f->fd, data, size);
    } while (result < 0 && errno == EINTR);

    return result;
}

/****************************************************************
 * LineReader interface methods
 */

bool _uw_file_start_read_lines(UwValuePtr self)
{
    struct _UwFile* f = _uw_get_data_ptr(self, UwTypeId_File, struct _UwFile*);

    uw_delete(&f->pushback);

    if (f->buffer == nullptr) {
        f->buffer = malloc(LINE_READER_BUFFER_SIZE);
        if (!f->buffer) {
            return false;
        }
    }
    f->partial_utf8_len = 0;

    // the following settings will force line_reader to read next chunk of data immediately:
    f->position = LINE_READER_BUFFER_SIZE;
    f->data_size = LINE_READER_BUFFER_SIZE;

    // reset file position
    lseek(f->fd, 0, SEEK_SET);

    return true;
}

UwValuePtr _uw_file_read_line(UwValuePtr self)
{
    UwValue result = uw_create("");
    if (!result) {
        return nullptr;
    }
    if (!_uw_file_read_line_inplace(self, result)) {
        return nullptr;
    }
    return uw_move(&result);
}

bool _uw_file_read_line_inplace(UwValuePtr self, UwValuePtr line)
{
    struct _UwFile* f = _uw_get_data_ptr(self, UwTypeId_File, struct _UwFile*);

    uw_string_truncate(line, 0);

    if (f->buffer == nullptr) {
        if (!_uw_file_start_read_lines(self)) {
            return false;
        }
    }

    if (f->pushback) {
        if (!uw_string_append(line, f->pushback)) {
            return false;
        }
        uw_delete(&f->pushback);
        return true;
    }

    if (f->buffer == nullptr) {
        return false;
    }

    do {
        if (f->position == f->data_size) {

            if (f->data_size < LINE_READER_BUFFER_SIZE) {
                // end of file
                return uw_strlen(line) != 0;
            }
            f->data_size = _uw_file_read(self, f->buffer, LINE_READER_BUFFER_SIZE);
            f->position = 0;
            if (f->data_size <= 0) {
                // end of file or error
                return uw_strlen(line) != 0;
            }

            if (f->partial_utf8_len) {
                // process partial UTF-8 sequence
                // simply append chars to partial_utf8 until uw_string_append_utf8 succeeds
                while (f->partial_utf8_len < 4) {

                    if (f->position == f->data_size) {
                        // premature end of file
                        return true;
                    }

                    char8_t c = f->buffer[f->position];
                    if (c < 0x80 || ((c & 0xC0) != 0x80)) {
                        // malformed UTF-8 sequence
                        break;
                    }
                    f->position++;
                    f->partial_utf8[f->partial_utf8_len++] = c;
                    size_t bytes_processed;
                    if (!uw_string_append_utf8(line, f->partial_utf8, f->partial_utf8_len, &bytes_processed)) {
                        // out of memory
                        return false;
                    }
                    if (bytes_processed) {
                        break;
                    }
                }
                f->partial_utf8_len = 0;
            }
        }

        char8_t* start = f->buffer + f->position;
        char8_t* lf = (char8_t*) strchr((char*) start, '\n');
        if (lf) {
            // found newline, don't care about partial UTF-8
            lf++;
            size_t end_pos = lf - f->buffer;
            size_t bytes_processed;

            if (!uw_string_append_utf8(line, start, end_pos - f->position, &bytes_processed)) {
                // out of memory
                return false;
            }
            f->position = end_pos;
            return true;

        } else {
            size_t n = f->data_size - f->position;
            size_t bytes_processed;
            if (!uw_string_append_utf8(line, start, n, &bytes_processed)) {
                // out of memory
                return false;
            }

            // move unprocessed data to partial_utf8
            char8_t* end = start + bytes_processed;
            while (bytes_processed < n) {
                f->partial_utf8[f->partial_utf8_len++] = *end++;
                bytes_processed++;
            }
            f->position = f->data_size;
        }
    } while(true);
}

bool _uw_file_unread_line(UwValuePtr self, UwValuePtr line)
{
    struct _UwFile* f = _uw_get_data_ptr(self, UwTypeId_File, struct _UwFile*);

    if (f->pushback) {
        return false;
    }
    f->pushback = uw_makeref(line);
    return true;
}

void _uw_file_stop_read_lines(UwValuePtr self)
{
    struct _UwFile* f = _uw_get_data_ptr(self, UwTypeId_File, struct _UwFile*);

    free(f->buffer);
    f->buffer = nullptr;
    uw_delete(&f->pushback);
}

/****************************************************************
 * Shorthand functions
 */

UwValuePtr uw_file_open(char8_t* name, int flags, mode_t mode)
{
    UwValuePtr file = uw_create_file();
    if (!file) {
        return nullptr;
    }
    UwInterface_File* iface = uw_get_interface(file, File);
    if (iface) {
        if (iface->open(file, name, flags, mode)) {
            return file;
        }
    }
    uw_delete(&file);
    return nullptr;
}

void uw_file_close(UwValuePtr file)
{
    UwInterface_File* iface = uw_get_interface(file, File);
    if (iface) {
        iface->close(file);
    }
}

bool uw_file_set_fd(UwValuePtr file, int fd)
{
    UwInterface_File* iface = uw_get_interface(file, File);
    if (!iface) {
        return false;
    }
    return iface->set_fd(file, fd);
}

bool uw_file_set_name(UwValuePtr file, char8_t* name)
{
    UwInterface_File* iface = uw_get_interface(file, File);
    if (!iface) {
        return false;
    }
    return iface->set_name(file, name);
}

ssize_t uw_file_read(UwValuePtr file, void* buffer, size_t buffer_size)
{
    UwInterface_FileReader* iface = uw_get_interface(file, FileReader);
    if (!iface) {
        return -1;
    }
    return iface->read(file, buffer, buffer_size);
}

ssize_t uw_file_write(UwValuePtr file, void* data, size_t size)
{
    UwInterface_FileWriter* iface = uw_get_interface(file, FileWriter);
    if (!iface) {
        return -1;
    }
    return iface->write(file, data, size);
}
