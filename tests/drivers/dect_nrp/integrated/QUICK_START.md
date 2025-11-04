# DECT NR+ Unity Integration Tests - Quick Start

Quick start guide for running Unity-based DECT NR+ integration tests with complete stack and mocked modem backend.

## Quick Start

### Prerequisites
- Ubuntu Linux system
- Nordic Connect SDK (NCS) v3.1.0 or later
- Unity testing framework (included)
- No physical hardware required

### Run Tests (Recommended Method)

```bash
# Navigate to test directory
cd <your_root>/ncs/nrf/tests/drivers/dect_nrp/integrated

# Build and run Unity tests with timeout (automatically exits after tests complete)
west build -p -b native_sim . && timeout 15s ./build/integrated/zephyr/zephyr.exe
```

### Expected Success Output

```
=== DECT NR+ TEST SUMMARY ===
Tests: 10 | Passed: 10 | Failed: 0 | Rate: 100%

TEST CASE RESULTS:
------------------
[PASS] test_dect_stack_initialization
[PASS] test_dect_settings_reset_to_defaults
[PASS] test_dect_scan_request_band1
[PASS] test_dect_pt_association_request
[PASS] test_dect_pt_neigbor_list_info_req
[PASS] test_dect_pt_status_info_req
[PASS] test_dect_pt_association_release
[PASS] test_dect_deactivate
[PASS] test_dect_deactivated_requests_fail
[PASS] test_dect_ft_configuration

ALL TESTS PASSED!

```

## Test Directory Structure

```
<your_root>/ncs/nrf/tests/drivers/dect_nrp/integrated/  ← Work from here
├── prj.conf                        # Kconfig settings (DECT stack + Unity)
├── CMakeLists.txt                  # Build configuration
├── testcase.yaml                   # Unity test framework configuration
├── README.md                       # Detailed documentation
├── QUICK_START.md                  # This file (native_sim focus)
├── MOCK_ARCHITECTURE.md            # Mock architecture documentation
├── CHANGELOG.md                    # Project changelog
└── src/
    ├── test_dect_integration.c     # Unity integration tests
    ├── main.c                      # Unity test runner
    └── mocks/
        ├── mock_nrf_modem_dect_mac.c   # Mock implementation
        └── mock_nrf_modem_dect_mac.h   # Mock header

Build output: ./build/integrated/zephyr/zephyr.exe (from test directory)
```
##  Adding New Tests

Edit `src/test_dect_integration.c` and add new test cases using Unity framework:

```c
void test_your_new_feature(void)
{
    TEST_ASSERT_EQUAL(expected, actual);
    TEST_ASSERT_TRUE(condition);
}
```

Then update `src/main.c` to include your new test in the test runner:

```c
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_dect_stack_initialization);
    RUN_TEST(test_your_new_feature);

    return UNITY_END();
}
```

Rebuild and run from test directory:
```bash
cd <your_root>/ncs/nrf/tests/drivers/dect_nrp/integrated
west build -p -b native_sim . && timeout 15s ./build/integrated/zephyr/zephyr.exe
```