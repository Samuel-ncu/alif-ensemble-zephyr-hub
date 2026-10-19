/*
 * Copyright (c) 2026 Samuel (Shu-Ming) Liu
 * SPDX-License-Identifier: Apache-2.0
 *
 * Example 05: Alif Ensemble E8 - Secure Enclave Services & Hardware Security Telemetry
 *
 * Demonstrates real-time inter-processor service requests between the Cortex-M55
 * application core and the onboard Secure Enclave (SES):
 * 1. SE Synchronization & Heartbeat
 * 2. Device Identity & Part Number (AE822FA0E5597LS0)
 * 3. Secure Boot Lifecycle State (LCS) & DCU settings
 * 4. Hardware Root-of-Trust Key Hashes (HBK0, HBK1, HBK_FW)
 * 5. Table of Contents (TOC) firmware image queries
 * 6. Live True Random Number Generation (TRNG) hardware stream
 * 7. Interactive commands over SEGGER RTT Viewer
 */

#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <se_service.h>
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

/* Helper to convert raw binary array into a clean hex string */
static void bytes_to_hex(char *dst, size_t dst_size, const volatile uint8_t *src, size_t len)
{
	size_t pos = 0;
	for (size_t i = 0; i < len && (pos + 3) < dst_size; i++) {
		pos += snprintf(&dst[pos], dst_size - pos, "%02X", src[i]);
	}
	dst[pos] = '\0';
}

/* Lifecycle State (LCS) decoder */
static const char *lcs_to_string(uint8_t lcs)
{
	switch (lcs) {
	case 0x00: return "CM (Chip Manufacture)";
	case 0x01: return "DM (Device Manufacture)";
	case 0x05: return "SE (Secure Enabled - Production)";
	case 0x07: return "RMA (Return Merchandise Authorization)";
	default:   return "Custom / Development";
	}
}

