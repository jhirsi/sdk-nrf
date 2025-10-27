# DECT NR+ Unity Integration Tests - Quick Start

Quick start guide for running Unity-based DECT NR+ integration tests with complete stack and mocked modem backend.

## 🚀 Quick Start

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
WARNING: Using a test - not safe - entropy source
nRF91 dect NR+ initialized
[00:00:00.000,000] <inf> fs_nvs: 4 Sectors of 4096 bytes
[00:00:00.000,000] <inf> fs_nvs: alloc wra: 0, f88
[00:00:00.000,000] <inf> fs_nvs: data wra: 0, 4ad
*** Booting nRF Connect SDK v3.1.99-dec1b76668d7 ***
*** Using Zephyr OS v4.1.99-4d916f7d9007 ***
=== DECT NR+ Simple Integration Tests ===
Target: native_sim
Framework: Unity
Scope: Basic DECT operations with mocked libmodem

[00:00:00.000,000] <inf> DECT_NRP_MAC: Starting DECT NR+ Stack initialization
MOCK: callback_set called, op_cb=0x81242e0, ntf_cb=0x81242a0
MOCK: Copied operation callbacks
MOCK: Copied notification callbacks, cluster_beacon_ntf=0x80e498b
DECT initialization test completed successfully:
- Callback set calls: 1
- Systemmode set calls: 1
- Configure calls: 1
- Functional mode set calls: 1
- NET_EVENT_DECT_ACTIVATE_DONE received: YES
- Activation status: 0

[... individual Unity test results ...]

-----------------------
10 Tests 0 Failures 0 Ignored
OK

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
Complete DECT lifecycle: Init→Reset→Scan→Associate→Release→Deactivate→FT config
==============================
Tests completed with result: 0. Use Ctrl+C to exit or run with timeout.
```

### Run Tests (Clean Build Method)

```bash
# Navigate to test directory
cd <your_root>/ncs/nrf/tests/drivers/dect_nrp/integrated

# Clean build for native simulator
west build -b native_sim . --pristine

# Run tests (executable is created in test directory build)
./build/integrated/zephyr/zephyr.exe
```

### Build Configuration Notes

All commands should be run from the test directory for consistency:

```bash
# Always navigate to test directory first
cd <your_root>/ncs/nrf/tests/drivers/dect_nrp/integrated

# All west build commands work from here
west build -b native_sim .
```

## 📊 Key Test Results

The tests include comprehensive DECT stack validation with detailed logging:

**✅ Stack Initialization**:
- `[00:00:00.000,000] <inf> DECT_NRP_MAC: Starting DECT NR+ Stack initialization`
- Mock backend properly intercepts `nrf_modem_dect_mac.h` calls
- All callback registrations verified

**✅ Network Operations**:
- `[00:00:00.060,000] <inf> DECT_NRP_MAC: Network scan initiated`
- `[00:00:00.230,000] <inf> DECT_NRP_MAC: Network scan completed: 2 channels scanned`
- Association and release cycles working properly

**✅ Error Handling**:
- `[00:00:01.190,000] <err> DECT_NRP_MAC: Network scan completed with err MAC_STATUS_NOT_ALLOWED (3)`
- Proper validation of deactivated stack behavior
- All error scenarios tested and verified

## 🔧 Test Directory Structure

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

## ❓ Troubleshooting

### Build Fails
```bash
# Clean and rebuild from test directory
cd <your_root>/ncs/nrf/tests/drivers/dect_nrp/integrated
west build -b native_sim . --pristine
```

### Can't Find Executable
```bash
# Check build output from test directory
ls -la ./build/integrated/zephyr/zephyr.exe
```

### Permission Issues
```bash
# Make executable if needed
chmod +x ./build/integrated/zephyr/zephyr.exe
```

## 🎯 Why Native Simulation?

- ✅ **No Hardware Required**: Test without nRF9161 board
- ✅ **Fast Development**: Instant compilation and execution
- ✅ **Easy Debugging**: Standard Linux debugging tools
- ✅ **CI/CD Friendly**: Perfect for automated testing
- ✅ **Ubuntu Compatible**: Runs directly on your development system

## 🔄 Quick Development Workflow

### From Test Directory (Recommended)

```bash
# Navigate to test directory
cd <your_root>/ncs/nrf/tests/drivers/dect_nrp/integrated

# Edit tests (if needed)
# vim src/test_dect_integration.c

# Build and run with timeout
west build -b native_sim . && timeout 15s ./build/integrated/zephyr/zephyr.exe

# Repeat as needed for development
```

### Debug Logging (Optional)

By default, tests run with minimal logging for clean output. To enable detailed debug logging:

```bash
# Edit prj.conf to change log level to DEBUG (4)
sed -i 's/CONFIG_LOG_DEFAULT_LEVEL=3/CONFIG_LOG_DEFAULT_LEVEL=4/' prj.conf

# Rebuild and run with verbose output
west build -b native_sim . && timeout 15s ./build/integrated/zephyr/zephyr.exe

# Restore minimal logging
sed -i 's/CONFIG_LOG_DEFAULT_LEVEL=4/CONFIG_LOG_DEFAULT_LEVEL=3/' prj.conf
```

**Debug Output Includes:**
- Detailed event handler traces
- Mock function call logging
- Thread context information
- Step-by-step test execution details
- Network event processing details

### Test Results Interpretation
The Unity test results show detailed test execution:

**✅ Unity Test Output**:
```bash
UNITY TEST RESULTS:
OK (5)
UNITY_OUTPUT_COMPLETE

TEST SUCCESS: all test_* functions passed
```

**� Test Coverage**:
- All initialization functions validated
- Complete DECT activation flow tested
- NET_EVENT_DECT_ACTIVATE_DONE callback verified
- Mock backend interaction confirmed

**🔍 Detailed Test Results**:
- Test functions show individual PASS/FAIL status
- Mock call verification for all API interactions
- Event callback validation confirms proper operation

### Test Output Analysis

**Unity Framework Results**:
```
WEST_TOPDIR/nrf/tests/drivers/dect_nrp/integrated/src/main.c:70:test_dect_stack_initialization:PASS
WEST_TOPDIR/nrf/tests/drivers/dect_nrp/integrated/src/main.c:71:
	test_dect_settings_reset_to_defaults:PASS
[... additional test results ...]

-----------------------
10 Tests 0 Failures 0 Ignored
OK
```

**Exit Behavior**:
- Tests automatically complete and show final result code
- `timeout 15s` command prevents hanging on native_sim
- Result code 124 indicates timeout (expected), 0 indicates clean completion

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
west build -b native_sim . && timeout 15s ./build/integrated/zephyr/zephyr.exe
```