#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include "include/uw_c.h"
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

#define TEST_ALLOCATOR_ID  3

unsigned num_allocated = 0;

static void* test_alloc(unsigned nbytes)
{
    void* result = malloc(nbytes);
//    printf("alloc %u bytes -> %p\n", nbytes, result);
    num_allocated++;
    return result;
}

static void* test_realloc(void* block, unsigned nbytes)
{
    void* result = realloc(block, nbytes);
    if (!block) {
        num_allocated++;
    }
//    printf("realloc %p %u bytes -> %p\n", block, nbytes, result);
    return result;
}

static void test_free(void* block)
{
    free(block);
//    printf("free %p\n", block);
    num_allocated--;
}

static UwAllocator test_allocator = {
    .id      = TEST_ALLOCATOR_ID,
    .alloc   = test_alloc,
    .realloc = test_realloc,
    .free    = test_free
};

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
    TEST(strcmp(uw_get_type_name((int) UwTypeId_Int), "Int") == 0);
    TEST(strcmp(uw_get_type_name((unsigned long long) UwTypeId_Float), "Float") == 0);

    // Null values
    UwValue null_1 = uw_create(nullptr);
    UwValue null_2 = uw_create(nullptr);
    TEST(uw_is_null(null_1));
    TEST(uw_is_null(null_2));

    TEST(strcmp(uw_get_type_name(null_1), "Null") == 0);  // generics test

    // Bool values
    UwValue bool_true  = uw_create(true);
    UwValue bool_false = uw_create(false);
    TEST(uw_is_bool(bool_true));
    TEST(uw_is_bool(bool_false));

    // Int values
    UwValue int_0 = uw_create(0);
    UwValue int_1 = uw_create(1);
    //UwValue int_1_1 = uw_create(1);
    UwValue int_neg1 = uw_create(-1);
    TEST(uw_is_int(int_0));
    TEST(uw_is_int(int_1));
    TEST(uw_is_int(int_neg1));
    //TEST(uw_compare(int_1, int_1_1) == UW_EQ);
    //TEST(uw_compare(int_0, 0) == UW_EQ);
    //TEST(uw_compare(int_1, 1) == UW_EQ);
    TEST(uw_equal(int_0, 0));
    TEST(!uw_equal(int_0, 1));
    TEST(uw_equal(int_1, 1));
    TEST(uw_equal(int_neg1, -1));

    UwValue int_2 = uw_create((char) 2);
    TEST(uw_is_int(int_2));
    TEST(uw_equal(int_2, 2));
    UwValue int_3 = uw_create((unsigned char) 3);
    TEST(uw_is_int(int_3));
    TEST(uw_equal(int_3, 3));
    UwValue int_4 = uw_create((short) 4);
    TEST(uw_is_int(int_4));
    TEST(uw_equal(int_4, 4));
    UwValue int_5 = uw_create((unsigned short) 5);
    TEST(uw_is_int(int_5));
    TEST(uw_equal(int_5, 5));
    UwValue int_6 = uw_create(6U);
    TEST(uw_is_int(int_6));
    TEST(uw_equal(int_6, 6));
    UwValue int_7 = uw_create(7L);
    TEST(uw_equal(int_7, 7));
    TEST(uw_is_int(int_7));
    UwValue int_8 = uw_create(8UL);
    TEST(uw_is_int(int_8));
    TEST(uw_equal(int_8, 8));
    UwValue int_9 = uw_create(9LL);
    TEST(uw_is_int(int_9));
    TEST(uw_equal(int_9, 9));
    UwValue int_10 = uw_create(10ULL);
    TEST(uw_is_int(int_10));
    TEST(uw_equal(int_10, 10));

    // Float values
    UwValue f_0 = uw_create(0.0);
    UwValue f_1 = _uwc_create_float(1.0);
    UwValue f_1_1 = uw_create(1.0);
    UwValue f_neg1 = uw_create(-1.0);
    TEST(uw_is_float(f_0));
    TEST(uw_is_float(f_1));
    TEST(uw_is_float(f_neg1));
    //TEST(uw_compare(f_0, f_0) == UW_EQ);
    TEST(uw_equal  (f_0, f_0));
    //TEST(uw_compare(f_1, f_1) == UW_EQ);
    TEST(uw_equal  (f_1, f_1));
    //TEST(uw_compare(f_1, f_1_1) == UW_EQ);
    TEST(uw_equal  (f_1, f_1_1));
    //TEST(uw_compare(f_0, 0.0) == UW_EQ);
    TEST(uw_equal  (f_0, 0.0));
    TEST(!uw_equal (f_0, 1.0));
    //TEST(uw_compare(f_1, 1.0) == UW_EQ);
    TEST(uw_equal  (f_1, 1.0));
    //TEST(uw_compare(f_neg1, -1.0) == UW_EQ);
    TEST(uw_equal  (f_neg1, -1.0));
    TEST(!uw_equal  (f_neg1, 1.0));

    UwValue f_2 = uw_create(2.0);
    TEST(uw_is_float(f_2));
    TEST(uw_equal(f_2, 2.0));
    UwValue f_3 = uw_create(3.0f);
    TEST(uw_is_float(f_3));
    TEST(uw_equal(f_3, 3.0));
    TEST(uw_equal(f_3, 3.0f));

    // null vs null
    //TEST(uw_compare(null_1, null_2) == UW_EQ);
    //TEST(uw_compare(null_1, nullptr) == UW_EQ);
    TEST(uw_equal  (null_1, null_2));
    TEST(uw_equal  (null_1, nullptr));

    // null vs bool
    //TEST(uw_compare(null_1, bool_true) == UW_NEQ);
    //TEST(uw_compare(null_1, bool_false) == UW_NEQ);
    //TEST(uw_compare(null_1, true) == UW_NEQ);
    //TEST(uw_compare(null_1, false) == UW_NEQ);
    TEST(!uw_equal (null_1, bool_true));
    TEST(!uw_equal (null_1, bool_false));
    TEST(!uw_equal (null_1, true));
    TEST(!uw_equal (null_1, false));

    // null vs int
    //TEST(uw_compare(null_1, int_0) == UW_NEQ);
    TEST(!uw_equal (null_1, int_0));

    //TEST(uw_compare(null_1, int_1) == UW_NEQ);
    TEST(!uw_equal (null_1, int_1));

    //TEST(uw_compare(null_1, int_neg1) == UW_NEQ);
    TEST(!uw_equal (null_1, int_neg1));

    //TEST(uw_compare(null_1, (char) 2) == UW_NEQ);
    TEST(!uw_equal (null_1, (char) 2));
    //TEST(uw_compare(null_1, (unsigned char) 2) == UW_NEQ);
    TEST(!uw_equal (null_1, (unsigned char) 2));
    //TEST(uw_compare(null_1, (short) 2) == UW_NEQ);
    TEST(!uw_equal (null_1, (short) 2));
    //TEST(uw_compare(null_1, (unsigned short) 2) == UW_NEQ);
    TEST(!uw_equal (null_1, (unsigned short) 2));
    //TEST(uw_compare(null_1, 2) == UW_NEQ);
    TEST(!uw_equal (null_1, 2));
    //TEST(uw_compare(null_1, 2U) == UW_NEQ);
    TEST(!uw_equal (null_1, 2U));
    //TEST(uw_compare(null_1, (long) 2) == UW_NEQ);
    TEST(!uw_equal (null_1, (unsigned long) 2));
    //TEST(uw_compare(null_1, (long long) 2) == UW_NEQ);
    TEST(!uw_equal (null_1, (unsigned long long) 2));

    // null vs float
    //TEST(uw_compare(null_1, f_0) == UW_NEQ);
    TEST(!uw_equal (null_1, f_0));

    //TEST(uw_compare(null_1, f_1) == UW_NEQ);
    TEST(!uw_equal (null_1, f_1));

    //TEST(uw_compare(null_1, f_neg1) == UW_NEQ);
    TEST(!uw_equal (null_1, f_neg1));

    //TEST(uw_compare(null_1, 2.0f) == UW_NEQ);
    TEST(!uw_equal (null_1, 2.0f));

    //TEST(uw_compare(null_1, 2.0) == UW_NEQ);
    TEST(!uw_equal (null_1, 2.0));

    // bool vs null
    //TEST(uw_compare(bool_true, null_1) == UW_NEQ);
    TEST(!uw_equal (bool_true, null_1));
    //TEST(uw_compare(bool_false, null_1) == UW_NEQ);
    TEST(!uw_equal (bool_false, null_1));
    //TEST(uw_compare(bool_true, nullptr) == UW_NEQ);
    TEST(!uw_equal (bool_true, nullptr));
    //TEST(uw_compare(bool_false, nullptr) == UW_NEQ);
    TEST(!uw_equal (bool_false, nullptr));

    // bool vs bool
    //TEST(uw_compare(bool_true, true) == UW_EQ);
    //TEST(uw_compare(bool_true, false) == UW_NEQ);
    //TEST(uw_compare(bool_false, false) == UW_EQ);
    //TEST(uw_compare(bool_false, true) == UW_NEQ);
    TEST(uw_equal  (bool_true, true));
    TEST(!uw_equal (bool_true, false));
    TEST(uw_equal  (bool_false, false));
    TEST(!uw_equal (bool_false, true));

    //TEST(uw_compare(bool_false, bool_false) == UW_EQ);
    //TEST(uw_compare(bool_true, bool_false) == UW_NEQ);
    //TEST(uw_compare(bool_false, bool_true) == UW_NEQ);
    TEST(uw_equal  (bool_true, bool_true));
    TEST(uw_equal  (bool_false, bool_false));
    TEST(!uw_equal (bool_true, bool_false));
    TEST(!uw_equal (bool_false, bool_true));

    // bool vs int
    //TEST(uw_compare(bool_true, int_0) == UW_NEQ);
    TEST(!uw_equal (bool_true, int_0));

    //TEST(uw_compare(bool_true, int_1) == UW_NEQ);
    TEST(!uw_equal (bool_true, int_1));

    //TEST(uw_compare(bool_true, int_neg1) == UW_NEQ);
    TEST(!uw_equal (bool_true, int_neg1));

    //TEST(uw_compare(bool_false, int_0) == UW_NEQ);
    TEST(!uw_equal (bool_false, int_0));

    //TEST(uw_compare(bool_false, int_1) == UW_NEQ);
    TEST(!uw_equal (bool_false, int_1));

    //TEST(uw_compare(bool_false, int_neg1) == UW_NEQ);
    TEST(!uw_equal (bool_false, int_neg1));

    //TEST(uw_compare(bool_true, (char) 0) == UW_NEQ);
    TEST(!uw_equal (bool_true, (char) 0));
    //TEST(uw_compare(bool_true, (char) 2) == UW_NEQ);
    TEST(!uw_equal (bool_true, (char) 2));

    //TEST(uw_compare(bool_false, (char) 0) == UW_NEQ);
    TEST(!uw_equal (bool_false, (char) 0));
    //TEST(uw_compare(bool_false, (char) 2) == UW_NEQ);
    TEST(!uw_equal (bool_false, (char) 2));

    //TEST(uw_compare(bool_true, (unsigned char) 0) == UW_NEQ);
    TEST(!uw_equal (bool_true, (unsigned char) 0));
    //TEST(uw_compare(bool_true, (unsigned char) 2) == UW_NEQ);
    TEST(!uw_equal (bool_true, (unsigned char) 2));

    //TEST(uw_compare(bool_false, (unsigned char) 0) == UW_NEQ);
    TEST(!uw_equal (bool_false, (unsigned char) 0));
    //TEST(uw_compare(bool_false, (unsigned char) 2) == UW_NEQ);
    TEST(!uw_equal (bool_false, (unsigned char) 2));

    //TEST(uw_compare(bool_true, (short) 0) == UW_NEQ);
    TEST(!uw_equal (bool_true, (short) 0));
    //TEST(uw_compare(bool_true, (short) 2) == UW_NEQ);
    TEST(!uw_equal (bool_true, (short) 2));

    //TEST(uw_compare(bool_false, (short) 0) == UW_NEQ);
    TEST(!uw_equal (bool_false, (short) 0));
    //TEST(uw_compare(bool_false, (short) 2) == UW_NEQ);
    TEST(!uw_equal (bool_false, (short) 2));

    //TEST(uw_compare(bool_true, (unsigned short) 0) == UW_NEQ);
    TEST(!uw_equal (bool_true, (unsigned short) 0));
    //TEST(uw_compare(bool_true, (unsigned short) 2) == UW_NEQ);
    TEST(!uw_equal (bool_true, (unsigned short) 2));

    //TEST(uw_compare(bool_false, (unsigned short) 0) == UW_NEQ);
    TEST(!uw_equal (bool_false, (unsigned short) 0));
    //TEST(uw_compare(bool_false, (unsigned short) 2) == UW_NEQ);
    TEST(!uw_equal (bool_false, (unsigned short) 2));

    //TEST(uw_compare(bool_true, 0) == UW_NEQ);
    TEST(!uw_equal (bool_true, 0));
    //TEST(uw_compare(bool_true, 2) == UW_NEQ);
    TEST(!uw_equal (bool_true, 2));

    //TEST(uw_compare(bool_false, 0) == UW_NEQ);
    TEST(!uw_equal (bool_false, 0));
    //TEST(uw_compare(bool_false, 2) == UW_NEQ);
    TEST(!uw_equal (bool_false, 2));

    //TEST(uw_compare(bool_true, 0U) == UW_NEQ);
    TEST(!uw_equal (bool_true, 0U));
    //TEST(uw_compare(bool_true, 2U) == UW_NEQ);
    TEST(!uw_equal (bool_true, 2U));

    //TEST(uw_compare(bool_false, 0U) == UW_NEQ);
    TEST(!uw_equal (bool_false, 0U));
    //TEST(uw_compare(bool_false, 2U) == UW_NEQ);
    TEST(!uw_equal (bool_false, 2U));

    //TEST(uw_compare(bool_true, 0L) == UW_NEQ);
    TEST(!uw_equal (bool_true, 0L));
    //TEST(uw_compare(bool_true, 2L) == UW_NEQ);
    TEST(!uw_equal (bool_true, 2L));

    //TEST(uw_compare(bool_false, 0L) == UW_NEQ);
    TEST(!uw_equal (bool_false, 0L));
    //TEST(uw_compare(bool_false, 2L) == UW_NEQ);
    TEST(!uw_equal (bool_false, 2L));

    //TEST(uw_compare(bool_true, 0UL) == UW_NEQ);
    TEST(!uw_equal(bool_true, 0UL));
    //TEST(uw_compare(bool_true, 2UL) == UW_NEQ);
    TEST(!uw_equal(bool_true, 2UL));

    //TEST(uw_compare(bool_false, 0UL) == UW_NEQ);
    TEST(!uw_equal  (bool_false, 0UL));
    //TEST(uw_compare(bool_false, 2UL) == UW_NEQ);
    TEST(!uw_equal (bool_false, 2UL));

    //TEST(uw_compare(bool_true, 0LL) == UW_NEQ);
    TEST(!uw_equal (bool_true, 0LL));
    //TEST(uw_compare(bool_true, 2LL) == UW_NEQ);
    TEST(!uw_equal  (bool_true, 2LL));

    //TEST(uw_compare(bool_false, 0LL) == UW_NEQ);
    TEST(!uw_equal  (bool_false, 0LL));
    //TEST(uw_compare(bool_false, 2LL) == UW_NEQ);
    TEST(!uw_equal (bool_false, 2LL));

    //TEST(uw_compare(bool_true, 0ULL) == UW_NEQ);
    TEST(!uw_equal (bool_true, 0ULL));
    //TEST(uw_compare(bool_true, 2ULL) == UW_NEQ);
    TEST(!uw_equal  (bool_true, 2ULL));

    //TEST(uw_compare(bool_false, 0ULL) == UW_NEQ);
    TEST(!uw_equal  (bool_false, 0ULL));
    //TEST(uw_compare(bool_false, 2ULL) == UW_NEQ);
    TEST(!uw_equal (bool_false, 2ULL));

    // bool vs float
    //TEST(uw_compare(bool_true, f_0) == UW_NEQ);
    TEST(!uw_equal (bool_true, f_0));

    //TEST(uw_compare(bool_true, f_1) == UW_NEQ);
    TEST(!uw_equal (bool_true, f_1));

    //TEST(uw_compare(bool_true, f_neg1) == UW_NEQ);
    TEST(!uw_equal (bool_true, f_neg1));

    //TEST(uw_compare(bool_false, f_0) == UW_NEQ);
    TEST(!uw_equal (bool_false, f_0));

    //TEST(uw_compare(bool_false, f_1) == UW_NEQ);
    TEST(!uw_equal (bool_false, f_1));

    //TEST(uw_compare(bool_false, f_neg1) == UW_NEQ);
    TEST(!uw_equal (bool_false, f_neg1));

    //TEST(uw_compare(bool_true, 0.0f) == UW_NEQ);
    TEST(!uw_equal (bool_true, 0.0f));
    //TEST(uw_compare(bool_true, 2.0f) == UW_NEQ);
    TEST(!uw_equal (bool_true, 2.0f));

    //TEST(uw_compare(bool_false, 0.0f) == UW_NEQ);
    TEST(!uw_equal (bool_false, 0.0f));
    //TEST(uw_compare(bool_false, 2.0f) == UW_NEQ);
    TEST(!uw_equal (bool_false, 2.0f));

    //TEST(uw_compare(bool_true, 0.0f) == UW_NEQ);
    TEST(!uw_equal (bool_true, 0.0f));
    //TEST(uw_compare(bool_true, 2.0f) == UW_NEQ);
    TEST(!uw_equal (bool_true, 2.0f));

    //TEST(uw_compare(bool_false, 0.0f) == UW_NEQ);
    TEST(!uw_equal (bool_false, 0.0f));
    //TEST(uw_compare(bool_false, 2.0f) == UW_NEQ);
    TEST(!uw_equal (bool_false, 2.0f));

    //TEST(uw_compare(bool_true, 0.0) == UW_NEQ);
    TEST(!uw_equal (bool_true, 0.0));
    //TEST(uw_compare(bool_true, 2.0) == UW_NEQ);
    TEST(!uw_equal (bool_true, 2.0));

    //TEST(uw_compare(bool_false, 0.0) == UW_NEQ);
    TEST(!uw_equal (bool_false, 0.0));
    //TEST(uw_compare(bool_false, 2.0) == UW_NEQ);
    TEST(!uw_equal (bool_false, 2.0));

    //TEST(uw_compare(bool_true, 0.0) == UW_NEQ);
    TEST(!uw_equal (bool_true, 0.0));
    //TEST(uw_compare(bool_true, 2.0) == UW_NEQ);
    TEST(!uw_equal (bool_true, 2.0));

    //TEST(uw_compare(bool_false, 0.0) == UW_NEQ);
    TEST(!uw_equal (bool_false, 0.0));
    //TEST(uw_compare(bool_false, 2.0) == UW_NEQ);
    TEST(!uw_equal (bool_false, 2.0));

    // int vs null
    //TEST(uw_compare(int_0, null_1) == UW_NEQ);
    TEST(!uw_equal (int_0, null_1));
    //TEST(uw_compare(int_0, nullptr) == UW_NEQ);
    TEST(!uw_equal (int_0, nullptr));

    //TEST(uw_compare(int_1, null_1) == UW_NEQ);
    TEST(!uw_equal (int_1, null_1));

    //TEST(uw_compare(int_neg1, null_1) == UW_NEQ);
    TEST(!uw_equal (int_neg1, null_1));

    // int vs bool
    //TEST(uw_compare(int_0, bool_true) == UW_NEQ);
    TEST(!uw_equal (int_0, bool_true));
    //TEST(uw_compare(int_0, bool_false) == UW_NEQ);
    TEST(!uw_equal (int_0, bool_false));
    //TEST(uw_compare(int_0, true) == UW_NEQ);
    TEST(!uw_equal (int_0, true));
    //TEST(uw_compare(int_0, false) == UW_NEQ);
    TEST(!uw_equal (int_0, false));

    //TEST(uw_compare(int_1, bool_true) == UW_NEQ);
    TEST(!uw_equal (int_1, bool_true));
    //TEST(uw_compare(int_1, bool_false) == UW_NEQ);
    TEST(!uw_equal (int_1, bool_false));
    //TEST(uw_compare(int_1, true) == UW_NEQ);
    TEST(!uw_equal (int_1, true));
    //TEST(uw_compare(int_1, false) == UW_NEQ);
    TEST(!uw_equal (int_1, false));

    //TEST(uw_compare(int_neg1, bool_true) == UW_NEQ);
    TEST(!uw_equal (int_neg1, bool_true));
    //TEST(uw_compare(int_neg1, bool_false) == UW_NEQ);
    TEST(!uw_equal (int_neg1, bool_false));
    //TEST(uw_compare(int_neg1, true) == UW_NEQ);
    TEST(!uw_equal (int_neg1, true));
    //TEST(uw_compare(int_neg1, false) == UW_NEQ);
    TEST(!uw_equal (int_neg1, false));

    // int vs int
    //TEST(uw_compare(int_0, int_0) == UW_EQ);
    TEST(uw_equal  (int_0, int_0));
    //TEST(uw_compare(int_1, int_1) == UW_EQ);
    TEST(uw_equal  (int_1, int_1));
    //TEST(uw_compare(int_neg1, int_neg1) == UW_EQ);
    TEST(uw_equal  (int_neg1, int_neg1));

    //TEST(uw_compare(int_0, int_1) == UW_LESS);
    TEST(!uw_equal (int_0, int_1));
    //TEST(uw_compare(int_1, int_0) == UW_GREATER);
    TEST(!uw_equal (int_1, int_0));
    //TEST(uw_compare(int_neg1, int_0) == UW_LESS);
    TEST(!uw_equal (int_neg1, int_0));
    //TEST(uw_compare(int_0, int_neg1) == UW_GREATER);
    TEST(!uw_equal (int_0, int_neg1));

    //TEST(uw_compare(int_1, (char) 1) == UW_EQ);
    TEST(uw_equal  (int_1, (char) 1));
    //TEST(uw_compare(int_1, (char) 2) == UW_LESS);
    TEST(!uw_equal (int_1, (char) 2));
    //TEST(uw_compare(int_1, (char) -1) == UW_GREATER);
    TEST(!uw_equal (int_1, (char) -1));

    //TEST(uw_compare(int_1, (unsigned char) 1) == UW_EQ);
    TEST(uw_equal  (int_1, (unsigned char) 1));
    //TEST(uw_compare(int_1, (unsigned char) 2) == UW_LESS);
    TEST(!uw_equal (int_1, (unsigned char) 2));
    //TEST(uw_compare(int_1, (unsigned char) 0) == UW_GREATER);
    TEST(!uw_equal (int_1, (unsigned char) 0));

    //TEST(uw_compare(int_1, (short) 1) == UW_EQ);
    TEST(uw_equal  (int_1, (short) 1));
    //TEST(uw_compare(int_1, (short) 2) == UW_LESS);
    TEST(!uw_equal (int_1, (short) 2));
    //TEST(uw_compare(int_1, (short) -1) == UW_GREATER);
    TEST(!uw_equal (int_1, (short) -1));

    //TEST(uw_compare(int_1, (unsigned short) 1) == UW_EQ);
    TEST(uw_equal  (int_1, (unsigned short) 1));
    //TEST(uw_compare(int_1, (unsigned short) 2) == UW_LESS);
    TEST(!uw_equal (int_1, (unsigned short) 2));
    //TEST(uw_compare(int_1, (unsigned short) 0) == UW_GREATER);
    TEST(!uw_equal (int_1, (unsigned short) 0));

    //TEST(uw_compare(int_1, 1) == UW_EQ);
    TEST(uw_equal  (int_1, 1));
    //TEST(uw_compare(int_1, 2) == UW_LESS);
    TEST(!uw_equal (int_1, 2));
    //TEST(uw_compare(int_1, -1) == UW_GREATER);
    TEST(!uw_equal (int_1, -1));

    //TEST(uw_compare(int_1, 1U) == UW_EQ);
    TEST(uw_equal  (int_1, 1U));
    //TEST(uw_compare(int_1, 2U) == UW_LESS);
    TEST(!uw_equal (int_1, 2U));
    //TEST(uw_compare(int_1, 0U) == UW_GREATER);
    TEST(!uw_equal (int_1, 0U));

    //TEST(uw_compare(int_1, 1L) == UW_EQ);
    TEST(uw_equal  (int_1, 1L));
    //TEST(uw_compare(int_1, 2L) == UW_LESS);
    TEST(!uw_equal (int_1, 2L));
    //TEST(uw_compare(int_1, -1L) == UW_GREATER);
    TEST(!uw_equal (int_1, -1L));

    //TEST(uw_compare(int_1, 1UL) == UW_EQ);
    TEST(uw_equal  (int_1, 1UL));
    //TEST(uw_compare(int_1, 2UL) == UW_LESS);
    TEST(!uw_equal (int_1, 2UL));
    //TEST(uw_compare(int_1, 0UL) == UW_GREATER);
    TEST(!uw_equal (int_1, 0UL));

    //TEST(uw_compare(int_1, 1LL) == UW_EQ);
    TEST(uw_equal  (int_1, 1LL));
    //TEST(uw_compare(int_1, 2LL) == UW_LESS);
    TEST(!uw_equal (int_1, 2LL));
    //TEST(uw_compare(int_1, -1LL) == UW_GREATER);
    TEST(!uw_equal (int_1, -1LL));

    //TEST(uw_compare(int_1, 1ULL) == UW_EQ);
    TEST(uw_equal  (int_1, 1ULL));
    //TEST(uw_compare(int_1, 2ULL) == UW_LESS);
    TEST(!uw_equal (int_1, 2ULL));
    //TEST(uw_compare(int_1, 0ULL) == UW_GREATER);
    TEST(!uw_equal (int_1, 0ULL));

    // int vs float
    //TEST(uw_compare(int_0, f_0) == UW_EQ);
    TEST(uw_equal  (int_0, f_0));
    //TEST(uw_compare(int_1, f_1) == UW_EQ);
    TEST(uw_equal  (int_1, f_1));
    //TEST(uw_compare(int_neg1, f_neg1) == UW_EQ);
    TEST(uw_equal  (int_neg1, f_neg1));

    //TEST(uw_compare(int_0, f_1) == UW_LESS);
    TEST(!uw_equal (int_0, f_1));
    //TEST(uw_compare(int_1, f_0) == UW_GREATER);
    TEST(!uw_equal (int_1, f_0));
    //TEST(uw_compare(int_neg1, f_0) == UW_LESS);
    TEST(!uw_equal (int_neg1, f_0));
    //TEST(uw_compare(int_0, f_neg1) == UW_GREATER);
    TEST(!uw_equal (int_0, f_neg1));

    //TEST(uw_compare(int_1, 1.0) == UW_EQ);
    TEST(uw_equal  (int_1, 1.0));
    //TEST(uw_compare(int_1, 2.0) == UW_LESS);
    TEST(!uw_equal (int_1, 2.0));
    //TEST(uw_compare(int_1, -1.0) == UW_GREATER);
    TEST(!uw_equal (int_1, -1.0));

    //TEST(uw_compare(int_1, 1.0f) == UW_EQ);
    TEST(uw_equal  (int_1, 1.0f));
    //TEST(uw_compare(int_1, 2.0f) == UW_LESS);
    TEST(!uw_equal (int_1, 2.0f));
    //TEST(uw_compare(int_1, -1.0f) == UW_GREATER);
    TEST(!uw_equal (int_1, -1.0f));

    // float vs null
    //TEST(uw_compare(f_0, null_1) == UW_NEQ);
    TEST(!uw_equal (f_0, null_1));
    //TEST(uw_compare(f_0, nullptr) == UW_NEQ);
    TEST(!uw_equal (f_0, nullptr));

    //TEST(uw_compare(f_1, null_1) == UW_NEQ);
    TEST(!uw_equal (f_1, null_1));

    //TEST(uw_compare(f_neg1, null_1) == UW_NEQ);
    TEST(!uw_equal (f_neg1, null_1));

    // float vs bool
    //TEST(uw_compare(f_0, bool_true) == UW_NEQ);
    TEST(!uw_equal (f_0, bool_true));
    //TEST(uw_compare(f_0, bool_false) == UW_NEQ);
    TEST(!uw_equal  (f_0, bool_false));
    //TEST(uw_compare(f_0, true) == UW_NEQ);
    TEST(!uw_equal (f_0, true));
    //TEST(uw_compare(f_0, false) == UW_NEQ);
    TEST(!uw_equal (f_0, false));

    //TEST(uw_compare(f_1, bool_true) == UW_NEQ);
    TEST(!uw_equal (f_1, bool_true));
    //TEST(uw_compare(f_1, bool_false) == UW_NEQ);
    TEST(!uw_equal (f_1, bool_false));
    //TEST(uw_compare(f_1, true) == UW_NEQ);
    TEST(!uw_equal (f_1, true));
    //TEST(uw_compare(f_1, false) == UW_NEQ);
    TEST(!uw_equal (f_1, false));

    //TEST(uw_compare(f_neg1, bool_true) == UW_NEQ);
    TEST(!uw_equal (f_neg1, bool_true));
    //TEST(uw_compare(f_neg1, bool_false) == UW_NEQ);
    TEST(!uw_equal (f_neg1, bool_false));
    //TEST(uw_compare(f_neg1, true) == UW_NEQ);
    TEST(!uw_equal (f_neg1, true));
    //TEST(uw_compare(f_neg1, false) == UW_NEQ);
    TEST(!uw_equal (f_neg1, false));

    // float vs int
    //TEST(uw_compare(f_0, int_0) == UW_EQ);
    TEST(uw_equal  (f_0, int_0));
    //TEST(uw_compare(f_1, int_1) == UW_EQ);
    TEST(uw_equal  (f_1, int_1));
    //TEST(uw_compare(f_neg1, int_neg1) == UW_EQ);
    TEST(uw_equal  (f_neg1, int_neg1));

    //TEST(uw_compare(f_0, int_1) == UW_LESS);
    TEST(!uw_equal (f_0, int_1));
    //TEST(uw_compare(f_1, int_0) == UW_GREATER);
    TEST(!uw_equal (f_1, int_0));
    //TEST(uw_compare(f_neg1, int_0) == UW_LESS);
    TEST(!uw_equal (f_neg1, int_0));
    //TEST(uw_compare(f_0, int_neg1) == UW_GREATER);
    TEST(!uw_equal (f_0, int_neg1));

    //TEST(uw_compare(f_1, (char) 1) == UW_EQ);
    TEST(uw_equal  (f_1, (char) 1));
    //TEST(uw_compare(f_1, (char) 2) == UW_LESS);
    TEST(!uw_equal (f_1, (char) 2));
    //TEST(uw_compare(f_1, (char) -1) == UW_GREATER);
    TEST(!uw_equal (f_1, (char) -1));

    //TEST(uw_compare(f_1, (unsigned char) 1) == UW_EQ);
    TEST(uw_equal  (f_1, (unsigned char) 1));
    //TEST(uw_compare(f_1, (unsigned char) 2) == UW_LESS);
    TEST(!uw_equal (f_1, (unsigned char) 2));
    //TEST(uw_compare(f_1, (unsigned char) 0) == UW_GREATER);
    TEST(!uw_equal (f_1, (unsigned char) 0));

    //TEST(uw_compare(f_1, (short) 1) == UW_EQ);
    TEST(uw_equal  (f_1, (short) 1));
    //TEST(uw_compare(f_1, (short) 2) == UW_LESS);
    TEST(!uw_equal (f_1, (short) 2));
    //TEST(uw_compare(f_1, (short) -1) == UW_GREATER);
    TEST(!uw_equal (f_1, (short) -1));

    //TEST(uw_compare(f_1, (unsigned short) 1) == UW_EQ);
    TEST(uw_equal  (f_1, (unsigned short) 1));
    //TEST(uw_compare(f_1, (unsigned short) 2) == UW_LESS);
    TEST(!uw_equal (f_1, (unsigned short) 2));
    //TEST(uw_compare(f_1, (unsigned short) 0) == UW_GREATER);
    TEST(!uw_equal (f_1, (unsigned short) 0));

    //TEST(uw_compare(f_1, 1) == UW_EQ);
    TEST(uw_equal  (f_1, 1));
    //TEST(uw_compare(f_1, 2) == UW_LESS);
    TEST(!uw_equal (f_1, 2));
    //TEST(uw_compare(f_1, -1) == UW_GREATER);
    TEST(!uw_equal (f_1, -1));

    //TEST(uw_compare(f_1, 1U) == UW_EQ);
    TEST(uw_equal  (f_1, 1U));
    //TEST(uw_compare(f_1, 2U) == UW_LESS);
    TEST(!uw_equal (f_1, 2U));
    //TEST(uw_compare(f_1, 0U) == UW_GREATER);
    TEST(!uw_equal (f_1, 0U));

    //TEST(uw_compare(f_1, 1L) == UW_EQ);
    TEST(uw_equal  (f_1, 1L));
    //TEST(uw_compare(f_1, 2L) == UW_LESS);
    TEST(!uw_equal (f_1, 2L));
    //TEST(uw_compare(f_1, -1L) == UW_GREATER);
    TEST(!uw_equal (f_1, -1L));

    //TEST(uw_compare(f_1, 1UL) == UW_EQ);
    TEST(uw_equal  (f_1, 1UL));
    //TEST(uw_compare(f_1, 2UL) == UW_LESS);
    TEST(!uw_equal (f_1, 2UL));
    //TEST(uw_compare(f_1, 0UL) == UW_GREATER);
    TEST(!uw_equal (f_1, 0UL));

    //TEST(uw_compare(f_1, 1LL) == UW_EQ);
    TEST(uw_equal  (f_1, 1LL));
    //TEST(uw_compare(f_1, 2LL) == UW_LESS);
    TEST(!uw_equal (f_1, 2LL));
    //TEST(uw_compare(f_1, -1LL) == UW_GREATER);
    TEST(!uw_equal (f_1, -1LL));

    //TEST(uw_compare(f_1, 1ULL) == UW_EQ);
    TEST(uw_equal  (f_1, 1ULL));
    //TEST(uw_compare(f_1, 2ULL) == UW_LESS);
    TEST(!uw_equal (f_1, 2ULL));
    //TEST(uw_compare(f_1, 0ULL) == UW_GREATER);
    TEST(!uw_equal (f_1, 0ULL));

    // float vs float
    //TEST(uw_compare(f_0, f_1) == UW_LESS);
    TEST(!uw_equal (f_0, f_1));
    //TEST(uw_compare(f_1, f_0) == UW_GREATER);
    TEST(!uw_equal (f_1, f_0));
    //TEST(uw_compare(f_neg1, f_0) == UW_LESS);
    TEST(!uw_equal (f_neg1, f_0));
    //TEST(uw_compare(f_0, f_neg1) == UW_GREATER);
    TEST(!uw_equal (f_0, f_neg1));

    //TEST(uw_compare(f_1, 1.0) == UW_EQ);
    TEST(uw_equal  (f_1, 1.0));
    //TEST(uw_compare(f_1, 2.0) == UW_LESS);
    TEST(!uw_equal (f_1, 2.0));
    //TEST(uw_compare(f_1, -1.0) == UW_GREATER);
    TEST(!uw_equal (f_1, -1.0));

    //TEST(uw_compare(f_1, 1.0f) == UW_EQ);
    TEST(uw_equal  (f_1, 1.0f));
    //TEST(uw_compare(f_1, 2.0f) == UW_LESS);
    TEST(!uw_equal (f_1, 2.0f));
    //TEST(uw_compare(f_1, -1.0f) == UW_GREATER);
    TEST(!uw_equal (f_1, -1.0f));
}

