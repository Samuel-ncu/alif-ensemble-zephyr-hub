/*
 * Copyright (c) 2026 Samuel (Shu-Ming) Liu
 * SPDX-License-Identifier: Apache-2.0
 *
 * Example 06: Alif Ensemble E8 - Universal Timer (UTIMER) Hardware PWM & RGB LED Breathing
 *
 * Demonstrates high-resolution multi-channel hardware PWM using Alif Ensemble's
 * on-chip 32-bit Universal Timers (UTIMER 5, 6, 7):
 * - Red LED   (LED1_R, P6_2) -> UTIMER5 Channel 0 (T0) via pwm5
 * - Green LED (LED1_G, P6_4) -> UTIMER6 Channel 0 (T0) via pwm6
 * - Blue LED  (LED1_B, P6_6) -> UTIMER7 Channel 0 (T0) via pwm7
 *
 * Features:
 * 1. Zero CPU-load hardware PWM pulse generation with 20ms (50Hz) or 1ms (1kHz) base period.
 * 2. Perceptual Gamma-corrected smooth breathing curves (sine/quadratic).
 * 3. 360-degree HSV Color Wheel smooth transitions.
 * 4. Real-time telemetry and interactive control via SEGGER RTT Viewer.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <SEGGER_RTT.h>

/* PWM Device Tree Specifications from app.overlay */
static const struct pwm_dt_spec pwm_r = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led_red));
static const struct pwm_dt_spec pwm_g = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led_green));
static const struct pwm_dt_spec pwm_b = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led_blue));

/* PWM Period: 20 ms (50 Hz standard servo/LED PWM frequency) */
#define PWM_PERIOD_DEFAULT_NS  PWM_MSEC(20)

/* Animation Modes */
enum anim_mode {
	MODE_RAINBOW = 0,    /* HSV 360-degree smooth color wheel */
	MODE_BREATHE_SYNC,   /* Synchronous breathing across all channels */
	MODE_BREATHE_CHASE,  /* 120-degree phase-shifted RGB breathing */
	MODE_STATIC_COLOR,   /* Fixed manual color output */
	MODE_MAX
};

static const char *mode_names[] = {
	"HSV Rainbow Color Wheel",
	"Synchronous White Breathing",
	"Phase-Shifted RGB Chase Breathing",
	"Static Color Hold"
};

/* Gamma correction table (quadratic scaling for human eye perception) */
static inline uint32_t apply_gamma(uint32_t val, uint32_t max)
{
	if (max == 0) return 0;
	return (val * val) / max;
}

/* Helper to convert Hue (0..359) to RGB (0..255) */
static void hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b)
{
	if (s == 0) {
		*r = *g = *b = v;
		return;
	}

	h = h % 360;
	uint8_t region = h / 60;
	uint16_t remainder = (h - (region * 60)) * 6;

	uint8_t p = (v * (255 - s)) >> 8;
	uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
	uint8_t t = (v * (255 - ((s * (1530 - remainder)) >> 8))) >> 8;

	switch (region) {
	case 0:  *r = v; *g = t; *b = p; break;
	case 1:  *r = q; *g = v; *b = p; break;
	case 2:  *r = p; *g = v; *b = t; break;
	case 3:  *r = p; *g = q; *b = v; break;
	case 4:  *r = t; *g = p; *b = v; break;
	default: *r = v; *g = p; *b = q; break;
	}
}

/* Set RGB PWM pulse widths based on 0..1000 per-mille (0.1% resolution) */
static int set_rgb_permille(uint32_t r_pm, uint32_t g_pm, uint32_t b_pm, uint32_t period_ns)
{
	int ret;

	if (r_pm > 1000) r_pm = 1000;
	if (g_pm > 1000) g_pm = 1000;
	if (b_pm > 1000) b_pm = 1000;

	/* Calculate pulse width with gamma correction */
	uint32_t pulse_r = (apply_gamma(r_pm, 1000) * period_ns) / 1000;
	uint32_t pulse_g = (apply_gamma(g_pm, 1000) * period_ns) / 1000;
	uint32_t pulse_b = (apply_gamma(b_pm, 1000) * period_ns) / 1000;

	ret = pwm_set_pulse_dt(&pwm_r, pulse_r);
	if (ret < 0) return ret;

	ret = pwm_set_pulse_dt(&pwm_g, pulse_g);
	if (ret < 0) return ret;

	ret = pwm_set_pulse_dt(&pwm_b, pulse_b);
	if (ret < 0) return ret;

	return 0;
}

