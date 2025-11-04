# DECT NR+ Stack Integration Tests

Unity-based integration tests for the complete DECT NR+ stack in NCS. Tests the full stack from `net_mgmt()` API down to mock libmodem backend.

## Architecture

```
Unity Tests → net_mgmt() → Real DECT Stack → Mock libmodem Backend
```

### Stack Layers Under Test

```
Application Layer (Tests)
     ↓
Real net_mgmt() API calls ← Unity tests use this
     ↓
DECT Management API (dect_net_l2_mgmt.h) ← Real implementation
     ↓
DECT L2 Networking Layer (subsys/net/l2_dect_nrp/) ← Real implementation
     ↓
DECT NR+ Driver Layer (drivers/dect_nrp/) ← Real implementation
     ↓
nrf_modem_dect_mac.h ← Mocked with callback simulation
```

- **Real Stack**: Complete DECT NR+ implementation (net_mgmt → driver)
- **Mock Backend**: `nrf_modem_dect_mac.h` functions with async callback simulation
- **Integration Focus**: End-to-end DECT workflows with real networking interface

## Test Coverage

Tests complete DECT PT device lifecycle:
- **Stack Initialization**: Driver setup with callback registration and activation
- **Settings Management**: Reset to defaults and FT/PT configuration
- **Network Scanning**: Async scan with beacon reception and event validation
- **Association Management**: PT association/release with async failure handling
- **Neighbor Discovery**: List and info requests with proper event validation
- **Stack Deactivation**: Graceful shutdown and deactivated request behavior
- **Error Scenarios**: Comprehensive validation of all commands when deactivated

### Mock Implementation Strategy

#### Mock Boundary
```
┌─────────────────────────────────────────┐
│        Unity Test Framework             │
│  ┌─────────────────────────────────┐    │
│  │      Real net_mgmt() calls      │    │
│  └─────────────┬───────────────────┘    │
└────────────────┼────────────────────────┘
                 │
┌────────────────┼───────────────────────┐
│                ▼                       │
│       DECT Management API              │ ← Real
│    (dect_net_l2_mgmt.h)                │
│                │                       │
│       DECT L2 Networking               │ ← Real
│   (subsys/net/l2_dect_nrp/)            │
│                │                       │
│       DECT NR+ Driver                  │ ← Real
│    (drivers/dect_nrp/)                 │
│                │                       │
└────────────────┼───────────────────────┘
                 │
┌────────────────┼───────────────────────┐
│                ▼                       │
│    Mock nrf_modem_dect_mac.h           │ ← Mock
│  (src/mocks/mock_nrf_modem_dect_mac.c) │
└────────────────────────────────────────┘
```

#### Key Principles
- **Mock at modem boundary** - everything above is real implementation
- **Async callback simulation** - proper callback timing and flow
- **Driver state isolation** - mocks don't access driver internals
- **Real API signatures** - use authentic nrfxlib header definitions

*See [MOCK_ARCHITECTURE.md](MOCK_ARCHITECTURE.md) for detailed implementation guide.*

## Build & Run

```bash
# Build and run tests
cd <your_root>/ncs/nrf/tests/drivers/dect_nrp/integrated
west build -p -b native_sim . && timeout 10s ./build/integrated/zephyr/zephyr.exe

# Generate code coverage (see Code Coverage section below)
./generate_coverage.sh
```

**Requirements**: NCS v3.1.0+, Linux with native_sim support, no hardware needed.

## Expected Output

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
Complete DECT lifecycle: Init→Reset→Scan→Associate→Release→Deactivate→FT config
```

## Code Coverage

Measure code coverage for the DECT NR+ stack components.

### Quick Start

```bash
# Generate coverage report (coverage is automatically enabled during build)
./generate_coverage.sh

