/*
 * minunit.h - Minimal unit testing framework for C
 * Based on: http://www.jera.com/techinfo/jtns/jtn002.html
 * Extended with additional assertions
 */

#ifndef MINUNIT_H
#define MINUNIT_H

#include <stdio.h>
#include <string.h>

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Colors for output */
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_RESET   "\x1b[0m"

/* Basic assertion - returns error message on failure */
#define mu_assert(message, test) do { \
    if (!(test)) { \
        return message; \
    } \
} while (0)

/* Run a test function */
#define mu_run_test(test) do { \
    printf("  Running: %s ... ", #test); \
    char *message = test(); \
    tests_run++; \
    if (message) { \
        tests_failed++; \
        printf(COLOR_RED "FAILED" COLOR_RESET "\n"); \
        printf("    -> %s\n", message); \
    } else { \
        tests_passed++; \
        printf(COLOR_GREEN "PASSED" COLOR_RESET "\n"); \
    } \
} while (0)

/* Assertion helpers */
#define mu_assert_int_eq(expected, actual) do { \
    int _expected = (expected); \
    int _actual = (actual); \
    if (_expected != _actual) { \
        static char _mu_msg[256]; \
        snprintf(_mu_msg, sizeof(_mu_msg), \
            "Expected %d but got %d at %s:%d", \
            _expected, _actual, __FILE__, __LINE__); \
        return _mu_msg; \
    } \
} while (0)

#define mu_assert_str_eq(expected, actual) do { \
    const char *_expected = (expected); \
    const char *_actual = (actual); \
    if (strcmp(_expected, _actual) != 0) { \
        static char _mu_msg[256]; \
        snprintf(_mu_msg, sizeof(_mu_msg), \
            "Expected \"%s\" but got \"%s\" at %s:%d", \
            _expected, _actual, __FILE__, __LINE__); \
        return _mu_msg; \
    } \
} while (0)

#define mu_assert_not_null(ptr) do { \
    if ((ptr) == NULL) { \
        static char _mu_msg[256]; \
        snprintf(_mu_msg, sizeof(_mu_msg), \
            "Expected non-NULL pointer at %s:%d", __FILE__, __LINE__); \
        return _mu_msg; \
    } \
} while (0)

#define mu_assert_null(ptr) do { \
    if ((ptr) != NULL) { \
        static char _mu_msg[256]; \
        snprintf(_mu_msg, sizeof(_mu_msg), \
            "Expected NULL pointer at %s:%d", __FILE__, __LINE__); \
        return _mu_msg; \
    } \
} while (0)

/* Print test summary */
#define mu_print_summary() do { \
    printf("\n----------------------------------------\n"); \
    printf("Tests run: %d\n", tests_run); \
    printf(COLOR_GREEN "Passed: %d" COLOR_RESET "\n", tests_passed); \
    if (tests_failed > 0) { \
        printf(COLOR_RED "Failed: %d" COLOR_RESET "\n", tests_failed); \
    } else { \
        printf("Failed: 0\n"); \
    } \
    printf("----------------------------------------\n"); \
} while (0)

/* Return exit code based on results */
#define mu_exit_code() (tests_failed > 0 ? 1 : 0)

#endif /* MINUNIT_H */
