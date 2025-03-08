#include "include/uw.h"
#include "src/uw_charptr_internal.h"
#include "src/uw_list_internal.h"

#define get_data_ptr(value)  ((_UwList*) _uw_get_data_ptr((value), UwTypeId_List))

[[noreturn]]
static void panic_status()
{
    uw_panic("List cannot contain Status values");
}

static UwResult list_init(UwValuePtr self, va_list ap)
{
    _UwCompoundData* cdata = (_UwCompoundData*) self->extra_data;
    _UwList*  list = get_data_ptr(self);

    UwTypeId type_id = self->type_id;
    _uw_init_compound_data(cdata);

    // allocate list and append items

    if (_uw_alloc_list(type_id, list, UWLIST_INITIAL_CAPACITY)) {
        UwValue status = uw_list_append_ap(self, ap);
        if (uw_error(&status)) {
            _uw_destroy_list(type_id, list, cdata);
        }
        return uw_move(&status);
    }
    return UwOOM();
}

static void list_fini(UwValuePtr self)
{
    _UwCompoundData* cdata = (_UwCompoundData*) self->extra_data;

    _uw_destroy_list(self->type_id, get_data_ptr(self), cdata);
    _uw_fini_compound_data(cdata);
}

static void list_hash(UwValuePtr self, UwHashContext* ctx)
{
    _uw_hash_uint64(ctx, self->type_id);
    _UwList* list = get_data_ptr(self);
    UwValuePtr item_ptr = list->items;
    for (unsigned n = list->length; n; n--, item_ptr++) {
        _uw_call_hash(item_ptr, ctx);
    }
}

static UwResult list_deepcopy(UwValuePtr self)
{
    UwValue dest = _uw_create(self->type_id, UwVaEnd());
    if (uw_error(&dest)) {
        return uw_move(&dest);
    }

    _UwList* src_list = get_data_ptr(self);
    _UwList* dest_list = get_data_ptr(&dest);

    if (!_uw_list_resize(dest.type_id, dest_list, src_list->length)) {
        return UwOOM();
    }

    UwValuePtr src_item_ptr = src_list->items;
    UwValuePtr dest_item_ptr = dest_list->items;
    for (unsigned i = 0; i < src_list->length; i++) {
        *dest_item_ptr = uw_deepcopy(src_item_ptr);
        if (uw_error(dest_item_ptr)) {
            return uw_move(dest_item_ptr);
        }
        src_item_ptr++;
        dest_item_ptr++;
        dest_list->length++;
    }
    return uw_move(&dest);
}

static void list_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    _uw_dump_base_extra_data(fp, self->extra_data);
    _uw_dump_compound_data(fp, (_UwCompoundData*) self->extra_data, next_indent);
    _uw_print_indent(fp, next_indent);

    UwValuePtr value_seen = _uw_on_chain(self, tail);
    if (value_seen) {
        fprintf(fp, "already dumped: %p, extra data %p\n", value_seen, value_seen->extra_data);
        return;
    }

    _UwCompoundChain this_link = {
        .prev = tail,
        .value = self
    };

    _UwList* list = get_data_ptr(self);
    fprintf(fp, "%u items, capacity=%u\n", list->length, list->capacity);

    next_indent += 4;
    UwValuePtr item_ptr = list->items;
    for (unsigned n = list->length; n; n--, item_ptr++) {
        _uw_call_dump(fp, item_ptr, next_indent, next_indent, &this_link);
    }
}

static UwResult list_to_string(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

static bool list_is_true(UwValuePtr self)
{
    return get_data_ptr(self)->length;
}

static bool list_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return _uw_list_eq(get_data_ptr(self), get_data_ptr(other));
}

static bool list_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        if (t == UwTypeId_List) {
            return _uw_list_eq(get_data_ptr(self), get_data_ptr(other));
        } else {
            // check base type
            t = _uw_types[t]->ancestor_id;
            if (t == UwTypeId_Null) {
                return false;
            }
        }
    }
}

UwType _uw_list_type = {
    .id              = UwTypeId_List,
    .ancestor_id     = UwTypeId_Null,  // no ancestor
    .name            = "List",
    .allocator       = &default_allocator,
    .data_offset     = sizeof(_UwCompoundData),
    .data_size       = sizeof(_UwList),
    .compound        = true,
    ._create         = _uw_default_create,
    ._destroy        = _uw_default_destroy,
    ._init           = list_init,
    ._fini           = list_fini,
    ._clone          = _uw_default_clone,
    ._hash           = list_hash,
    ._deepcopy       = list_deepcopy,
    ._dump           = list_dump,
    ._to_string      = list_to_string,
    ._is_true        = list_is_true,
    ._equal_sametype = list_equal_sametype,
    ._equal          = list_equal,

    .num_interfaces  = 0,
    .interfaces      = nullptr
        // [UwInterfaceId_RandomAccess] = &list_type_random_access_interface,
        // [UwInterfaceId_List]         = &list_type_list_interface
};

