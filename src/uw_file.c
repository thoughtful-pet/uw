#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "include/uw.h"
#include "src/uw_charptr_internal.h"
#include "src/uw_file_internal.h"

/****************************************************************
 * Basic interface methods
 */

UwResult _uw_file_init(UwValuePtr self, va_list ap)
{
    struct _UwFile* f = _uw_get_file_ptr(self);
    f->fd = -1;
    f->name = UwNull();
    f->pushback = UwNull();
    return UwOK();
}

void _uw_file_fini(UwValuePtr self)
{
    _uwi_file_close(self);
}

void _uw_file_hash(UwValuePtr self, UwHashContext* ctx)
{
    // it's not a hash of entire file content!

    struct _UwFile* f = _uw_get_file_ptr(self);

    _uw_hash_uint64(ctx, self->type_id);

    // XXX
    _uw_call_hash(&f->name, ctx);
    _uw_hash_uint64(ctx, f->fd);
    _uw_hash_uint64(ctx, f->is_external_fd);
}

UwResult _uw_file_deepcopy(UwValuePtr self)
{
    // XXX duplicate fd?
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

void _uw_file_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    struct _UwFile* f = _uw_get_file_ptr(self);

    _uw_dump_start(fp, self, first_indent);
    _uw_dump_base_extra_data(fp, self->extra_data);

    if (uw_is_string(&f->name)) {
        UW_CSTRING_LOCAL(file_name, &f->name);
        fprintf(fp, " name: %s", file_name);
    } else {
        fprintf(fp, " name: Null");
    }
    fprintf(fp, " fd: %d", f->fd);
    if (f->is_external_fd) {
        fprintf(fp, " (external)");
    }
    fputc('\n', fp);
}

