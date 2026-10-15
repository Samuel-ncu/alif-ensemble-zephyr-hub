/*
 * Copyright (c) 2026 Samuel (Shu-Ming) Liu
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

/* The devicetree node identifier for the "sw0" alias (Button SW0 / LPGPIO_4) */
#define SW0_NODE DT_ALIAS(sw0)
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios, {0});
static struct gpio_callback joy_cb_data;

/* The devicetree node identifier for the "led0" alias (LED1_G / GPIO6_4) */
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(LED0_NODE, gpios, {0});

/* 5-way Joystick Pin Definitions on LPGPIO (Port 15) */
#define JOY_PIN_UP     0  /* LPGPIO_0 */
#define JOY_PIN_DOWN   1  /* LPGPIO_1 */
#define JOY_PIN_LEFT   2  /* LPGPIO_2 */
#define JOY_PIN_RIGHT  3  /* LPGPIO_3 */
#define JOY_PIN_CENTER 4  /* LPGPIO_4 */

#define ALL_JOY_PINS_MASK 0xFF  /* Support all LPGPIO pins 0..7 */

static volatile uint32_t event_count = 0;
static volatile int64_t last_irq_time = 0;

/* Hardware Interrupt Callback for SW2 Joystick/Button */
void joy_pressed_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	int64_t now = k_uptime_get();
	if (now - last_irq_time < 120) {
		/* Debounce */
		return;
	}
	last_irq_time = now;
	event_count++;

	/* Toggle Green LED on any joystick action */
	gpio_pin_toggle_dt(&led);

	printf("[IRQ #%u] SW2 Triggered (Pins=0x%02X, Time=%lld ms)\n",
	       event_count, (unsigned int)(pins & 0xFF), now);
}

int main(void)
{
	int ret;

	printf("\n==================================================\n");
	printf("*** Alif Ensemble E8 - SW2 Joystick & Button IRQ ***\n");
	printf("Target Board: %s\n", CONFIG_BOARD);
	printf("LED Device  : %s (Pin %d)\n", led.port->name, led.pin);
	printf("Joystick Dev: %s (LPGPIO Pins 0..7)\n", button.port->name);
	printf("==================================================\n");

	/* 1. Configure Green LED */
	if (!gpio_is_ready_dt(&led)) {
		printf("Error: LED device %s is not ready!\n", led.port->name);
		return 0;
	}
	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT);
	if (ret != 0) {
		printf("Error: Failed to configure LED (%d)\n", ret);
		return 0;
	}

	/* Self-test: Blink LED 3 times quickly on boot to confirm new FW is alive */
	printf(">> Boot Self-test: Blinking LED 3 times...\n");
	for (int i = 0; i < 3; i++) {
		gpio_pin_set_dt(&led, 0); /* ON/OFF phase */
		k_msleep(150);
		gpio_pin_set_dt(&led, 1); /* Opposite phase */
		k_msleep(150);
	}
	/* Ensure LED is OFF initially (Pin High = Cathode High = OFF) */
	gpio_pin_set_dt(&led, 1);

	/* 2. Configure LPGPIO Pins 0..7 (SW2 5-Way Joystick) as GPIO_INPUT */
	if (!gpio_is_ready_dt(&button)) {
		printf("Error: LPGPIO device %s is not ready!\n", button.port->name);
		return 0;
	}

	for (int pin = 0; pin < 8; pin++) {
		/* DesignWare GPIO requires GPIO_INPUT without GPIO_PULL_UP */
		ret = gpio_pin_configure(button.port, pin, GPIO_INPUT);
		if (ret != 0) {
			printf("Error: Failed to configure pin %d (%d)\n", pin, ret);
		}

		/* Trigger interrupt on both edges (press and release) */
		ret = gpio_pin_interrupt_configure(button.port, pin, GPIO_INT_EDGE_BOTH);
		if (ret != 0) {
			printf("Error: Failed to configure IRQ on pin %d (%d)\n", pin, ret);
		}
	}

	/* 3. Register Global Callback for All Joystick Pins */
	gpio_init_callback(&joy_cb_data, joy_pressed_callback, ALL_JOY_PINS_MASK);
	ret = gpio_add_callback(button.port, &joy_cb_data);
	if (ret != 0) {
		printf("Error: Failed to add callback (%d)\n", ret);
	}

	printf(">> SW2 Joystick & Button IRQ Ready!\n");
	printf(">> Press SW2 in any direction to toggle the Green LED!\n");

	/* 4. Background polling fallback for 100% guarantee */
	uint32_t last_pins = 0;
	for (int p = 0; p < 8; p++) {
		int val = gpio_pin_get(button.port, p);
		if (val > 0) {
			last_pins |= BIT(p);
		}
	}
	printf(">> Initial LPGPIO Pins State: 0x%02X\n", last_pins);

	while (1) {
		uint32_t cur_pins = 0;
		for (int p = 0; p < 8; p++) {
			int val = gpio_pin_get(button.port, p);
			if (val > 0) {
				cur_pins |= BIT(p);
			}
		}

		if (cur_pins != last_pins) {
			int64_t now = k_uptime_get();
			if (now - last_irq_time >= 100) {
				event_count++;
				gpio_pin_toggle_dt(&led);
				last_irq_time = now;
				printf("[Poll #%u] SW2 Pin Changed: 0x%02X -> 0x%02X | LED Toggled!\n",
				       event_count, last_pins, cur_pins);
			}
			last_pins = cur_pins;
		}

		k_msleep(30);
	}

	return 0;
}
