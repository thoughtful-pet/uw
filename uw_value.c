#include "uw_value.h"

char* _uw_get_type_name_by_id(uint8_t type_id)
{
    switch (type_id) {
        case UwTypeId_Null:   return "Null";
        case UwTypeId_Bool:   return "Bool";
        case UwTypeId_Int:    return "Int";
        case UwTypeId_Float:  return "Float";
        case UwTypeId_String: return "String";
        case UwTypeId_List:   return "List";
        case UwTypeId_Map:    return "Map";
        default:
            return "UNKNOWN";
    }
}

char* _uw_get_type_name_from_value(UwValuePtr value)
{
    return _uw_get_type_name_by_id(value->type_id);
}

[[ noreturn ]]
static void bad_type_id(UwValuePtr value)
{
    fprintf(stderr, "Bad type id %u", (unsigned int) value->type_id);
    perror("");
    exit(1);
}

UwValuePtr _uw_create_null(nullptr_t dummy)
{
    UwValuePtr value = _uw_alloc_value();
    value->type_id = UwTypeId_Null;
    value->null_value = nullptr;
    return value;
}

UwValuePtr _uw_create_bool(UwType_Bool initializer)
{
    UwValuePtr value = _uw_alloc_value();
    value->type_id = UwTypeId_Bool;
    value->bool_value = initializer;
    return value;
}

UwValuePtr _uw_create_int(UwType_Int initializer)
{
    UwValuePtr value = _uw_alloc_value();
    value->type_id = UwTypeId_Int;
    value->int_value = initializer;
    return value;
}

UwValuePtr _uw_create_float(UwType_Float initializer)
{
    UwValuePtr value = _uw_alloc_value();
    value->type_id = UwTypeId_Float;
    value->float_value = initializer;
    return value;
}

void uw_delete_value(UwValueRef value)
{
    if (!*value) {
        return;
    }
    UwValuePtr v = *value;
    switch (v->type_id) {
        case UwTypeId_Null:
        case UwTypeId_Bool:
        case UwTypeId_Int:
        case UwTypeId_Float:
            break;
        case UwTypeId_String:
            _uw_delete_string(v->string_value);
            break;
        case UwTypeId_List:
            _uw_delete_list(v->list_value);
            break;
        case UwTypeId_Map:
            _uw_delete_map(v->map_value);
            break;
        default:
            uw_assert(false);
            break;
    }
    free(v);
    *value = nullptr;
}

int _uw_compare_null(UwValuePtr a, nullptr_t b)
{
    return uw_is_null(a)? UW_EQ : UW_NEQ;
}

int _uw_compare_bool(UwValuePtr a, bool b)
{
    switch(a->type_id) {

        case UwTypeId_Bool:
            if (a->bool_value == b) {
                return UW_EQ;
            } else {
                return UW_NEQ;
            }
        case UwTypeId_Int:
            if ( ((UwType_Bool) a->int_value) == b) {
                return UW_EQ;
            } else {
                return UW_NEQ;
            }
        case UwTypeId_Float:
            if ( ((UwType_Bool) a->float_value) == b) {
                return UW_EQ;
            } else {
                return UW_NEQ;
            }
        case UwTypeId_Null:
        case UwTypeId_String:
        case UwTypeId_List:
        case UwTypeId_Map:
            return UW_NEQ;

        default:
            bad_type_id(a);
    }
}

#define COMPARE_WITH_SIGNED  \
{  \
    switch(a->type_id) {  \
        case UwTypeId_Bool:  \
            if (a->bool_value == (UwType_Bool) b) {  \
                return UW_EQ;  \
            } else {  \
                return UW_NEQ;  \
            }  \
        case UwTypeId_Int: {  \
            UwType_Int diff = a->int_value - (UwType_Int) b;  \
            if (diff < 0) {  \
                return UW_LESS;  \
            } else if (diff > 0) {  \
                return UW_GREATER;  \
            } else {  \
                return UW_EQ;  \
            }  \
        }  \
        case UwTypeId_Float: {  \
            UwType_Int diff = a->float_value - (UwType_Float) b;  \
            if (diff < 0) {  \
                return UW_LESS;  \
            } else if (diff > 0) {  \
                return UW_GREATER;  \
            } else {  \
                return UW_EQ;  \
            }  \
        }  \
        case UwTypeId_Null:  \
        case UwTypeId_String:  \
        case UwTypeId_List:  \
        case UwTypeId_Map:  \
            return UW_NEQ;  \
        default:  \
            bad_type_id(a);  \
    }  \
}

