# DECT NR+ RPC Implementation Notes

## Directory Structure

```
nrf/subsys/net/l2_dect_nrp/rpc/
├── common/
│   ├── dect_rpc_ids.h          # RPC command/event ID definitions
│   ├── dect_rpc_common.h       # Common helper functions
│   └── dect_rpc_common.c       # Common helper implementations
├── client/
│   ├── dect_rpc_client.h       # Client API header
│   ├── dect_rpc_client.c       # Client implementation
│   ├── dect_rpc_serialize.c    # Client-side serialization
│   └── dect_rpc_events.c       # Client-side event handling
└── server/
    ├── dect_rpc_server.h       # Server API header
    ├── dect_rpc_server.c       # Server implementation
    ├── dect_rpc_handlers.c     # RPC command handlers
    ├── dect_rpc_deserialize.c  # Server-side deserialization
    └── dect_rpc_event_fwd.c   # Event forwarding from net_mgmt to RPC
```

## Implementation Steps

### Step 1: RPC Group Definition

Define the RPC group in both client and server:

```c
// In client and server code
NRF_RPC_GROUP_DECLARE(dect_rpc_group);
```

For UART transport (as configured in prj.conf):
```c
NRF_RPC_UART_TRANSPORT(dect_rpc_group_tr, DEVICE_DT_GET(DT_NODELABEL(uart0)));
NRF_RPC_GROUP_DEFINE(dect_rpc_group, "dect_rpc", &dect_rpc_group_tr, NULL, NULL, NULL);
```

### Step 2: Basic Command Implementation (Example: Activate)

**Client side** (`dect_rpc_client.c`):
```c
int dect_rpc_activate(struct net_if *iface)
{
    struct nrf_rpc_cbor_ctx ctx;
    int result;
    int iface_index = dect_rpc_get_iface_index(iface);
    
    if (iface_index < 0) {
        return -EINVAL;
    }
    
    NRF_RPC_CBOR_ALLOC(&dect_rpc_group, ctx, 4);
    nrf_rpc_encode_int(&ctx, iface_index);
    
    nrf_rpc_cbor_cmd_rsp_no_err(&dect_rpc_group, DECT_RPC_CMD_ACTIVATE, &ctx);
    
    result = nrf_rpc_decode_int(&ctx);
    
    if (!nrf_rpc_decoding_done_and_check(&dect_rpc_group, &ctx)) {
        dect_rpc_report_rsp_decoding_error(DECT_RPC_CMD_ACTIVATE);
        return -EIO;
    }
    
    return result;
}
```

**Server side** (`dect_rpc_handlers.c`):
```c
static void dect_rpc_cmd_activate(const struct nrf_rpc_group *group,
                                   struct nrf_rpc_cbor_ctx *ctx,
                                   void *handler_data)
{
    struct net_if *iface;
    int iface_index;
    int ret;
    
    iface_index = nrf_rpc_decode_int(ctx);
    
    if (!nrf_rpc_decoding_done_and_check(group, ctx)) {
        dect_rpc_report_cmd_decoding_error(DECT_RPC_CMD_ACTIVATE);
        return;
    }
    
    iface = dect_rpc_get_iface_by_index(iface_index);
    if (!iface) {
        nrf_rpc_rsp_send_int(group, -EINVAL);
        return;
    }
    
    ret = net_mgmt(NET_REQUEST_DECT_ACTIVATE, iface, NULL, 0);
    
    nrf_rpc_rsp_send_int(group, ret);
}

NRF_RPC_CBOR_CMD_DECODER(dect_rpc_group, dect_rpc_cmd_activate,
                         DECT_RPC_CMD_ACTIVATE, dect_rpc_cmd_activate, NULL);
```

### Step 3: Event Forwarding

