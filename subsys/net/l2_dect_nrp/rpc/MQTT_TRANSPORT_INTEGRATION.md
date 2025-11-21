# DECT RPC MQTT Transport Integration Guide

## Overview

This guide explains how to integrate the DECT RPC MQTT transport with nRF Cloud. The MQTT transport enables remote control of the DECT NR+ stack from nRF Cloud or any MQTT client.

## Architecture

The MQTT transport uses nRF Cloud's public event API (`NRF_CLOUD_EVT_RX_DATA_GENERAL`) to receive DECT RPC commands, similar to how `cloud_shell_parse_desh_cmd()` works. This approach:

- ✅ **No nRF Cloud library modifications required**
- ✅ **Uses public nRF Cloud API**
- ✅ **Follows existing patterns (like DeSh shell)**
- ✅ **Clean separation of concerns**

## Integration Steps

### Step 1: Application Initialization Sequence

The application must initialize components in the correct order and handle nRF Cloud events:

```c
#include <zephyr/net/nrf_cloud.h>
#include <net/l2_dect_nrp/rpc/common/dect_rpc_mqtt_transport.h>
#include <net/l2_dect_nrp/rpc/server/dect_rpc_server.h>
#include <nrf_rpc.h>

/* nRF Cloud event handler */
static void nrf_cloud_event_handler(const struct nrf_cloud_evt *evt)
{
	switch (evt->type) {
	case NRF_CLOUD_EVT_TRANSPORT_CONNECTED:
		LOG_INF("nRF Cloud MQTT connected");
		break;
	case NRF_CLOUD_EVT_READY:
		LOG_INF("nRF Cloud ready");
		break;
	case NRF_CLOUD_EVT_RX_DATA_GENERAL:
		/* Check if this is a DECT RPC packet */
		if (dect_rpc_mqtt_transport_handle_rx_data(&evt->data)) {
			/* Data was handled by DECT RPC transport */
			LOG_DBG("DECT RPC packet received and handled");
		}
		/* If not handled, it's other cloud data - process as needed */
		break;
	case NRF_CLOUD_EVT_TRANSPORT_DISCONNECTED:
		LOG_INF("nRF Cloud disconnected");
		break;
	default:
		break;
	}
}

int main(void)
{
	int ret;

	/* 1. Initialize nRF Cloud */
	struct nrf_cloud_init_param init_param = {
		.event_handler = nrf_cloud_event_handler,
		/* ... other params ... */
	};
	
	ret = nrf_cloud_init(&init_param);
	if (ret) {
		LOG_ERR("nRF Cloud init failed: %d", ret);
		return ret;
	}

	/* 2. Connect to nRF Cloud */
	ret = nrf_cloud_connect();
	if (ret) {
		LOG_ERR("nRF Cloud connect failed: %d", ret);
		return ret;
	}

	/* Wait for MQTT connection (or handle in event callback) */
	/* ... */

	/* 3. Initialize nRF RPC */
	ret = nrf_rpc_init(err_handler);
	if (ret) {
		LOG_ERR("RPC init failed: %d", ret);
		return ret;
	}

	/* 4. Initialize DECT RPC server */
	ret = dect_rpc_server_init();
	if (ret) {
		LOG_ERR("DECT RPC server init failed: %d", ret);
		return ret;
	}

	return 0;
}
```

### Step 2: Configuration

Add to your `prj.conf`:

```kconfig
# nRF Cloud MQTT
CONFIG_NRF_CLOUD=y
CONFIG_NRF_CLOUD_MQTT=y

# DECT RPC with MQTT transport
CONFIG_DECT_NRP_RPC=y
CONFIG_DECT_NRP_RPC_SERVER=y
CONFIG_DECT_NRP_RPC_MQTT_TRANSPORT=y
```

## Data Flow

### Receiving Data (Cloud → Device)

1. Cloud publishes data to nRF Cloud data channel (c2d topic)
2. nRF Cloud receives MQTT message
3. nRF Cloud routes to `NRF_CLOUD_EVT_RX_DATA_GENERAL` event
4. Application's event handler calls `dect_rpc_mqtt_transport_handle_rx_data()`
5. Transport checks if data is DECT RPC packet (not JSON)
6. If valid, routes to nRF RPC receive handler

### Sending Data (Device → Cloud)

1. nRF RPC calls transport `send()` function
2. Transport uses `nrf_cloud_send()` with `NRF_CLOUD_TOPIC_MESSAGE`
3. nRF Cloud publishes to data channel (d2c topic)
4. Cloud receives on standard nRF Cloud data channel

**Note**: The transport uses nRF Cloud's standard data channel topics. No custom topic subscription needed!

## Cloud-Side Client

To control DECT NR+ from the cloud, send data via nRF Cloud's standard data channel:

- **Send commands**: Use nRF Cloud REST API or MQTT to send data to device
  - Data: nRF RPC packet (5-byte header + CBOR payload)
  - The data will be received as `NRF_CLOUD_EVT_RX_DATA_GENERAL`
  
- **Receive responses**: Monitor nRF Cloud data channel for responses
  - Responses are sent via `nrf_cloud_send()` with `NRF_CLOUD_TOPIC_MESSAGE`
  - Standard nRF Cloud data channel (d2c topic)

## Testing

1. Build and flash the server application with MQTT transport enabled
2. Connect device to nRF Cloud
3. Use nRF Cloud web portal or MQTT client to publish commands
4. Monitor responses and events on the respective topics

## Troubleshooting

### Transport Not Initialized

- Ensure nRF Cloud MQTT is connected before initializing DECT RPC
- Check that `nrf_rpc_init()` was called after nRF Cloud connection

### Messages Not Received

- Verify `dect_rpc_mqtt_transport_handle_rx_data()` is called in event handler
- Check that data is not JSON (doesn't start with '{')
- Verify data has minimum size (at least 5 bytes for RPC header)
- Check nRF Cloud connection status

### Compilation Errors

- Ensure `CONFIG_NRF_CLOUD_MQTT=y` is set
- Ensure `CONFIG_DECT_NRP_RPC_MQTT_TRANSPORT=y` is set
- Check that nRF Cloud headers are accessible

## Packet Identification

The transport identifies DECT RPC packets using heuristics:

1. **Minimum size**: Must be at least 5 bytes (nRF RPC header size)
2. **Not JSON**: Doesn't start with '{' (0x7B)
3. **Binary data**: Contains non-printable characters (typical for binary RPC)

This allows the transport to coexist with other cloud data (like JSON commands) on the same data channel.

## Future Improvements

1. **Magic Marker**: Add optional magic byte sequence for more reliable identification
2. **Topic-based Routing**: Use custom topics if nRF Cloud supports it
3. **Connection State Management**: Auto-retry on reconnection
4. **Packet Validation**: Validate nRF RPC header format