#define COMPARE_WITH_UNSIGNED  \
{  \
    switch(a->type_id) {  \
        case UwTypeId_Bool:  \
            if (a->bool_value == (UwType_Bool) b) {  \
                return UW_EQ;  \
            } else {  \
                return UW_NEQ;  \
            }  \
        case UwTypeId_Int: {  \
            if (a->int_value < (UwType_Int) b) {  \
                return UW_LESS;  \
            } else if (a->int_value > (UwType_Int) b) {  \
                return UW_GREATER;  \
            } else {  \
                return UW_EQ;  \
            }  \
        }  \
        case UwTypeId_Float: {  \
            if (a->float_value < (UwType_Float) b) {  \
                return UW_LESS;  \
            } else if (a->float_value > (UwType_Float) b) {  \
                return UW_GREATER;  \
            } else {  \
                return UW_EQ;  \
            }  \
        }  \
        case UwTypeId_Null:  \
        case UwTypeId_String:  \
        case UwTypeId_List:  \
        case UwTypeId_Map:  \
            return UW_NEQ;  \
        default:  \
            bad_type_id(a);  \
    }  \
}

int _uw_compare_char     (UwValuePtr a, char               b) COMPARE_WITH_SIGNED
int _uw_compare_uchar    (UwValuePtr a, unsigned char      b) COMPARE_WITH_UNSIGNED
int _uw_compare_short    (UwValuePtr a, short              b) COMPARE_WITH_SIGNED
int _uw_compare_ushort   (UwValuePtr a, unsigned short     b) COMPARE_WITH_UNSIGNED
int _uw_compare_int      (UwValuePtr a, int                b) COMPARE_WITH_SIGNED
int _uw_compare_uint     (UwValuePtr a, unsigned int       b) COMPARE_WITH_UNSIGNED
int _uw_compare_long     (UwValuePtr a, long               b) COMPARE_WITH_SIGNED
int _uw_compare_ulong    (UwValuePtr a, unsigned long      b) COMPARE_WITH_UNSIGNED
int _uw_compare_longlong (UwValuePtr a, long long          b) COMPARE_WITH_SIGNED
int _uw_compare_ulonglong(UwValuePtr a, unsigned long long b) COMPARE_WITH_UNSIGNED
int _uw_compare_float    (UwValuePtr a, float              b) COMPARE_WITH_SIGNED
int _uw_compare_double   (UwValuePtr a, double             b) COMPARE_WITH_SIGNED