void test_string()
{
    struct _UwString* s;
    CapMethods* capmeth;

    { // testing allocation
        UwValue v = uw_create_empty_string(253, 1);
        s = *_uw_get_string_pptr(v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 253);
        TEST(s->char_size == 0);
        TEST(s->cap_size == 0);
        TEST(s->block_count == 7);
        //uw_dump(v);
    }
    { // testing allocation
        UwValue v = uw_create_empty_string(254, 1);
        s = *_uw_get_string_pptr(v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 258);
        TEST(s->char_size == 0);
        TEST(s->cap_size == 1);
        TEST(s->block_count == 7);
        //uw_dump(v);
    }
    { // testing cap_size=1 char_size=1
        UwValue v = uw_create_empty_string(0, 1);
        s = *_uw_get_string_pptr(v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 5);
        TEST(s->char_size == 0);
        TEST(s->cap_size == 0);
        TEST(s->block_count == 0);
        //uw_dump(v);

        uw_string_append(v, "hello");

        s = *_uw_get_string_pptr(v);
        capmeth = get_cap_methods(s);

        TEST(capmeth->get_length(s) == 5);
        TEST(capmeth->get_capacity(s) == 5);
        TEST(s->block_count == 0);
        //uw_dump(v);

        uw_string_append(v, '!');

        s = *_uw_get_string_pptr(v);
        capmeth = get_cap_methods(s);

        TEST(capmeth->get_length(s) == 6);
        TEST(capmeth->get_capacity(s) == 13);
        TEST(s->block_count == 1);
        //uw_dump(s_1_1);

        // increase capacity to 2 bytes
        for (int i = 0; i < 250; i++) {
            uw_string_append_char(v, ' ');
        }
        s = *_uw_get_string_pptr(v);
        capmeth = get_cap_methods(s);

        TEST(capmeth->get_length(s) == 256);
        TEST(capmeth->get_capacity(s) == 258);
        TEST(s->char_size == 0);
        TEST(s->cap_size == 1);
        TEST(s->block_count == 7);
        //uw_dump(v);

        uw_string_append(v, "everybody");
        uw_string_erase(v, 5, 255);
        TEST(uw_equal(v, "hello everybody"));
        TEST(!uw_equal(v, ""));

        // test comparison
        UwValue v2 = uw_create("hello everybody");
        TEST(uw_equal(v, v2));
        TEST(uw_equal_cstr(v, "hello everybody"));
        TEST(uw_equal_cstr(v2, "hello everybody"));
        TEST(!uw_equal_cstr(v, "hello Everybody"));
        TEST(!uw_equal_cstr(v2, "hello Everybody"));
        TEST(uw_equal(v, "hello everybody"));
        TEST(uw_equal(v2, "hello everybody"));
        TEST(!uw_equal(v, "hello Everybody"));
        TEST(!uw_equal(v2, "hello Everybody"));
        TEST(uw_equal(v, u8"hello everybody"));
        TEST(uw_equal(v2, u8"hello everybody"));
        TEST(!uw_equal(v, u8"hello Everybody"));
        TEST(!uw_equal(v2, u8"hello Everybody"));
        TEST(uw_equal(v, U"hello everybody"));
        TEST(uw_equal(v2, U"hello everybody"));
        TEST(!uw_equal(v, U"hello Everybody"));
        TEST(!uw_equal(v2, U"hello Everybody"));
#       ifdef DEBUG
            TEST(!uw_eq_fast(*_uw_get_string_pptr(v), *_uw_get_string_pptr(v2)));  // not equal because capacity is different
#       endif
        UwValue v3 = uw_create("hello everybody");
#       ifdef DEBUG
            TEST(uw_eq_fast(*_uw_get_string_pptr(v2), *_uw_get_string_pptr(v3)));
#       endif

        // test C string
        UW_CSTRING_LOCAL(cv3, v3);
        TEST(strcmp(cv3, "hello everybody") == 0);

        // test substring
        TEST(uw_substring_eq_cstr(v, 4, 7, "o e"));
        TEST(!uw_substring_eq_cstr(v, 4, 7, ""));
        TEST(uw_substring_eq_cstr(v, 0, 4, "hell"));
        TEST(uw_substring_eq_cstr(v, 11, 100, "body"));
        TEST(uw_substring_eq(v, 4, 7, "o e"));
        TEST(!uw_substring_eq(v, 4, 7, ""));
        TEST(uw_substring_eq(v, 0, 4, "hell"));
        TEST(uw_substring_eq(v, 11, 100, "body"));
        TEST(uw_substring_eq(v, 4, 7, u8"o e"));
        TEST(!uw_substring_eq(v, 4, 7, u8""));
        TEST(uw_substring_eq(v, 0, 4, u8"hell"));
        TEST(uw_substring_eq(v, 11, 100, u8"body"));
        TEST(uw_substring_eq(v, 4, 7, U"o e"));
        TEST(!uw_substring_eq(v, 4, 7, U""));
        TEST(uw_substring_eq(v, 0, 4, U"hell"));
        TEST(uw_substring_eq(v, 11, 100, U"body"));

        // test erase and truncate
        uw_string_erase(v, 4, 255);
        TEST(uw_equal(v, "hell"));

        uw_string_erase(v, 0, 2);
        TEST(uw_equal(v, "ll"));

        uw_string_truncate(v, 0);
        TEST(uw_equal(v, ""));

        s = *_uw_get_string_pptr(v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 266);
        //uw_dump(v);

        // test append substring
        uw_string_append_substring(v, "0123456789", 3, 7);
        TEST(uw_equal(v, "3456"));
        uw_string_append_substring(v, u8"0123456789", 3, 7);
        TEST(uw_equal(v, "34563456"));
        uw_string_append_substring(v, U"0123456789", 3, 7);
        TEST(uw_equal(v, "345634563456"));
        uw_string_truncate(v, 0);

        // change char size to 2-byte by appending wider chars
        uw_string_append(v, u8"สวัสดี");

        s = *_uw_get_string_pptr(v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 6);
        TEST(capmeth->get_capacity(s) == 134);  // memsize remains the same and capacity decreases
        TEST(s->char_size == 1);
        TEST(s->cap_size == 0);
        TEST(uw_equal(v, u8"สวัสดี"));
        //uw_dump(v);
    }

#   ifdef DEBUG
    { // testing cap_size=2 char_size=1
        UwValue v = uw_create_empty_string2(2, 1);
        s = *_uw_get_string_pptr(v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 2);
        TEST(s->char_size == 0);
        TEST(s->cap_size == 1);
        TEST(s->block_count == 0);
        //uw_dump(v);
    }

    { // testing cap_size=4 char_size=1
        UwValue v = uw_create_empty_string2(4, 1);
        s = *_uw_get_string_pptr(v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 4);
        TEST(s->char_size == 0);
        TEST(s->cap_size == 3);
        TEST(s->block_count == 1);
        //uw_dump(v);
    }

#   if UINT_WIDTH > 32
    { // testing cap_size=8 char_size=1
        UwValue v = uw_create_empty_string2(8, 1);
        s = *_uw_get_string_pptr(v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 8);
        TEST(s->char_size == 0);
        TEST(s->cap_size == 7);
        TEST(s->block_count == 3);
        //uw_dump(v);
    }
#   endif
#   endif

    { // testing cap_size=1 char_size=2
        UwValue v = uw_create_empty_string(0, 2);
        s = *_uw_get_string_pptr(v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 2);
        TEST(s->char_size == 1);
        TEST(s->cap_size == 0);
        TEST(s->block_count == 0);
        //uw_dump(v);

        uw_string_append(v, u8"สบาย");

        TEST(capmeth->get_length(s) == 4);
        TEST(capmeth->get_capacity(s) == 6);
        TEST(s->block_count == 1);
        //uw_dump(v);

        uw_string_append(v, 0x0e14);
        uw_string_append(v, 0x0e35);

        TEST(capmeth->get_length(s) == 6);
        TEST(capmeth->get_capacity(s) == 6);
        TEST(s->block_count == 1);
        TEST(uw_equal(v, u8"สบายดี"));
        //uw_dump(v);

        // test truncate
        uw_string_truncate(v, 4);
        TEST(uw_equal(v, u8"สบาย"));
        TEST(!uw_equal(v, ""));

        // increase capacity to 2 bytes
        for (int i = 0; i < 251; i++) {
            uw_string_append(v, ' ');
        }
        s = *_uw_get_string_pptr(v);
        capmeth = get_cap_methods(s);

        TEST(capmeth->get_length(s) == 255);
        TEST(capmeth->get_capacity(s) == 257);
        TEST(s->char_size == 1);
        TEST(s->cap_size == 1);
        TEST(s->block_count == 7);
        //uw_dump(v);

        uw_string_append(v, U"สบาย");
        uw_string_erase(v, 4, 255);
        TEST(uw_equal(v, u8"สบายสบาย"));
        TEST(!uw_equal(v, ""));

        // test comparison
        UwValue v2 = uw_create(u8"สบายสบาย");
        TEST(uw_equal(v, v2));
        TEST(uw_equal(v, u8"สบายสบาย"));
        TEST(uw_equal(v2, u8"สบายสบาย"));
        TEST(!uw_equal(v, u8"ความสบาย"));
        TEST(!uw_equal(v2, u8"ความสบาย"));
        TEST(uw_equal(v, U"สบายสบาย"));
        TEST(uw_equal(v2, U"สบายสบาย"));
        TEST(!uw_equal(v, U"ความสบาย"));
        TEST(!uw_equal(v2, U"ความสบาย"));
#       ifdef DEBUG
            TEST(!uw_eq_fast(*_uw_get_string_pptr(v), *_uw_get_string_pptr(v2)));  // not equal because capacity is different
#       endif
#       ifdef DEBUG
            UwValue v3 = uw_create("สบายสบาย");
            TEST(uw_eq_fast(*_uw_get_string_pptr(v2), *_uw_get_string_pptr(v3)));
#       endif

        // test substring
        TEST(uw_substring_eq(v, 3, 5, u8"ยส"));
        TEST(!uw_substring_eq(v, 3, 5, u8""));
        TEST(uw_substring_eq(v, 0, 3, u8"สบา"));
        TEST(uw_substring_eq(v, 6, 100, u8"าย"));
        TEST(uw_substring_eq(v, 3, 5, U"ยส"));
        TEST(!uw_substring_eq(v, 3, 5, U""));
        TEST(uw_substring_eq(v, 0, 3, U"สบา"));
        TEST(uw_substring_eq(v, 6, 100, U"าย"));

        // test erase and truncate
        uw_string_erase(v, 4, 255);
        TEST(uw_equal(v, u8"สบาย"));

        uw_string_erase(v2, 0, 4);
        TEST(uw_equal(v, u8"สบาย"));

        uw_string_truncate(v, 0);
        TEST(uw_equal(v, ""));

        // test append substring
        uw_string_append_substring(v, u8"สบายสบาย", 1, 4);
        TEST(uw_equal(v, u8"บาย"));
        uw_string_append_substring(v, U"สบายสบาย", 1, 4);
        TEST(uw_equal(v, U"บายบาย"));
        uw_string_truncate(v, 0);

        s = *_uw_get_string_pptr(v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 261);
        //uw_dump(v);

//        // change char size to 2-byte by appending wider chars
//        uw_string_append(v, u8"สวัสดี");
//
//        s = *_uw_get_string_pptr(v);
//        capmeth = get_cap_methods(s);
//        TEST(capmeth->get_length(s) == 6);
//        TEST(capmeth->get_capacity(s) == 134);  // memsize remains the same and capacity decreases
//        TEST(s->char_size == 1);
//        TEST(s->cap_size == 0);
//        TEST(uw_equal(v, u8"สวัสดี"));
//        //uw_dump(v);
    }

#   ifdef DEBUG
    { // testing cap_size=2 char_size=2
        UwValue v = uw_create_empty_string2(2, 2);
        s = *_uw_get_string_pptr(v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 1);
        TEST(s->char_size == 1);
        TEST(s->cap_size == 1);
        TEST(s->block_count == 0);
        //uw_dump(v);
    }

    { // testing cap_size=4 char_size=2
        UwValue v = uw_create_empty_string2(4, 2);
        s = *_uw_get_string_pptr(v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 2);
        TEST(s->char_size == 1);
        TEST(s->cap_size == 3);
        TEST(s->block_count == 1);
        //uw_dump(v);
    }

#   if UINT_WIDTH > 32
    { // testing cap_size=8 char_size=2
        UwValue v = uw_create_empty_string2(8, 2);
        //uw_dump(s_8_2);
        s = *_uw_get_string_pptr(v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 4);
        TEST(s->char_size == 1);
        TEST(s->cap_size == 7);
        TEST(s->block_count == 3);
        //uw_dump(v);
    }
#   endif
#   endif

    { // testing cap_size=1 char_size=3
        UwValue v = uw_create_empty_string(0, 3);
        s = *_uw_get_string_pptr(v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 1);
        TEST(s->char_size == 2);
        TEST(s->cap_size == 0);
        TEST(s->block_count == 0);
        //uw_dump(v);
    }

#   ifdef DEBUG
    { // testing cap_size=2 char_size=3
        UwValue v = uw_create_empty_string2(2, 3);
        s = *_uw_get_string_pptr(v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 3);
        TEST(s->char_size == 2);
        TEST(s->cap_size == 1);
        TEST(s->block_count == 1);
        //uw_dump(v);
    }

    { // testing cap_size=4 char_size=3
        UwValue v = uw_create_empty_string2(4, 3);
        s = *_uw_get_string_pptr(v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 1);
        TEST(s->char_size == 2);
        TEST(s->cap_size == 3);
        TEST(s->block_count == 1);
        //uw_dump(v);
    }

#   if UINT_WIDTH > 32
    { // testing cap_size=8 char_size=3
        UwValue v = uw_create_empty_string2(8, 3);
        s = *_uw_get_string_pptr(v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 2);
        TEST(s->char_size == 2);
        TEST(s->cap_size == 7);
        TEST(s->block_count == 3);
        //uw_dump(v);
    }
#   endif
#   endif

    { // testing cap_size=1 char_size=4
        UwValue v = uw_create_empty_string(0, 4);
        s = *_uw_get_string_pptr(v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 1);
        TEST(s->char_size == 3);
        TEST(s->cap_size == 0);
        TEST(s->block_count == 0);
        //uw_dump(v);
    }

#   ifdef DEBUG
    { // testing cap_size=2 char_size=4
        UwValue v = uw_create_empty_string2(2, 4);
        s = *_uw_get_string_pptr(v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 2);
        TEST(s->char_size == 3);
        TEST(s->cap_size == 1);
        TEST(s->block_count == 1);
        //uw_dump(v);
    }

    { // testing cap_size=4 char_size=4
        UwValue v = uw_create_empty_string2(4, 4);
        s = *_uw_get_string_pptr(v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 1);
        TEST(s->char_size == 3);
        TEST(s->cap_size == 3);
        TEST(s->block_count == 1);
        //uw_dump(v);
    }

#if UINT_WIDTH > 32
    { // testing cap_size=8 char_size=4
        UwValue v = uw_create_empty_string2(8, 4);
        s = *_uw_get_string_pptr(v);
        capmeth = get_cap_methods(s);
        TEST(capmeth->get_length(s) == 0);
        TEST(capmeth->get_capacity(s) == 2);
        TEST(s->char_size == 3);
        TEST(s->cap_size == 7);
        TEST(s->block_count == 3);
        //uw_dump(v);
    }
#   endif
#   endif

    { // test trimming
        UwValue v = uw_create(u8"  สวัสดี   ");
        TEST(uw_strlen(v) == 11);
        uw_string_ltrim(v);
        TEST(uw_equal(v, u8"สวัสดี   "));
        uw_string_rtrim(v);
        TEST(uw_equal(v, u8"สวัสดี"));
        TEST(uw_strlen(v) == 6);
    }

    { // test join
        UwValue list = uw_create_list();
        uw_list_append(list, "Hello");
        uw_list_append(list, u8"สวัสดี");
        uw_list_append(list, "Thanks");
        uw_list_append(list, U"mulțumesc");
        UwValue v = uw_string_join('/', list);
        TEST(uw_equal(v, U"Hello/สวัสดี/Thanks/mulțumesc"));
        //uw_dump(v);
    }

    { // test split/join
        UwValue str = uw_create(U"สบาย/สบาย/yo/yo");
        UwValue list = uw_string_split(str, '/');
        //uw_dump(list);
        UwValue v = uw_string_join('/', list);
        TEST(uw_equal(v, U"สบาย/สบาย/yo/yo"));
    }
}

