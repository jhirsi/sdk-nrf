/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

package com.nordicsemi.dect_rpc

import co.nstant.`in`.cbor.CborEncoder
import co.nstant.`in`.cbor.CborDecoder
import co.nstant.`in`.cbor.model.DataItem
import java.io.ByteArrayOutputStream
import java.io.ByteArrayInputStream

/**
 * nRF RPC protocol implementation
 */
internal object NrfRpcProtocol {
    // nRF RPC packet types (from nrf_rpc.h)
    private const val RPC_PACKET_TYPE_EVT: Byte = 0x00
    private const val RPC_PACKET_TYPE_RSP: Byte = 0x01
    private const val RPC_PACKET_TYPE_ACK: Byte = 0x02
    private const val RPC_PACKET_TYPE_ERR: Byte = 0x03
    private const val RPC_PACKET_TYPE_INIT: Byte = 0x04
    private const val RPC_PACKET_TYPE_CMD: Byte = 0x80.toByte()

    private const val RPC_GROUP_ID: Byte = 0x00 // dect_rpc group ID
    private const val RPC_ID_UNKNOWN: Byte = 0xFF.toByte()
    private const val RPC_HEADER_SIZE = 5

    /**
     * Encode a command packet
     * Header format (5 bytes):
     * Byte 0: Packet type (0x80) | Source context ID (7 bits)
     * Byte 1: Command ID
     * Byte 2: Destination context ID (0xFF = unknown)
     * Byte 3: Source group ID
     * Byte 4: Destination group ID
     */
    fun encodeCommand(cmdId: Byte, srcContextId: Byte, cborPayload: DataItem): ByteArray {
        val payloadBytes = encodeCbor(cborPayload)
        val packet = ByteArray(RPC_HEADER_SIZE + payloadBytes.size)

        // Header according to nRF RPC protocol specification
        packet[0] = (RPC_PACKET_TYPE_CMD.toInt() or (srcContextId.toInt() and 0x7F)).toByte() // Type | Source context ID
        packet[1] = cmdId                                                // Command ID
        packet[2] = RPC_ID_UNKNOWN                                       // Destination context ID (unknown initially)
        packet[3] = RPC_GROUP_ID                                         // Source group ID
        packet[4] = RPC_GROUP_ID                                         // Destination group ID

        // Payload
        System.arraycopy(payloadBytes, 0, packet, RPC_HEADER_SIZE, payloadBytes.size)

        return packet
    }

    /**
     * Decode a packet (response, event, etc.)
     * According to nRF RPC protocol:
     * - Command packets (0x80): Byte 0 = 0x80 | source context ID (7 bits)
     * - Response packets (0x01): Byte 0 = 0x01 | source context ID (7 bits), Byte 1 = 0xFF (not used)
     * - Other packets: Byte 0 = packet type, no context ID in byte 0
     */
    fun decodePacket(packet: ByteArray): PacketInfo {
        if (packet.size < RPC_HEADER_SIZE) {
            throw IllegalArgumentException("Invalid packet size")
        }

        val byte0 = packet[0]
        val packetType: Byte
        val srcContextId: Byte

        // Extract packet type and source context ID
        if ((byte0.toInt() and 0x80) != 0) {
            // Command packet: type is 0x80, context ID is in lower 7 bits
            packetType = RPC_PACKET_TYPE_CMD
            srcContextId = (byte0.toInt() and 0x7F).toByte()
        } else if (byte0 == RPC_PACKET_TYPE_RSP) {
            // Response packet: type is 0x01, source context ID is in lower 7 bits of byte 0
            // According to protocol spec, response packets have source context ID in byte 0
            packetType = RPC_PACKET_TYPE_RSP
            srcContextId = (byte0.toInt() and 0x7F).toByte()
        } else {
            // Other packet types (event, ack, error, init): type is in byte 0, no context ID
            packetType = byte0
            srcContextId = RPC_ID_UNKNOWN
        }

        val cmdId = packet[1]
        val dstContextId = packet[2]
        val srcGroupId = packet[3]
        val dstGroupId = packet[4]

        val payload = ByteArray(packet.size - RPC_HEADER_SIZE)
        System.arraycopy(packet, RPC_HEADER_SIZE, payload, 0, payload.size)

        return PacketInfo(packetType, cmdId, srcContextId, dstContextId, srcGroupId, dstGroupId, payload)
    }

    /**
     * Encode CBOR DataItem to bytes
     */
    private fun encodeCbor(dataItem: DataItem): ByteArray {
        val baos = ByteArrayOutputStream()
        CborEncoder(baos).encode(dataItem)
        return baos.toByteArray()
    }

    /**
     * Decode CBOR bytes to DataItem
     */
    fun decodeCbor(data: ByteArray): DataItem {
        val bais = ByteArrayInputStream(data)
        val decoder = CborDecoder(bais)
        val dataItems = decoder.decode()
        if (dataItems.isEmpty()) {
            throw IllegalArgumentException("Empty CBOR data")
        }
        return dataItems[0]
    }

    /**
     * Packet information
     */
    data class PacketInfo(
        val packetType: Byte,
        val cmdId: Byte,
        val srcContextId: Byte,
        val dstContextId: Byte,
        val srcGroupId: Byte,
        val dstGroupId: Byte,
        val payload: ByteArray
    )
}


