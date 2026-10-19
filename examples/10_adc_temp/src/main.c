/*
 * Copyright (c) 2026 Samuel (Shu-Ming) Liu
 * SPDX-License-Identifier: Apache-2.0
 *
 * Example 10: Alif Ensemble E8 - On-Chip 12-Bit ADC & Internal Die Temperature Sensor
 *
 * Features:
 *  - 12-bit SAR ADC (adc0 @ 0x49028000) sampling internal temperature sensor (TSENS, Channel 6)
 *  - Real-time internal chip die temperature monitoring in Celsius (°C) and Fahrenheit (°F)
 *  - Precision millivolt (mV) conversion using 1.2V internal analog reference
 *  - Statistical tracking (Minimum, Maximum, and Running Average temperature)
 *  - Visual thermal status on RGB LED (Green=Normal <35°C, Yellow=Warm 35-50°C, Red=Hot >50°C)
 *  - Zero-latency SEGGER RTT Console output at 0x20000410 (JTAG, 4000 kHz)
 *  - Interactive RTT keyboard commands ('s': single sample, 'r': reset stats, 'm': toggle mode, 'h': help)
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <SEGGER_RTT.h>

#include "temperature.h"

/* ADC Channel 6 connects to internal on-chip Temperature Sensor (TSENS) */
#define ADC_CH_TEMP_SENSOR 6

/* Onboard RGB LED (Port 6: LED1_R=Pin 2, LED1_G=Pin 4, LED1_B=Pin 6) */
#define PIN_LED_R 2
#define PIN_LED_G 4
#define PIN_LED_B 6

static const struct device *gpio6_dev = DEVICE_DT_GET(DT_NODELABEL(gpio6));
static const struct device *adc_dev   = DEVICE_DT_GET(DT_NODELABEL(adc0));

/* Active-High RGB LED Helper */
static inline void set_rgb(int r, int g, int b)
{
	if (gpio6_dev && device_is_ready(gpio6_dev)) {
		gpio_pin_set(gpio6_dev, PIN_LED_R, r ? 1 : 0);
		gpio_pin_set(gpio6_dev, PIN_LED_G, g ? 1 : 0);
		gpio_pin_set(gpio6_dev, PIN_LED_B, b ? 1 : 0);
	}
}

/* Render ASCII Thermometer Gauge */
static void render_temp_gauge(char *buf, size_t buf_len, float temp_c, float min_c, float max_c, int width)
{
	int center = width / 2;
	float ref_c = 25.0f; /* Room temperature center reference */
	float span = (max_c - min_c) / 2.0f;
	if (span < 1.0f) span = 1.0f;

	int pos = center + (int)(((temp_c - ref_c) / span) * (float)center);
	if (pos < 0) pos = 0;
	if (pos >= width) pos = width - 1;

	for (int i = 0; i < width; i++) {
		if (i == pos) {
			buf[i] = '#';
		} else if (i == center) {
			buf[i] = '|';
		} else if ((pos < center && i >= pos && i < center) ||
		           (pos > center && i <= pos && i > center)) {
			buf[i] = '=';
		} else {
			buf[i] = '-';
		}
	}
	buf[width] = '\0';
}

static void print_help(void)
{
	printf("\n===============================================================\n");
	printf("   🌡️  ADC Die Temperature Interactive Keyboard Commands  🌡️\n");
	printf("===============================================================\n");
	printf("  's' : Trigger immediate manual ADC sample reading\n");
	printf("  'r' : Reset Min/Max/Avg temperature tracking statistics\n");
	printf("  'm' : Toggle display mode (Visual Thermometer Bar <-> Stream)\n");
	printf("  'h' : Display this help menu\n");
	printf("===============================================================\n\n");
}