static unsigned round_capacity(unsigned capacity)
{
    if (capacity <= UWLIST_CAPACITY_INCREMENT) {
        return align_unsigned(capacity, UWLIST_INITIAL_CAPACITY);
    } else {
        return align_unsigned(capacity, UWLIST_CAPACITY_INCREMENT);
    }
}

bool _uw_alloc_list(UwTypeId type_id, _UwList* list, unsigned capacity)
{
    if (capacity >= (UINT_MAX - UWLIST_CAPACITY_INCREMENT)) {
        return false;
    }

    list->length = 0;
    list->capacity = round_capacity(capacity);

    unsigned memsize = list->capacity * sizeof(_UwValue);
    list->items = _uw_types[type_id]->allocator->allocate(memsize, true);

    return list->items != nullptr;
}

void _uw_destroy_list(UwTypeId type_id, _UwList* list, _UwCompoundData* parent_cdata)
{
    if (list->items) {
        UwValuePtr item_ptr = list->items;
        for (unsigned n = list->length; n; n--, item_ptr++) {
            if (_uw_types[item_ptr->type_id]->compound) {
                _UwCompoundData* child_cdata = (_UwCompoundData*) item_ptr->extra_data;
                _uw_abandon(parent_cdata, child_cdata);
            }
            uw_destroy(item_ptr);
        }
        unsigned memsize = list->capacity * sizeof(_UwValue);
        _uw_types[type_id]->allocator->release((void**) &list->items, memsize);
    }
}

bool _uw_list_resize(UwTypeId type_id, _UwList* list, unsigned desired_capacity)
{
    if (desired_capacity < list->length) {
        desired_capacity = list->length;
    } else if (desired_capacity >= (UINT_MAX - UWLIST_CAPACITY_INCREMENT)) {
        return false;
    }
    unsigned new_capacity = round_capacity(desired_capacity);

    Allocator* allocator = _uw_types[type_id]->allocator;

    unsigned old_memsize = list->capacity * sizeof(_UwValue);
    unsigned new_memsize = new_capacity * sizeof(_UwValue);

    if (!allocator->reallocate((void**) &list->items, old_memsize, new_memsize, true, nullptr)) {
        return false;
    }
    list->capacity = new_capacity;
    return true;
}

bool uw_list_resize(UwValuePtr list, unsigned desired_capacity)
{
    uw_assert_list(list);
    return _uw_list_resize(list->type_id, get_data_ptr(list), desired_capacity);
}

unsigned uw_list_length(UwValuePtr list)
{
    uw_assert_list(list);
    return _uw_list_length(get_data_ptr(list));
}

bool _uw_list_eq(_UwList* a, _UwList* b)
{
    unsigned n = a->length;
    if (b->length != n) {
        // lists have different lengths
        return false;
    }

    UwValuePtr a_ptr = a->items;
    UwValuePtr b_ptr = b->items;
    while (n) {
        if (!_uw_equal(a_ptr, b_ptr)) {
            return false;
        }
        n--;
        a_ptr++;
        b_ptr++;
    }
    return true;
}

bool _uw_list_append(UwValuePtr list, UwValuePtr item)
// XXX this will be an interface method, _uwi_list_append
{
    uw_assert_list(list);

    UwValue v = uw_clone(item);
    if (uw_error(&v)) {
        uw_destroy(&v);  // make error checker happy
        return false;
    }
    return _uw_list_append_item(list->type_id, get_data_ptr(list), &v, list);
}

bool _uw_list_append_item(UwTypeId type_id, _UwList* list, UwValuePtr item, UwValuePtr parent)
{
    if (uw_is_status(item)) {
        // prohibit appending Status values
        panic_status();
    }
    uw_assert(list->length <= list->capacity);

    if (list->length == list->capacity) {
        unsigned new_capacity;
        if (list->capacity <= UWLIST_CAPACITY_INCREMENT) {
            new_capacity = list->capacity + UWLIST_INITIAL_CAPACITY;
        } else {
            new_capacity = list->capacity + UWLIST_CAPACITY_INCREMENT;
        }
        if (!_uw_list_resize(type_id, list, new_capacity)) {
            return false;
        }
    }
    _uw_embrace(parent, item);
    list->items[list->length] = uw_move(item);
    list->length++;
    return true;
}

