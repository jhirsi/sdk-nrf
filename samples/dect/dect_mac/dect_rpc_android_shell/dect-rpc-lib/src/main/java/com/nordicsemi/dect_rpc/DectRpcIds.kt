/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

package com.nordicsemi.dect_rpc

/**
 * Command IDs sent from client to server (server handles these)
 */
enum class DectRpcCmdServer(val value: Byte) {
    ACTIVATE(0),
    DEACTIVATE(1),
    RSSI_SCAN(2),
    SCAN(3),
    ASSOCIATION_REQ(4),
    ASSOCIATION_RELEASE(5),
    CLUSTER_START(6),
    CLUSTER_RECONFIGURE(7),
    CLUSTER_STOP(8),
    NW_BEACON_START(9),
    NW_BEACON_STOP(10),
    NETWORK_CREATE(11),
    NETWORK_REMOVE(12),
    NETWORK_JOIN(13),
    NETWORK_UNJOIN(14),
    SETTINGS_READ(15),
    SETTINGS_WRITE(16),
    STATUS_INFO_GET(17),
    NEIGHBOR_LIST(18),
    NEIGHBOR_INFO(19),
    CLUSTER_INFO(20),
    EVENT_SUBSCRIBE(21)
}

/**
 * Event IDs sent from server to client (client handles these)
 */
enum class DectRpcEvtClient(val value: Byte) {
    ACTIVATE_DONE(0),
    DEACTIVATE_DONE(1),
    RSSI_SCAN_RESULT(2),
    RSSI_SCAN_DONE(3),
    SCAN_RESULT(4),
    SCAN_DONE(5),
    ASSOCIATION_CHANGED(6),
    NETWORK_STATUS(7),
    SINK_STATUS(8),
    CLUSTER_CREATED_RESULT(9),
    CLUSTER_STOPPED_RESULT(10),
    NW_BEACON_START_RESULT(11),
    NW_BEACON_STOP_RESULT(12),
    NEIGHBOR_LIST(13),
    NEIGHBOR_INFO(14),
    CLUSTER_INFO(15)
}