int main(void)
{
	int ret;
	uint32_t dev_part_num = 0;
	uint32_t toc_count = 0;
	uint32_t toc_version = 0;
	uint8_t se_rev[80] = {0};
	get_device_revision_data_t dev_data;
	char hex_buf[128];
	uint32_t loop_count = 0;
	bool led_state = false;

	/* 1. Initialize Onboard Status LEDs */
	if (gpio6_dev && device_is_ready(gpio6_dev)) {
		gpio_pin_configure(gpio6_dev, PIN_LED_R, GPIO_OUTPUT_INACTIVE);
		gpio_pin_configure(gpio6_dev, PIN_LED_G, GPIO_OUTPUT_INACTIVE);
		gpio_pin_configure(gpio6_dev, PIN_LED_B, GPIO_OUTPUT_INACTIVE);
		set_rgb(0, 0, 0);
	}

	/* Startup 3x Blue flash */
	for (int i = 0; i < 3; i++) {
		set_rgb(0, 0, 1);
		k_msleep(100);
		set_rgb(0, 0, 0);
		k_msleep(100);
	}

	/* Turn Blue ON during SE handshake */
	set_rgb(0, 0, 1);

	/* 2. Welcome Banner */
	printf("\n\n===============================================================\n");
	printf("🛡️  Alif Ensemble E8 - Secure Enclave (SES) Services Active 🛡️\n");
	printf(" Target Core : ARM Cortex-M55 (%s)\n",
#if defined(CONFIG_SOC_AE822FA0E5597XX0_RTSS_HP)
	       "RTSS-HP"
#else
	       "RTSS-HE"
#endif
	);
	printf(" Communication: Inter-Processor Message Handling Unit (MHU)\n");
	printf("===============================================================\n\n");

	/* 3. Synchronize with Secure Enclave */
	printf("[SE-SVC] Synchronizing with Secure Enclave...\n");
	ret = se_service_sync();
	if (ret != 0) {
		printf("[ERROR] SE synchronization failed! (ret = %d)\n", ret);
		/* Flash Red on failure */
		set_rgb(1, 0, 0);
		return ret;
	}
	printf("[SE-SVC] Secure Enclave Handshake: SUCCESS (SE is Ready!)\n\n");

	/* 4. Query SE Firmware Revision */
	ret = se_service_get_se_revision(se_rev);
	if (ret == 0) {
		printf("  📌 SE Firmware Rev : %s\n", se_rev);
	}

	/* 5. Query Device Part Number */
	ret = se_service_get_device_part_number(&dev_part_num);
	if (ret == 0) {
		printf("  📌 Part Number ID  : %u (0x%04X)\n", dev_part_num, dev_part_num);
	}

	/* 6. Query Table of Contents (TOC) Info */
	se_service_get_toc_number(&toc_count);
	se_service_get_toc_version(&toc_version);
	printf("  📌 TOC Image Count : %u entries (TOC Version: 0x%08X)\n", toc_count, toc_version);

	/* 7. Query Comprehensive Security & Identity Data */
	memset(&dev_data, 0, sizeof(dev_data));
	ret = se_service_system_get_device_data(&dev_data);
	if (ret == 0) {
		printf("  📌 Silicon Part#   : %s\n", dev_data.ALIF_PN);
		printf("  📌 Revision ID     : %u (0x%08X)\n", dev_data.revision_id, dev_data.revision_id);

		bytes_to_hex(hex_buf, sizeof(hex_buf), dev_data.SerialN, sizeof(dev_data.SerialN));
		printf("  📌 Serial Number   : 0x%s\n", hex_buf);

		printf("  📌 Lifecycle State : 0x%02X -> %s\n", dev_data.LCS, lcs_to_string(dev_data.LCS));

		bytes_to_hex(hex_buf, sizeof(hex_buf), dev_data.HBK0, sizeof(dev_data.HBK0));
		printf("  🔑 Root Hash (HBK0): 0x%s\n", hex_buf);

		bytes_to_hex(hex_buf, sizeof(hex_buf), dev_data.HBK1, sizeof(dev_data.HBK1));
		printf("  🔑 App Hash (HBK1) : 0x%s\n", hex_buf);

		bytes_to_hex(hex_buf, sizeof(hex_buf), dev_data.DCU, sizeof(dev_data.DCU));
		printf("  🔒 DCU Permissions : 0x%s\n", hex_buf);

		bytes_to_hex(hex_buf, sizeof(hex_buf), dev_data.MfgData, sizeof(dev_data.MfgData));
		printf("  🏭 Mfg MetaData    : 0x%s\n", hex_buf);
	} else {
		printf("[WARN] Could not retrieve full device security data (ret = %d)\n", ret);
	}

	printf("\n===============================================================\n");
	printf(">> Interactive Commands (Type in RTT Viewer and hit Enter):\n");
	printf("   'r' : Generate 16 bytes of hardware TRNG entropy\n");
	printf("   'h' : Send ping heartbeat to Secure Enclave\n");
	printf("   't' : Re-query Table of Contents (TOC) status\n");
	printf("===============================================================\n\n");

	/* Flash Green 3 times to signal full SE initialization success */
	for (int i = 0; i < 3; i++) {
		set_rgb(0, 1, 0);
		k_msleep(100);
		set_rgb(0, 0, 0);
		k_msleep(100);
	}

	/* 8. Main Monitoring Loop */
	while (1) {
		uint8_t trng_bytes[8] = {0};
		int64_t uptime_ms = k_uptime_get();
		led_state = !led_state;

		/* Heartbeat: Toggle Cyan (Green + Blue) */
		set_rgb(0, led_state ? 1 : 0, led_state ? 1 : 0);

		/* Query Hardware TRNG from Secure Enclave every 2 seconds */
		ret = se_service_get_rnd_num(trng_bytes, sizeof(trng_bytes));
		if (ret == 0) {
			bytes_to_hex(hex_buf, sizeof(hex_buf), trng_bytes, sizeof(trng_bytes));
			printf("[SE-TRNG #%04u] Uptime: %6lld ms | Hardware Entropy: 0x%s | OK\n",
			       loop_count++, uptime_ms, hex_buf);
		} else {
			printf("[SE-TRNG #%04u] Uptime: %6lld ms | TRNG Query Error: %d\n",
			       loop_count++, uptime_ms, ret);
		}

		/* Check for interactive user input via SEGGER RTT */
		if (SEGGER_RTT_HasKey()) {
			int key = SEGGER_RTT_GetKey();

			if (key == 'r' || key == 'R') {
				uint8_t entropy[16] = {0};
				set_rgb(1, 0, 1); /* Purple */
				if (se_service_get_rnd_num(entropy, sizeof(entropy)) == 0) {
					bytes_to_hex(hex_buf, sizeof(hex_buf), entropy, sizeof(entropy));
					printf("\n>>> [USER CMD: TRNG 128-bit] 0x%s <<<\n\n", hex_buf);
				}
			} else if (key == 'h' || key == 'H') {
				set_rgb(1, 1, 0); /* Yellow */
				int hb_ret = se_service_heartbeat();
				printf("\n>>> [USER CMD: SE Heartbeat] Status: %s (code: %d) <<<\n\n",
				       hb_ret == 0 ? "ALIVE & READY" : "NO RESPONSE", hb_ret);
			} else if (key == 't' || key == 'T') {
				set_rgb(0, 1, 1); /* Cyan */
				se_service_get_toc_number(&toc_count);
				se_service_get_toc_version(&toc_version);
				printf("\n>>> [USER CMD: TOC Info] Count: %u, Version: 0x%08X <<<\n\n",
				       toc_count, toc_version);
			} else {
				set_rgb(1, 0, 1); /* Purple */
				printf(">>> [RTT RX] Key '%c' (0x%02X) received. Available: 'r', 'h', 't' <<<\n",
				       (key >= 32 && key <= 126) ? (char)key : '?', (unsigned char)key);
			}
			k_msleep(100);
		}

		k_msleep(2000);
	}

	return 0;
}

