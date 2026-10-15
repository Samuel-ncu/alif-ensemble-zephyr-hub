/*
 * Copyright (c) 2026 Samuel (Shu-Ming) Liu
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/* 500 msec = 1 Hz blinking frequency */
#define SLEEP_TIME_MS   500

/* The devicetree node identifier for the "led0" alias (LED1_G / GPIO6_4) */
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

int main(void)
{
	int ret;
	bool led_state = true;
	uint32_t count = 0;

	printf("\n========================================\n");
	printf("*** Alif Ensemble E8 - Blinky (Zephyr RTOS) ***\n");
	printf("Target Board: %s\n", CONFIG_BOARD);
	printf("Target LED: %s (Port: %s, Pin: %d)\n",
	       "LED1_G", led.port->name, led.pin);
	printf("Blink Interval: %d ms\n", SLEEP_TIME_MS);
	printf("========================================\n");

	if (!gpio_is_ready_dt(&led)) {
		printf("Error: GPIO device %s is not ready!\n", led.port->name);
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		printf("Error: Failed to configure GPIO pin (%d)\n", ret);
		return 0;
	}

	while (1) {
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			printf("Error: Failed to toggle GPIO pin (%d)\n", ret);
			return 0;
		}

		led_state = !led_state;
		printf("[Blinky #%u] Green LED: %s\n", ++count, led_state ? "ON" : "OFF");
		k_msleep(SLEEP_TIME_MS);
	}

	return 0;
}

