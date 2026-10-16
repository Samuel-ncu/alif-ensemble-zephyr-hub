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

	/* Startup flash to indicate firmware has started: 3 quick Blue flashes */
	for (int i = 0; i < 3; i++) {
		set_rgb(0, 0, 1);
		k_msleep(100);
		set_rgb(0, 0, 0);
		k_msleep(100);
	}

	/* 2. Welcome Banner over RTT */
	printf("\n\n===============================================================\n");
	printf("⚡ Alif Ensemble E8 - SEGGER RTT High-Speed Logging Active ⚡\n");
	printf(" Target Board : %s\n", CONFIG_BOARD);
	printf(" Architecture : ARM Cortex-M55 (%s)\n",
#if defined(CONFIG_SOC_AE822FA0E5597XX0_RTSS_HP)
	       "RTSS-HP"
#else
	       "RTSS-HE"
#endif
	);
	printf(" RTT Buffer   : Local 0x%p\n", &_SEGGER_RTT);
	printf("===============================================================\n\n");

	while (1) {
		int64_t uptime_ms = k_uptime_get();
		led_state = !led_state;

		/* Heartbeat: Toggle Cyan (Green + Blue) LED */
		set_rgb(0, led_state ? 1 : 0, led_state ? 1 : 0);

		/* Print formatted heartbeat telemetry via RTT */
		printf("[RTT #%04u] Uptime: %lld ms | Heartbeat LED: %s | System OK\n",
		       count++, uptime_ms, led_state ? "ON " : "OFF");

		/* Check for interactive keystrokes received from J-Link RTT Viewer / Client */
		if (SEGGER_RTT_HasKey()) {
			int key = SEGGER_RTT_GetKey();
			printf(">>> [RTT RX] Received key from host: '%c' (0x%02X) <<<\n",
			       (key >= 32 && key <= 126) ? (char)key : '?', (unsigned char)key);

			/* Flash Purple/Magenta on keypress */
			set_rgb(1, 0, 1);
			k_msleep(80);
		}

		k_msleep(1000);
	}

	return 0;
}
