/*
 * Copyright (c) 2026 Samuel (Shu-Ming) Liu
 * SPDX-License-Identifier: Apache-2.0
 *
 * Alif Ensemble E8 - SW2 5-Way Joystick & Pure RGB Color Calibration
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

/* GPIO Port 6 (Onboard RGB LEDs: LED1_R, LED1_G, LED1_B) */
#define PIN_LED_R 2  /* GPIO6_2: Red LED */
#define PIN_LED_G 4  /* GPIO6_4: Green LED (led0 alias) */
#define PIN_LED_B 6  /* GPIO6_6: Blue LED */

static const struct device *gpio6_dev = DEVICE_DT_GET(DT_NODELABEL(gpio6));

/*
 * Calibrated LED Control: Active-High logic (1 = ON, 0 = OFF)
 */
static inline void set_rgb(int r, int g, int b) {
	if (gpio6_dev) {
		gpio_pin_set(gpio6_dev, PIN_LED_R, r ? 1 : 0);
		gpio_pin_set(gpio6_dev, PIN_LED_G, g ? 1 : 0);
		gpio_pin_set(gpio6_dev, PIN_LED_B, b ? 1 : 0);
	}
}

/* Enable hardware pull-up resistors in the SoC Pad Control block */
static void configure_lpgpio_hardware_pullups(void)
{
	volatile uint32_t *pad_ctrl = (volatile uint32_t *)LPGPIO_PADCTRL_BASE;
	for (int i = 0; i < 5; i++) {
		pad_ctrl[i] = PADCTRL_VAL_PULL_UP;
	}
}

int main(void)
{
	int ret;

	printf("\n=======================================================\n");
	printf("*** Alif Ensemble E8 - SW2 Joystick Calibrated Test ***\n");
	printf("Target Board: %s (RTSS-HE)\n", CONFIG_BOARD);
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

	/* Startup LED Self-Test: Red -> Green -> Blue in clean sequence */
	printf(">> Startup Self-Test: Red -> Green -> Blue\n");
	set_rgb(1, 0, 0); k_msleep(300); set_rgb(0, 0, 0); k_msleep(100);
	set_rgb(0, 1, 0); k_msleep(300); set_rgb(0, 0, 0); k_msleep(100);
	set_rgb(0, 0, 1); k_msleep(300); set_rgb(0, 0, 0); k_msleep(200);

	/* 2. Configure Hardware Pull-ups on Pad Control */
	printf(">> Activating hardware internal pull-up on LPGPIO 0..4 (0x42007000 = 0x2B)...\n");
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
	}

	printf(">> Calibration Loop Running!\n");
	printf("   [Idle / 放開]   : 純綠燈 1Hz 心跳閃爍 (紅/藍全滅)\n");
	printf("   [Center / 按壓] : 純紅燈 恆亮 (Solid RED)\n");
	printf("   [Up / 向上推]   : 純藍燈 恆亮 (Solid BLUE)\n");
	printf("   [Down / 向下推] : 純綠燈 恆亮 (Solid GREEN)\n");
	printf("   [Left / 向左推] : 藍綠光 恆亮 (Cyan: Blue + Green)\n");
	printf("   [Right / 向右推]: 紫光 恆亮   (Magenta: Red + Blue)\n");

	bool idle_green_toggle = false;
	uint32_t last_mask = 0xFF;
	uint32_t loop_tick = 0;

	while (1) {
		/* Read SW2 pins (Active-Low: 0 = Pressed, 1 = Idle/High) */
		int p_up     = gpio_pin_get(joy_center.port, 0); /* LPGPIO_0 */
		int p_down   = gpio_pin_get(joy_center.port, 1); /* LPGPIO_1 */
		int p_left   = gpio_pin_get(joy_center.port, 2); /* LPGPIO_2 */
		int p_right  = gpio_pin_get(joy_center.port, 3); /* LPGPIO_3 */
		int p_center = gpio_pin_get(joy_center.port, 4); /* LPGPIO_4 */

		uint32_t current_mask = (p_up << 0) | (p_down << 1) | (p_left << 2) |
		                        (p_right << 3) | (p_center << 4);

		if (current_mask != last_mask) {
			printf("[SW2 State Change] Mask=0x%02X (C=%d R=%d L=%d D=%d U=%d)\n",
			       current_mask, p_center, p_right, p_left, p_down, p_up);
			last_mask = current_mask;
		}

		/* Check which action is active */
		if (p_center == 0) {
			/* Center Push -> Pure Solid RED */
			set_rgb(1, 0, 0);
			k_msleep(50);
		} else if (p_up == 0) {
			/* Up -> Pure Solid BLUE */
			set_rgb(0, 0, 1);
			k_msleep(50);
		} else if (p_down == 0) {
			/* Down -> Pure Solid GREEN */
			set_rgb(0, 1, 0);
			k_msleep(50);
		} else if (p_left == 0) {
			/* Left -> Cyan (Blue + Green) */
			set_rgb(0, 1, 1);
			k_msleep(50);
		} else if (p_right == 0) {
			/* Right -> Magenta (Red + Blue) */
			set_rgb(1, 0, 1);
			k_msleep(50);
		} else {
			/* IDLE: Pure Green 1 Hz Heartbeat Blink (500ms ON / 500ms OFF) */
			idle_green_toggle = !idle_green_toggle;
			set_rgb(0, idle_green_toggle ? 1 : 0, 0);

			if (++loop_tick % 4 == 0) {
				printf("[Heartbeat] Uptime=%lld ms | Idle Pins=0x%02X\n",
				       k_uptime_get(), current_mask);
			}
			k_msleep(500);
		}
	}

	return 0;
}