# View HTML report
xdg-open build/coverage_report/html/index.html
```

**Note**: Coverage is **disabled by default** in `prj.conf` to allow normal test execution with full output. The `generate_coverage.sh` script automatically enables it temporarily during coverage measurement.

### What Gets Measured

The coverage report is **filtered to only include** the following DECT NR+ stack directories:

1. **DECT NR+ Driver Layer** (`nrf/drivers/dect_nrp/nrf91_mac/`):
   - `dect_nrf91_ctrl.c` - Control plane and MAC management
   - `dect_nrf91_rx.c` - RX thread and packet reception
   - `dect_nrf91.c` - Main driver initialization
   - `dect_nrf91_utils.c` - Utility functions
   - `dect_nrf91_sink.c` - Sink mode support
   - `dect_nrf91_settings.c` - Settings management

2. **DECT L2 Networking Layer** (`nrf/subsys/net/l2_dect_nrp/`):
   - `dect_net_l2.c` - Core L2 routing and association management
   - `dect_net_l2_mgmt.c` - Management API implementation
   - `dect_net_l2_ipv6.c` - IPv6 addressing and prefix management
   - `dect_net_l2_sink.c` - Border router/sink functionality
   - `dect_net_conn_mgr.c` - Connection manager integration

3. **DECT Utilities** (`nrf/subsys/net/lib/dect_nrp/src/utils/`):
   - `dect_nrp_utils.c` - DECT NR+ utility functions

**Note**: All other code is excluded, including Zephyr core, test framework, mocks, and build system files.

### Coverage Report Formats

- **HTML Report**: `build/coverage_report/html/index.html` - Interactive browsing with line-by-line coverage
- **LCOV Format**: `build/coverage_report/dect_coverage.info` - Machine-readable for CI/CD integration
- **Text Summary**: `build/coverage_report/coverage_summary.txt` - Quick overview (if gcovr installed)

### Understanding Coverage Metrics

- **Line Coverage**: Percentage of executable lines executed during tests
- **Branch Coverage**: Percentage of conditional branches (if/else, switch cases) taken
- **Function Coverage**: Percentage of functions called at least once

### How It Works

The coverage script (`generate_coverage.sh`):
1. Automatically enables `CONFIG_COVERAGE=y` in a temporary prj.conf if not already enabled
2. Builds the test with coverage instrumentation enabled
3. Runs tests (which exit via `posix_exit()` when coverage is enabled to ensure coverage data is written)
4. Captures coverage data from build directory
5. Filters to only DECT NR+ stack files using Python-based filtering
6. Generates HTML reports with detailed line-by-line coverage
7. Provides summary statistics
8. Cleans up temporary configuration files

**Important**: When coverage is enabled, tests exit immediately via `posix_exit()` which may truncate some output. For normal test runs without coverage, tests return normally and show full output.

### Requirements

- **lcov**: Required for HTML report generation
  ```bash
  sudo apt-get install lcov  # Debian/Ubuntu
  sudo yum install lcov      # RHEL/CentOS
  ```
- **gcovr**: Optional, for JSON/text summaries
  ```bash
  pip install gcovr
  ```

### Troubleshooting

**No .gcda files generated:**
- The script automatically enables coverage, but you can verify: `grep CONFIG_COVERAGE prj.conf.coverage` (temporary file created by script)
- Ensure test executable ran to completion (when coverage is enabled, tests exit via `posix_exit()`)
- Check that you're building for `native_sim` board

**lcov not found:**
- Install lcov package (see Requirements above)

**Coverage report includes wrong files:**
- The script automatically filters to DECT stack directories only
- Check `generate_coverage.sh` if filtering needs adjustment

**Low coverage:**
- Review test cases in `test_dect_integration.c`
- Add test cases for uncovered paths
- Check conditional compilation (`CONFIG_*`) that might exclude code

### Advanced Usage

```bash
# Custom build directory
BUILD_DIR=my_build ./generate_coverage.sh

# Custom coverage output directory
COVERAGE_DIR=custom_coverage ./generate_coverage.sh

# Different board
BOARD=native_posix ./generate_coverage.sh

# Increase test timeout
TIMEOUT=120 ./generate_coverage.sh
```

### CI/CD Integration

```bash
# Generate coverage in CI
./generate_coverage.sh

# Upload to coverage service (e.g., Codecov)
codecov -f build/coverage_report/dect_coverage.info
```