#ifndef HS_TEST_H
#define HS_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int hs_tests_run = 0;
static int hs_tests_passed = 0;
static int hs_tests_failed = 0;
static int hs_current_failed = 0;

#define TEST(name) static void test_##name(void)

#define RUN_TEST(name) do { \
    hs_current_failed = 0; \
    printf("  %-55s", #name); \
    test_##name(); \
    hs_tests_run++; \
    if (hs_current_failed) { \
        hs_tests_failed++; \
    } else { \
        hs_tests_passed++; \
        printf("PASS\n"); \
    } \
} while (0)

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        printf("FAIL\n    assertion failed: %s\n    at %s:%d\n", \
               #expr, __FILE__, __LINE__); \
        hs_current_failed = 1; \
        return; \
    } \
} while (0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

#define ASSERT_EQ(a, b) do { \
    long long av_ = (long long)(a), bv_ = (long long)(b); \
    if (av_ != bv_) { \
        printf("FAIL\n    expected %lld == %lld\n    at %s:%d\n", \
               av_, bv_, __FILE__, __LINE__); \
        hs_current_failed = 1; \
        return; \
    } \
} while (0)

#define ASSERT_NE(a, b) do { \
    long long av_ = (long long)(a), bv_ = (long long)(b); \
    if (av_ == bv_) { \
        printf("FAIL\n    expected %lld != %lld\n    at %s:%d\n", \
               av_, bv_, __FILE__, __LINE__); \
        hs_current_failed = 1; \
        return; \
    } \
} while (0)

#define ASSERT_STREQ(a, b) do { \
    const char *as_ = (a), *bs_ = (b); \
    if (!as_ || !bs_ || strcmp(as_, bs_) != 0) { \
        printf("FAIL\n    expected \"%s\" == \"%s\"\n    at %s:%d\n", \
               as_ ? as_ : "(null)", bs_ ? bs_ : "(null)", \
               __FILE__, __LINE__); \
        hs_current_failed = 1; \
        return; \
    } \
} while (0)

#define ASSERT_STRNE(a, b) do { \
    const char *as_ = (a), *bs_ = (b); \
    if (as_ && bs_ && strcmp(as_, bs_) == 0) { \
        printf("FAIL\n    expected \"%s\" != \"%s\"\n    at %s:%d\n", \
               as_, bs_, __FILE__, __LINE__); \
        hs_current_failed = 1; \
        return; \
    } \
} while (0)

#define ASSERT_NOT_NULL(p) do { \
    if ((p) == NULL) { \
        printf("FAIL\n    expected non-null: %s\n    at %s:%d\n", \
               #p, __FILE__, __LINE__); \
        hs_current_failed = 1; \
        return; \
    } \
} while (0)

#define ASSERT_NULL(p) do { \
    if ((p) != NULL) { \
        printf("FAIL\n    expected null: %s\n    at %s:%d\n", \
               #p, __FILE__, __LINE__); \
        hs_current_failed = 1; \
        return; \
    } \
} while (0)

#define ASSERT_STR_CONTAINS(haystack, needle) do { \
    const char *h_ = (haystack), *n_ = (needle); \
    if (!h_ || !n_ || !strstr(h_, n_)) { \
        printf("FAIL\n    \"%s\" does not contain \"%s\"\n    at %s:%d\n", \
               h_ ? h_ : "(null)", n_ ? n_ : "(null)", \
               __FILE__, __LINE__); \
        hs_current_failed = 1; \
        return; \
    } \
} while (0)

#define TEST_MAIN_BEGIN(suite) \
    int main(void) { \
        printf("Running %s tests...\n", suite);

#define TEST_MAIN_END() \
        printf("\n%d/%d passed", hs_tests_passed, hs_tests_run); \
        if (hs_tests_failed) printf(", %d failed", hs_tests_failed); \
        printf("\n"); \
        return hs_tests_failed > 0 ? 1 : 0; \
    }

#endif
