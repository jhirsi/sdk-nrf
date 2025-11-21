# Kconfig Hints for DECT RPC MQTT Transport

## Problem

When `CONFIG_DECT_NRP_RPC_MQTT_TRANSPORT=y` is selected, the base `NRF_RPC_TRANSPORT` choice
still defaults to `NRF_RPC_IPC_SERVICE`, which causes compilation errors because:
1. The IPC transport code tries to use device tree nodes that don't exist
2. The MQTT transport doesn't use the base transport choice at all

## Solution

You must explicitly disable the base nRF RPC transport options in your `prj.conf`:

```kconfig
# Enable DECT RPC with MQTT transport
CONFIG_DECT_NRP_RPC_MQTT_TRANSPORT=y

# Disable base nRF RPC transports (required for MQTT transport)
CONFIG_NRF_RPC_IPC_SERVICE=n
CONFIG_NRF_RPC_UART_TRANSPORT=n
```

## Why This Is Necessary

The `NRF_RPC_TRANSPORT` choice in `nrf/subsys/nrf_rpc/Kconfig` has a default of
`NRF_RPC_IPC_SERVICE`. Kconfig choices require one option to be selected, and the default
is applied before our `depends on !NRF_RPC_IPC_SERVICE` dependency is evaluated.

By explicitly setting both base transport options to `n` in `prj.conf`, we ensure that:
1. The choice doesn't default to IPC
2. The MQTT transport can be used without conflicts

## Alternative Solutions (Not Implemented)

1. **Make the base choice optional**: Modify `nrf/subsys/nrf_rpc/Kconfig` to make the
   `NRF_RPC_TRANSPORT` choice optional. This would allow no transport to be selected,
   but requires modifying upstream code.

2. **Add a "None" option**: Add a third option to the base choice for "No transport"
   or "Custom transport", but this also requires modifying upstream code.

3. **Use configdefault**: Override the choice default using `configdefault` in a
   `Kconfig.defconfig` file, but this is complex and may not work reliably.

The current solution (explicitly disabling in prj.conf) is the simplest and most
maintainable approach.

