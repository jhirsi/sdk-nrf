# DECT RPC Transport Information

## Transport Layer

**The RPC transport is already over UART** - it does NOT require networking or net_if on the client side!

### Current Configuration

Your `prj.conf` already has:
```kconfig
CONFIG_NRF_RPC_UART_TRANSPORT=y
CONFIG_NRF_RPC_UART_RELIABLE=y
```

This means:
- ✅ RPC communication uses **UART serial communication**
- ✅ No network stack required on client side
- ✅ Works between two processors/cores over UART
- ✅ Can work from nRF Cloud or any application that can send/receive UART data

### Architecture

```
┌─────────────────────────────────────┐
│  nRF Cloud / Application            │
│  (No net_if required!)              │
│  dect_rpc_activate(0)               │
└──────────────┬──────────────────────┘
               │
               │ UART Serial (CBOR)
               │
┌──────────────▼──────────────────────┐
│  RPC Server (DECT Stack Side)       │
│  - Has net_if (required for         │
│    net_mgmt() calls)                 │
│  - Converts iface_index → net_if    │
└──────────────┬──────────────────────┘
               │
               │ net_mgmt() API
               │
┌──────────────▼──────────────────────┐
│  DECT NR+ Stack                     │
└─────────────────────────────────────┘
```

## Simplified API (No net_if Required)

The client API has been updated to **not require net_if**:

```c
/* Simple API - just use interface index (0 for first/default) */
int dect_rpc_activate(int iface_index);
int dect_rpc_deactivate(int iface_index);
int dect_rpc_status_info_get(int iface_index, struct dect_status_info *status_info);
int dect_rpc_settings_read(int iface_index, struct dect_settings *settings);
int dect_rpc_settings_write(int iface_index, const struct dect_settings *settings);
int dect_rpc_event_subscribe(int iface_index, uint32_t event_mask);

/* Convenience functions if you have net_if (optional) */
int dect_rpc_activate_iface(struct net_if *iface);
int dect_rpc_deactivate_iface(struct net_if *iface);
```

### Usage Example (No net_if)

```c
#include <dect_rpc_client.h>

/* Initialize RPC client */
dect_rpc_client_init();

/* Use interface index 0 (first/default DECT interface) */
int ret = dect_rpc_activate(0);
if (ret == 0) {
    /* Success - DECT stack activated via UART RPC */
}

/* Read status */
struct dect_status_info status;
ret = dect_rpc_status_info_get(0, &status);

/* Register event callback */
void my_event_handler(int iface_index, uint32_t event, 
                      const void *event_data, size_t event_data_len)
{
    /* Handle event - iface_index tells which interface */
}

dect_rpc_register_event_callback(my_event_handler, NULL);

/* Subscribe to events */
uint32_t event_mask = NET_EVENT_DECT_ACTIVATE_DONE |
                      NET_EVENT_DECT_SCAN_RESULT;
dect_rpc_event_subscribe(0, event_mask);
```

## Why net_if is Still Used on Server Side

The **server side** (where DECT stack runs) still needs `net_if` because:
- `net_mgmt()` API requires `struct net_if *` parameter
- This is a Zephyr networking API requirement
- The server converts `iface_index` → `net_if` internally

## Summary

| Component | Requires net_if? | Transport |
|-----------|------------------|-----------|
| **Client** | ❌ No | UART (serial) |
| **RPC Transport** | ❌ No | UART/IPC |
| **Server** | ✅ Yes (internal) | - |
| **DECT Stack** | ✅ Yes | - |

**Answer**: You can use this **entirely over UART** without requiring `net_if` on the client side! The client just needs to send interface index (integer) over UART.

