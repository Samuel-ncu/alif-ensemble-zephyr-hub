/*
 * Copyright (c) 2026 Samuel (Shu-Ming) Liu
 * SPDX-License-Identifier: Apache-2.0
 *
 * Example 04: Alif Ensemble E8 - SEGGER RTT Real-Time Logging & Console
 *
 * Demonstrates high-speed, zero-latency logging via SEGGER RTT directly
 * through J-Link debug probes, eliminating the need to physically toggle
 * the board's SW4 switch between SE (flashing) and UART2 (console).
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <SEGGER_RTT.h>

/* GPIO Port 6 (Onboard RGB LEDs: LED1_R, LED1_G, LED1_B) */
#define PIN_LED_R 2  /* GPIO6_2: Red LED */
#define PIN_LED_G 4  /* GPIO6_4: Green LED */
#define PIN_LED_B 6  /* GPIO6_6: Blue LED */

static const struct device *gpio6_dev = DEVICE_DT_GET(DT_NODELABEL(gpio6));

/* Active-High RGB LED Helper (1 = ON, 0 = OFF) */
static inline void set_rgb(int r, int g, int b)
{
	if (gpio6_dev && device_is_ready(gpio6_dev)) {
		gpio_pin_set(gpio6_dev, PIN_LED_R, r ? 1 : 0);
		gpio_pin_set(gpio6_dev, PIN_LED_G, g ? 1 : 0);
		gpio_pin_set(gpio6_dev, PIN_LED_B, b ? 1 : 0);
	}
}

int main(void)
{
	uint32_t count = 0;
	bool led_state = false;

	/* 1. Initialize On-board Status LEDs */
	if (gpio6_dev && device_is_ready(gpio6_dev)) {
		gpio_pin_configure(gpio6_dev, PIN_LED_R, GPIO_OUTPUT_INACTIVE);
		gpio_pin_configure(gpio6_dev, PIN_LED_G, GPIO_OUTPUT_INACTIVE);
		gpio_pin_configure(gpio6_dev, PIN_LED_B, GPIO_OUTPUT_INACTIVE);
		set_rgb(0, 0, 0);
	}

	/* 2. Welcome Banner over RTT & Console */
	printf("\n" RTT_CTRL_CLEAR);
	printf(RTT_CTRL_TEXT_BRIGHT_CYAN "===============================================================\n" RTT_CTRL_RESET);
	printf(RTT_CTRL_TEXT_BRIGHT_GREEN "⚡ Alif Ensemble E8 - SEGGER RTT High-Speed Logging Active ⚡\n" RTT_CTRL_RESET);
	printf(RTT_CTRL_TEXT_BRIGHT_WHITE " Target Board : %s\n" RTT_CTRL_RESET, CONFIG_BOARD);
	printf(RTT_CTRL_TEXT_BRIGHT_WHITE " Architecture : ARM Cortex-M55 (RTSS-HE)\n" RTT_CTRL_RESET);
	printf(RTT_CTRL_TEXT_BRIGHT_YELLOW " >> Zero-Switch Mode: SW4 can remain on SE (Top) at all times!\n" RTT_CTRL_RESET);
	printf(RTT_CTRL_TEXT_BRIGHT_YELLOW " >> RTT Channel 0 is streaming directly through J-Link RAM buffer.\n" RTT_CTRL_RESET);
	printf(RTT_CTRL_TEXT_BRIGHT_CYAN "===============================================================\n\n" RTT_CTRL_RESET);

	printf(RTT_CTRL_TEXT_BRIGHT_MAGENTA "[INFO] SEGGER RTT Up-Buffer initialized at %p\n" RTT_CTRL_RESET, &_SEGGER_RTT);
	printf("[INFO] Type any character in J-Link RTT Viewer to test interactive echo.\n\n");

	while (1) {
		int64_t uptime_ms = k_uptime_get();
		led_state = !led_state;

		/* Heartbeat: Toggle Cyan (Green + Blue) LED */
		set_rgb(0, led_state ? 1 : 0, led_state ? 1 : 0);

		/* Print formatted heartbeat telemetry via RTT */
		printf(RTT_CTRL_TEXT_BRIGHT_GREEN "[RTT #%04u]" RTT_CTRL_RESET
		       " Uptime: " RTT_CTRL_TEXT_BRIGHT_YELLOW "%lld ms" RTT_CTRL_RESET
		       " | Heartbeat LED: %s | System OK\n",
		       count++, uptime_ms, led_state ? "ON " : "OFF");

		/* Check for interactive keystrokes received from J-Link RTT Viewer / Client */
		if (SEGGER_RTT_HasKey()) {
			int key = SEGGER_RTT_GetKey();
			printf(RTT_CTRL_TEXT_BRIGHT_RED ">>> [RTT RX] Received key from host: '%c' (0x%02X) <<<\n" RTT_CTRL_RESET,
			       (key >= 32 && key <= 126) ? (char)key : '?', (unsigned char)key);

			/* Flash Purple/Magenta on keypress */
			set_rgb(1, 0, 1);
			k_msleep(50);
		}

		k_msleep(1000);
	}

	return 0;
}