int main(void)
{
	int ret;
	uint32_t sample_buffer[1] = {0};
	uint32_t sample_count = 0;
	bool visual_mode = true;
	char gauge_buf[33];

	float min_temp = 999.0f;
	float max_temp = -999.0f;
	float sum_temp = 0.0f;

	/* 1. Initialize Status RGB LEDs */
	if (gpio6_dev && device_is_ready(gpio6_dev)) {
		gpio_pin_configure(gpio6_dev, PIN_LED_R, GPIO_OUTPUT_INACTIVE);
		gpio_pin_configure(gpio6_dev, PIN_LED_G, GPIO_OUTPUT_INACTIVE);
		gpio_pin_configure(gpio6_dev, PIN_LED_B, GPIO_OUTPUT_INACTIVE);
		set_rgb(0, 0, 0);
	}

	/* Power-on self test: 3 quick Green-Yellow pulses */
	for (int i = 0; i < 3; i++) {
		set_rgb(0, 1, 0);
		k_msleep(80);
		set_rgb(0, 0, 0);
		k_msleep(80);
	}

	/* 2. Welcome Banner over SEGGER RTT */
	printf("\n\n===============================================================\n");
	printf("🌡️ Alif Ensemble E8 - On-Chip 12-Bit ADC & Die Temp Sensor 🌡️\n");
	printf(" Target Core   : ARM Cortex-M55 (RTSS-HP @ 400 MHz)\n");
	printf(" ADC Peripheral: adc0 @ 0x49028000 (12-Bit SAR ADC)\n");
	printf(" Sensor Source : Channel 6 -> Internal Die TSENS (Silicon)\n");
	printf(" RTT Buffer    : Local 0x%p\n", &_SEGGER_RTT);
	printf("===============================================================\n");

	/* 3. Check ADC device binding */
	if (!device_is_ready(adc_dev)) {
		printf("FATAL: ADC device adc0 is NOT ready!\n");
		set_rgb(1, 0, 0);
		return -1;
	}
	printf("✓ ADC device bound successfully: %s\n", adc_dev->name);

	/* 4. Configure ADC Channel 6 for Internal TSENS */
	struct adc_channel_cfg channel_cfg = {
		.differential = 0,
		.channel_id   = ADC_CH_TEMP_SENSOR,
	};

	ret = adc_channel_setup(adc_dev, &channel_cfg);
	if (ret < 0) {
		printf("FATAL: Failed to setup ADC channel %d (%d)!\n", ADC_CH_TEMP_SENSOR, ret);
		set_rgb(1, 0, 0);
		return -1;
	}
	printf("✓ ADC Channel %d configured (Single-ended, 12-bit)\n", ADC_CH_TEMP_SENSOR);
	print_help();

	struct adc_sequence sequence = {
		.options     = NULL,
		.buffer      = (void *)sample_buffer,
		.buffer_size = sizeof(sample_buffer),
		.channels    = BIT(ADC_CH_TEMP_SENSOR),
	};

	while (1) {
		/* Check interactive host keystrokes */
		if (SEGGER_RTT_HasKey()) {
			int key = SEGGER_RTT_GetKey();
			switch (key) {
			case 's':
			case 'S':
				printf("\n>>> Manual ADC Sample Triggered <<<\n");
				break;
			case 'r':
			case 'R':
				min_temp = 999.0f;
				max_temp = -999.0f;
				sum_temp = 0.0f;
				sample_count = 0;
				printf("\n>>> [RESET] Temperature statistics reset! <<<\n\n");
				set_rgb(0, 0, 1);
				k_msleep(150);
				break;
			case 'm':
			case 'M':
				visual_mode = !visual_mode;
				printf("\n>>> [MODE] Switched to %s mode <<<\n\n",
				       visual_mode ? "Visual Gauge Bar" : "Compact Telemetry Stream");
				break;
			case 'h':
			case 'H':
				print_help();
				break;
			default:
				break;
			}
		}

		/* Perform ADC conversion */
		ret = adc_read(adc_dev, &sequence);
		if (ret < 0) {
			printf("Warning: ADC read failed (%d)\n", ret);
			set_rgb(1, 0, 0);
			k_msleep(500);
			continue;
		}

		uint32_t raw_code = sample_buffer[0] & 0x0FFF; /* 12-bit mask */
		float voltage_mv = ((float)raw_code * 1200.0f) / 4095.0f;
		float temp_c = get_temperature(raw_code);

		if (temp_c == -1.0f) {
			/* Out of range fallback using typical silicon sensitivity: ~1.8 mV / °C */
			temp_c = 25.0f + ((float)raw_code - 1080.0f) * 0.4f;
		}

		float temp_f = temp_c * 1.8f + 32.0f;

		sample_count++;
		sum_temp += temp_c;
		if (temp_c < min_temp) min_temp = temp_c;
		if (temp_c > max_temp) max_temp = temp_c;
		float avg_temp = sum_temp / (float)sample_count;

		/* Thermal status classification & RGB LED feedback */
		const char *status_str = "NORMAL 🟢";
		if (temp_c >= 50.0f) {
			status_str = "HOT 🔥 🔴";
			set_rgb(1, 0, 0); /* Red: Hot */
		} else if (temp_c >= 35.0f) {
			status_str = "WARM 🟡";
			set_rgb(1, 1, 0); /* Yellow: Warm */
		} else {
			status_str = "NORMAL 🟢";
			set_rgb(0, 1, 0); /* Green: Normal */
		}

		if (visual_mode) {
			render_temp_gauge(gauge_buf, sizeof(gauge_buf) - 1, temp_c, 10.0f, 60.0f, 31);
			printf("[#%04u] Die Temp %s %5.1f°C (%5.1f°F) | Raw: %4u | Volt: %5.1f mV | [%s]\n",
			       sample_count, gauge_buf, temp_c, temp_f, raw_code, voltage_mv, status_str);
			printf("        Statistics: Min: %.1f°C | Max: %.1f°C | Avg: %.1f°C | Samples: %u\n\n",
			       min_temp, max_temp, avg_temp, sample_count);
		} else {
			printf("[ADC #%04u] Temp: %5.1f°C (%5.1f°F) | Raw: %4u (0x%03X) | Volt: %5.1f mV | Min/Avg/Max: %.1f/%.1f/%.1f°C\n",
			       sample_count, temp_c, temp_f, raw_code, raw_code, voltage_mv,
			       min_temp, avg_temp, max_temp);
		}

		k_msleep(500); /* 2 Hz monitoring rate */
	}

	return 0;
}