static void print_banner(void)
{
	printf("\n\n===============================================================\n");
	printf("🌈  Alif Ensemble E8 - Hardware UTIMER PWM Color Engine 🌈\n");
	printf(" Target Core : ARM Cortex-M55 (RTSS-HP)\n");
	printf(" Controllers : UTIMER5 (Ch0: Red), UTIMER6 (Ch0: Green), UTIMER7 (Ch0: Blue)\n");
	printf(" Resolution  : 32-bit Hardware Counter, 20ms (50Hz) Base Period\n");
	printf(" Console     : SEGGER RTT Real-Time Monitor\n");
	printf("===============================================================\n\n");
	printf("🎮 Interactive Commands (Type in RTT Viewer Input Bar):\n");
	printf("  [m] Next Animation Mode (Rainbow -> Sync -> Chase -> Static)\n");
	printf("  [+] Increase Animation Speed\n");
	printf("  [-] Decrease Animation Speed\n");
	printf("  [p] Pause / Resume Animation\n");
	printf("  [r] Static RED        [g] Static GREEN     [b] Static BLUE\n");
	printf("  [c] Static CYAN       [m] Static MAGENTA   [y] Static YELLOW\n");
	printf("  [w] Static WHITE      [o] Turn OFF (0%%)\n");
	printf("  [h] Print Help Menu\n");
	printf("---------------------------------------------------------------\n\n");
}

