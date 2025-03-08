#ifndef _GNU_SOURCE
#   define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "include/uw_base.h"
#include "include/uw_string.h"

struct _UwStatusExtraData {
    _UwExtraData value_data;
    _UwValue description;  // string
};

static char* basic_statuses[] = {
    [UW_SUCCESS]                   = "SUCCESS",
    [UW_STATUS_VA_END]             = "VA_END",
    [UW_ERROR_ERRNO]               = "ERRNO",
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

    num_statuses = _UWC_LENGTH_OF(basic_statuses);
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
        return UW_ERROR_OOM;
    }
    if (num_statuses == statuses_capacity) {
        size_t page_size = sysconf(_SC_PAGE_SIZE);
        size_t old_memsize = (statuses_capacity * sizeof(char*) + page_size - 1) & ~(page_size - 1);
        size_t new_memsize = ((statuses_capacity + 1) * sizeof(char*) + page_size - 1) & ~(page_size - 1);

        char** new_statuses = mremap(statuses, old_memsize, new_memsize, MREMAP_MAYMOVE);
        if (new_statuses == MAP_FAILED) {
            perror("mremap");
            return UW_ERROR_OOM;
        }
        statuses = new_statuses;
        statuses_capacity = new_memsize / sizeof(char*);
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

UwResult uw_status_desc(UwValuePtr status)
{
    if (!status) {
        return UwString_1_12(4, 'n', 'u', 'l', 'l', 0, 0, 0, 0, 0, 0, 0, 0);
    }
    if (!uw_is_status(status)) {
        return UwString_1_12(10, 'b', 'a', 'd', ' ', 's', 't', 'a', 't', 'u', 's', 0, 0);
    }
    struct _UwStatusExtraData* status_data = (struct _UwStatusExtraData*) status->extra_data;
    if (status_data) {
        if (uw_is_string(&status_data->description)) {
            return uw_clone(&status_data->description);
        }
    }
    return UwString_1_12(4, 'n', 'o', 'n', 'e', 0, 0, 0, 0, 0, 0, 0, 0);
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

    struct _UwStatusExtraData* status_data = (struct _UwStatusExtraData*) status->extra_data;
    if (status_data) {
        uw_destroy(&status_data->description);
    } else {
        if (!_uw_mandatory_alloc_extra_data(status)) {
            return;
        }
        status_data = (struct _UwStatusExtraData*) status->extra_data;
    }
    char* desc;
    if (vasprintf(&desc, fmt, ap) == -1) {
        return;
    }
    status_data->description = uw_create_string(desc);
    if (uw_error(&status_data->description)) {
        uw_destroy(&status_data->description);
    }
    free(desc);
}

/****************************************************************
 * Basic interface methods
 */

static UwResult status_create(UwTypeId type_id, va_list ap)
/*
 * Status constructor expects status class, status_code,
 * description format string and its arguments.
 * Status class and code should be integer values.
 * Description can be string or null.
 */
{
    unsigned status_code  = va_arg(ap, unsigned);
    char*    description  = va_arg(ap, char*);

    // using not autocleaned variable here, no uw_move necessary on exit
    __UWDECL_Status(result, status_code);
    if (description) {
        _uw_set_status_desc_ap(&result, description, ap);
    }
    return result;
}

static void status_fini(UwValuePtr self)
{
    struct _UwStatusExtraData* status_data = (struct _UwStatusExtraData*) self->extra_data;
    if (status_data) {
        uw_destroy(&status_data->description);
    }
}

static void status_hash(UwValuePtr self, UwHashContext* ctx)
{
    _uw_hash_uint64(ctx, self->type_id);
    _uw_hash_uint64(ctx, self->status_code);
    if (self->status_code == UW_ERROR_ERRNO) {
        _uw_hash_uint64(ctx, self->uw_errno);
    }
    // XXX do not hash extra data
}

static UwResult status_deepcopy(UwValuePtr self)
{
    UwValue result = *self;
    if (!result.extra_data) {
        return uw_move(&result);
    }
    result.extra_data = nullptr;

    if (!_uw_mandatory_alloc_extra_data(self)) {
        return UwOOM();
    }
    struct _UwStatusExtraData* status_data = (struct _UwStatusExtraData*) self->extra_data;
    uw_destroy(&status_data->description);
    status_data->description = uw_deepcopy(&status_data->description);
    return uw_move(&result);
}

static void status_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    if (self->extra_data) {
        _uw_dump_base_extra_data(fp, self->extra_data);
    }
    if (self->status_code == UW_ERROR_ERRNO) {
        fprintf(fp, "\nerrno %d: %s\n", self->uw_errno, strerror(self->uw_errno));
    } else {
        UwValue desc = uw_status_desc(self);
        UW_CSTRING_LOCAL(cdesc, &desc);
        fprintf(fp, "\n%s (%u): %s\n", uw_status_str(self->status_code), self->status_code, cdesc);
        if (self->extra_data) {
            _uw_dump_base_extra_data(fp, self->extra_data);
        }
    }
}

static UwResult status_to_string(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

static bool status_is_true(UwValuePtr self)
{
    // XXX ???
    return false;
}

static bool status_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    if (self->status_code == UW_ERROR_ERRNO) {
        return other->status_code == UW_ERROR_ERRNO && self->uw_errno == other->uw_errno;
    }
    else {
        return self->status_code == other->status_code;
    }
}

static bool status_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        if (t == UwTypeId_Status) {
            return status_equal_sametype(self, other);
        } else {
            // check base type
            t = _uw_types[t]->ancestor_id;
            if (t == UwTypeId_Null) {
                return false;
            }
        }
    }
}

UwType _uw_status_type = {
    .id              = UwTypeId_Status,
    .ancestor_id     = UwTypeId_Null,  // no ancestor
    .compound        = false,
    .data_optional   = true,
    .name            = "Status",
    .data_offset     = offsetof(struct _UwStatusExtraData, description),  // extra_data is optional and not allocated by default
    .data_size       = sizeof(_UwValue),
    .allocator       = &default_allocator,
    ._create         = status_create,
    ._destroy        = _uw_default_destroy,
    ._init           = nullptr,
    ._fini           = status_fini,
    ._clone          = _uw_default_clone,
    ._hash           = status_hash,
    ._deepcopy       = status_deepcopy,
    ._dump           = status_dump,
    ._to_string      = status_to_string,
    ._is_true        = status_is_true,
    ._equal_sametype = status_equal_sametype,
    ._equal          = status_equal,

    .num_interfaces  = 0,
    .interfaces      = nullptr
};