void test_list()
{
    UwValue list = uw_create_list();

    TEST(uw_list_length(list) == 0);

    for(unsigned i = 0; i < 1000; i++) {
        {
            UwValue item = uw_create(i);

            uw_list_append(list, item);
            TEST(item->refcount == 2);
        }

        TEST(uw_list_length(list) == i + 1);

        {
            UwValue v = uw_list_item(list, i);
            UwValue item = uw_create(i);
            TEST(uw_equal(v, item));
        }
    }

    {
        UwValue item = uw_list_item(list, -2);
        UwValue v = uw_create(998);
        TEST(uw_equal(v, item));
    }

    uw_list_del(list, 100, 200);
    TEST(uw_list_length(list) == 900);

    {
        UwValue item = uw_list_item(list, 99);
        UwValue v = uw_create(99);
        TEST(uw_equal(v, item));
    }
    {
        UwValue item = uw_list_item(list, 100);
        UwValue v = uw_create(200);
        TEST(uw_equal(v, item));
    }

    {
        UwValue slice = uw_list_slice(list, 750, 850);
        TEST(uw_list_length(slice) == 100);
        {
            UwValue item = uw_list_item(slice, 1);
            TEST(uw_equal(item, 851));
        }
        {
            UwValue item = uw_list_item(slice, 98);
            TEST(uw_equal(item, 948));
        }
    }
}

