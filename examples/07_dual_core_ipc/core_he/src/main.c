/*
 * Copyright (c) 2026 Alif Semiconductor / Antigravity
 * SPDX-License-Identifier: Apache-2.0
 *
 * Example 07: Dual-Core IPC Worker (Cortex-M55 HE)
 * Target Core: alif_e8_dk/ae822fa0e5597xx0/rtss_he
 * Inter-Processor Communication over ARM MHUv2 Mailbox & Hardware Semaphores
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/ipm.h>

#include "ipc_protocol.h"

#define WORKER_MSG_QUEUE_SIZE 16

/* Hardware Mailbox Devices */
static const struct device *mhu_r;
static const struct device *mhu_s;

/* Onboard Green LED (LED1_G, P6_4) */
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

/* Synchronization semaphores */
static K_SEM_DEFINE(sem_tx_done, 0, 1);

/* Message Queue for decoupling ISR from processing */
K_MSGQ_DEFINE(ipc_rx_msgq, sizeof(uint32_t), WORKER_MSG_QUEUE_SIZE, 4);

/* Statistics counters */
static uint32_t total_rx_count = 0;
static uint32_t total_tx_count = 0;
static bool led_state = false;

/* Receiver ISR Callback */
static void mhu_recv_cb(const struct device *dev, void *user_data,
			uint32_t channel_id, volatile void *data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);
	ARG_UNUSED(channel_id);

	if (data != NULL) {
		uint32_t msg = *((uint32_t *)data);
		k_msgq_put(&ipc_rx_msgq, &msg, K_NO_WAIT);
	}
}

/* Sender ISR Callback (Doorbell Acknowledgment) */
static void mhu_send_cb(const struct device *dev, void *user_data,
			uint32_t channel_id, volatile void *data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);
	ARG_UNUSED(channel_id);
	ARG_UNUSED(data);

	k_sem_give(&sem_tx_done);
}

/* Transmit 32-bit IPC response with timeout protection */
static int send_ipc_response(uint32_t resp_msg)
{
	int ret = ipm_send(mhu_s, 0, 0, &resp_msg, sizeof(resp_msg));
	if (ret != 0) {
		printk("[M55-HE Error] ipm_send failed: %d\n", ret);
		return ret;
	}

	/* Wait for remote core to acknowledge */
	ret = k_sem_take(&sem_tx_done, K_MSEC(200));
	if (ret != 0) {
		printk("[M55-HE Warning] TX ack timeout!\n");
		return -ETIMEDOUT;
	}

	total_tx_count++;
	return 0;
}

int main(void)
{
	int ret;

	printk("\n");
	printk("=====================================================\n");
	printk("  Alif Ensemble Dual-Core IPC Worker (Cortex-M55 HE) \n");
	printk("  Hardware Mailbox: ARM MHUv2 (0x40080000 / 0x40090000)\n");
	printk("  Serial Console: UART2 @ 115200 baud (SW4 = UART2)  \n");
	printk("=====================================================\n\n");

	/* 1. Initialize Green LED */
	if (!gpio_is_ready_dt(&led_green)) {
		printk("[M55-HE Error] Green LED device not ready!\n");
	} else {
		ret = gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
		if (ret < 0) {
			printk("[M55-HE Error] Failed to configure Green LED: %d\n", ret);
		} else {
			printk("[M55-HE Init] Green LED (LED1_G, P6_4) configured successfully.\n");
		}
	}

	/* 2. Bind MHUv2 Sender & Receiver */
	mhu_r = DEVICE_DT_GET(DT_ALIAS(rtssmhu0r));
	mhu_s = DEVICE_DT_GET(DT_ALIAS(rtssmhu0s));

	if (!device_is_ready(mhu_r)) {
		printk("[M55-HE Fatal] MHU0 Receiver (rtssmhu0r) not ready!\n");
		return -1;
	}
	if (!device_is_ready(mhu_s)) {
		printk("[M55-HE Fatal] MHU0 Sender (rtssmhu0s) not ready!\n");
		return -1;
	}

	printk("[M55-HE Init] MHU0 Receiver and Sender bound.\n");

	/* 3. Register Callbacks and Enable RX Interrupts */
	ipm_register_callback(mhu_r, mhu_recv_cb, NULL);
	ipm_register_callback(mhu_s, mhu_send_cb, NULL);

	ret = ipm_set_enabled(mhu_r, true);
	if (ret != 0) {
		printk("[M55-HE Fatal] Failed to enable MHU0 RX interrupt: %d\n", ret);
		return -1;
	}

	printk("[M55-HE Ready] Listening for IPC commands from Master (M55-HP)...\n\n");

	/* 4. Main Event Loop */
	while (1) {
		uint32_t raw_msg;

		/* Wait for incoming IPC packet */
		if (k_msgq_get(&ipc_rx_msgq, &raw_msg, K_FOREVER) == 0) {
			total_rx_count++;
			uint8_t opcode = IPC_GET_OPCODE(raw_msg);
			uint8_t seq = IPC_GET_SEQ(raw_msg);
			uint16_t payload = IPC_GET_PAYLOAD(raw_msg);

			switch (opcode) {
			case IPC_OP_PING: {
				/* Pulse Green LED on ping */
				gpio_pin_toggle_dt(&led_green);
				led_state = !led_state;

				printk("[IPC Worker] Received PING (Seq: #%u, Payload: 0x%04X) -> Sending PONG\n",
				       seq, payload);

				uint32_t resp = IPC_PACK_MSG(IPC_RESP_PONG, seq, (uint16_t)(total_rx_count & 0xFFFF));
				send_ipc_response(resp);

				/* Keep LED on briefly then restore */
				k_busy_wait(1000);
				gpio_pin_set_dt(&led_green, 0);
				led_state = false;
				break;
			}

			case IPC_OP_LED_SET: {
				if (payload == 0) {
					gpio_pin_set_dt(&led_green, 0);
					led_state = false;
				} else if (payload == 1) {
					gpio_pin_set_dt(&led_green, 1);
					led_state = true;
				} else {
					gpio_pin_toggle_dt(&led_green);
					led_state = !led_state;
				}

				printk("[IPC Worker] Commanded LED SET (Action: %u, State now: %s)\n",
				       payload, led_state ? "ON" : "OFF");

				uint32_t resp = IPC_PACK_MSG(IPC_RESP_LED_STATUS, seq, (uint16_t)led_state);
				send_ipc_response(resp);
				break;
			}

			case IPC_OP_GET_METRICS: {
				printk("[IPC Worker] Metrics queried by HP (Total RX: %u, TX: %u)\n",
				       total_rx_count, total_tx_count);

				uint32_t resp = IPC_PACK_MSG(IPC_RESP_METRICS, seq, (uint16_t)(total_rx_count & 0xFFFF));
				send_ipc_response(resp);
				break;
			}

			case IPC_OP_BURST_PING: {
				/* Fast path: No serial printing or sleep, instant turnaround */
				uint32_t resp = IPC_PACK_MSG(IPC_RESP_BURST_PONG, seq, (uint16_t)seq);
				send_ipc_response(resp);
				break;
			}

			default:
				printk("[IPC Worker Warning] Unknown opcode: 0x%02X\n", opcode);
				break;
			}
		}
	}

	return 0;
}