UwResult _uw_file_to_string(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
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

/****************************************************************
 * File interface methods
 */

UwResult _uwi_file_open(UwValuePtr self, UwValuePtr file_name, int flags, mode_t mode)
{
    struct _UwFile* f = _uw_get_file_ptr(self);

    if (f->fd != -1) {
        return UwError(UW_ERROR_FILE_ALREADY_OPENED);
    }

    // open file
    UW_CSTRING_LOCAL(filename_cstr, file_name);
    do {
        f->fd = open(filename_cstr, flags, mode);
    } while (f->fd == -1 && errno == EINTR);

    if (f->fd == -1) {
        f->error = errno;
        return UwErrno(errno);
    }

    // set file name
    uw_destroy(&f->name);
    f->name = uw_clone(file_name);

    f->is_external_fd = false;
    f->line_number = 0;

    uw_destroy(&f->pushback);

    return UwOK();
}

UwResult _uwi_file_close(UwValuePtr self)
{
    struct _UwFile* f = _uw_get_file_ptr(self);

    if (f->fd != -1 && !f->is_external_fd) {
        close(f->fd);
    }
    f->fd = -1;
    f->error = 0;
    uw_destroy(&f->name);

    if (f->buffer) {
        free(f->buffer);
        f->buffer = nullptr;
    }
    uw_destroy(&f->pushback);
    return UwOK();
}

UwResult _uwi_file_set_fd(UwValuePtr self, int fd)
{
    struct _UwFile* f = _uw_get_file_ptr(self);

    if (f->fd != -1) {
        // fd already set
        return UwError(UW_ERROR_FD_ALREADY_SET);
    }
    f->fd = fd;
    f->is_external_fd = true;
    f->line_number = 0;
    uw_destroy(&f->pushback);
    return UwOK();
}

UwResult _uwi_file_get_name(UwValuePtr self)
{
    struct _UwFile* f = _uw_get_file_ptr(self);
    return uw_clone(&f->name);
}

UwResult _uwi_file_set_name(UwValuePtr self, UwValuePtr file_name)
{
    struct _UwFile* f = _uw_get_file_ptr(self);

    if (f->fd != -1 && !f->is_external_fd) {
        // not an externally set fd
        return UwError(UW_ERROR_CANNOT_SET_FILENAME);
    }

    // set file name
    uw_destroy(&f->name);
    f->name = uw_clone(file_name);

    return UwOK();
}

/****************************************************************
 * FileReader interface methods
 */

UwResult _uwi_file_read(UwValuePtr self, void* buffer, unsigned buffer_size, unsigned* bytes_read)
{
    struct _UwFile* f = _uw_get_file_ptr(self);

    ssize_t result;
    do {
        result = read(f->fd, buffer, buffer_size);
    } while (result < 0 && errno == EINTR);

    if (result < 0) {
        return UwErrno(errno);
    } else {
        *bytes_read = (unsigned) result;
        return UwOK();
    }
}

/****************************************************************
 * FileWriter interface methods
 */

UwResult _uwi_file_write(UwValuePtr self, void* data, unsigned size, unsigned* bytes_written)
{
    struct _UwFile* f = _uw_get_file_ptr(self);

    ssize_t result;
    do {
        result = write(f->fd, data, size);
    } while (result < 0 && errno == EINTR);

    if (result < 0) {
        return UwErrno(errno);
    } else {
        *bytes_written = (unsigned) result;
        return UwOK();
    }
}

/****************************************************************
 * LineReader interface methods
 */

UwResult _uwi_file_start_read_lines(UwValuePtr self)
{
    struct _UwFile* f = _uw_get_file_ptr(self);

    uw_destroy(&f->pushback);

    if (f->buffer == nullptr) {
        f->buffer = malloc(LINE_READER_BUFFER_SIZE);
        if (!f->buffer) {
            return UwOOM();
        }
    }
    f->partial_utf8_len = 0;

    // the following settings will force line_reader to read next chunk of data immediately:
    f->position = LINE_READER_BUFFER_SIZE;
    f->data_size = LINE_READER_BUFFER_SIZE;

    // reset file position
    if (lseek(f->fd, 0, SEEK_SET) == -1) {
        return UwErrno(errno);
    }
    f->line_number = 0;
    return UwOK();
}

UwResult _uwi_file_read_line(UwValuePtr self)
{
    UwValue result = UwString();
    if (uw_ok(&result)) {
        UwValue status =_uwi_file_read_line_inplace(self, &result);
        if (uw_error(&status)) {
            return uw_move(&status);
        }
    }
    return uw_move(&result);
}

UwResult _uwi_file_read_line_inplace(UwValuePtr self, UwValuePtr line)
{
    struct _UwFile* f = _uw_get_file_ptr(self);

    uw_string_truncate(line, 0);

    if (f->buffer == nullptr) {
        UwValue status = _uwi_file_start_read_lines(self);
        if (uw_error(&status)) {
            return uw_move(&status);
        }
    }

    if (uw_is_string(&f->pushback)) {
        if (!uw_string_append(line, &f->pushback)) {
            return UwOOM();
        }
        uw_destroy(&f->pushback);
        f->line_number++;
        return UwOK();
    }

    do {
        if (f->position == f->data_size) {

            if (f->data_size < LINE_READER_BUFFER_SIZE) {
                // end of file
                return UwError(UW_ERROR_EOF);
            }
            f->position = 0;
            {
                UwValue status = _uwi_file_read(self, f->buffer, LINE_READER_BUFFER_SIZE, &f->data_size);
                if (uw_error(&status)) {
                    return uw_move(&status);
                }
                if (f->data_size == 0) {
                    // XXX warn if f->partial_utf8_len != 0
                    return UwError(UW_ERROR_EOF);
                }
            }

            if (f->partial_utf8_len) {
                // process partial UTF-8 sequence
                // simply append chars to partial_utf8 until uw_string_append_utf8 succeeds
                while (f->partial_utf8_len < 4) {

                    if (f->position == f->data_size) {
                        // premature end of file
                        // XXX warn?
                        return UwError(UW_ERROR_EOF);
                    }

                    char8_t c = f->buffer[f->position];
                    if (c < 0x80 || ((c & 0xC0) != 0x80)) {
                        // malformed UTF-8 sequence
                        break;
                    }
                    f->position++;
                    f->partial_utf8[f->partial_utf8_len++] = c;
                    unsigned bytes_processed;
                    if (!uw_string_append_utf8(line, f->partial_utf8, f->partial_utf8_len, &bytes_processed)) {
                        return UwOOM();
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
            unsigned end_pos = lf - f->buffer;
            unsigned bytes_processed;

            if (!uw_string_append_utf8(line, start, end_pos - f->position, &bytes_processed)) {
                return UwOOM();
            }
            f->position = end_pos;
            f->line_number++;
            return UwOK();

        } else {
            unsigned n = f->data_size - f->position;
            unsigned bytes_processed;
            if (!uw_string_append_utf8(line, start, n, &bytes_processed)) {
                return UwOOM();
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

UwResult _uwi_file_unread_line(UwValuePtr self, UwValuePtr line)
{
    struct _UwFile* f = _uw_get_file_ptr(self);

    if (uw_is_null(&f->pushback)) {
        f->pushback = uw_clone(line);
        f->line_number--;
        return UwOK();
    } else {
        return UwError(UW_ERROR_PUSHBACK_FAILED);
    }
}

UwResult _uwi_file_get_line_number(UwValuePtr self)
{
    return UwUnsigned(_uw_get_file_ptr(self)->line_number);
}

UwResult _uwi_file_stop_read_lines(UwValuePtr self)
{
    struct _UwFile* f = _uw_get_file_ptr(self);

    free(f->buffer);
    f->buffer = nullptr;
    uw_destroy(&f->pushback);
    return UwOK();
}

/****************************************************************
 * Shorthand functions
 */

UwResult _uw_file_open(UwValuePtr file_name, int flags, mode_t mode)
{
    UwValue normalized_filename = uw_clone(file_name);  // this converts CharPtr to String
    if (uw_error(&normalized_filename)) {
        return uw_move(&normalized_filename);
    }
    UwValue file = _uw_create(UwTypeId_File);
    if (uw_error(&file)) {
        return uw_move(&file);
    }
    UwValue status = uw_ifcall(&file, File, open, &normalized_filename, flags, mode);
    if (uw_error(&status)) {
        return uw_move(&status);
    }
    return uw_move(&file);
}