void test_map()
{
    {
        UwValue map = uw_create_map();
        UwValue key = uw_create(0);
        UwValue value = uw_create(false);

        uw_map_update(map, key, value);
        TEST(key->refcount == 1);    // key is copied, so refcount is intact
        TEST(value->refcount == 2);  // value is added by reference
        uw_delete(&key);
        uw_delete(&value);

        key = uw_create(0);
        TEST(uw_map_has_key(map, key));
        uw_delete(&key);

        key = uw_create(nullptr);
        TEST(!uw_map_has_key(map, key));
        uw_delete(&key);

        for (int i = 1; i < 50; i++) {
            key = uw_create(i);
            value = uw_create(i);
            uw_map_update(map, key, value);
            TEST(key->refcount == 1);    // key is copied, so refcount is intact
            TEST(value->refcount == 2);  // value is added by reference
            uw_delete(&key);
            uw_delete(&value);
        }
        uw_map_del(map, 25);

        TEST(uw_map_length(map) == 49);
//        uw_dump(map);
    }

    {
        UwValue map = uw_create_map_va(
            uwc_charptr,  "let's",       uwc_charptr,   "go!",
            uwc_nullptr,  nullptr,       uwc_bool,      true,
            uwc_bool,     true,          uwc_charptr,   "true",
            uwc_char,     -10,           uwc_bool,      false,
            uwc_uchar,    'b',           uwc_short,     -42,
            uwc_ushort,   42,            uwc_int,       -1000,
            uwc_uint,     100,           uwc_long,      -1000000L,
            uwc_ulong,      9UL,         uwc_longlong,  10LL,
            uwc_ulonglong, 100000000ULL, uwc_int32,     -500000,
            uwc_uint32,    700,          uwc_int64,     -1000000000000ULL,
            uwc_uint64,    300000000ULL, uwc_float,     1.23,
            uwc_double,   3.45,          uwc_charptr,   "hello",
            uwc_char8ptr, u8"สวัสดี",      uwc_char32ptr, U"สบาย",
            uwc_charptr,  "finally",     uwc_value_ptr, uw_create_map_va(uwc_charptr, "ok", uwc_charptr, "done", -1),
            -1
        );
        TEST(map != nullptr);
//        uw_dump(map);
    }
}

