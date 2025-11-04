/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <unity.h>
#include <zephyr/kernel.h>
#include <stdlib.h>
#ifdef CONFIG_BOARD_NATIVE_SIM
#include "posix_board_if.h"
#endif

/* Include test functions from test_dect_integration.c */
extern void test_dect_stack_initialization(void);
extern void test_dect_settings_reset_to_defaults(void);
extern void test_dect_scan_request_band1(void);
extern void test_dect_pt_association_request(void);
extern void test_dect_pt_neigbor_list_info_req(void);
extern void test_dect_pt_status_info_req(void);
extern void test_dect_pt_association_release(void);
extern void test_dect_deactivate(void);
extern void test_dect_deactivated_requests_fail(void);
extern void test_dect_ft_configuration(void);
extern void test_dect_pt_complete_workflow(void);

/* Test result tracking structure */
typedef struct {
	const char *name;
	const char *function_name;
	bool passed;
} test_result_t;

/* Global test result tracking array */
static test_result_t test_results[] = {
	{"DECT stack initialization", "test_dect_stack_initialization", false},
	{"Settings reset to defaults", "test_dect_settings_reset_to_defaults", false},
	{"Network scan (Band 1)", "test_dect_scan_request_band1", false},
	{"PT association request", "test_dect_pt_association_request", false},
	{"PT neighbor list/info req", "test_dect_pt_neigbor_list_info_req", false},
	{"PT status info request", "test_dect_pt_status_info_req", false},
	{"PT association release", "test_dect_pt_association_release", false},
	{"DECT stack deactivation", "test_dect_deactivate", false},
	{"Deactivated requests fail", "test_dect_deactivated_requests_fail", false},
	{"FT configuration", "test_dect_ft_configuration", false}};

#define NUM_TESTS ARRAY_SIZE(test_results)

/**
 * Helper function to run a test and track its result
 */
#define RUN_TEST_AND_TRACK(test_func, test_index)                                                  \
	do {                                                                                       \
		int tests_before = Unity.TestFailures;                                             \
		RUN_TEST(test_func);                                                               \
		test_results[test_index].passed = (Unity.TestFailures == tests_before);            \
	} while (0)

/**
 * Main function - runs all Unity tests
 */
int main(void)
{
	/* Initialize Unity */
	UNITY_BEGIN();

	printk("=== DECT NR+ Simple Integration Tests ===\n");
	printk("Target: native_sim\n");
	printk("Framework: Unity\n");
	printk("Scope: Basic DECT operations with mocked libmodem\n\n");

	/* Run DECT integration tests with result tracking */
	RUN_TEST_AND_TRACK(test_dect_stack_initialization, 0);
	RUN_TEST_AND_TRACK(test_dect_settings_reset_to_defaults, 1);
	RUN_TEST_AND_TRACK(test_dect_scan_request_band1, 2);
	RUN_TEST_AND_TRACK(test_dect_pt_association_request, 3);
	RUN_TEST_AND_TRACK(test_dect_pt_neigbor_list_info_req, 4);
	RUN_TEST_AND_TRACK(test_dect_pt_status_info_req, 5);
	RUN_TEST_AND_TRACK(test_dect_pt_association_release, 6);
	RUN_TEST_AND_TRACK(test_dect_deactivate, 7);
	RUN_TEST_AND_TRACK(test_dect_deactivated_requests_fail, 8);
	RUN_TEST_AND_TRACK(test_dect_ft_configuration, 9);

	/* Capture Unity statistics before UNITY_END() */
	int total_tests = Unity.NumberOfTests;
	int failed_tests = Unity.TestFailures;
	int passed_tests = total_tests - failed_tests;
	int success_rate_int = total_tests > 0 ? (passed_tests * 100) / total_tests : 0;

	/* Complete and report test results */
	int result = UNITY_END();

	/* Print minimal test summary with detailed test case status */
	printk("\n");
	printk("=== DECT NR+ TEST SUMMARY ===\n");
	printk("Tests: %d | Passed: %d | Failed: %d | Rate: %d%%\n", total_tests, passed_tests,
	       failed_tests, success_rate_int);

	printk("\nTEST CASE RESULTS:\n");
	printk("------------------\n");

	/* Print results based on actual test outcomes */
	for (int i = 0; i < NUM_TESTS; i++) {
		if (test_results[i].passed) {
			printk("\033[32m[PASS]\033[0m %-35s\n", test_results[i].function_name);
		} else {
			printk("\033[31m[FAIL]\033[0m %-35s\n", test_results[i].function_name);
		}
	}

	if (result == 0) {
		printk("\n\033[32mALL TESTS PASSED!\033[0m\n");
		printk("\033[32mComplete DECT lifecycle: Init→Reset→Scan→Associate→"
		       "Release→Deactivate→FT config\033[0m\n");
	} else {
		printk("\n\033[31mSOME TESTS FAILED!\033[0m\n");
		printk("\033[33mCheck individual test results above for details\033[0m\n");
	}
	printk("==============================\n");

	/* Exit automatically for native_sim - prevents hanging after tests complete */
	printk("Tests completed with result: %d\n", result);

#ifdef CONFIG_BOARD_NATIVE_SIM
#ifdef CONFIG_COVERAGE
	/* When coverage is enabled, we need to explicitly exit to ensure coverage data is written*/
	/* This ensures gcov coverage data (.gcda files) are properly flushed on exit */
	/* Note: This exits immediately, so output may be truncated - coverage data is prioritized*/
	posix_exit(result);

	/* Should be unreachable: */
	return 1;
#else
	/* Without coverage, let the program exit normally so all output is visible */
	return result;
#endif
#else
	return result;
#endif
}
