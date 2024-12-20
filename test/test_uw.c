#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include "include/uw.h"
#include "src/uw_string_internal.h"

int num_tests = 0;
int num_ok = 0;
int num_fail = 0;
//bool print_ok = true;
bool print_ok = false;

#define TEST(condition) \
    do {  \
        if (condition) {  \
            num_ok++;  \
            if (print_ok) printf("OK: " #condition "\n");  \
        } else {  \
            num_fail++;  \
            fprintf(stderr, "FAILED at line %d: " #condition "\n", __LINE__);  \
        }  \
        num_tests++;  \
    } while (false)

void test_icu()
{
#   ifdef UW_WITH_ICU
        puts("With ICU");
        TEST(uw_char_isspace(' '));
        TEST(uw_char_isspace(0x2003));
#   else
        puts("Without ICU");
        TEST(uw_char_isspace(' '));
        TEST(!uw_char_isspace(0x2003));
#   endif
}

void test_integral_types()
{
    // generics test
    TEST(strcmp(uw_get_type_name((char) UwTypeId_Bool), "Bool") == 0);
    TEST(strcmp(uw_get_type_name((int) UwTypeId_Signed), "Signed") == 0);
    TEST(strcmp(uw_get_type_name((unsigned long long) UwTypeId_Float), "Float") == 0);

    // Null values
    UwValue null_1 = uw_create(nullptr);
    UwValue null_2 = uw_create(nullptr);
    TEST(uw_is_null(&null_1));
    TEST(uw_is_null(&null_2));

    TEST(strcmp(uw_get_type_name(&null_1), "Null") == 0);  // generics test

    // Bool values
    UwValue bool_true  = uw_create(true);
    UwValue bool_false = uw_create(false);
    TEST(uw_is_bool(&bool_true));
    TEST(uw_is_bool(&bool_false));

    // Int values
    UwValue int_0 = uw_create(0);
    UwValue int_1 = uw_create(1);
    UwValue int_neg1 = uw_create(-1);
    TEST(uw_is_int(&int_0));
    TEST(uw_is_int(&int_1));
    TEST(uw_is_signed(&int_1));
    TEST(uw_is_signed(&int_neg1));
    TEST(uw_equal(&int_0, 0));
    TEST(!uw_equal(&int_0, 1));
    TEST(uw_equal(&int_1, 1));
    TEST(uw_equal(&int_neg1, -1));

    UwValue int_2 = uw_create((char) 2);
    TEST(uw_is_signed(&int_2));
    TEST(uw_equal(&int_2, 2));
    UwValue int_3 = uw_create((unsigned char) 3);
    TEST(uw_is_unsigned(&int_3));
    TEST(uw_equal(&int_3, 3));
    UwValue int_4 = uw_create((short) 4);
    TEST(uw_is_signed(&int_4));
    TEST(uw_equal(&int_4, 4));
    UwValue int_5 = uw_create((unsigned short) 5);
    TEST(uw_is_unsigned(&int_5));
    TEST(uw_equal(&int_5, 5));
    UwValue int_6 = uw_create(6U);
    TEST(uw_is_unsigned(&int_6));
    TEST(uw_equal(&int_6, 6));
    UwValue int_7 = uw_create(7L);
    TEST(uw_equal(&int_7, 7));
    TEST(uw_is_signed(&int_7));
    UwValue int_8 = uw_create(8UL);
    TEST(uw_is_unsigned(&int_8));
    TEST(uw_equal(&int_8, 8));
    UwValue int_9 = uw_create(9LL);
    TEST(uw_is_signed(&int_9));
    TEST(uw_equal(&int_9, 9));
    UwValue int_10 = uw_create(10ULL);
    TEST(uw_is_unsigned(&int_10));
    TEST(uw_equal(&int_10, 10));

    // Float values
    UwValue f_0 = uw_create(0.0);
    UwValue f_1 = uw_create(1.0);
    UwValue f_neg1 = uw_create(-1.0);
    TEST(uw_is_float(&f_0));
    TEST(uw_is_float(&f_1));
    TEST(uw_is_float(&f_neg1));
    TEST(uw_equal(&f_0, &f_0));
    TEST(uw_equal(&f_1, &f_1));
    TEST(uw_equal(&f_0, 0.0));
    TEST(!uw_equal(&f_0, 1.0));
    TEST(uw_equal(&f_1, 1.0));
    TEST(uw_equal(&f_neg1, -1.0));
    TEST(!uw_equal(&f_neg1, 1.0));

    UwValue f_2 = uw_create(2.0);
    TEST(uw_is_float(&f_2));
    TEST(uw_equal(&f_2, 2.0));
    UwValue f_3 = uw_create(3.0f);
    TEST(uw_is_float(&f_3));
    TEST(uw_equal(&f_3, 3.0));
    TEST(uw_equal(&f_3, 3.0f));

    // null vs null
    TEST(uw_equal(&null_1, &null_2));
    TEST(uw_equal(&null_1, nullptr));

    // null vs bool
    TEST(!uw_equal(&null_1, &bool_true));
    TEST(!uw_equal(&null_1, &bool_false));
    TEST(!uw_equal(&null_1, true));
    TEST(!uw_equal(&null_1, false));

    // null vs int
    TEST(!uw_equal(&null_1, &int_0));
    TEST(!uw_equal(&null_1, &int_1));
    TEST(!uw_equal(&null_1, &int_neg1));
    TEST(!uw_equal(&null_1, (char) 2));
    TEST(!uw_equal(&null_1, (unsigned char) 2));
    TEST(!uw_equal(&null_1, (short) 2));
    TEST(!uw_equal(&null_1, (unsigned short) 2));
    TEST(!uw_equal(&null_1, 2));
    TEST(!uw_equal(&null_1, 2U));
    TEST(!uw_equal(&null_1, (unsigned long) 2));
    TEST(!uw_equal(&null_1, (unsigned long long) 2));

    // null vs float
    TEST(!uw_equal(&null_1, &f_0));
    TEST(!uw_equal(&null_1, &f_1));
    TEST(!uw_equal(&null_1, &f_neg1));
    TEST(!uw_equal(&null_1, 2.0f));
    TEST(!uw_equal(&null_1, 2.0));

    // bool vs null
    TEST(!uw_equal(&bool_true, &null_1));
    TEST(!uw_equal(&bool_false, &null_1));
    TEST(!uw_equal(&bool_true, nullptr));
    TEST(!uw_equal(&bool_false, nullptr));

    // bool vs bool
    TEST(uw_equal(&bool_true, true));
    TEST(!uw_equal(&bool_true, false));
    TEST(uw_equal(&bool_false, false));
    TEST(!uw_equal(&bool_false, true));

    TEST(uw_equal(&bool_true, &bool_true));
    TEST(uw_equal(&bool_false, &bool_false));
    TEST(!uw_equal(&bool_true, &bool_false));
    TEST(!uw_equal(&bool_false, &bool_true));

    // bool vs int
    TEST(!uw_equal(&bool_true, &int_0));
    TEST(!uw_equal(&bool_true, &int_1));
    TEST(!uw_equal(&bool_true, &int_neg1));
    TEST(!uw_equal(&bool_false, &int_0));
    TEST(!uw_equal(&bool_false, &int_1));
    TEST(!uw_equal(&bool_false, &int_neg1));
    TEST(!uw_equal(&bool_true, (char) 0));
    TEST(!uw_equal(&bool_true, (char) 2));
    TEST(!uw_equal(&bool_false, (char) 0));
    TEST(!uw_equal(&bool_false, (char) 2));
    TEST(!uw_equal(&bool_true, (unsigned char) 0));
    TEST(!uw_equal(&bool_true, (unsigned char) 2));
    TEST(!uw_equal(&bool_false, (unsigned char) 0));
    TEST(!uw_equal(&bool_false, (unsigned char) 2));
    TEST(!uw_equal(&bool_true, (short) 0));
    TEST(!uw_equal(&bool_true, (short) 2));
    TEST(!uw_equal(&bool_false, (short) 0));
    TEST(!uw_equal(&bool_false, (short) 2));
    TEST(!uw_equal(&bool_true, (unsigned short) 0));
    TEST(!uw_equal(&bool_true, (unsigned short) 2));
    TEST(!uw_equal(&bool_false, (unsigned short) 0));
    TEST(!uw_equal(&bool_false, (unsigned short) 2));
    TEST(!uw_equal(&bool_true, 0));
    TEST(!uw_equal(&bool_true, 2));
    TEST(!uw_equal(&bool_false, 0));
    TEST(!uw_equal(&bool_false, 2));
    TEST(!uw_equal(&bool_true, 0U));
    TEST(!uw_equal(&bool_true, 2U));
    TEST(!uw_equal(&bool_false, 0U));
    TEST(!uw_equal(&bool_false, 2U));
    TEST(!uw_equal(&bool_true, 0L));
    TEST(!uw_equal(&bool_true, 2L));
    TEST(!uw_equal(&bool_false, 0L));
    TEST(!uw_equal(&bool_false, 2L));
    TEST(!uw_equal(&bool_true, 0UL));
    TEST(!uw_equal(&bool_true, 2UL));
    TEST(!uw_equal(&bool_false, 0UL));
    TEST(!uw_equal(&bool_false, 2UL));
    TEST(!uw_equal(&bool_true, 0LL));
    TEST(!uw_equal(&bool_true, 2LL));
    TEST(!uw_equal(&bool_false, 0LL));
    TEST(!uw_equal(&bool_false, 2LL));
    TEST(!uw_equal(&bool_true, 0ULL));
    TEST(!uw_equal(&bool_true, 2ULL));
    TEST(!uw_equal(&bool_false, 0ULL));
    TEST(!uw_equal(&bool_false, 2ULL));

    // bool vs float
    TEST(!uw_equal(&bool_true, &f_0));
    TEST(!uw_equal(&bool_true, &f_1));
    TEST(!uw_equal(&bool_true, &f_neg1));
    TEST(!uw_equal(&bool_false, &f_0));
    TEST(!uw_equal(&bool_false, &f_1));
    TEST(!uw_equal(&bool_false, &f_neg1));
    TEST(!uw_equal(&bool_true, 0.0f));
    TEST(!uw_equal(&bool_true, 2.0f));
    TEST(!uw_equal(&bool_false, 0.0f));
    TEST(!uw_equal(&bool_false, 2.0f));
    TEST(!uw_equal(&bool_true, 0.0f));
    TEST(!uw_equal(&bool_true, 2.0f));
    TEST(!uw_equal(&bool_false, 0.0f));
    TEST(!uw_equal(&bool_false, 2.0f));
    TEST(!uw_equal(&bool_true, 0.0));
    TEST(!uw_equal(&bool_true, 2.0));
    TEST(!uw_equal(&bool_false, 0.0));
    TEST(!uw_equal(&bool_false, 2.0));
    TEST(!uw_equal(&bool_true, 0.0));
    TEST(!uw_equal(&bool_true, 2.0));
    TEST(!uw_equal(&bool_false, 0.0));
    TEST(!uw_equal(&bool_false, 2.0));

    // int vs null
    TEST(!uw_equal(&int_0, &null_1));
    TEST(!uw_equal(&int_0, nullptr));
    TEST(!uw_equal(&int_1, &null_1));
    TEST(!uw_equal(&int_neg1, &null_1));

    // int vs bool
    TEST(!uw_equal(&int_0, &bool_true));
    TEST(!uw_equal(&int_0, &bool_false));
    TEST(!uw_equal(&int_0, true));
    TEST(!uw_equal(&int_0, false));
    TEST(!uw_equal(&int_1, &bool_true));
    TEST(!uw_equal(&int_1, &bool_false));
    TEST(!uw_equal(&int_1, true));
    TEST(!uw_equal(&int_1, false));
    TEST(!uw_equal(&int_neg1, &bool_true));
    TEST(!uw_equal(&int_neg1, &bool_false));
    TEST(!uw_equal(&int_neg1, true));
    TEST(!uw_equal(&int_neg1, false));

    // int vs int
    TEST(uw_equal(&int_0, &int_0));
    TEST(uw_equal(&int_1, &int_1));
    TEST(uw_equal(&int_neg1, &int_neg1));
    TEST(!uw_equal(&int_0, &int_1));
    TEST(!uw_equal(&int_1, &int_0));
    TEST(!uw_equal(&int_neg1, &int_0));
    TEST(!uw_equal(&int_0, &int_neg1));
    TEST(uw_equal(&int_1, (char) 1));
    TEST(!uw_equal(&int_1, (char) 2));
    TEST(!uw_equal(&int_1, (char) -1));
    TEST(uw_equal(&int_1, (unsigned char) 1));
    TEST(!uw_equal(&int_1, (unsigned char) 2));
    TEST(!uw_equal(&int_1, (unsigned char) 0));
    TEST(uw_equal(&int_1, (short) 1));
    TEST(!uw_equal(&int_1, (short) 2));
    TEST(!uw_equal(&int_1, (short) -1));
    TEST(uw_equal(&int_1, (unsigned short) 1));
    TEST(!uw_equal(&int_1, (unsigned short) 2));
    TEST(!uw_equal(&int_1, (unsigned short) 0));
    TEST(uw_equal(&int_1, 1));
    TEST(!uw_equal(&int_1, 2));
    TEST(!uw_equal(&int_1, -1));
    TEST(uw_equal(&int_1, 1U));
    TEST(!uw_equal(&int_1, 2U));
    TEST(!uw_equal(&int_1, 0U));
    TEST(uw_equal(&int_1, 1L));
    TEST(!uw_equal(&int_1, 2L));
    TEST(!uw_equal(&int_1, -1L));
    TEST(uw_equal(&int_1, 1UL));
    TEST(!uw_equal(&int_1, 2UL));
    TEST(!uw_equal(&int_1, 0UL));
    TEST(uw_equal(&int_1, 1LL));
    TEST(!uw_equal(&int_1, 2LL));
    TEST(!uw_equal(&int_1, -1LL));
    TEST(uw_equal(&int_1, 1ULL));
    TEST(!uw_equal(&int_1, 2ULL));
    TEST(!uw_equal(&int_1, 0ULL));

    // int vs float
    TEST(uw_equal(&int_0, &f_0));
    TEST(uw_equal(&int_1, &f_1));
    TEST(uw_equal(&int_neg1, &f_neg1));
    TEST(!uw_equal(&int_0, &f_1));
    TEST(!uw_equal(&int_1, &f_0));
    TEST(!uw_equal(&int_neg1, &f_0));
    TEST(!uw_equal(&int_0, &f_neg1));
    TEST(uw_equal(&int_1, 1.0));
    TEST(!uw_equal(&int_1, 2.0));
    TEST(!uw_equal(&int_1, -1.0));
    TEST(uw_equal(&int_1, 1.0f));
    TEST(!uw_equal(&int_1, 2.0f));
    TEST(!uw_equal(&int_1, -1.0f));

    // float vs null
    TEST(!uw_equal(&f_0, &null_1));
    TEST(!uw_equal(&f_0, nullptr));
    TEST(!uw_equal(&f_1, &null_1));
    TEST(!uw_equal(&f_neg1, &null_1));

    // float vs bool
    TEST(!uw_equal(&f_0, &bool_true));
    TEST(!uw_equal(&f_0, &bool_false));
    TEST(!uw_equal(&f_0, true));
    TEST(!uw_equal(&f_0, false));
    TEST(!uw_equal(&f_1, &bool_true));
    TEST(!uw_equal(&f_1, &bool_false));
    TEST(!uw_equal(&f_1, true));
    TEST(!uw_equal(&f_1, false));
    TEST(!uw_equal(&f_neg1, &bool_true));
    TEST(!uw_equal(&f_neg1, &bool_false));
    TEST(!uw_equal(&f_neg1, true));
    TEST(!uw_equal(&f_neg1, false));

    // float vs int
    TEST(uw_equal(&f_0, &int_0));
    TEST(uw_equal(&f_1, &int_1));
    TEST(uw_equal(&f_neg1, &int_neg1));
    TEST(!uw_equal(&f_0, &int_1));
    TEST(!uw_equal(&f_1, &int_0));
    TEST(!uw_equal(&f_neg1, &int_0));
    TEST(!uw_equal(&f_0, &int_neg1));
    TEST(uw_equal(&f_1, (char) 1));
    TEST(!uw_equal(&f_1, (char) 2));
    TEST(!uw_equal(&f_1, (char) -1));
    TEST(uw_equal(&f_1, (unsigned char) 1));
    TEST(!uw_equal(&f_1, (unsigned char) 2));
    TEST(!uw_equal(&f_1, (unsigned char) 0));
    TEST(uw_equal(&f_1, (short) 1));
    TEST(!uw_equal(&f_1, (short) 2));
    TEST(!uw_equal(&f_1, (short) -1));
    TEST(uw_equal(&f_1, (unsigned short) 1));
    TEST(!uw_equal(&f_1, (unsigned short) 2));
    TEST(!uw_equal(&f_1, (unsigned short) 0));
    TEST(uw_equal(&f_1, 1));
    TEST(!uw_equal(&f_1, 2));
    TEST(!uw_equal(&f_1, -1));
    TEST(uw_equal(&f_1, 1U));
    TEST(!uw_equal(&f_1, 2U));
    TEST(!uw_equal(&f_1, 0U));
    TEST(uw_equal(&f_1, 1L));
    TEST(!uw_equal(&f_1, 2L));
    TEST(!uw_equal(&f_1, -1L));
    TEST(uw_equal(&f_1, 1UL));
    TEST(!uw_equal(&f_1, 2UL));
    TEST(!uw_equal(&f_1, 0UL));
    TEST(uw_equal(&f_1, 1LL));
    TEST(!uw_equal(&f_1, 2LL));
    TEST(!uw_equal(&f_1, -1LL));
    TEST(uw_equal(&f_1, 1ULL));
    TEST(!uw_equal(&f_1, 2ULL));
    TEST(!uw_equal(&f_1, 0ULL));

    // float vs float
    TEST(!uw_equal(&f_0, &f_1));
    TEST(!uw_equal(&f_1, &f_0));
    TEST(!uw_equal(&f_neg1, &f_0));
    TEST(!uw_equal(&f_0, &f_neg1));
    TEST(uw_equal(&f_1, 1.0));
    TEST(!uw_equal(&f_1, 2.0));
    TEST(!uw_equal(&f_1, -1.0));
    TEST(uw_equal(&f_1, 1.0f));
    TEST(!uw_equal(&f_1, 2.0f));
    TEST(!uw_equal(&f_1, -1.0f));
}

void test_string()
{
    struct _UwString* s;
    CapMethods* capmeth;

    { // testing max allocation with 1-byte capacity
        unsigned capacity_1_byte = 256 - offsetof(struct _UwStringExtraData, string_data)
                                   - 3 /* header, length, and capacity */;
        UwValue v = uw_create_empty_string(capacity_1_byte, 1);
        s = _uw_get_string_ptr(&v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == capacity_1_byte);
        TEST(_uw_string_char_size(s) == 1);
        TEST(_uw_string_cap_size(s) == 1);
        //uw_dump(stdout, &v);
    }
    { // testing min allocation with 2-bytes capacity
        unsigned capacity_2_bytes = 272 - offsetof(struct _UwStringExtraData, string_data)
                                    - 1 /* header */ - 1 /* padding */ - 2 /* length */ - 2 /* capacity */;
        UwValue v = uw_create_empty_string(capacity_2_bytes, 1);
        s = _uw_get_string_ptr(&v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == capacity_2_bytes);
        TEST(_uw_string_char_size(s) == 1);
        TEST(_uw_string_cap_size(s) == 2);
        //uw_dump(stdout, &v);
    }
    { // testing cap_size=1 char_size=1
        UwValue v = uw_create_empty_string(0, 1);
        s = _uw_get_string_ptr(&v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 12);
        TEST(_uw_string_char_size(s) == 1);
        TEST(_uw_string_cap_size(s) == 1);
        //uw_dump(stdout, &v);

        uw_string_append(&v, "hello");

        s = _uw_get_string_ptr(&v);
        capmeth = get_cap_methods(s);

        TEST(capmeth->get_length(s) == 5);
        TEST(capmeth->get_capacity(s) == 12);
        //uw_dump(stdout, &v);

        uw_string_append(&v, '!');

        s = _uw_get_string_ptr(&v);
        capmeth = get_cap_methods(s);

        TEST(capmeth->get_length(s) == 6);
        TEST(capmeth->get_capacity(s) == 12);
        //uw_dump(stdout, &v);

        // increase capacity to 2 bytes
        for (int i = 0; i < 250; i++) {
            uw_string_append_char(&v, ' ');
        }
        s = _uw_get_string_ptr(&v);
        capmeth = get_cap_methods(s);

        TEST(capmeth->get_length(s) == 256);
        TEST(capmeth->get_capacity(s) == 278);
        TEST(_uw_string_char_size(s) == 1);
        TEST(_uw_string_cap_size(s) == 2);
        //uw_dump(stdout, &v);

        uw_string_append(&v, "everybody");
        uw_string_erase(&v, 5, 255);
        TEST(uw_equal(&v, "hello everybody"));
        TEST(!uw_equal(&v, ""));

        // test comparison
        UwValue v2 = uw_create("hello everybody");
        TEST(uw_equal(&v, &v2));
        TEST(uw_equal_cstr(&v, "hello everybody"));
        TEST(uw_equal_cstr(&v2, "hello everybody"));
        TEST(!uw_equal_cstr(&v, "hello Everybody"));
        TEST(!uw_equal_cstr(&v2, "hello Everybody"));
        TEST(uw_equal(&v, "hello everybody"));
        TEST(uw_equal(&v2, "hello everybody"));
        TEST(!uw_equal(&v, "hello Everybody"));
        TEST(!uw_equal(&v2, "hello Everybody"));
        TEST(uw_equal(&v, u8"hello everybody"));
        TEST(uw_equal(&v2, u8"hello everybody"));
        TEST(!uw_equal(&v, u8"hello Everybody"));
        TEST(!uw_equal(&v2, u8"hello Everybody"));
        TEST(uw_equal(&v, U"hello everybody"));
        TEST(uw_equal(&v2, U"hello everybody"));
        TEST(!uw_equal(&v, U"hello Everybody"));
        TEST(!uw_equal(&v2, U"hello Everybody"));

        UwValue v3 = uw_create("hello everybody");
        // test C string
        UW_CSTRING_LOCAL(cv3, &v3);
        TEST(strcmp(cv3, "hello everybody") == 0);

        // test substring
        TEST(uw_substring_eq_cstr(&v, 4, 7, "o e"));
        TEST(!uw_substring_eq_cstr(&v, 4, 7, ""));
        TEST(uw_substring_eq_cstr(&v, 0, 4, "hell"));
        TEST(uw_substring_eq_cstr(&v, 11, 100, "body"));
        TEST(uw_substring_eq(&v, 4, 7, "o e"));
        TEST(!uw_substring_eq(&v, 4, 7, ""));
        TEST(uw_substring_eq(&v, 0, 4, "hell"));
        TEST(uw_substring_eq(&v, 11, 100, "body"));
        TEST(uw_substring_eq(&v, 4, 7, u8"o e"));
        TEST(!uw_substring_eq(&v, 4, 7, u8""));
        TEST(uw_substring_eq(&v, 0, 4, u8"hell"));
        TEST(uw_substring_eq(&v, 11, 100, u8"body"));
        TEST(uw_substring_eq(&v, 4, 7, U"o e"));
        TEST(!uw_substring_eq(&v, 4, 7, U""));
        TEST(uw_substring_eq(&v, 0, 4, U"hell"));
        TEST(uw_substring_eq(&v, 11, 100, U"body"));

        // test erase and truncate
        uw_string_erase(&v, 4, 255);
        TEST(uw_equal(&v, "hell"));

        uw_string_erase(&v, 0, 2);
        TEST(uw_equal(&v, "ll"));

        uw_string_truncate(&v, 0);
        TEST(uw_equal(&v, ""));

        s = _uw_get_string_ptr(&v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 278);
        //uw_dump(stdout, &v);

        // test append substring
        uw_string_append_substring(&v, "0123456789", 3, 7);
        TEST(uw_equal(&v, "3456"));
        uw_string_append_substring(&v, u8"0123456789", 3, 7);
        TEST(uw_equal(&v, "34563456"));
        uw_string_append_substring(&v, U"0123456789", 3, 7);
        TEST(uw_equal(&v, "345634563456"));
        uw_string_truncate(&v, 0);

        // change char size to 2-byte by appending wider chars -- the string will be copied
        uw_string_append(&v, u8"สวัสดี");

        s = _uw_get_string_ptr(&v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 6);
        TEST(capmeth->get_capacity(s) == 6);
        TEST(_uw_string_char_size(s) == 2);
        TEST(_uw_string_cap_size(s) == 1);
        TEST(uw_equal(&v, u8"สวัสดี"));
        //uw_dump(stdout, &v);
    }

#   ifdef DEBUG
    { // testing cap_size=2 char_size=1
        UwValue v = uw_create_empty_string2(2, 1);
        s = _uw_get_string_ptr(&v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 2);
        TEST(_uw_string_char_size(s) == 1);
        TEST(_uw_string_cap_size(s) == 2);
        uw_dump(stdout, &v);
    }

    { // testing cap_size=4 char_size=1
        UwValue v = uw_create_empty_string2(4, 1);
        s = _uw_get_string_ptr(&v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 4);
        TEST(_uw_string_char_size(s) == 1);
        TEST(_uw_string_cap_size(s) == 4);
        //uw_dump(stdout, &v);
    }

#   if UINT_WIDTH > 32
    { // testing cap_size=8 char_size=1
        UwValue v = uw_create_empty_string2(8, 1);
        s = _uw_get_string_ptr(&v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 8);
        TEST(_uw_string_char_size(s) == 1);
        TEST(_uw_string_cap_size(s) == 8);
        //uw_dump(stdout, &v);
    }
#   endif
#   endif

    { // testing cap_size=1 char_size=2
        UwValue v = uw_create_empty_string(1, 2);
        s = _uw_get_string_ptr(&v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 6);
        TEST(_uw_string_char_size(s) == 2);
        TEST(_uw_string_cap_size(s) == 1);
        //uw_dump(stdout, &v);

        uw_string_append(&v, u8"สบาย");

        TEST(capmeth->get_length(s) == 4);
        TEST(capmeth->get_capacity(s) == 6);
        //uw_dump(stdout, &v);

        uw_string_append(&v, 0x0e14);
        uw_string_append(&v, 0x0e35);

        TEST(capmeth->get_length(s) == 6);
        TEST(capmeth->get_capacity(s) == 6);
        TEST(uw_equal(&v, u8"สบายดี"));
        //uw_dump(stdout, &v);

        // test truncate
        uw_string_truncate(&v, 4);
        TEST(uw_equal(&v, u8"สบาย"));
        TEST(!uw_equal(&v, ""));

        // increase capacity to 2 bytes
        for (int i = 0; i < 251; i++) {
            uw_string_append(&v, ' ');
        }
        s = _uw_get_string_ptr(&v);
        capmeth = get_cap_methods(s);

        TEST(capmeth->get_length(s) == 255);
        TEST(capmeth->get_capacity(s) == 267);
        TEST(_uw_string_char_size(s) == 2);
        TEST(_uw_string_cap_size(s) == 2);
        //uw_dump(stdout, &v);

        uw_string_append(&v, U"สบาย");
        uw_string_erase(&v, 4, 255);
        TEST(uw_equal(&v, u8"สบายสบาย"));
        TEST(!uw_equal(&v, ""));

        // test comparison
        UwValue v2 = uw_create(u8"สบายสบาย");
        TEST(uw_equal(&v, &v2));
        TEST(uw_equal(&v, u8"สบายสบาย"));
        TEST(uw_equal(&v2, u8"สบายสบาย"));
        TEST(!uw_equal(&v, u8"ความสบาย"));
        TEST(!uw_equal(&v2, u8"ความสบาย"));
        TEST(uw_equal(&v, U"สบายสบาย"));
        TEST(uw_equal(&v2, U"สบายสบาย"));
        TEST(!uw_equal(&v, U"ความสบาย"));
        TEST(!uw_equal(&v2, U"ความสบาย"));

        // test substring
        TEST(uw_substring_eq(&v, 3, 5, u8"ยส"));
        TEST(!uw_substring_eq(&v, 3, 5, u8""));
        TEST(uw_substring_eq(&v, 0, 3, u8"สบา"));
        TEST(uw_substring_eq(&v, 6, 100, u8"าย"));
        TEST(uw_substring_eq(&v, 3, 5, U"ยส"));
        TEST(!uw_substring_eq(&v, 3, 5, U""));
        TEST(uw_substring_eq(&v, 0, 3, U"สบา"));
        TEST(uw_substring_eq(&v, 6, 100, U"าย"));

        // test erase and truncate
        uw_string_erase(&v, 4, 255);
        TEST(uw_equal(&v, u8"สบาย"));

        uw_string_erase(&v2, 0, 4);
        TEST(uw_equal(&v, u8"สบาย"));

        uw_string_truncate(&v, 0);
        TEST(uw_equal(&v, ""));

        // test append substring
        uw_string_append_substring(&v, u8"สบายสบาย", 1, 4);
        TEST(uw_equal(&v, u8"บาย"));
        uw_string_append_substring(&v, U"สบายสบาย", 1, 4);
        TEST(uw_equal(&v, U"บายบาย"));
        uw_string_truncate(&v, 0);

        s = _uw_get_string_ptr(&v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 267);
        //uw_dump(stdout, &v);
    }

#   ifdef DEBUG
    { // testing cap_size=2 char_size=2
        UwValue v = uw_create_empty_string2(2, 2);
        s = _uw_get_string_ptr(&v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 1);
        TEST(_uw_string_char_size(s) == 2);
        TEST(_uw_string_cap_size(s) == 2);
        uw_dump(stdout, &v);
    }

    { // testing cap_size=4 char_size=2
        UwValue v = uw_create_empty_string2(4, 2);
        s = _uw_get_string_ptr(&v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 2);
        TEST(_uw_string_char_size(s) == 2);
        TEST(_uw_string_cap_size(s) == 4);
        //uw_dump(stdout, &v);
    }

#   if UINT_WIDTH > 32
    { // testing cap_size=8 char_size=2
        UwValue v = uw_create_empty_string2(8, 2);
        //uw_dump(stdout, s_8_2);
        s = _uw_get_string_ptr(&v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 4);
        TEST(_uw_string_char_size(s) == 2);
        TEST(_uw_string_cap_size(s) == 8);
        //uw_dump(stdout, &v);
    }
#   endif
#   endif

    { // testing cap_size=1 char_size=3
        UwValue v = uw_create_empty_string(1, 3);
        s = _uw_get_string_ptr(&v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 4);
        TEST(_uw_string_char_size(s) == 3);
        TEST(_uw_string_cap_size(s) == 1);
        //uw_dump(stdout, &v);
    }

#   ifdef DEBUG
    { // testing cap_size=2 char_size=3
        UwValue v = uw_create_empty_string2(2, 3);
        s = _uw_get_string_ptr(&v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 3);
        TEST(_uw_string_char_size(s) == 3);
        TEST(_uw_string_cap_size(s) == 2);
        //uw_dump(stdout, &v);
    }

    { // testing cap_size=4 char_size=3
        UwValue v = uw_create_empty_string2(4, 3);
        s = _uw_get_string_ptr(&v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 1);
        TEST(_uw_string_char_size(s) == 3);
        TEST(_uw_string_cap_size(s) == 4);
        //uw_dump(stdout, &v);
    }

#   if UINT_WIDTH > 32
    { // testing cap_size=8 char_size=3
        UwValue v = uw_create_empty_string2(8, 3);
        s = _uw_get_string_ptr(&v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 2);
        TEST(_uw_string_char_size(s) == 3);
        TEST(_uw_string_cap_size(s) == 8);
        //uw_dump(stdout, &v);
    }
#   endif
#   endif

    { // testing cap_size=1 char_size=4
        UwValue v = uw_create_empty_string(1, 4);
        s = _uw_get_string_ptr(&v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 3);
        TEST(_uw_string_char_size(s) == 4);
        TEST(_uw_string_cap_size(s) == 1);
        //uw_dump(stdout, &v);
    }

#   ifdef DEBUG
    { // testing cap_size=2 char_size=4
        UwValue v = uw_create_empty_string2(2, 4);
        s = _uw_get_string_ptr(&v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 2);
        TEST(_uw_string_char_size(s) == 4);
        TEST(_uw_string_cap_size(s) == 2);
        //uw_dump(stdout, &v);
    }

    { // testing cap_size=4 char_size=4
        UwValue v = uw_create_empty_string2(4, 4);
        s = _uw_get_string_ptr(&v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 1);
        TEST(_uw_string_char_size(s) == 4);
        TEST(_uw_string_cap_size(s) == 4);
        //uw_dump(stdout, &v);
    }

#if UINT_WIDTH > 32
    { // testing cap_size=8 char_size=4
        UwValue v = uw_create_empty_string2(8, 4);
        s = _uw_get_string_ptr(&v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 2);
        TEST(_uw_string_char_size(s) == 4);
        TEST(_uw_string_cap_size(s) == 8);
        //uw_dump(stdout, &v);
    }
#   endif
#   endif

    { // test trimming
        UwValue v = uw_create(u8"  สวัสดี   ");
        TEST(uw_strlen(&v) == 11);
        uw_string_ltrim(&v);
        TEST(uw_equal(&v, u8"สวัสดี   "));
        uw_string_rtrim(&v);
        TEST(uw_equal(&v, u8"สวัสดี"));
        TEST(uw_strlen(&v) == 6);
    }

    { // test join
        UwValue list = UwList();
        uw_list_append(&list, "Hello");
        uw_list_append(&list, u8"สวัสดี");
        uw_list_append(&list, "Thanks");
        uw_list_append(&list, U"mulțumesc");
        UwValue v = uw_string_join('/', &list);
        TEST(uw_equal(&v, U"Hello/สวัสดี/Thanks/mulțumesc"));
        //uw_dump(stdout, &v);
    }

    { // test join with CharPtr
        UwValue list       = UwList();
        UwValue sawatdee   = UwChar8Ptr(u8"สวัสดี");
        UwValue thanks     = UwCharPtr("Thanks");
        UwValue multsumesc = UwChar32Ptr(U"mulțumesc");
        UwValue wat        = UwChar32Ptr(U"🙏");
        uw_list_append(&list, "Hello");
        uw_list_append(&list, &sawatdee);
        uw_list_append(&list, &thanks);
        uw_list_append(&list, &multsumesc);
        UwValue v = uw_string_join(&wat, &list);
        TEST(uw_equal(&v, U"Hello🙏สวัสดี🙏Thanks🙏mulțumesc"));
        //uw_dump(stdout, &v);
    }

    { // test uw_strcat
        UwValue v = uw_strcat(
            uw_create_string("Hello! "), UwCharPtr("Thanks"), UwChar32Ptr(U"🙏"), UwChar8Ptr(u8"สวัสดี")
        );
        TEST(uw_equal(&v, U"Hello! Thanks🙏สวัสดี"));
        //uw_dump(stdout, &v);
    }

    { // test split/join
        UwValue str = uw_create(U"สบาย/สบาย/yo/yo");
        UwValue list = uw_string_split_chr(&str, '/');
        //uw_dump(stdout, &list);
        UwValue v = uw_string_join('/', &list);
        TEST(uw_equal(&v, U"สบาย/สบาย/yo/yo"));
    }

    // test append_buffer
    {
        char8_t data[2500];
        memset(data, '1', sizeof(data));

        UWDECL_String(str);

        uw_string_append_buffer(&str, data, sizeof(data));

        s = _uw_get_string_ptr(&str);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_capacity(s) >= capmeth->get_length(s));
        TEST(capmeth->get_length(s) == 2500);
        //uw_dump(stdout, &str);
    }
}

void test_list()
{
    UwValue list = UwList();

    TEST(uw_list_length(&list) == 0);

    for(unsigned i = 0; i < 1000; i++) {
        {
            UwValue item = uw_create(i);
            uw_list_append(&list, &item);
        }

        TEST(uw_list_length(&list) == i + 1);

        {
            UwValue v = uw_list_item(&list, i);
            UwValue item = uw_create(i);
            TEST(uw_equal(&v, &item));
        }
    }

    {
        UwValue item = uw_list_item(&list, -2);
        UwValue v = uw_create(998);
        TEST(uw_equal(&v, &item));
    }

    uw_list_del(&list, 100, 200);
    TEST(uw_list_length(&list) == 900);

    {
        UwValue item = uw_list_item(&list, 99);
        UwValue v = uw_create(99);
        TEST(uw_equal(&v, &item));
    }
    {
        UwValue item = uw_list_item(&list, 100);
        UwValue v = uw_create(200);
        TEST(uw_equal(&v, &item));
    }

    {
        UwValue slice = uw_list_slice(&list, 750, 850);
        TEST(uw_list_length(&slice) == 100);
        {
            UwValue item = uw_list_item(&slice, 1);
            TEST(uw_equal(&item, 851));
        }
        {
            UwValue item = uw_list_item(&slice, 98);
            TEST(uw_equal(&item, 948));
        }
    }
}

void test_map()
{
    {
        UwValue map = UwMap();
        UwValue key = uw_create(0);
        UwValue value = uw_create(false);

        uw_map_update(&map, &key, &value);
        TEST(uw_map_length(&map) == 1);
        uw_destroy(&key);
        uw_destroy(&value);

        key = uw_create(0);
        TEST(uw_map_has_key(&map, &key));
        uw_destroy(&key);

        key = uw_create(nullptr);
        TEST(!uw_map_has_key(&map, &key));
        uw_destroy(&key);

        for (int i = 1; i < 50; i++) {
            key = uw_create(i);
            value = uw_create(i);
            uw_map_update(&map, &key, &value);
            uw_destroy(&key);
            uw_destroy(&value);
        }
        uw_map_del(&map, 25);

        TEST(uw_map_length(&map) == 49);
        //uw_dump(stdout, &map);
    }

    {
        // XXX CType leftovers
        UwValue map = UwMap(
            UwCharPtr("let's"),       UwCharPtr("go!"),
            UwNull(),                 UwBool(true),
            UwBool(true),             UwCharPtr("true"),
            UwSigned(-10),            UwBool(false),
            UwSigned('b'),            UwSigned(-42),
            UwUnsigned(100),          UwSigned(-1000000L),
            UwUnsigned(300000000ULL), UwFloat(1.23),
            UwChar8Ptr(u8"สวัสดี"),     UwChar32Ptr(U"สบาย"),
            UwCharPtr("finally"),     UwMap(UwCharPtr("ok"), UwCharPtr("done"))
        );
        TEST(uw_map_length(&map) == 9);
        //uw_dump(stdout, &map);
    }
}

void test_file()
{
    char8_t a[] = u8"###################################################################################################\n";
    char8_t b[] = u8"##############################################################################################\n";
    char8_t c[] = u8"สบาย\n";

    char8_t data_filename[] = u8"./test/data/utf8-crossing-buffer-boundary";

    UwValue file = uw_file_open(data_filename, O_RDONLY, 0);
    UwValue status = uw_start_read_lines(&file);
    TEST(uw_ok(&status));
    UwValue line = uw_create("");
    for (;;) {
        {
            UwValue status = uw_read_line_inplace(&file, &line);
            TEST(uw_ok(&status));
            if (uw_error(&status)) {
                return;
            }
            if (!uw_equal(&line, a)) {
                break;
            }
        }
    }
    TEST(uw_equal(&line, b));
    {
        UwValue status = uw_read_line_inplace(&file, &line);
        TEST(uw_ok(&status));
    }
    TEST(uw_equal(&line, c));
}

void test_string_io()
{
    UwValue sio = uw_create_string_io("one\ntwo\nthree");
    {
        UwValue line = uw_read_line(&sio);
        TEST(uw_equal(&line, "one\n"));
    }
    {
        UwValue line = UwString();
        UwValue status = uw_read_line_inplace(&sio, &line);
        TEST(uw_ok(&status));
        TEST(uw_equal(&line, "two\n"));

        // push back
        {
            UwValue status = uw_unread_line(&sio, &line);
            TEST(uw_ok(&status));
        }
    }
    {
        // read pushed back
        UwValue line = uw_create("");
        UwValue status = uw_read_line_inplace(&sio, &line);
        TEST(uw_ok(&status));
        TEST(uw_equal(&line, "two\n"));
    }
    {
        UWDECL_String(line);
        UwValue status = uw_read_line_inplace(&sio, &line);
        TEST(uw_ok(&status));
        TEST(uw_equal(&line, "three"));
    }
    {
        // EOF
        UWDECL_String(line);
        UwValue status = uw_read_line_inplace(&sio, &line);
        TEST(uw_error(&status));
    }
    // start over again
    {
        UwValue status = uw_start_read_lines(&sio);
        TEST(uw_ok(&status));
        UwValue line = uw_read_line(&sio);
        TEST(uw_equal(&line, "one\n"));
    }
}

int main(int argc, char* argv[])
{
    _uw_default_allocator = _uw_debug_allocator;
    //_uw_allocator_verbose = true;

    printf("Types capacity: %d; interfaces capacity: %d\n", UW_TYPE_CAPACITY, UW_INTERFACE_CAPACITY);

    test_icu();
    test_integral_types();
    test_string();
    test_list();
    test_map();
    test_file();
    test_string_io();

    if (num_fail == 0) {
        printf("%d test%s OK\n", num_tests, (num_tests == 1)? "" : "s");
    }

    printf("leaked blocks: %u\n", _uw_blocks_allocated);
}
