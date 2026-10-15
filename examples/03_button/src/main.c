/*
 * Copyright (c) 2026 Samuel (Shu-Ming) Liu
 * SPDX-License-Identifier: Apache-2.0
 *
 * Example 03: Alif Ensemble E8 - SW2 5-Way Joystick Navigation & LED Control
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

/* LPGPIO Pad Control Base (Low-Power Subsystem) */
#define LPGPIO_PADCTRL_BASE 0x42007000
/* PADCTRL: READ_ENABLE (0x01) | SCHMITT (0x02) | DRIVER_DISABLED_PULL_UP (0x08) | 4MA (0x20) = 0x2B */
#define PADCTRL_VAL_PULL_UP (0x01U | 0x02U | 0x08U | 0x20U)

/* Devicetree Node for SW0 (LPGPIO_4 Center Push) */
#define SW0_NODE DT_ALIAS(sw0)
static const struct gpio_dt_spec joy_center = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios, {0});
static struct gpio_callback joy_cb_data;

/* Hardware-Verified Pin Definitions for SW2 on LPGPIO (Port 15) */
#define JOY_PIN_DOWN   0  /* LPGPIO_0: Physical Down */
#define JOY_PIN_UP     1  /* LPGPIO_1: Physical Up */
#define JOY_PIN_LEFT   2  /* LPGPIO_2: Physical Left */
#define JOY_PIN_RIGHT  3  /* LPGPIO_3: Physical Right */
#define JOY_PIN_CENTER 4  /* LPGPIO_4: Physical Center Push */

#define ALL_JOY_PINS_MASK (BIT(JOY_PIN_DOWN) | BIT(JOY_PIN_UP) | \
                           BIT(JOY_PIN_LEFT) | BIT(JOY_PIN_RIGHT) | \
                           BIT(JOY_PIN_CENTER))

/* GPIO Port 6 (Onboard RGB LEDs: LED1_R, LED1_G, LED1_B) */
#define PIN_LED_R 2  /* GPIO6_2: Red LED */
#define PIN_LED_G 4  /* GPIO6_4: Green LED (led0 alias) */
#define PIN_LED_B 6  /* GPIO6_6: Blue LED */

static const struct device *gpio6_dev = DEVICE_DT_GET(DT_NODELABEL(gpio6));

/* Active-High RGB LED Helper (1 = ON, 0 = OFF) */
static inline void set_rgb(int r, int g, int b) {
	if (gpio6_dev) {
		gpio_pin_set(gpio6_dev, PIN_LED_R, r ? 1 : 0);
		gpio_pin_set(gpio6_dev, PIN_LED_G, g ? 1 : 0);
		gpio_pin_set(gpio6_dev, PIN_LED_B, b ? 1 : 0);
	}
}

static volatile bool green_led_state = false;
static volatile uint32_t event_count = 0;
static volatile int64_t last_action_time = 0;

/* Enable hardware pull-up resistors in the SoC Pad Control block */
static void configure_lpgpio_hardware_pullups(void)
{
	volatile uint32_t *pad_ctrl = (volatile uint32_t *)LPGPIO_PADCTRL_BASE;
	for (int i = 0; i < 5; i++) {
		pad_ctrl[i] = PADCTRL_VAL_PULL_UP;
	}
}

/* Handle SW2 actions with debouncing */
static void handle_sw2_action(const char *action_name, int toggle_green, int force_green)
{
	int64_t now = k_uptime_get();
	if (now - last_action_time < 180) {
		return; /* Debounce */
	}
	last_action_time = now;
	event_count++;

	if (force_green >= 0) {
		green_led_state = (force_green != 0);
	} else if (toggle_green) {
		green_led_state = !green_led_state;
	}

	/* Update green LED */
	set_rgb(0, green_led_state ? 1 : 0, 0);

	printf("[SW2 Event #%u] Action: %s -> Green LED: %s (Time: %lld ms)\n",
	       event_count, action_name, green_led_state ? "ON" : "OFF", now);
}

/* Hardware Interrupt Callback for SW2 */
void joy_pressed_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	if (pins & BIT(JOY_PIN_CENTER)) {
		handle_sw2_action("CENTER PUSH (Toggle)", 1, -1);
	} else if (pins & BIT(JOY_PIN_UP)) {
		handle_sw2_action("UP (Turn ON)", 0, 1);
	} else if (pins & BIT(JOY_PIN_DOWN)) {
		handle_sw2_action("DOWN (Turn OFF / 熄滅)", 0, 0);
	} else if (pins & BIT(JOY_PIN_LEFT)) {
		handle_sw2_action("LEFT (Toggle)", 1, -1);
	} else if (pins & BIT(JOY_PIN_RIGHT)) {
		handle_sw2_action("RIGHT (Toggle)", 1, -1);
	}
}

