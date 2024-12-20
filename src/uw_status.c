#define _GNU_SOURCE

#include <string.h>

#include "include/uw_base.h"
#include "src/uw_status_internal.h"


/****************************************************************
 * Helper functions.
 */

struct Statuses {
    uint16_t start_code;
    uint16_t num_statuses;
    char** statuses;
};

static char* uw_status_str(uint16_t status_code)
{
    static char* basic_statuses[] = {
        "SUCCESS",
        "VA_END",
        "OOM",
        "NOT IMPLEMENTED",
        "INCOMPATIBLE_TYPE",
        "NO_INTERFACE",
        "EOF",
        "GONE"
    };
    static char* list_statuses[] = {
        "POP_FROM_EMPTY_LIST"
    };
    static char* map_statuses[] = {
        "KEY_NOT_FOUND"
    };
    static char* file_statuses[] = {
        "FILE_ALREADY_OPENED",
        "CANNOT_SET_FILENAME",
        "FD_ALREADY_SET"
    };
    static char* stringio_statuses[] = {
        "PUSHBACK_FAILED"
    };
    static struct Statuses statuses[] = {
        {
            .start_code = 0,
            .num_statuses = _UWC_LENGTH_OF(basic_statuses),
            .statuses = basic_statuses,
        },
        {
            .start_code = 100,
            .num_statuses = _UWC_LENGTH_OF(list_statuses),
            .statuses = list_statuses,
        },
        {
            .start_code = 200,
            .num_statuses = _UWC_LENGTH_OF(map_statuses),
            .statuses = map_statuses,
        },
        {
            .start_code = 300,
            .num_statuses = _UWC_LENGTH_OF(file_statuses),
            .statuses = file_statuses,
        },
        {
            .start_code = 400,
            .num_statuses = _UWC_LENGTH_OF(stringio_statuses),
            .statuses = stringio_statuses,
        }
    };
    static char unknown[] = "(unknown)";

    for (size_t i = 0;  i < _UWC_LENGTH_OF(statuses); i++) {
        struct Statuses* s = &statuses[i];
        if (status_code >= s->start_code && status_code < s->start_code + s->num_statuses) {
            return s->statuses[status_code - s->start_code];
        }
    }
    return unknown;
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