int _uw_compare_uw(UwValuePtr a, UwValuePtr b)
{
    if (a == b) {
        // compare with self
        return UW_EQ;
    }

    switch(a->type_id) {

        // integral types
        case UwTypeId_Null:
            // null can be compared only with null
            if (b->type_id == UwTypeId_Null) {
                return UW_EQ;
            } else {
                return UW_NEQ;
            }

        case UwTypeId_Bool:
            // bool can be compared with bool, int, and float
            switch (b->type_id) {
                case UwTypeId_Bool:
                    if (a->bool_value == b->bool_value) {
                        return UW_EQ;
                    } else {
                        return UW_NEQ;
                    }
                case UwTypeId_Int:
                    if (a->bool_value == (UwType_Bool) b->int_value) {
                        return UW_EQ;
                    } else {
                        return UW_NEQ;
                    }
                case UwTypeId_Float:
                    if (a->bool_value == (UwType_Bool) b->float_value) {
                        return UW_EQ;
                    } else {
                        return UW_NEQ;
                    }
                default:
                    return UW_NEQ;
            }

        case UwTypeId_Int:
            // int can be compared with int, bool, and float
            switch (b->type_id) {
                case UwTypeId_Bool:
                    if ( ((UwType_Bool) a->int_value) == b->bool_value) {
                        return UW_EQ;
                    } else {
                        return UW_NEQ;
                    }
                case UwTypeId_Int: {
                    UwType_Int diff = a->int_value - b->int_value;
                    if (diff < 0) {
                        return UW_LESS;
                    } else if (diff > 0) {
                        return UW_GREATER;
                    } else {
                        return UW_EQ;
                    }
                }
                case UwTypeId_Float: {
                    UwType_Float diff = ((UwType_Float) a->int_value) - b->float_value;
                    if (diff < 0) {
                        return UW_LESS;
                    } else if (diff > 0) {
                        return UW_GREATER;
                    } else {
                        return UW_EQ;
                    }
                }
                default:
                    return UW_NEQ;
            }

        case UwTypeId_Float:
            // float can be compared with float, bool, and int
            switch (b->type_id) {
                case UwTypeId_Bool:
                    if ( ((UwType_Bool) a->float_value) == b->bool_value) {
                        return UW_EQ;
                    } else {
                        return UW_NEQ;
                    }
                case UwTypeId_Int: {
                    UwType_Int diff = a->float_value - (UwType_Float) b->int_value;
                    if (diff < 0) {
                        return UW_LESS;
                    } else if (diff > 0) {
                        return UW_GREATER;
                    } else {
                        return UW_EQ;
                    }
                }
                case UwTypeId_Float: {
                    UwType_Float diff = (a->float_value) - b->float_value;
                    if (diff < 0) {
                        return UW_LESS;
                    } else if (diff > 0) {
                        return UW_GREATER;
                    } else {
                        return UW_EQ;
                    }
                }
                default:
                    return UW_NEQ;
            }

        case UwTypeId_String:
            // strings can be compared with strings
            if (b->type_id == UwTypeId_String) {
                return _uw_string_cmp(a->string_value, b->string_value);
            } else {
                return UW_NEQ;
            }

        case UwTypeId_List:
            // lists can be compared with lists
            if (b->type_id == UwTypeId_List) {
                return _uw_list_cmp(a->list_value, b->list_value);
            } else {
                return UW_NEQ;
            }

        case UwTypeId_Map:
            // maps can be compared with maps
            if (b->type_id == UwTypeId_Map) {
                return _uw_map_cmp(a->map_value, b->map_value);
            } else {
                return UW_NEQ;
            }

        default:
            bad_type_id(a);
    }
}

bool _uw_equal_null(UwValuePtr a, nullptr_t b)
{
    return uw_is_null(a);
}

bool _uw_equal_bool(UwValuePtr a, bool b)
{
    switch(a->type_id) {
        case UwTypeId_Bool:  return a->bool_value == b;
        case UwTypeId_Int:   return ((UwType_Bool) a->int_value) == b;
        case UwTypeId_Float: return ((UwType_Bool) a->float_value) == b;
        case UwTypeId_Null:
        case UwTypeId_String:
        case UwTypeId_List:
        case UwTypeId_Map:
            return false;
        default:
            bad_type_id(a);
    }
}

#define EQUAL_TO  \
{  \
    switch(a->type_id) {  \
        case UwTypeId_Bool:  return a->bool_value == (UwType_Bool) b;  \
        case UwTypeId_Int:   return a->int_value == (UwType_Int) b;  \
        case UwTypeId_Float: return a->float_value == (UwType_Float) b;  \
        case UwTypeId_Null:  \
        case UwTypeId_String:  \
        case UwTypeId_List:  \
        case UwTypeId_Map:  \
            return false;  \
        default:  \
            bad_type_id(a);  \
    }  \
}

