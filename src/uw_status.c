#ifndef _GNU_SOURCE
#   define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "include/uw_base.h"
#include "src/uw_status_internal.h"

static char* basic_statuses[] = {
    [UW_SUCCESS]                   = "SUCCESS",
    [UW_STATUS_VA_END]             = "VA_END",
    [UW_ERROR_OOM]                 = "OOM",
    [UW_ERROR_NOT_IMPLEMENTED]     = "NOT IMPLEMENTED",
    [UW_ERROR_INCOMPATIBLE_TYPE]   = "INCOMPATIBLE_TYPE",
    [UW_ERROR_NO_INTERFACE]        = "NO_INTERFACE",
    [UW_ERROR_EOF]                 = "EOF",
    [UW_ERROR_POP_FROM_EMPTY_LIST] = "POP_FROM_EMPTY_LIST",
    [UW_ERROR_KEY_NOT_FOUND]       = "KEY_NOT_FOUND",
    [UW_ERROR_FILE_ALREADY_OPENED] = "FILE_ALREADY_OPENED",
    [UW_ERROR_CANNOT_SET_FILENAME] = "CANNOT_SET_FILENAME",
    [UW_ERROR_FD_ALREADY_SET]      = "FD_ALREADY_SET",
    [UW_ERROR_PUSHBACK_FAILED]     = "PUSHBACK_FAILED"
};

static char** statuses = nullptr;
static size_t statuses_capacity = 0;
static uint16_t num_statuses = 0;

