/*
 * Copyright (c) 2026 Alif Semiconductor / Antigravity
 * SPDX-License-Identifier: Apache-2.0
 *
 * Example 07: Dual-Core IPC Master & Telemetry (Cortex-M55 HP)
 * Target Core: alif_e8_dk/ae822fa0e5597xx0/rtss_hp
 * Inter-Processor Communication over ARM MHUv2 Mailbox & Hardware Semaphores
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/ipm.h>
#include <SEGGER_RTT.h>

#include "ipc_protocol.h"

/* Mailbox Devices */
static const struct device *mhu_r;
static const struct device *mhu_s;

/* LEDs: Red (P6_2) and Blue (P6_6) */
static const struct gpio_dt_spec led_red = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led_blue = GPIO_DT_SPEC_GET(DT_ALIAS(led_blue), gpios);

/* Synchronization primitives */
static K_SEM_DEFINE(sem_tx_ack, 0, 1);
static K_SEM_DEFINE(sem_rx_pong, 0, 1);

/* Latest received response storage */
static volatile uint32_t last_rx_msg = 0;
static volatile bool rx_flag = false;

/* Master state */
static uint8_t g_seq = 0;
static bool g_auto_heartbeat = true;
static uint32_t g_total_sent = 0;
static uint32_t g_total_received = 0;

/* Receiver ISR Callback */
static void mhu_recv_cb(const struct device *dev, void *user_data,
			uint32_t channel_id, volatile void *data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);
	ARG_UNUSED(channel_id);

	if (data != NULL) {
		last_rx_msg = *((uint32_t *)data);
		rx_flag = true;
		g_total_received++;
		k_sem_give(&sem_rx_pong);
	}
}

/* Sender ISR Callback (Handshake Completed) */
static void mhu_send_cb(const struct device *dev, void *user_data,
			uint32_t channel_id, volatile void *data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);
	ARG_UNUSED(channel_id);
	ARG_UNUSED(data);

	k_sem_give(&sem_tx_ack);
}

/* Synchronous IPC Transaction with Microsecond Latency Measurement */
static int ipc_transact(uint8_t opcode, uint16_t payload, uint32_t *resp_out, uint32_t *latency_us)
{
	uint8_t seq = ++g_seq;
	uint32_t tx_msg = IPC_PACK_MSG(opcode, seq, payload);
	int ret;

	/* Drain any stale semaphore */
	k_sem_reset(&sem_rx_pong);
	k_sem_reset(&sem_tx_ack);
	rx_flag = false;

	/* Pulse Blue LED on transmit */
	gpio_pin_set_dt(&led_blue, 1);

	uint32_t t_start = k_cycle_get_32();

	/* 1. Send 32-bit packet over MHU0 Channel 0 */
	ret = ipm_send(mhu_s, 0, 0, &tx_msg, sizeof(tx_msg));
	if (ret != 0) {
		gpio_pin_set_dt(&led_blue, 0);
		return ret;
	}
	g_total_sent++;

	/* 2. Wait for remote core to process and respond */
	ret = k_sem_take(&sem_rx_pong, K_MSEC(250));

	uint32_t t_end = k_cycle_get_32();
	gpio_pin_set_dt(&led_blue, 0);

	if (ret != 0) {
		/* Timeout: Remote worker did not reply in time */
		return -ETIMEDOUT;
	}

	uint32_t elapsed_cycles = t_end - t_start;
	if (latency_us != NULL) {
		*latency_us = k_cyc_to_us_floor32(elapsed_cycles);
	}

	if (resp_out != NULL) {
		*resp_out = last_rx_msg;
	}

	return 0;
}

static void print_banner(void)
{
	printf("\n\n");
	printf("=====================================================\n");
	printf("  Alif Ensemble Dual-Core IPC Master (M55-HP)        \n");
	printf("  Inter-Processor Communication via ARM MHUv2        \n");
	printf("  Telemetry Console @ 0x20000410 | CoreSight AP[0]   \n");
	printf("=====================================================\n");
	printf("Master Core : Cortex-M55 HP (400 MHz)\n");
	printf("Worker Core : Cortex-M55 HE (160 MHz)\n");
	printf("Hardware Bus: ARM MHUv2 Mailbox (Shared Interconnect)\n");
	printf("  - Sender Register   : 0x40090000 (rtsshe_rtsshp_mhu0_s)\n");
	printf("  - Receiver Register : 0x40080000 (rtsshp_rtsshe_mhu0_r)\n");
	printf("=====================================================\n");
	printf("Interactive Commands:\n");
	printf("  [p] : Send Single Ping & Measure Hardware Latency\n");
	printf("  [l] : Remote Toggle M55-HE Worker Green LED (P6_4)\n");
	printf("  [b] : Run 100-Packet Burst Stress Benchmark\n");
	printf("  [s] : Query Remote M55-HE Status & Metrics\n");
	printf("  [a] : Toggle Auto-Heartbeat Telemetry (Default: ON)\n");
	printf("  [h] : Show this Help Menu\n");
	printf("=====================================================\n\n");
	fflush(stdout);
}

