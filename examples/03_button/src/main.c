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

#define ALL_JOY_PINS_MASK (BIT(JOY_PIN_UP) | BIT(JOY_PIN_DOWN) | \
                           BIT(JOY_PIN_LEFT) | BIT(JOY_PIN_RIGHT) | \
                           BIT(JOY_PIN_CENTER))

static volatile uint32_t press_count = 0;

/* Hardware Interrupt Callback for SW2 Joystick/Button */
void joy_pressed_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	const char *direction = "UNKNOWN";

	if (pins & BIT(JOY_PIN_CENTER)) {
		direction = "CENTER PUSH";
	} else if (pins & BIT(JOY_PIN_UP)) {
		direction = "UP";
	} else if (pins & BIT(JOY_PIN_DOWN)) {
		direction = "DOWN";
	} else if (pins & BIT(JOY_PIN_LEFT)) {
		direction = "LEFT";
	} else if (pins & BIT(JOY_PIN_RIGHT)) {
		direction = "RIGHT";
	}

	press_count++;

	/* Toggle Green LED on any joystick action */
	gpio_pin_toggle_dt(&led);

	printf("[IRQ Event #%u] SW2 Triggered: %s (Pins=0x%02X, Time=%u ms)\n",
	       press_count, direction, (unsigned int)(pins & 0xFF), k_uptime_get_32());
}

int main(void)
{
	int ret;

	printf("\n==================================================\n");
	printf("*** Alif Ensemble E8 - SW2 Joystick & Button IRQ ***\n");
	printf("Target Board: %s\n", CONFIG_BOARD);
	printf("LED Device  : %s (Pin %d)\n", led.port->name, led.pin);
	printf("Joystick Dev: %s (LPGPIO Pins 0..4)\n", button.port->name);
	printf("==================================================\n");

	/* 1. Configure Green LED */
	if (!gpio_is_ready_dt(&led)) {
		printf("Error: LED device %s is not ready!\n", led.port->name);
		return 0;
	}
	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret != 0) {
		printf("Error: Failed to configure LED (%d)\n", ret);
		return 0;
	}

	/* Self-test: Blink LED 3 times quickly on boot to confirm new FW is alive */
	printf(">> Boot Self-test: Blinking LED 3 times...\n");
	for (int i = 0; i < 3; i++) {
		gpio_pin_set_dt(&led, 1);
		k_msleep(150);
		gpio_pin_set_dt(&led, 0);
		k_msleep(150);
	}
	/* Leave LED OFF initially */
	gpio_pin_set_dt(&led, 0);

	/* 2. Configure LPGPIO Pins 0..4 (SW2 5-Way Joystick) with Pull-Up */
	if (!gpio_is_ready_dt(&button)) {
		printf("Error: LPGPIO device %s is not ready!\n", button.port->name);
		return 0;
	}

	for (int pin = 0; pin <= 4; pin++) {
		ret = gpio_pin_configure(button.port, pin, GPIO_INPUT | GPIO_PULL_UP);
		if (ret != 0) {
			printf("Error: Failed to configure pin %d (%d)\n", pin, ret);
		}

		/* Trigger interrupt on Falling Edge (pressed to GND) */
		ret = gpio_pin_interrupt_configure(button.port, pin, GPIO_INT_EDGE_FALLING);
		if (ret != 0) {
			printf("Error: Failed to configure IRQ on pin %d (%d)\n", pin, ret);
		}
	}

	/* 3. Register Global Callback for All 5 Joystick Pins */
	gpio_init_callback(&joy_cb_data, joy_pressed_callback, ALL_JOY_PINS_MASK);
	gpio_add_callback(button.port, &joy_cb_data);

	printf(">> SW2 Joystick & Button IRQ Ready!\n");
	printf(">> Press SW2 (Center, Up, Down, Left, or Right) to toggle LED!\n");

	/* 4. Background polling fallback for maximum reliability */
	int last_center_val = 1;
	while (1) {
		int center_val = gpio_pin_get(button.port, JOY_PIN_CENTER);
		if (center_val == 0 && last_center_val == 1) {
			/* Center pressed (active low) */
			press_count++;
			gpio_pin_toggle_dt(&led);
			printf("[Poll Event #%u] SW2 CENTER Pressed! Toggle LED.\n", press_count);
		}
		last_center_val = center_val;
		k_msleep(50);
	}

	return 0;
}