bool _uw_equal_char      (UwValuePtr a, char               b) EQUAL_TO
bool _uw_equal_uchar     (UwValuePtr a, unsigned char      b) EQUAL_TO
bool _uw_equal_short     (UwValuePtr a, short              b) EQUAL_TO
bool _uw_equal_ushort    (UwValuePtr a, unsigned short     b) EQUAL_TO
bool _uw_equal_int       (UwValuePtr a, int                b) EQUAL_TO
bool _uw_equal_uint      (UwValuePtr a, unsigned int       b) EQUAL_TO
bool _uw_equal_long      (UwValuePtr a, long               b) EQUAL_TO
bool _uw_equal_ulong     (UwValuePtr a, unsigned long      b) EQUAL_TO
bool _uw_equal_longlong  (UwValuePtr a, long long          b) EQUAL_TO
bool _uw_equal_ulonglong (UwValuePtr a, unsigned long long b) EQUAL_TO
bool _uw_equal_float     (UwValuePtr a, float              b) EQUAL_TO
bool _uw_equal_double    (UwValuePtr a, double             b) EQUAL_TO

bool _uw_equal_uw(UwValuePtr a, UwValuePtr b)
{
    if (a == b) {
        // compare with self
        return true;
    }

    switch(a->type_id) {

        // integral types
        case UwTypeId_Null:
            // null can be compared only with null
            return b->type_id == UwTypeId_Null;

        case UwTypeId_Bool:
            // bool can be compared with bool, int, and float
            switch (b->type_id) {
                case UwTypeId_Bool:  return a->bool_value == b->bool_value;
                case UwTypeId_Int:   return a->bool_value == (UwType_Bool) b->int_value;
                case UwTypeId_Float: return a->bool_value == (UwType_Bool) b->float_value;
                default:              return false;
            }

        case UwTypeId_Int:
            // int can be compared with int, bool, and float
            switch (b->type_id) {
                case UwTypeId_Bool:  return ((UwType_Bool) a->int_value) == b->bool_value;
                case UwTypeId_Int:   return a->int_value == b->int_value;
                case UwTypeId_Float: return ((UwType_Float) a->int_value) == b->float_value;
                default:              return false;
            }

        case UwTypeId_Float:
            // float can be compared with float, bool, and int
            switch (b->type_id) {
                case UwTypeId_Bool:  return ((UwType_Bool) a->float_value) == b->bool_value;
                case UwTypeId_Int:   return a->float_value == (UwType_Float) b->int_value;
                case UwTypeId_Float: return a->float_value == b->float_value;
                default:              return false;
            }

        case UwTypeId_String:
            // strings can be compared with strings
            if (b->type_id == UwTypeId_String) {
                return _uw_string_eq(a->string_value, b->string_value);
            } else {
                return false;
            }

        case UwTypeId_List:
            // lists can be compared with lists
            if (b->type_id == UwTypeId_List) {
                return _uw_list_eq(a->list_value, b->list_value);
            } else {
                return false;
            }

        case UwTypeId_Map:
            // maps can be compared with maps
            if (b->type_id == UwTypeId_Map) {
                return _uw_map_eq(a->map_value, b->map_value);
            } else {
                return false;
            }

        default:
            bad_type_id(a);
    }
}

UwType_Hash uw_hash(UwValuePtr value)
{
    UwHashContext ctx;
    _uw_hash_init(&ctx);
    _uw_calculate_hash(value, &ctx);
    return _uw_hash_finish(&ctx);
}

void _uw_calculate_hash(UwValuePtr value, UwHashContext* ctx)
{
    uint64_t idata;
    size_t size;
    uint8_t type_id = value->type_id;

    _uw_hash_uint64(ctx, type_id);

    switch(type_id) {
        case UwTypeId_Null:   _uw_hash_uint64(ctx, 0); return;
        case UwTypeId_Bool:   _uw_hash_uint64(ctx, value->bool_value); return;
        case UwTypeId_Int:
        case UwTypeId_Float:  _uw_hash_uint64(ctx, value->int_value); return;
        case UwTypeId_String: _uw_string_hash(value->string_value, ctx); return;
        case UwTypeId_List:   _uw_list_hash(value->list_value, ctx); return;
        case UwTypeId_Map:    _uw_map_hash(value->map_value, ctx); return;
        default:
            bad_type_id(value);
    }
}