int main(void)
{
	int ret;
	uint32_t period_ns = pwm_r.period ? pwm_r.period : PWM_PERIOD_DEFAULT_NS;
	enum anim_mode mode = MODE_RAINBOW;
	bool paused = false;
	uint32_t step_delay_ms = 25;  /* Animation refresh rate: ~40 FPS */

	/* Animation state variables */
	uint16_t hue = 0;
	uint32_t breathe_counter = 0;
	bool breathe_up = true;
	uint32_t r_val = 0, g_val = 0, b_val = 0;
	uint32_t last_log_time = 0;

	print_banner();

	/* 1. Verify PWM Device Readiness */
	printf("[PWM-INIT] Checking Hardware PWM Devices...\n");

	if (!pwm_is_ready_dt(&pwm_r)) {
		printf("[ERROR] PWM device for RED LED (UTIMER5) is not ready!\n");
		return -ENODEV;
	}
	printf("  ✔ RED LED   : UTIMER5 Ch0 (P6_2) Ready (Period: %u ns)\n", pwm_r.period);

	if (!pwm_is_ready_dt(&pwm_g)) {
		printf("[ERROR] PWM device for GREEN LED (UTIMER6) is not ready!\n");
		return -ENODEV;
	}
	printf("  ✔ GREEN LED : UTIMER6 Ch0 (P6_4) Ready (Period: %u ns)\n", pwm_g.period);

	if (!pwm_is_ready_dt(&pwm_b)) {
		printf("[ERROR] PWM device for BLUE LED (UTIMER7) is not ready!\n");
		return -ENODEV;
	}
	printf("  ✔ BLUE LED  : UTIMER7 Ch0 (P6_6) Ready (Period: %u ns)\n", pwm_b.period);

	printf("[PWM-INIT] All 3 Hardware Universal Timers Online & Locked!\n\n");

	/* Initial startup sequence: Quick 3-color test pulse */
	set_rgb_permille(1000, 0, 0, period_ns); k_msleep(200);
	set_rgb_permille(0, 1000, 0, period_ns); k_msleep(200);
	set_rgb_permille(0, 0, 1000, period_ns); k_msleep(200);
	set_rgb_permille(0, 0, 0, period_ns);    k_msleep(200);

	printf("[PWM-RUN] Starting Active Animation: %s\n\n", mode_names[mode]);

	/* Main Animation & Telemetry Loop */
	while (1) {
		/* Handle Interactive Keyboard Commands from RTT Viewer */
		if (SEGGER_RTT_HasKey()) {
			int ch = SEGGER_RTT_GetKey();
			switch (ch) {
			case 'm':
			case 'M':
				mode = (mode + 1) % MODE_MAX;
				printf("\n>>> Switched to Mode [%u]: %s <<<\n", mode, mode_names[mode]);
				break;
			case '+':
			case '=':
				if (step_delay_ms > 5) step_delay_ms -= 5;
				printf(">>> Speed Up: Step delay = %u ms (~%u FPS) <<<\n",
				       step_delay_ms, 1000 / step_delay_ms);
				break;
			case '-':
			case '_':
				if (step_delay_ms < 200) step_delay_ms += 5;
				printf(">>> Slow Down: Step delay = %u ms (~%u FPS) <<<\n",
				       step_delay_ms, 1000 / step_delay_ms);
				break;
			case 'p':
			case 'P':
			case ' ':
				paused = !paused;
				printf(">>> Animation %s <<<\n", paused ? "PAUSED" : "RESUMED");
				break;
			case 'r': case 'R':
				mode = MODE_STATIC_COLOR; r_val = 1000; g_val = 0; b_val = 0;
				printf(">>> Static Color: RED <<<\n");
				break;
			case 'g': case 'G':
				mode = MODE_STATIC_COLOR; r_val = 0; g_val = 1000; b_val = 0;
				printf(">>> Static Color: GREEN <<<\n");
				break;
			case 'b': case 'B':
				mode = MODE_STATIC_COLOR; r_val = 0; g_val = 0; b_val = 1000;
				printf(">>> Static Color: BLUE <<<\n");
				break;
			case 'c': case 'C':
				mode = MODE_STATIC_COLOR; r_val = 0; g_val = 1000; b_val = 1000;
				printf(">>> Static Color: CYAN <<<\n");
				break;
			case 'y': case 'Y':
				mode = MODE_STATIC_COLOR; r_val = 1000; g_val = 1000; b_val = 0;
				printf(">>> Static Color: YELLOW <<<\n");
				break;
			case 'w': case 'W':
				mode = MODE_STATIC_COLOR; r_val = 1000; g_val = 1000; b_val = 1000;
				printf(">>> Static Color: WHITE <<<\n");
				break;
			case 'o': case 'O':
				mode = MODE_STATIC_COLOR; r_val = 0; g_val = 0; b_val = 0;
				printf(">>> LEDs OFF (0%%) <<<\n");
				break;
			case 'h': case 'H':
			case '?':
				print_banner();
				break;
			default:
				break;
			}
		}

		/* Compute Animation Frame if not paused */
		if (!paused) {
			switch (mode) {
			case MODE_RAINBOW: {
				uint8_t r8, g8, b8;
				hsv_to_rgb(hue, 255, 255, &r8, &g8, &b8);
				r_val = (r8 * 1000) / 255;
				g_val = (g8 * 1000) / 255;
				b_val = (b8 * 1000) / 255;

				hue = (hue + 1) % 360;
				break;
			}
			case MODE_BREATHE_SYNC: {
				if (breathe_up) {
					breathe_counter += 10;
					if (breathe_counter >= 1000) {
						breathe_counter = 1000;
						breathe_up = false;
					}
				} else {
					if (breathe_counter <= 10) {
						breathe_counter = 0;
						breathe_up = true;
					} else {
						breathe_counter -= 10;
					}
				}
				r_val = g_val = b_val = breathe_counter;
				break;
			}
			case MODE_BREATHE_CHASE: {
				/* Triangular chase breathing with 120-degree phase shift */
				static uint32_t phase = 0;
				phase = (phase + 1) % 300;

				int r_p = (int)phase;
				int g_p = (int)((phase + 100) % 300);
				int b_p = (int)((phase + 200) % 300);

				r_val = (r_p < 150) ? (r_p * 1000 / 150) : ((300 - r_p) * 1000 / 150);
				g_val = (g_p < 150) ? (g_p * 1000 / 150) : ((300 - g_p) * 1000 / 150);
				b_val = (b_p < 150) ? (b_p * 1000 / 150) : ((300 - b_p) * 1000 / 150);
				break;
			}
			case MODE_STATIC_COLOR:
			default:
				/* Maintain current static values */
				break;
			}

			/* Apply to Hardware Universal Timers */
			ret = set_rgb_permille(r_val, g_val, b_val, period_ns);
			if (ret < 0) {
				printf("[WARN] Failed to set PWM duty: %d\n", ret);
			}
		}

		/* Periodic Telemetry Stream every ~2000 ms */
		uint32_t now = k_uptime_get_32();
		if (now - last_log_time >= 2000) {
			last_log_time = now;
			printf("📊 [UTIMER PWM] Mode: %-25s | Duty R:%3u%% G:%3u%% B:%3u%% | Delay: %ums\n",
			       mode_names[mode],
			       r_val / 10, g_val / 10, b_val / 10,
			       step_delay_ms);
		}

		k_msleep(step_delay_ms);
	}

	return 0;
}