**Server side** (`dect_rpc_event_fwd.c`):
```c
static void dect_rpc_net_mgmt_event_cb(struct net_mgmt_event_callback *cb,
                                       uint32_t mgmt_event,
                                       struct net_if *iface)
{
    struct nrf_rpc_cbor_ctx ctx;
    enum dect_rpc_cmd_client event_id;
    int iface_index;
    
    // Map net_mgmt event to RPC event ID
    if (mgmt_event == NET_EVENT_DECT_ACTIVATE_DONE) {
        event_id = DECT_RPC_EVT_ACTIVATE_DONE;
    } else if (mgmt_event == NET_EVENT_DECT_SCAN_RESULT) {
        event_id = DECT_RPC_EVT_SCAN_RESULT;
    }
    // ... map other events
    
    iface_index = dect_rpc_get_iface_index(iface);
    
    // Serialize event data based on event type
    NRF_RPC_CBOR_ALLOC(&dect_rpc_group, ctx, estimated_size);
    
    nrf_rpc_encode_int(&ctx, iface_index);
    nrf_rpc_encode_uint(&ctx, mgmt_event);
    
    // Encode event-specific data
    // (This depends on the event type - see dect_net_l2_mgmt.h for event structures)
    
    nrf_rpc_cbor_evt(&dect_rpc_group, event_id, &ctx);
}

static struct net_mgmt_event_callback dect_rpc_event_cb;

void dect_rpc_server_init_events(void)
{
    net_mgmt_init_event_callback(&dect_rpc_event_cb,
                                  dect_rpc_net_mgmt_event_cb,
                                  NET_EVENT_DECT_ACTIVATE_DONE |
                                  NET_EVENT_DECT_DEACTIVATE_DONE |
                                  NET_EVENT_DECT_SCAN_RESULT |
                                  NET_EVENT_DECT_SCAN_DONE |
                                  NET_EVENT_DECT_RSSI_SCAN_RESULT |
                                  NET_EVENT_DECT_RSSI_SCAN_DONE |
                                  NET_EVENT_DECT_ASSOCIATION_CHANGED |
                                  NET_EVENT_DECT_NETWORK_STATUS |
                                  NET_EVENT_DECT_SINK_STATUS |
                                  NET_EVENT_DECT_CLUSTER_CREATED_RESULT |
                                  NET_EVENT_DECT_CLUSTER_STOPPED_RESULT |
                                  NET_EVENT_DECT_NW_BEACON_START_RESULT |
                                  NET_EVENT_DECT_NW_BEACON_STOP_RESULT |
                                  NET_EVENT_DECT_NEIGHBOR_LIST |
                                  NET_EVENT_DECT_NEIGHBOR_INFO |
                                  NET_EVENT_DECT_CLUSTER_INFO);
    
    net_mgmt_add_event_callback(&dect_rpc_event_cb);
}
```

**Client side** (`dect_rpc_events.c`):
```c
static void (*event_callback)(struct net_if *iface, uint32_t event,
                              const void *event_data, size_t event_data_len);
static void *event_callback_user_data;

static void dect_rpc_evt_handler(const struct nrf_rpc_group *group,
                                 struct nrf_rpc_cbor_ctx *ctx,
                                 void *handler_data)
{
    struct net_if *iface;
    uint32_t mgmt_event;
    int iface_index;
    const void *event_data = NULL;
    size_t event_data_len = 0;
    
    iface_index = nrf_rpc_decode_int(ctx);
    mgmt_event = nrf_rpc_decode_uint(ctx);
    
    iface = dect_rpc_get_iface_by_index(iface_index);
    
    // Decode event-specific data based on event type
    // ...
    
    if (event_callback) {
        event_callback(iface, mgmt_event, event_data, event_data_len);
    }
}

// Register handlers for each event type
NRF_RPC_CBOR_EVT_DECODER(dect_rpc_group, dect_rpc_evt_activate_done,
                         DECT_RPC_EVT_ACTIVATE_DONE, dect_rpc_evt_handler, NULL);
// ... register other event decoders

void dect_rpc_register_event_callback(
    void (*callback)(struct net_if *iface, uint32_t event,
                     const void *event_data, size_t event_data_len),
    void *user_data)
{
    event_callback = callback;
    event_callback_user_data = user_data;
}
```

### Step 4: Complex Data Structure Serialization

For structures like `dect_settings` and `dect_status_info`, create serialization helpers:

**Client side** (`dect_rpc_serialize.c`):
```c
void dect_rpc_encode_settings(struct nrf_rpc_cbor_ctx *ctx,
                               const struct dect_settings *settings)
{
    // Serialize each field
    nrf_rpc_encode_bool(&ctx, settings->cmd_params.reset_to_driver_defaults);
    nrf_rpc_encode_uint(&ctx, settings->cmd_params.write_scope_bitmap);
    // ... continue for all fields
}

void dect_rpc_decode_settings(struct nrf_rpc_cbor_ctx *ctx,
                               struct dect_settings *settings)
{
    // Deserialize each field
    settings->cmd_params.reset_to_driver_defaults = nrf_rpc_decode_bool(ctx);
    settings->cmd_params.write_scope_bitmap = nrf_rpc_decode_uint(ctx);
    // ... continue for all fields
}
```

## Testing

Create a test application similar to `nrf/samples/nrf_rpc/protocols_serialization`:

1. **Client application**: Runs on one core, calls DECT RPC functions
2. **Server application**: Runs on another core, handles RPC and manages DECT stack

## Configuration

Add to `prj.conf`:

```kconfig
# Enable DECT RPC
CONFIG_DECT_NRP_RPC=y
CONFIG_DECT_NRP_RPC_CLIENT=y  # or CONFIG_DECT_NRP_RPC_SERVER=y
```

## Next Steps

1. Implement basic commands (activate, deactivate, status_info_get)
2. Implement event forwarding mechanism
3. Add serialization for complex structures
4. Implement remaining commands
5. Create test application
6. Add documentation

