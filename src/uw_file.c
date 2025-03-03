#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "include/uw.h"
#include "src/uw_charptr_internal.h"

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

#define LINE_READER_BUFFER_SIZE  4096  // typical filesystem block size

// forward declarations
static UwResult file_close(UwValuePtr self);
static UwResult read_line_inplace(UwValuePtr self, UwValuePtr line);

/****************************************************************
 * Basic interface methods
 */

static UwResult file_init(UwValuePtr self, va_list ap)
{
    struct _UwFile* f = _uw_get_file_ptr(self);
    f->fd = -1;
    f->name = UwNull();
    f->pushback = UwNull();
    return UwOK();
}

static void file_fini(UwValuePtr self)
{
    file_close(self);
}

static void file_hash(UwValuePtr self, UwHashContext* ctx)
{
    // it's not a hash of entire file content!

    struct _UwFile* f = _uw_get_file_ptr(self);

    _uw_hash_uint64(ctx, self->type_id);

    // XXX
    _uw_call_hash(&f->name, ctx);
    _uw_hash_uint64(ctx, f->fd);
    _uw_hash_uint64(ctx, f->is_external_fd);
}

static UwResult file_deepcopy(UwValuePtr self)
{
    // XXX duplicate fd?
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

static void file_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
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

static UwResult file_to_string(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

static bool file_is_true(UwValuePtr self)
{
    // XXX
    return false;
}

static bool file_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    // XXX
    return false;
}

static bool file_equal(UwValuePtr self, UwValuePtr other)
{
    // XXX
    return false;
}

/****************************************************************
 * File interface methods
 */

static UwResult file_open(UwValuePtr self, UwValuePtr file_name, int flags, mode_t mode)
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

static UwResult file_close(UwValuePtr self)
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

static UwResult file_set_fd(UwValuePtr self, int fd)
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

static UwResult file_get_name(UwValuePtr self)
{
    struct _UwFile* f = _uw_get_file_ptr(self);
    return uw_clone(&f->name);
}

static UwResult file_set_name(UwValuePtr self, UwValuePtr file_name)
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

static UwResult file_read(UwValuePtr self, void* buffer, unsigned buffer_size, unsigned* bytes_read)
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

static UwResult file_write(UwValuePtr self, void* data, unsigned size, unsigned* bytes_written)
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

static UwResult start_read_lines(UwValuePtr self)
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

static UwResult read_line(UwValuePtr self)
{
    UwValue result = UwString();
    if (uw_ok(&result)) {
        UwValue status = read_line_inplace(self, &result);
        if (uw_error(&status)) {
            return uw_move(&status);
        }
    }
    return uw_move(&result);
}

static UwResult read_line_inplace(UwValuePtr self, UwValuePtr line)
{
    struct _UwFile* f = _uw_get_file_ptr(self);

    uw_string_truncate(line, 0);

    if (f->buffer == nullptr) {
        UwValue status = start_read_lines(self);
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
                UwValue status = file_read(self, f->buffer, LINE_READER_BUFFER_SIZE, &f->data_size);
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

static UwResult unread_line(UwValuePtr self, UwValuePtr line)
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

static UwResult get_line_number(UwValuePtr self)
{
    return UwUnsigned(_uw_get_file_ptr(self)->line_number);
}

static UwResult stop_read_lines(UwValuePtr self)
{
    struct _UwFile* f = _uw_get_file_ptr(self);

    free(f->buffer);
    f->buffer = nullptr;
    uw_destroy(&f->pushback);
    return UwOK();
}

/****************************************************************
 * File type and interfaces
 */

UwTypeId UwTypeId_File = 0;

static UwInterface_File file_interface = {
    ._open     = file_open,
    ._close    = file_close,
    ._set_fd   = file_set_fd,
    ._get_name = file_get_name,
    ._set_name = file_set_name
};

static UwInterface_FileReader file_reader_interface = {
    ._read = file_read
};

static UwInterface_FileWriter file_writer_interface = {
    ._write = file_write
};

static UwInterface_LineReader line_reader_interface = {
    ._start             = start_read_lines,
    ._read_line         = read_line,
    ._read_line_inplace = read_line_inplace,
    ._get_line_number   = get_line_number,
    ._unread_line       = unread_line,
    ._stop              = stop_read_lines
};

static _UwInterface file_interfaces[4] = {
    // {UwInterfaceId_File,       &file_interface},
    // {UwInterfaceId_FileReader, &file_reader_interface},
    // {UwInterfaceId_FileWriter, &file_writer_interface},
    // {UwInterfaceId_LineReader, &line_reader_interface}
};

static UwType file_type = {
    .id              = 0,
    .ancestor_id     = UwTypeId_Null,  // no ancestor
    .compound        = false,
    .data_optional   = false,
    .name            = "File",
    .data_offset     = offsetof(struct _UwFileExtraData, file_data),
    .data_size       = sizeof(struct _UwFile),
    .allocator       = &default_allocator,
    ._create         = _uw_default_create,
    ._destroy        = _uw_default_destroy,
    ._init           = file_init,
    ._fini           = file_fini,
    ._clone          = _uw_default_clone,
    ._hash           = file_hash,
    ._deepcopy       = file_deepcopy,
    ._dump           = file_dump,
    ._to_string      = file_to_string,
    ._is_true        = file_is_true,
    ._equal_sametype = file_equal_sametype,
    ._equal          = file_equal,

    .num_interfaces  = _UWC_LENGTH_OF(file_interfaces),
    .interfaces      = file_interfaces
};

[[ gnu::constructor ]]
static void init_file_type()
{
    if (UwInterfaceId_File == 0)       { UwInterfaceId_File       = uw_register_interface(); }
    if (UwInterfaceId_FileReader == 0) { UwInterfaceId_FileReader = uw_register_interface(); }
    if (UwInterfaceId_FileWriter == 0) { UwInterfaceId_FileWriter = uw_register_interface(); }
    if (UwInterfaceId_LineReader == 0) { UwInterfaceId_LineReader = uw_register_interface(); }

    file_interfaces[0].interface_id = UwInterfaceId_File;
    file_interfaces[0].interface_methods = &file_interface;

    file_interfaces[1].interface_id = UwInterfaceId_FileReader;
    file_interfaces[1].interface_methods = &file_reader_interface;

    file_interfaces[2].interface_id = UwInterfaceId_FileWriter;
    file_interfaces[2].interface_methods = &file_writer_interface;

    file_interfaces[3].interface_id = UwInterfaceId_LineReader;
    file_interfaces[3].interface_methods = &line_reader_interface;

    UwTypeId_File = uw_add_type(&file_type);
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