static void do_single_ping(void)
{
	uint32_t resp = 0;
	uint32_t latency = 0;

	printf("[CMD: Ping] Sending Ping packet (Seq: #%u)...\n", (uint8_t)(g_seq + 1));
	int ret = ipc_transact(IPC_OP_PING, 0xABCD, &resp, &latency);

	if (ret == 0) {
		uint8_t resp_op = IPC_GET_OPCODE(resp);
		uint8_t resp_seq = IPC_GET_SEQ(resp);
		uint16_t resp_payload = IPC_GET_PAYLOAD(resp);

		printf("  --> [SUCCESS] Pong received from M55-HE!\n");
		printf("      - Round-Trip Latency: %u us\n", latency);
		printf("      - Remote Opcode     : 0x%02X (%s)\n", resp_op,
		       (resp_op == IPC_RESP_PONG) ? "IPC_RESP_PONG" : "UNKNOWN");
		printf("      - Sequence Matched  : #%u (Sent: #%u)\n", resp_seq, g_seq);
		printf("      - Remote RX Counter : %u packets\n", resp_payload);
	} else if (ret == -ETIMEDOUT) {
		printf("  --> [TIMEOUT] No response from M55-HE within 250ms!\n");
		printf("      (Ensure Core HE binary is booted into MRAM @ 0x80000000)\n");
		gpio_pin_toggle_dt(&led_red);
	} else {
		printf("  --> [ERROR] ipm_send failed with code %d\n", ret);
	}
	fflush(stdout);
}

static void do_toggle_remote_led(void)
{
	uint32_t resp = 0;
	uint32_t latency = 0;

	printf("[CMD: LED] Sending Remote LED TOGGLE command to M55-HE...\n");
	int ret = ipc_transact(IPC_OP_LED_SET, 2, &resp, &latency);

	if (ret == 0) {
		uint16_t state = IPC_GET_PAYLOAD(resp);
		printf("  --> [SUCCESS] Remote Green LED state is now: %s (Latency: %u us)\n",
		       state ? "ON" : "OFF", latency);
	} else {
		printf("  --> [FAIL] LED toggle failed (err: %d)\n", ret);
	}
	fflush(stdout);
}

static void do_query_metrics(void)
{
	uint32_t resp = 0;
	uint32_t latency = 0;

	printf("[CMD: Metrics] Querying M55-HE system telemetry...\n");
	int ret = ipc_transact(IPC_OP_GET_METRICS, 0, &resp, &latency);

	if (ret == 0) {
		uint16_t rx_pkts = IPC_GET_PAYLOAD(resp);
		printf("  --> [TELEMETRY REPORT]\n");
		printf("      - Master Total Sent     : %u pkts\n", g_total_sent);
		printf("      - Master Total Received : %u pkts\n", g_total_received);
		printf("      - Worker Total Packets  : %u pkts\n", rx_pkts);
		printf("      - Last Query Latency    : %u us\n", latency);
	} else {
		printf("  --> [FAIL] Query metrics failed (err: %d)\n", ret);
	}
	fflush(stdout);
}

