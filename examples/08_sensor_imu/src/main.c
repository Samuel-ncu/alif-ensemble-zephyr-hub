/*
 * Copyright (c) 2026 Samuel (Shu-Ming) Liu
 * SPDX-License-Identifier: Apache-2.0
 *
 * Example 08: Alif Ensemble E8 - Bosch BMI323 6-Axis IMU over I3C
 *
 * Features:
 *  - I3C Bus Master communication with onboard Bosch BMI323 (dynamic address 0xA)
 *  - High-speed 3-axis Accelerometer & 3-axis Gyroscope sampling (100 Hz)
 *  - Cortex-M55 FPU-accelerated 3D Attitude Estimation (Pitch & Roll)
 *  - Dynamic On-board RGB LED attitude indicator (Level=Green, Pitch=Red, Roll=Blue, Motion=Flash)
 *  - Zero-latency SEGGER RTT Console output at 0x20000410 (JTAG, 4000 kHz)
 *  - Interactive RTT keyboard commands ('c': tare/zero, 'r': reset cal, 'm': toggle mode, 'h': help)
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>
#include <SEGGER_RTT.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define RAD_TO_DEG (180.0f / M_PI)

/* Onboard RGB LED (Port 6: LED1_R=Pin 2, LED1_G=Pin 4, LED1_B=Pin 6) */
#define PIN_LED_R 2
#define PIN_LED_G 4
#define PIN_LED_B 6

static const struct device *gpio6_dev = DEVICE_DT_GET(DT_NODELABEL(gpio6));
static const struct device *imu_dev   = DEVICE_DT_GET_ONE(bosch_bmi323);

/* RGB LED Helper */
static inline void set_rgb(int r, int g, int b)
{
	if (gpio6_dev && device_is_ready(gpio6_dev)) {
		gpio_pin_set(gpio6_dev, PIN_LED_R, r ? 1 : 0);
		gpio_pin_set(gpio6_dev, PIN_LED_G, g ? 1 : 0);
		gpio_pin_set(gpio6_dev, PIN_LED_B, b ? 1 : 0);
	}
}

/* Helper to convert sensor_value struct to float */
static inline float to_float(const struct sensor_value *val)
{
	return (float)val->val1 + ((float)val->val2 / 1000000.0f);
}

/* Configure BMI323 Accelerometer & Gyroscope */
static int configure_bmi323(const struct device *dev)
{
	struct sensor_value full_scale, sampling_freq, feat_mask;
	int ret;

	/* 1. Configure Accelerometer: +/- 2G, 100 Hz, High Performance */
	full_scale.val1 = 2;
	full_scale.val2 = 0;
	sampling_freq.val1 = 100;
	sampling_freq.val2 = 0;
	feat_mask.val1 = 1;
	feat_mask.val2 = 0;

	ret = sensor_attr_set(dev, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_FULL_SCALE, &full_scale);
	if (ret < 0) {
		printf("Error: Failed to set Accel full scale (%d)\n", ret);
		return ret;
	}

	ret = sensor_attr_set(dev, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_FEATURE_MASK, &feat_mask);
	if (ret < 0) {
		printf("Error: Failed to set Accel feature mask (%d)\n", ret);
		return ret;
	}

	ret = sensor_attr_set(dev, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &sampling_freq);
	if (ret < 0) {
		printf("Error: Failed to set Accel sampling rate (%d)\n", ret);
		return ret;
	}

	/* 2. Configure Gyroscope: +/- 500 dps, 100 Hz, High Performance */
	full_scale.val1 = 500;
	full_scale.val2 = 0;
	sampling_freq.val1 = 100;
	sampling_freq.val2 = 0;
	feat_mask.val1 = 1;
	feat_mask.val2 = 0;

	ret = sensor_attr_set(dev, SENSOR_CHAN_GYRO_XYZ, SENSOR_ATTR_FULL_SCALE, &full_scale);
	if (ret < 0) {
		printf("Error: Failed to set Gyro full scale (%d)\n", ret);
		return ret;
	}

	ret = sensor_attr_set(dev, SENSOR_CHAN_GYRO_XYZ, SENSOR_ATTR_FEATURE_MASK, &feat_mask);
	if (ret < 0) {
		printf("Error: Failed to set Gyro feature mask (%d)\n", ret);
		return ret;
	}

	ret = sensor_attr_set(dev, SENSOR_CHAN_GYRO_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &sampling_freq);
	if (ret < 0) {
		printf("Error: Failed to set Gyro sampling rate (%d)\n", ret);
		return ret;
	}

	return 0;
}

