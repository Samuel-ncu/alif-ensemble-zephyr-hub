/*
 * Copyright (c) 2026 Alif Semiconductor / Antigravity
 * SPDX-License-Identifier: Apache-2.0
 *
 * Inter-Processor Communication (IPC) Protocol Definition
 * Designed for 32-bit register channels over ARM MHUv2.
 */

#ifndef IPC_PROTOCOL_H_
#define IPC_PROTOCOL_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * MHUv2 32-bit Register Payload Bitfield:
 * [31:24] : Opcode (Command or Response)
 * [23:16] : Sequence Number (0..255)
 * [15:0]  : 16-bit Payload or Parameter
 */

/* Master -> Worker Opcodes */
#define IPC_OP_PING            0x01  /* Standard ping test */
#define IPC_OP_LED_SET         0x02  /* Remote LED control (param: 0=OFF, 1=ON, 2=TOGGLE) */
#define IPC_OP_GET_METRICS     0x03  /* Request worker runtime metrics */
#define IPC_OP_BURST_PING      0x04  /* Fast burst ping (no worker serial print) */

/* Worker -> Master Opcodes */
#define IPC_RESP_PONG          0x81  /* Ping response (payload: worker pong count) */
#define IPC_RESP_LED_STATUS    0x82  /* LED status response (payload: 0=OFF, 1=ON) */
#define IPC_RESP_METRICS       0x83  /* Worker metrics response (payload: rx count) */
#define IPC_RESP_BURST_PONG    0x84  /* Fast burst response */

/* Macro utilities for encoding and decoding */
#define IPC_PACK_MSG(opcode, seq, payload) \
	((((uint32_t)(opcode) & 0xFF) << 24) | \
	 (((uint32_t)(seq) & 0xFF) << 16) | \
	 ((uint32_t)(payload) & 0xFFFF))

#define IPC_GET_OPCODE(msg)   (((uint32_t)(msg) >> 24) & 0xFF)
#define IPC_GET_SEQ(msg)      (((uint32_t)(msg) >> 16) & 0xFF)
#define IPC_GET_PAYLOAD(msg)  ((uint32_t)(msg) & 0xFFFF)

#ifdef __cplusplus
}
#endif

#endif /* IPC_PROTOCOL_H_ */

