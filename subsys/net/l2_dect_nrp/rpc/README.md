# DECT NR+ RPC Implementation - MVP

## Overview

This directory contains the MVP (Minimum Viable Product) implementation of RPC support for DECT NR+ net mgmt access. The implementation allows DECT NR+ stack management commands to be called remotely via nRF RPC.

## Architecture

```
┌─────────────────────────────────────┐
│         RPC Client (nRF Cloud)      │
│  - dect_rpc_activate()              │
│  - dect_rpc_status_info_get()      │
│  - dect_rpc_settings_read/write()  │
└──────────────┬──────────────────────┘
               │ RPC (CBOR over UART/IPC)
┌──────────────▼──────────────────────┐
│         RPC Server (DECT Stack)     │
│  - net_mgmt() handlers              │
│  - Event forwarding                 │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│      DECT NR+ Stack (net_mgmt)      │
└─────────────────────────────────────┘
```

## Files Structure

```
rpc/
├── common/
│   ├── dect_rpc_ids.h          # RPC command/event IDs
│   ├── dect_rpc_common.h       # Common helper functions
│   ├── dect_rpc_common.c       # Common implementations
│   ├── dect_rpc_group.h        # RPC group declaration
│   ├── dect_rpc_group.c        # RPC group definition
│   ├── dect_rpc_serialize.h    # Serialization API
│   └── dect_rpc_serialize.c    # Serialization implementations
├── client/
│   ├── dect_rpc_client.h       # Client API
│   └── dect_rpc_client.c      # Client implementation
└── server/
    ├── dect_rpc_server.h       # Server API
    └── dect_rpc_server.c      # Server implementation
```

## MVP Features

### Implemented Commands

1. **Activate/Deactivate**
   - `dect_rpc_activate()` - Activate DECT NR+ stack
   - `dect_rpc_deactivate()` - Deactivate DECT NR+ stack

2. **Status Information**
   - `dect_rpc_status_info_get()` - Get current stack status

3. **Settings Management**
   - `dect_rpc_settings_read()` - Read DECT settings
   - `dect_rpc_settings_write()` - Write DECT settings

4. **Event Subscription**
   - `dect_rpc_event_subscribe()` - Subscribe to DECT events
   - `dect_rpc_register_event_callback()` - Register event callback

### Event Forwarding

The server forwards the following DECT events to the client:
- `NET_EVENT_DECT_ACTIVATE_DONE`
- `NET_EVENT_DECT_DEACTIVATE_DONE`
- `NET_EVENT_DECT_SCAN_RESULT`
- `NET_EVENT_DECT_SCAN_DONE`
- `NET_EVENT_DECT_ASSOCIATION_CHANGED`
- `NET_EVENT_DECT_NETWORK_STATUS`
- `NET_EVENT_DECT_SINK_STATUS`
- And more...

## Usage

### Client Side

```c
#include <dect_rpc_client.h>

/* Initialize RPC client */
dect_rpc_client_init();

/* Get DECT interface */
struct net_if *dect_iface = net_if_get_by_name("dect0");

/* Activate DECT stack */
int ret = dect_rpc_activate(dect_iface);
if (ret == 0) {
    /* Success */
}

/* Read status */
struct dect_status_info status;
ret = dect_rpc_status_info_get(dect_iface, &status);

/* Register event callback */
dect_rpc_register_event_callback(my_event_handler, NULL);

/* Subscribe to events */
uint32_t event_mask = NET_EVENT_DECT_ACTIVATE_DONE |
                      NET_EVENT_DECT_SCAN_RESULT;
dect_rpc_event_subscribe(dect_iface, event_mask);
```

### Server Side

```c
#include <dect_rpc_server.h>

/* Initialize RPC server */
dect_rpc_server_init();

/* Server automatically handles RPC commands and forwards events */
```

## Configuration

Add to `prj.conf`:

```kconfig
# Enable nRF RPC
CONFIG_NRF_RPC=y
CONFIG_NRF_RPC_CBOR=y
CONFIG_NRF_RPC_UART_TRANSPORT=y
CONFIG_NRF_RPC_UART_RELIABLE=y

# Enable DECT RPC
CONFIG_DECT_NRP_RPC=y
CONFIG_DECT_NRP_RPC_CLIENT=y  # or CONFIG_DECT_NRP_RPC_SERVER=y
CONFIG_DECT_NRP_RPC_LOG_LEVEL_DBG=y
```

## Serialization

The implementation uses CBOR serialization for all data structures:

- **Simple types**: Direct encoding (int, uint, bool)
- **Complex structures**: Field-by-field serialization
  - `dect_settings` - All fields serialized
  - `dect_status_info` - All fields including arrays
  - `in6_addr` - 16-byte buffer

## Limitations (MVP)

1. **Event Data**: Events are forwarded with basic info only (event type, iface). Full event data structures can be added in future versions.

2. **Single Client**: Current implementation supports one client subscription mask. Can be extended for multiple clients.

3. **Error Handling**: Basic error handling implemented. Can be enhanced.

## Future Enhancements

- Full event data serialization
- Multiple client support
- Remaining DECT commands (scan, associate, cluster operations)
- Enhanced error reporting
- Performance optimizations

## Testing

To test the implementation:

1. Build client application with `CONFIG_DECT_NRP_RPC_CLIENT=y`
2. Build server application with `CONFIG_DECT_NRP_RPC_SERVER=y`
3. Connect via UART or IPC
4. Call RPC functions from client
5. Verify responses and events

## Notes

- The linter may show false positives for `zephyr/net/net_if.h` - this is a standard Zephyr header and should compile correctly.
- Interface pointers are converted to indices for cross-core communication.
- All RPC operations are synchronous (commands) or asynchronous (events).