int main(void)
{
	int ret;

	printf("\n=======================================================\n");
	printf("*** Alif Ensemble E8 - SW2 Joystick & LED Control ***\n");
	printf("Target Board: %s (RTSS-HE)\n", CONFIG_BOARD);
	printf("SW2 Physical Mapping (Hardware-Verified):\n");
	printf("  - LPGPIO_1: UP        -> Turn ON Green LED\n");
	printf("  - LPGPIO_0: DOWN      -> Turn OFF Green LED (熄滅)\n");
	printf("  - LPGPIO_4: CENTER    -> Toggle Green LED (切換)\n");
	printf("  - LPGPIO_2: LEFT      -> Toggle Green LED (切換)\n");
	printf("  - LPGPIO_3: RIGHT     -> Toggle Green LED (切換)\n");
	printf("=======================================================\n");

	/* 1. Initialize RGB LEDs on GPIO6 */
	if (!device_is_ready(gpio6_dev)) {
		printf("[ERROR] GPIO6 controller not ready!\n");
		return 0;
	}

	gpio_pin_configure(gpio6_dev, PIN_LED_R, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure(gpio6_dev, PIN_LED_G, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure(gpio6_dev, PIN_LED_B, GPIO_OUTPUT_INACTIVE);

	/* Turn off all LEDs initially */
	set_rgb(0, 0, 0);

	/* Startup LED Self-Test: Red -> Green -> Blue */
	printf(">> Startup Self-Test: Red -> Green -> Blue\n");
	set_rgb(1, 0, 0); k_msleep(250); set_rgb(0, 0, 0); k_msleep(80);
	set_rgb(0, 1, 0); k_msleep(250); set_rgb(0, 0, 0); k_msleep(80);
	set_rgb(0, 0, 1); k_msleep(250); set_rgb(0, 0, 0); k_msleep(150);

	/* Ensure Green LED is OFF initially */
	green_led_state = false;
	set_rgb(0, 0, 0);

	/* 2. Configure Hardware Pull-ups on Pad Control Block */
	printf(">> Activating hardware internal pull-up on LPGPIO 0..4...\n");
	configure_lpgpio_hardware_pullups();

	/* 3. Configure LPGPIO Pins 0..4 as Standard GPIO_INPUT */
	if (!gpio_is_ready_dt(&joy_center)) {
		printf("[ERROR] LPGPIO controller %s not ready!\n", joy_center.port->name);
		return 0;
	}

	for (int pin = 0; pin < 5; pin++) {
		ret = gpio_pin_configure(joy_center.port, pin, GPIO_INPUT);
		if (ret != 0) {
			printf("[ERROR] Failed to configure LPGPIO pin %d: %d\n", pin, ret);
		}

		/* Configure falling edge interrupt (Active-Low press) */
		ret = gpio_pin_interrupt_configure(joy_center.port, pin, GPIO_INT_EDGE_FALLING);
		if (ret != 0) {
			printf("[ERROR] Failed to configure IRQ on pin %d: %d\n", pin, ret);
		}
	}

	/* 4. Register Hardware Interrupt Callback */
	gpio_init_callback(&joy_cb_data, joy_pressed_callback, ALL_JOY_PINS_MASK);
	ret = gpio_add_callback(joy_center.port, &joy_cb_data);
	if (ret != 0) {
		printf("[ERROR] Failed to register callback: %d\n", ret);
	}

	printf(">> Ready! Press or tilt SW2:\n");
	printf("   - Push CENTER: Toggle Green LED\n");
	printf("   - Push UP    : Turn Green LED ON\n");
	printf("   - Push DOWN  : Turn Green LED OFF (熄滅)\n");
	printf("   - Push LEFT  : Toggle Green LED\n");
	printf("   - Push RIGHT : Toggle Green LED\n");

	/* 5. Polling fallback in main loop */
	uint32_t last_pins = 0x1F; /* All 5 pins high initially */

	while (1) {
		int p_down   = gpio_pin_get(joy_center.port, JOY_PIN_DOWN);
		int p_up     = gpio_pin_get(joy_center.port, JOY_PIN_UP);
		int p_left   = gpio_pin_get(joy_center.port, JOY_PIN_LEFT);
		int p_right  = gpio_pin_get(joy_center.port, JOY_PIN_RIGHT);
		int p_center = gpio_pin_get(joy_center.port, JOY_PIN_CENTER);

		uint32_t cur_pins = (p_down << 0) | (p_up << 1) | (p_left << 2) |
		                    (p_right << 3) | (p_center << 4);

		if (cur_pins != last_pins) {
			if (p_center == 0 && (last_pins & BIT(JOY_PIN_CENTER))) {
				handle_sw2_action("CENTER PUSH (Toggle)", 1, -1);
			} else if (p_up == 0 && (last_pins & BIT(JOY_PIN_UP))) {
				handle_sw2_action("UP (Turn ON)", 0, 1);
			} else if (p_down == 0 && (last_pins & BIT(JOY_PIN_DOWN))) {
				handle_sw2_action("DOWN (Turn OFF / 熄滅)", 0, 0);
			} else if (p_left == 0 && (last_pins & BIT(JOY_PIN_LEFT))) {
				handle_sw2_action("LEFT (Toggle)", 1, -1);
			} else if (p_right == 0 && (last_pins & BIT(JOY_PIN_RIGHT))) {
				handle_sw2_action("RIGHT (Toggle)", 1, -1);
			}
			last_pins = cur_pins;
		}

		k_msleep(25);
	}

	return 0;
}