void test_file()
{
    char8_t a[] = u8"###################################################################################################\n";
    char8_t b[] = u8"##############################################################################################\n";
    char8_t c[] = u8"สบาย\n";
    char8_t* expected_result = a;

    char8_t data_filename[] = u8"./test/data/utf8-crossing-buffer-boundary";

    UwValue file = uw_file_open(data_filename, O_RDONLY, 0);
    TEST(uw_start_read_lines(file));
    UwValue line = uw_create("");
    while (uw_read_line_inplace(file, line)) {
        if (!uw_equal(line, expected_result)) {
            if (expected_result == a) {
                expected_result = b;
                TEST(uw_equal(line, expected_result));
                expected_result = c;
            }
            else {
                TEST(uw_equal(line, expected_result));
            }
        }
    }
}

void test_string_io()
{
    UwValue sio = uw_create_string_io("one\ntwo\nthree");
    UwValue line = uw_read_line(sio);
    TEST(line != nullptr);
    TEST(uw_equal(line, "one\n"));
    TEST(uw_read_line_inplace(sio, line));
    TEST(uw_equal(line, "two\n"));
    TEST(uw_unread_line(sio, line));
    // re-create line to avoid modification of unread one
    uw_delete(&line);
    line = uw_create("");
    TEST(uw_read_line_inplace(sio, line));
    TEST(uw_equal(line, "two\n"));
    uw_read_line_inplace(sio, line);
    TEST(uw_equal(line, "three"));
    TEST(!uw_read_line_inplace(sio, line));
    TEST(uw_read_line(sio) == nullptr);

    TEST(uw_start_read_lines(sio));
    uw_delete(&line);
    line = uw_read_line(sio);
    TEST(uw_equal(line, "one\n"));
}

int main(int argc, char* argv[])
{
    _uw_allocators[TEST_ALLOCATOR_ID] = test_allocator;
    uw_set_allocator(TEST_ALLOCATOR_ID);

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

    printf("leaked blocks: %u\n", num_allocated);
}