static void do_burst_benchmark(void)
{
	const int BURST_COUNT = 100;
	uint32_t min_us = 0xFFFFFFFF;
	uint32_t max_us = 0;
	uint64_t total_us = 0;
	int success_count = 0;

	printf("\n>>> [BENCHMARK] Starting %d-packet back-to-back IPC burst test...\n", BURST_COUNT);
	fflush(stdout);

	uint32_t t_burst_start = k_cycle_get_32();

	for (int i = 0; i < BURST_COUNT; i++) {
		uint32_t resp = 0;
		uint32_t lat = 0;
		int ret = ipc_transact(IPC_OP_BURST_PING, (uint16_t)i, &resp, &lat);

		if (ret == 0) {
			success_count++;
			if (lat < min_us) min_us = lat;
			if (lat > max_us) max_us = lat;
			total_us += lat;
		} else {
			break;
		}
	}

	uint32_t t_burst_end = k_cycle_get_32();
	uint32_t burst_total_us = k_cyc_to_us_floor32(t_burst_end - t_burst_start);

	printf(">>> [BENCHMARK RESULTS]\n");
	printf("    - Delivered Packets : %d / %d (Success Rate: %d%%)\n",
	       success_count, BURST_COUNT, (success_count * 100) / BURST_COUNT);

	if (success_count > 0 && burst_total_us > 0) {
		uint32_t avg_us = (uint32_t)(total_us / success_count);
		uint32_t throughput = (uint32_t)(((uint64_t)success_count * 1000000ULL) / burst_total_us);

		printf("    - Min Latency       : %u us\n", min_us);
		printf("    - Avg Latency       : %u us\n", avg_us);
		printf("    - Max Latency       : %u us\n", max_us);
		printf("    - Total Burst Time  : %u us (%u ms)\n", burst_total_us, burst_total_us / 1000);
		printf("    - IPC Throughput    : %u transactions / sec\n\n", throughput);
	}
	fflush(stdout);
}

int main(void)
{
	int ret;

	/* 1. Initialize LEDs */
	if (gpio_is_ready_dt(&led_red)) {
		gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
	}
	if (gpio_is_ready_dt(&led_blue)) {
		gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_INACTIVE);
	}

	/* 2. Bind MHUv2 Mailbox */
	mhu_r = DEVICE_DT_GET(DT_ALIAS(rtssmhu0r));
	mhu_s = DEVICE_DT_GET(DT_ALIAS(rtssmhu0s));

	if (!device_is_ready(mhu_r) || !device_is_ready(mhu_s)) {
		printf("[Fatal] MHU0 devices not ready!\n");
		return -1;
	}

	/* 3. Register Callbacks & Enable Interrupts */
	ipm_register_callback(mhu_r, mhu_recv_cb, NULL);
	ipm_register_callback(mhu_s, mhu_send_cb, NULL);

	ret = ipm_set_enabled(mhu_r, true);
	if (ret != 0) {
		printf("[Fatal] Failed to enable MHU0 RX interrupt: %d\n", ret);
		return -1;
	}

	/* 4. Display Banner */
	print_banner();

	int heartbeat_timer = 0;

	/* 5. Main Loop: Handle RTT user inputs + Periodic Heartbeat */
	while (1) {
		/* Check RTT keystrokes */
		if (SEGGER_RTT_HasKey()) {
			int ch = SEGGER_RTT_GetKey();
			switch (ch) {
			case 'p':
			case 'P':
			case '1':
				do_single_ping();
				break;
			case 'l':
			case 'L':
			case '2':
				do_toggle_remote_led();
				break;
			case 'b':
			case 'B':
			case '3':
				do_burst_benchmark();
				break;
			case 's':
			case 'S':
			case '4':
				do_query_metrics();
				break;
			case 'a':
			case 'A':
				g_auto_heartbeat = !g_auto_heartbeat;
				printf("[CONFIG] Auto-Heartbeat Telemetry is now: %s\n",
				       g_auto_heartbeat ? "ENABLED" : "PAUSED");
				fflush(stdout);
				break;
			case 'h':
			case 'H':
			case '?':
				print_banner();
				break;
			default:
				break;
			}
		}

		/* Automated periodic heartbeat every ~3 seconds */
		k_msleep(50);
		heartbeat_timer += 50;

		if (g_auto_heartbeat && heartbeat_timer >= 3000) {
			heartbeat_timer = 0;
			uint32_t resp = 0;
			uint32_t latency = 0;

			ret = ipc_transact(IPC_OP_PING, (uint16_t)g_seq, &resp, &latency);
			if (ret == 0) {
				uint16_t worker_rx = IPC_GET_PAYLOAD(resp);
				printf("[Heartbeat #%03u] Pong OK | RTT Latency: %u us | Remote RX Count: %u\n",
				       g_seq, latency, worker_rx);
			} else {
				printf("[Heartbeat #%03u] Timeout waiting for M55-HE response.\n", g_seq);
				gpio_pin_toggle_dt(&led_red);
			}
			fflush(stdout);
		}
	}

	return 0;
}
