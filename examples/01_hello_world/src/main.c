/*
 * Copyright (c) 2026 Samuel (Shu-Ming) Liu
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>

int main(void)
{
	uint32_t count = 0;

	printf("\n========================================\n");
	printf("*** Alif Ensemble E8 - Hello World ***\n");
	printf("Target Board: %s\n", CONFIG_BOARD);
	printf("RTOS: Zephyr OS %s\n", KERNEL_VERSION_STRING);
	printf("========================================\n");

	while (1) {
		printf("Hello World from Alif Ensemble! (count=%u)\n", ++count);
		k_msleep(1000);
	}

	return 0;
}

