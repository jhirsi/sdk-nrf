# Mock Implementation Details

Implementation details for the DECT NR+ mock backend. See main README.md for architecture overview.

## Mock Components

### Key Mock Functions

| Function | Purpose | Callback Simulation |
|----------|---------|-------------------|
| `nrf_modem_dect_mac_callback_set()` | Register driver callbacks | Stores callbacks for later use |
| `nrf_modem_dect_control_systemmode_set()` | Set MAC mode | Triggers `control_systemmode` callback |
| `nrf_modem_dect_control_configure()` | Configure modem | Triggers `control_configure` callback |
| `nrf_modem_dect_control_functional_mode_set()` | Activate modem | Triggers `control_functional_mode` callback |
| `nrf_modem_dect_mac_network_scan()` | Network scan | Returns success, stores call count |

## Callback Simulation Architecture

#### Callback Storage
```c
struct nrf_modem_dect_mac_op_callbacks mock_op_callbacks;
struct nrf_modem_dect_mac_ntf_callbacks mock_ntf_callbacks;
```

#### Async Callback Simulation
```c
void simulate_async_callback(void (*callback_func)(void *), void *params) {
    /* For unit tests, call callback immediately */
    if (callback_func && params) {
        callback_func(params);
    }
}
```

#### Example: systemmode_set Mock
```c
int nrf_modem_dect_control_systemmode_set(enum nrf_modem_dect_control_systemmode mode)
{
    mock_nrf_modem_dect_control_systemmode_set_call_count++;

    if (mode == NRF_MODEM_DECT_MODE_MAC) {
        if (mock_op_callbacks.control_systemmode) {
            struct nrf_modem_dect_mac_control_systemmode_cb_params cb_params = {
                .status = 0  /* Success */
            };
            simulate_async_callback((void (*)(void *))mock_op_callbacks.control_systemmode, &cb_params);
        }
        return 0;
    }
    return -1;
}
```

## Initialization Flow

```
Driver Init → Mock Response → Driver Callback → Driver State Change
     │               │              │                   │
     ▼               ▼              ▼                   ▼
systemmode_set → SUCCESS → systemmode_cb → sem_give()
configure()    → SUCCESS → configure_cb  → sem_give() + activation
func_mode_set  → SUCCESS → func_mode_cb  → NET_EVENT_DECT_ACTIVATE_DONE
```

**Key Principle**: Mock triggers callbacks, driver handles its own semaphores and state.

## Adding New Mock Functions

### Template for New Modem Function Mock

```c
// In mock_nrf_modem_dect_mac.h
extern int mock_nrf_modem_dect_new_function_call_count;

// In mock_nrf_modem_dect_mac.c
int mock_nrf_modem_dect_new_function_call_count = 0;

int nrf_modem_dect_new_function(struct nrf_modem_dect_new_function_params *params)
{
    mock_nrf_modem_dect_new_function_call_count++;

    /* Simulate successful operation */
    if (mock_op_callbacks.new_function) {
        struct nrf_modem_dect_mac_new_function_cb_params cb_params = {
            .status = 0  /* Success */
        };
        simulate_async_callback((void (*)(void *))mock_op_callbacks.new_function, &cb_params);
    }
    return 0;
}
```

### Key Principles for New Mocks

1. **Use Real Signatures**: Copy exact function signatures from nrfxlib headers
2. **Add Call Counters**: Provide verification capabilities for tests
3. **Simulate Success**: Default to successful operation unless testing failure cases
4. **Trigger Callbacks**: Provide proper asynchronous callback simulation
5. **No Driver Access**: Don't access driver internals, let driver manage itself

## Troubleshooting Mock Issues

### Common Problems
1. **Missing Callbacks**: Check that `nrf_modem_dect_mac_callback_set()` was called first
2. **Wrong Enum Types**: Ensure mock uses correct enum types from real headers
3. **Callback Parameters**: Verify callback parameter structures match real API
4. **Call Counts**: Check that mock call counters are properly incremented

### Debug Techniques
1. **Add Debug Prints**: Add temporary printf statements in mock functions
2. **Check Call Counters**: Verify expected vs actual call counts
3. **Callback Validation**: Ensure callbacks are stored and triggered correctly
4. **Event Verification**: Check that net_mgmt events are generated and received