/* Render ASCII Horizon Bar */
static void render_bar(char *buf, size_t buf_len, float angle, float max_angle, int width)
{
	int center = width / 2;
	int pos = center + (int)((angle / max_angle) * (float)center);
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
	printf("   🕹️  BMI323 6-Axis IMU Interactive Keyboard Commands  🕹️\n");
	printf("===============================================================\n");
	printf("  'c' : Calibrate / Tare current attitude to (0.0°, 0.0°)\n");
	printf("  'r' : Reset attitude calibration to factory zero\n");
	printf("  'm' : Toggle display mode (Compact Stream <-> Horizon Bar)\n");
	printf("  'h' : Display this help menu\n");
	printf("===============================================================\n\n");
}

int main(void)
{
	int ret;
	uint32_t sample_count = 0;
	float pitch_offset = 0.0f;
	float roll_offset = 0.0f;
	bool visual_mode = true;
	char bar_buf[33];

	/* 1. Initialize RGB LED */
	if (gpio6_dev && device_is_ready(gpio6_dev)) {
		gpio_pin_configure(gpio6_dev, PIN_LED_R, GPIO_OUTPUT_INACTIVE);
		gpio_pin_configure(gpio6_dev, PIN_LED_G, GPIO_OUTPUT_INACTIVE);
		gpio_pin_configure(gpio6_dev, PIN_LED_B, GPIO_OUTPUT_INACTIVE);
		set_rgb(0, 0, 0);
	}

	/* Power-on indicator: 3 quick Cyan pulses */
	for (int i = 0; i < 3; i++) {
		set_rgb(0, 1, 1);
		k_msleep(80);
		set_rgb(0, 0, 0);
		k_msleep(80);
	}

	/* 2. Welcome Banner over SEGGER RTT */
	printf("\n\n===============================================================\n");
	printf("🎯 Alif Ensemble E8 - Bosch BMI323 6-Axis IMU over I3C 🎯\n");
	printf(" Target Core  : ARM Cortex-M55 (RTSS-HP @ 400 MHz)\n");
	printf(" Bus Protocol : I3C Master @ 0x49034000 (Dynamic Addr: 0x%02X)\n", 0x0A);
	printf(" RTT Buffer   : Local 0x%p\n", &_SEGGER_RTT);
	printf("===============================================================\n");

	/* 3. Check BMI323 device binding */
	if (!device_is_ready(imu_dev)) {
		printf("FATAL: Bosch BMI323 IMU device is NOT ready!\n");
		set_rgb(1, 0, 0); /* Red solid error */
		return 0;
	}
	printf("✓ BMI323 IMU device bound successfully: %s\n", imu_dev->name);

	/* 4. Configure sensor attributes */
	ret = configure_bmi323(imu_dev);
	if (ret < 0) {
		printf("FATAL: Failed to configure BMI323 attributes (%d)\n", ret);
		set_rgb(1, 0, 0);
		return 0;
	}
	printf("✓ BMI323 Accelerometer & Gyroscope initialized (100 Hz, HP mode)\n");
	print_help();

	while (1) {
		struct sensor_value acc[3], gyr[3], temp;
		int retries = 10;

		/* Check interactive keystrokes from J-Link RTT Viewer */
		if (SEGGER_RTT_HasKey()) {
			int key = SEGGER_RTT_GetKey();
			switch (key) {
			case 'c':
			case 'C': {
				/* Fetch fresh sample to calculate tare offset */
				sensor_sample_fetch(imu_dev);
				sensor_channel_get(imu_dev, SENSOR_CHAN_ACCEL_XYZ, acc);
				float ax = to_float(&acc[0]);
				float ay = to_float(&acc[1]);
				float az = to_float(&acc[2]);
				pitch_offset = atan2f(ax, sqrtf(ay * ay + az * az)) * RAD_TO_DEG;
				roll_offset  = atan2f(ay, az) * RAD_TO_DEG;
				printf("\n>>> [TARE] Calibration set! Pitch offset: %+.2f°, Roll offset: %+.2f° <<<\n\n",
				       pitch_offset, roll_offset);
				set_rgb(1, 1, 0); /* Yellow flash */
				k_msleep(150);
				break;
			}
			case 'r':
			case 'R':
				pitch_offset = 0.0f;
				roll_offset  = 0.0f;
				printf("\n>>> [RESET] Calibration reset to zero <<<\n\n");
				set_rgb(1, 0, 1); /* Magenta flash */
				k_msleep(150);
				break;
			case 'm':
			case 'M':
				visual_mode = !visual_mode;
				printf("\n>>> [MODE] Switched to %s mode <<<\n\n",
				       visual_mode ? "Visual Horizon Bar" : "Compact Telemetry Stream");
				break;
			case 'h':
			case 'H':
				print_help();
				break;
			default:
				break;
			}
		}

		/* Fetch sample with retry for DRDY */
		do {
			ret = sensor_sample_fetch(imu_dev);
			if (ret != -ENODATA) {
				break;
			}
			k_msleep(10);
		} while (--retries);

		if (ret < 0) {
			printf("Warning: Sensor fetch failed (%d)\n", ret);
			k_msleep(100);
			continue;
		}

		sensor_channel_get(imu_dev, SENSOR_CHAN_ACCEL_XYZ, acc);
		sensor_channel_get(imu_dev, SENSOR_CHAN_GYRO_XYZ, gyr);
		sensor_channel_get(imu_dev, SENSOR_CHAN_DIE_TEMP, &temp);

		float ax = to_float(&acc[0]);
		float ay = to_float(&acc[1]);
		float az = to_float(&acc[2]);

		float gx = to_float(&gyr[0]);
		float gy = to_float(&gyr[1]);
		float gz = to_float(&gyr[2]);

		float die_temp = to_float(&temp);

		/* 3D Attitude Estimation */
		float pitch = (atan2f(ax, sqrtf(ay * ay + az * az)) * RAD_TO_DEG) - pitch_offset;
		float roll  = (atan2f(ay, az) * RAD_TO_DEG) - roll_offset;

		/* Angular velocity magnitude */
		float gyro_mag = sqrtf(gx * gx + gy * gy + gz * gz);

		/* Attitude classification & RGB LED feedback */
		const float DEAD_ZONE = 6.0f; /* Degrees */
		bool is_level = (fabsf(pitch) < DEAD_ZONE) && (fabsf(roll) < DEAD_ZONE);
		bool is_moving = (gyro_mag > 40.0f);

		if (is_moving) {
			/* High dynamic rotation: Flash White */
			set_rgb(1, 1, 1);
		} else if (is_level) {
			/* Horizon level: Solid Green */
			set_rgb(0, 1, 0);
		} else {
			/* Tilted: Red for Pitch, Blue for Roll, Magenta for both */
			int r = (fabsf(pitch) >= DEAD_ZONE) ? 1 : 0;
			int b = (fabsf(roll) >= DEAD_ZONE) ? 1 : 0;
			set_rgb(r, 0, b);
		}

		sample_count++;

		if (visual_mode) {
			/* Visual ASCII Horizon Bar */
			render_bar(bar_buf, sizeof(bar_buf) - 1, pitch, 45.0f, 31);
			printf("[#%04u] Pitch %s %+.1f°  | Acc: [%+.2f, %+.2f, %+.2f]g\n",
			       sample_count, bar_buf, pitch, ax, ay, az);

			render_bar(bar_buf, sizeof(bar_buf) - 1, roll, 45.0f, 31);
			printf("        Roll  %s %+.1f°  | Gyr: [%+.1f, %+.1f, %+.1f]°/s | T: %.1f°C | %s\n\n",
			       bar_buf, roll, gx, gy, gz, die_temp,
			       is_level ? "[LEVEL 🟢]" : "[TILTED 🔴]");
		} else {
			/* Compact Telemetry Line */
			printf("[IMU #%04u] P:%+05.1f° R:%+05.1f° | A:[%+0.2f,%+0.2f,%+0.2f]g | G:[%+05.1f,%+05.1f,%+05.1f]°/s | T:%4.1f°C | %s\n",
			       sample_count, pitch, roll, ax, ay, az, gx, gy, gz, die_temp,
			       is_level ? "LEVEL" : (fabsf(pitch) > fabsf(roll) ? "PITCH" : "ROLL"));
		}

		k_msleep(100); /* 10 Hz telemetry refresh */
	}

	return 0;
}