UwResult _uw_list_append_va(UwValuePtr list, ...)
{
    va_list ap;
    va_start(ap);
    UwValue result = uw_list_append_ap(list, ap);
    va_end(ap);
    return uw_move(&result);
}

UwResult uw_list_append_ap(UwValuePtr dest, va_list ap)
{
    uw_assert_list(dest);

    UwTypeId type_id = dest->type_id;
    _UwList* list = get_data_ptr(dest);
    unsigned num_appended = 0;
    UwValue error = UwOOM();  // default error is OOM unless some arg is a status
    for(;;) {
        {
            UwValue arg = va_arg(ap, _UwValue);
            if (uw_is_status(&arg)) {
                if (uw_va_end(&arg)) {
                    return UwOK();
                }
                uw_destroy(&error);
                error = uw_move(&arg);
                goto failure;
            }
            if (!uw_charptr_to_string_inplace(&arg)) {
                goto failure;
            }
            if (!_uw_list_append_item(type_id, list, &arg, dest)) {
                goto failure;
            }
            num_appended++;
        }
    }

failure:
    // rollback
    while (num_appended--) {
        UwValue v = _uw_list_pop(list);
        uw_destroy(&v);
    }
    // consume args
    for (;;) {
        {
            UwValue arg = va_arg(ap, _UwValue);
            if (uw_va_end(&arg)) {
                break;
            }
        }
    }
    return uw_move(&error);
}

UwResult uw_list_item(UwValuePtr self, int index)
{
    uw_assert_list(self);

    _UwList* list = get_data_ptr(self);

    if (index < 0) {
        index = list->length + index;
        uw_assert(index >= 0);
    } else {
        uw_assert(((unsigned) index) < list->length);
    }
    //return uw_clone(&list->items[index]);
    return uw_clone(&list->items[index]);
}

UwResult uw_list_pop(UwValuePtr self)
{
    uw_assert_list(self);
    return _uw_list_pop(get_data_ptr(self));
}

UwResult _uw_list_pop(_UwList* list)
{
    if (list->length == 0) {
        return UwError(UW_ERROR_POP_FROM_EMPTY_LIST);
    }
    list->length--;
    return uw_move(&list->items[list->length]);
}

void uw_list_del(UwValuePtr self, unsigned start_index, unsigned end_index)
{
    uw_assert_list(self);
    _uw_list_del(get_data_ptr(self), start_index, end_index);
}

void _uw_list_del(_UwList* list, unsigned start_index, unsigned end_index)
{
    if (list->length == 0) {
        return;
    }
    if (end_index > list->length) {
        end_index = list->length;
    }
    if (start_index >= end_index) {
        return;
    }

    UwValuePtr item_ptr = &list->items[start_index];
    for (unsigned i = start_index; i < end_index; i++, item_ptr++) {
        uw_destroy(item_ptr);
    }
    unsigned new_length = list->length - (end_index - start_index);
    unsigned tail_length = list->length - end_index;
    if (tail_length) {
        memmove(&list->items[start_index], &list->items[end_index], tail_length * sizeof(_UwValue));
        memset(&list->items[new_length], 0, (list->length - new_length) * sizeof(_UwValue));
    }
    list->length = new_length;
}

UwResult uw_list_slice(UwValuePtr self, unsigned start_index, unsigned end_index)
{
    _UwList* src_list = get_data_ptr(self);
    unsigned length = _uw_list_length(src_list);

    if (end_index > length) {
        end_index = length;
    }
    if (start_index >= end_index) {
        // return empty list
        return _uw_create(self->type_id, UwVaEnd());
    }
    unsigned slice_len = end_index - start_index;

    UwValue dest = _uw_create(self->type_id, UwVaEnd());
    if (uw_error(&dest)) {
        return uw_move(&dest);
    }
    if (!uw_list_resize(&dest, slice_len)) {
        return UwOOM();
    }
    _UwList* dest_list = get_data_ptr(&dest);

    UwValuePtr src_item_ptr = &src_list->items[start_index];
    UwValuePtr dest_item_ptr = dest_list->items;
    for (unsigned i = start_index; i < end_index; i++) {
        *dest_item_ptr = uw_clone(src_item_ptr);
        if (uw_error(dest_item_ptr)) {
            return uw_move(dest_item_ptr);
        }
        src_item_ptr++;
        dest_item_ptr++;
        dest_list->length++;
    }
    return uw_move(&dest);
}