[[ gnu::constructor ]]
static void init_statuses()
{
    if (statuses) {
        return;
    }

    size_t page_size = sysconf(_SC_PAGE_SIZE);
    size_t memsize = (sizeof(basic_statuses) + page_size - 1) & ~(page_size - 1);
    statuses_capacity = memsize / sizeof(char*);

    statuses = mmap(NULL, memsize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (statuses == MAP_FAILED) {
        perror("mmap");
        abort();
    }

    num_statuses = sizeof(basic_statuses) / sizeof(basic_statuses[0]);
    for(uint16_t i = 0; i < num_statuses; i++) {
        char* status = basic_statuses[i];
        if (!status) {
            fprintf(stderr, "Status %u is not defined\n", i);
            abort();
        }
        statuses[i] = status;
    }
}

uint16_t uw_define_status(char* status)
{
    // the order constructor are called is undefined, make sure statuses are initialized
    init_statuses();

    if (num_statuses == 65535) {
        fprintf(stderr, "Cannot define more statuses than %u\n", num_statuses);
        abort();
    }
    if (num_statuses == statuses_capacity) {
        size_t page_size = sysconf(_SC_PAGE_SIZE);
        size_t old_memsize = (statuses_capacity * sizeof(char*) + page_size - 1) & ~(page_size - 1);
        size_t new_memsize = ((statuses_capacity + 1) * sizeof(char*) + page_size - 1) & ~(page_size - 1);
        statuses_capacity = new_memsize / sizeof(char*);

        statuses = mremap(statuses, old_memsize, new_memsize, MREMAP_MAYMOVE);
        if (statuses == MAP_FAILED) {
            perror("mremap");
            abort();
        }
    }
    uint16_t status_code = num_statuses++;
    statuses[status_code] = status;
    return status_code;
}

char* uw_status_str(uint16_t status_code)
{
    if (status_code < num_statuses) {
        return statuses[status_code];
    } else {
        static char unknown[] = "(unknown)";
        return unknown;
    }
}

char* uw_status_desc(UwValuePtr status)
{
    static char* no_desc = "no description";
    static char* null_str = "null";
    static char* bad_status = "bad status";

    if (!status) {
        return null_str;
    }
    if (!uw_is_status(status)) {
        return bad_status;
    }
    struct _UwStatusExtraData* extra_data = (struct _UwStatusExtraData*) status->extra_data;
    if (extra_data) {
        if (extra_data->status_desc) {
            return extra_data->status_desc;
        }
    }
    return no_desc;
}

void _uw_set_status_desc(UwValuePtr status, char* fmt, ...)
{
    va_list ap;
    va_start(ap);
    _uw_set_status_desc_ap(status, fmt, ap);
    va_end(ap);
}

void _uw_set_status_desc_ap(UwValuePtr status, char* fmt, va_list ap)
{
    uw_assert_status(status);

    struct _UwStatusExtraData* extra_data = (struct _UwStatusExtraData*) status->extra_data;
    if (!extra_data) {
        if (!_uw_mandatory_alloc_extra_data(status)) {
            status->status_class = UWSC_DEFAULT;
            status->status_code  = UW_ERROR_OOM;
            return;
        }
        extra_data = (struct _UwStatusExtraData*) status->extra_data;
    }
    if (vasprintf(&extra_data->status_desc, fmt, ap) == -1) {
        extra_data->status_desc = nullptr;
        status->status_class = UWSC_DEFAULT;
        status->status_code  = UW_ERROR_OOM;
    }
}

/****************************************************************
 * Basic interface methods
 */

UwResult _uw_status_create(UwTypeId type_id, va_list ap)
/*
 * Status constructor expects status class, status_code,
 * description format string and its arguments.
 * Status class and code should be integer values.
 * Description can be string or null.
 */
{
    unsigned status_class = va_arg(ap, unsigned);
    unsigned status_code  = va_arg(ap, unsigned);
    char*    description  = va_arg(ap, char*);

    // using not autocleaned variable here, no uw_move necessary on exit
    __UWDECL_Status(result, status_class, status_code);
    if (description) {
        _uw_set_status_desc_ap(&result, description, ap);
    }
    return result;
}

void _uw_status_fini(UwValuePtr self)
{
    struct _UwStatusExtraData* extra_data = (struct _UwStatusExtraData*) self->extra_data;
    if (extra_data) {
        if (extra_data->status_desc) {
            free(extra_data->status_desc);
            extra_data->status_desc = nullptr;
        }
    }
}

void _uw_status_hash(UwValuePtr self, UwHashContext* ctx)
{
    _uw_hash_uint64(ctx, self->type_id);
    _uw_hash_uint64(ctx, self->status_class);
    switch (self->status_class) {
        case UWSC_DEFAULT: _uw_hash_uint64(ctx, self->status_code); break;
        case UWSC_ERRNO:   _uw_hash_uint64(ctx, self->uw_errno); break;
        default: break;
    }
    // XXX do not hash extra data
}

UwResult _uw_status_deepcopy(UwValuePtr self)
{
    UwValue result = *self;
    if (self->status_class == UWSC_ERRNO) {
        return uw_move(&result);
    }
    uw_assert(self->status_class == UWSC_DEFAULT);
    if (!result.extra_data) {
        return uw_move(&result);
    }
    result.extra_data = nullptr;

    // copy description

    if (_uw_alloc_extra_data(&result)) {

        struct _UwStatusExtraData* self_extra_data   = (struct _UwStatusExtraData*) self->extra_data;
        struct _UwStatusExtraData* result_extra_data = (struct _UwStatusExtraData*) result.extra_data;

        result_extra_data->status_desc = strdup(self_extra_data->status_desc);
    }
    return uw_move(&result);
}

void _uw_status_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    switch (self->status_class) {
        case UWSC_DEFAULT:
            if (self->extra_data) {
                _uw_dump_base_extra_data(fp, self->extra_data);
            }
            fprintf(fp, "\n%s (%u): %s\n", uw_status_str(self->status_code), self->status_code, uw_status_desc(self));
            break;

        case UWSC_ERRNO:
            fprintf(fp, "\nerrno %d: %s\n", self->uw_errno, strerror(self->uw_errno));
            break;

        default:
            if (self->extra_data) {
                _uw_dump_base_extra_data(fp, self->extra_data);
            }
            fprintf(fp, "\nstatus class=%u (unknown); status code=%u (unknown)\n", self->status_class, self->status_code);
            break;
    }
}

UwResult _uw_status_to_string(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

bool _uw_status_is_true(UwValuePtr self)
{
    // XXX ???
    return false;
}

bool _uw_status_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    switch (self->status_class) {
        case UWSC_DEFAULT: return self->status_code == other->status_code;
        case UWSC_ERRNO:   return self->uw_errno == other->uw_errno;
        default: return false;
    }
}

bool _uw_status_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        if (t == UwTypeId_Status) {
            return _uw_status_equal_sametype(self, other);
        } else {
            // check base type
            t = _uw_types[t]->ancestor_id;
            if (t == UwTypeId_Null) {
                return false;
            }
        }
    }
}
