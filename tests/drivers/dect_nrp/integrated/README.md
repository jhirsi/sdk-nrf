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

# Generate code coverage (requires CONFIG_COVERAGE=y)
west build -t coverage_report
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

```bash
# Generate coverage report (requires CONFIG_COVERAGE=y in prj.conf)
west build -t coverage_report

# View HTML report
firefox build/coverage_report/index.html
```

Coverage includes:
- **DECT L2 Management**: `dect_net_l2_mgmt.c` API validation
- **DECT Driver**: `dect_nrp_mac.c` initialization and request handling
- **Mock Integration**: Callback simulation and async event handling
- **Error Paths**: Deactivated stack behavior and failure scenarios