UwValuePtr uw_copy(UwValuePtr value)
{
   switch(value->type_id) {
        case UwTypeId_Null:   return _uw_create_null(nullptr);
        case UwTypeId_Bool:   return _uw_create_bool(value->bool_value);
        case UwTypeId_Int:    return _uw_create_int(value->int_value);
        case UwTypeId_Float:  return _uw_create_float(value->float_value);
        case UwTypeId_String: return _uw_copy_string(value->string_value);
        case UwTypeId_List:   return _uw_copy_list(value->list_value);
        case UwTypeId_Map:    return _uw_copy_map(value->map_value);
        default:
            bad_type_id(value);
    }
}

UwValuePtr uw_create_from_ctype(int ctype, va_list args)
{
    switch (ctype) {
        case uw_nullptr:   va_arg(args, void*); return uw_create(nullptr);  // XXX something I don't get about nullptr_t
        case uw_bool:      return uw_create((bool) va_arg(args, int /*bool*/));
        case uw_char:      return uw_create(va_arg(args, int /*char*/));
        case uw_uchar:     return uw_create(va_arg(args, unsigned int /*char*/));
        case uw_short:     return uw_create(va_arg(args, int /*short*/));
        case uw_ushort:    return uw_create(va_arg(args, unsigned int /*short*/));
        case uw_int:       return uw_create(va_arg(args, int));
        case uw_uint:      return uw_create(va_arg(args, unsigned int));
        case uw_long:      return uw_create(va_arg(args, long));
        case uw_ulong:     return uw_create(va_arg(args, unsigned long));
        case uw_longlong:  return uw_create(va_arg(args, long long));
        case uw_ulonglong: return uw_create(va_arg(args, unsigned long long));
        case uw_int32:     return uw_create(va_arg(args, int32_t));
        case uw_uint32:    return uw_create(va_arg(args, uint32_t));
        case uw_int64:     return uw_create(va_arg(args, int64_t));
        case uw_uint64:    return uw_create(va_arg(args, uint64_t));
        case uw_float:     return uw_create(va_arg(args, double /*float*/));
        case uw_double:    return uw_create(va_arg(args, double));
        case uw_charptr:   return uw_create_string(va_arg(args, char*));
        case uw_char8ptr:  return uw_create(va_arg(args, char8_t*));
        case uw_char32ptr: return uw_create(va_arg(args, char32_t*));
        case uw_uw:        return va_arg(args, UwValuePtr);
        default:
            // panic
            fprintf(stderr, "%s: bad C type identifier %d\n", __func__, ctype);
            exit(1);
    }
}

#ifdef DEBUG

void _uw_print_indent(int indent)
{
    for (int i = 0; i < indent; i++ ) {
        putchar(' ');
    }
}

void _uw_dump_value(UwValuePtr value, int indent, char* label)
{
    static char* type_ids[] = {
        "Null",
        "Bool",
        "Int",
        "Float",
        "String",
        "List",
        "Map"
    };

    _uw_print_indent(indent);

    if (value == nullptr) {
        puts("nullptr");
        return;
    }

    printf("%s%p %s ", label, value, (value->type_id <= UwTypeId_Map)? type_ids[value->type_id] : "BAD TYPE\n");

    switch(value->type_id) {

        // integral types
        case UwTypeId_Null:   printf("\n"); break;
        case UwTypeId_Bool:   printf("%d\n", (int) value->bool_value); break;
        case UwTypeId_Int:    printf("%lld\n", (long long) value->int_value); break;
        case UwTypeId_Float:  printf("%f\n", value->float_value); break;

        // complex types
        case UwTypeId_String: _uw_dump_string(value->string_value, indent); break;
        case UwTypeId_List:   _uw_dump_list(value->list_value, indent); break;
        case UwTypeId_Map:    _uw_dump_map(value->map_value, indent); break;
    }
}

void uw_dump_value(UwValuePtr value)
{
    _uw_dump_value(value, 0, "");
}

#endif
