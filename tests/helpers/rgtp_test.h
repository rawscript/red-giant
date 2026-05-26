/**
 * @file rgtp_test.h
 * @brief Lightweight test harness for RGTP unit tests.
 *
 * Provides RGTP_ASSERT, RGTP_ASSERT_OK, RGTP_RUN_TEST, and RGTP_PRINT_RESULTS
 * macros. No external dependencies — pure C17.
 */

#ifndef RGTP_TEST_H
#define RGTP_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include "rgtp/rgtp.h"

#ifdef __cplusplus
extern "C" {
#endif

static int rgtp_test_passed  = 0;
static int rgtp_test_failures = 0;
static const char* rgtp_current_test = "(none)";

#define RGTP_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL [%s]: %s (line %d)\n", \
                rgtp_current_test, (msg), __LINE__); \
        rgtp_test_failures++; \
    } \
} while(0)

#define RGTP_ASSERT_OK(expr) do { \
    rgtp_error_t _err = (expr); \
    if (_err != RGTP_OK) { \
        fprintf(stderr, "  FAIL [%s]: %s returned %s (line %d)\n", \
                rgtp_current_test, #expr, rgtp_strerror(_err), __LINE__); \
        rgtp_test_failures++; \
    } \
} while(0)

#define RGTP_ASSERT_ERR(expr, expected_err) do { \
    rgtp_error_t _err = (expr); \
    if (_err != (expected_err)) { \
        fprintf(stderr, "  FAIL [%s]: %s returned %s, expected %s (line %d)\n", \
                rgtp_current_test, #expr, \
                rgtp_strerror(_err), rgtp_strerror(expected_err), __LINE__); \
        rgtp_test_failures++; \
    } \
} while(0)

#define RGTP_RUN_TEST(fn) do { \
    rgtp_current_test = #fn; \
    int _before = rgtp_test_failures; \
    fn(); \
    if (rgtp_test_failures == _before) { \
        printf("  PASS %s\n", #fn); \
        rgtp_test_passed++; \
    } \
} while(0)

#define RGTP_PRINT_RESULTS() do { \
    printf("\n%d passed, %d failed\n", \
           rgtp_test_passed, rgtp_test_failures); \
} while(0)

#ifdef __cplusplus
}
#endif

#endif /* RGTP_TEST_H */
