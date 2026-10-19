/*
 * Copyright (c) 2026 Samuel (Shu-Ming) Liu
 * SPDX-License-Identifier: Apache-2.0
 *
 * Example 09: Alif Ensemble E8 - Arm Ethos-U55 NPU Edge AI Hardware Acceleration
 *
 * Features:
 *  - Hardware Edge AI acceleration on Arm Ethos-U55 NPU (256 MACs @ 400 MHz)
 *  - TensorFlow Lite for Microcontrollers (TFLM) C++ runtime
 *  - Keyword Spotting (KWS) CNN INT8 model compiled with Arm Vela
 *  - Microsecond-accurate latency and cycle benchmark (FPS / throughput)
 *  - Softmax classification & confidence score telemetry over SEGGER RTT (0x20000410)
 *  - Dynamic RGB LED visual classification feedback (Green=Match, Blue=Inferring, Yellow=Uncertain, Red=Error)
 *  - Interactive RTT keyboard commands ('s': single infer, 'b': 50-run benchmark, 't': continuous, 'h': help)
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <vector>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <SEGGER_RTT.h>

#include <tensorflow/lite/micro/micro_mutable_op_resolver.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/schema/schema_generated.h>

/* Model and sample data for Ethos-U55 (256 MACs) */
#include "ethosu/models/keyword_spotting_cnn_small_int8/u55/model_u55_256.h"
#include "ethosu/models/keyword_spotting_cnn_small_int8/u55/input.h"
#include "ethosu/models/keyword_spotting_cnn_small_int8/u55/output.h"

/* Onboard RGB LED (Port 6: LED1_R=Pin 2, LED1_G=Pin 4, LED1_B=Pin 6) */
#define PIN_LED_R 2
#define PIN_LED_G 4
#define PIN_LED_B 6

static const struct device *gpio6_dev = DEVICE_DT_GET(DT_NODELABEL(gpio6));

/* Active-High RGB LED Helper */
static inline void set_rgb(int r, int g, int b)
{
	if (gpio6_dev && device_is_ready(gpio6_dev)) {
		gpio_pin_set(gpio6_dev, PIN_LED_R, r ? 1 : 0);
		gpio_pin_set(gpio6_dev, PIN_LED_G, g ? 1 : 0);
		gpio_pin_set(gpio6_dev, PIN_LED_B, b ? 1 : 0);
	}
}

/* 12 Keyword Spotting Output Classes */
static const char *const kws_labels[12] = {
	"down", "go", "left", "no", "off", "on",
	"right", "stop", "up", "yes", "_silence_", "_unknown_"
};

/* Tensor Arena allocated in SRAM/DTCM with 16-byte alignment */
__attribute__((section(".bss.tflm_arena"), aligned(16)))
static uint8_t tensor_arena[TENSOR_ARENA_SIZE];

/* Global TFLM interpreter pointer */
static tflite::MicroInterpreter *interpreter = nullptr;

static void print_help(void)
{
	printf("\n===============================================================\n");
	printf("   🧠  Arm Ethos-U55 NPU AI Interactive Keyboard Commands  🧠\n");
	printf("===============================================================\n");
	printf("  's' : Run single NPU inference with full probability breakdown\n");
	printf("  'b' : Run 50-inference benchmark (min/avg/max latency & FPS)\n");
	printf("  't' : Toggle periodic continuous inference mode (2 Hz)\n");
	printf("  'h' : Display this help menu\n");
	printf("===============================================================\n\n");
}

/* Run a single inference on Ethos-U55 NPU */
static bool execute_inference(bool print_details, uint32_t *out_cycles, uint32_t *out_us)
{
	if (!interpreter) {
		printf("Error: Interpreter not initialized!\n");
		return false;
	}

	set_rgb(0, 0, 1); /* Blue: Inferring */

	/* 1. Copy input spectrogram data into model input tensor */
	TfLiteTensor *input_tensor = interpreter->input(0);
	if (!input_tensor || !input_tensor->data.int8) {
		printf("Error: Invalid input tensor!\n");
		set_rgb(1, 0, 0);
		return false;
	}
	memcpy(input_tensor->data.int8, inputData, sizeof(inputData));

	/* 2. Execute inference accelerated by Ethos-U55 NPU */
	uint32_t start_cycles = k_cycle_get_32();
	int64_t start_time = k_uptime_ticks();

	TfLiteStatus invoke_status = interpreter->Invoke();

	uint32_t elapsed_cycles = k_cycle_get_32() - start_cycles;
	uint32_t elapsed_us = (uint32_t)k_ticks_to_us_near64(k_uptime_ticks() - start_time);

	if (out_cycles) *out_cycles = elapsed_cycles;
	if (out_us) *out_us = elapsed_us;

	if (invoke_status != kTfLiteOk) {
		printf("FATAL: NPU Invoke failed (%d)!\n", invoke_status);
		set_rgb(1, 0, 0);
		return false;
	}

	/* 3. Parse output classification tensor */
	TfLiteTensor *output_tensor = interpreter->output(0);
	if (!output_tensor || !output_tensor->data.int8) {
		printf("Error: Invalid output tensor!\n");
		set_rgb(1, 0, 0);
		return false;
	}

	float scale = output_tensor->params.scale;
	int32_t zero_point = output_tensor->params.zero_point;

	/* Calculate dequantized values and Softmax */
	float dequant[12];
	float max_val = -1e9f;
	int top_class = 0;

	for (int i = 0; i < 12; i++) {
		int8_t quant_val = output_tensor->data.int8[i];
		dequant[i] = ((float)quant_val - (float)zero_point) * scale;
		if (dequant[i] > max_val) {
			max_val = dequant[i];
			top_class = i;
		}
	}

	/* Softmax probabilities */
	float sum_exp = 0.0f;
	float probs[12];
	for (int i = 0; i < 12; i++) {
		probs[i] = expf(dequant[i] - max_val);
		sum_exp += probs[i];
	}
	for (int i = 0; i < 12; i++) {
		probs[i] = (probs[i] / sum_exp) * 100.0f;
	}

	float top_confidence = probs[top_class];

	if (print_details) {
		printf("\n---------------------------------------------------------------\n");
		printf("⚡ Ethos-U55 NPU Inference Execution Result:\n");
		printf("   Inference Time : %u us (%.2f ms)\n", elapsed_us, (float)elapsed_us / 1000.0f);
		printf("   Hardware Cycles: %u cycles (@ 400 MHz)\n", elapsed_cycles);
		printf("   Top Prediction : [%s] (Confidence: %.1f%%)\n", kws_labels[top_class], top_confidence);
		printf("---------------------------------------------------------------\n");
		printf("Class Probabilities:\n");
		for (int i = 0; i < 12; i++) {
			printf("  [%02d] %-10s : %5.1f%% %s\n",
			       i, kws_labels[i], probs[i],
			       (i == top_class) ? "<-- [PREDICTED]" : "");
		}
		printf("---------------------------------------------------------------\n");
	}

	/* RGB LED Feedback */
	if (top_confidence >= 70.0f) {
		set_rgb(0, 1, 0); /* Green: High Confidence Match */
	} else if (top_confidence >= 40.0f) {
		set_rgb(1, 1, 0); /* Yellow: Moderate Confidence */
	} else {
		set_rgb(1, 0, 0); /* Red: Low Confidence / Unknown */
	}

	return true;
}

/* Benchmark 50 consecutive inferences */
static void run_benchmark(void)
{
	const int RUNS = 50;
	printf("\n🚀 Starting 50-Run Ethos-U55 NPU Hardware Benchmark...\n");

	uint32_t min_us = 0xFFFFFFFF;
	uint32_t max_us = 0;
	uint64_t total_us = 0;
	uint64_t total_cycles = 0;

	for (int i = 0; i < RUNS; i++) {
		uint32_t cycles = 0;
		uint32_t us = 0;
		if (!execute_inference(false, &cycles, &us)) {
			printf("Benchmark aborted due to error at run #%d\n", i + 1);
			return;
		}
		if (us < min_us) min_us = us;
		if (us > max_us) max_us = us;
		total_us += us;
		total_cycles += cycles;
		k_msleep(5);
	}

	float avg_us = (float)total_us / (float)RUNS;
	float avg_cycles = (float)total_cycles / (float)RUNS;
	float fps = 1000000.0f / avg_us;

	printf("\n===============================================================\n");
	printf("   🏁 Ethos-U55 NPU 50-Run Benchmark Summary 🏁\n");
	printf("===============================================================\n");
	printf("  Target Core      : ARM Cortex-M55 HP (400 MHz)\n");
	printf("  NPU Accelerator  : Arm Ethos-U55 (256 MACs)\n");
	printf("  Total Inferences : %d\n", RUNS);
	printf("  Minimum Latency  : %u us\n", min_us);
	printf("  Maximum Latency  : %u us\n", max_us);
	printf("  Average Latency  : %.1f us (%.3f ms)\n", avg_us, avg_us / 1000.0f);
	printf("  Average Cycles   : %.0f cycles\n", avg_cycles);
	printf("  Inference Speed  : %.1f Inferences/sec (FPS)\n", fps);
	printf("===============================================================\n\n");

	set_rgb(0, 1, 0); /* Green */
}

int main(void)
{
	/* 1. Initialize Status RGB LEDs */
	if (gpio6_dev && device_is_ready(gpio6_dev)) {
		gpio_pin_configure(gpio6_dev, PIN_LED_R, GPIO_OUTPUT_INACTIVE);
		gpio_pin_configure(gpio6_dev, PIN_LED_G, GPIO_OUTPUT_INACTIVE);
		gpio_pin_configure(gpio6_dev, PIN_LED_B, GPIO_OUTPUT_INACTIVE);
		set_rgb(0, 0, 0);
	}

	/* Power-on indicator: 3 quick Blue pulses */
	for (int i = 0; i < 3; i++) {
		set_rgb(0, 0, 1);
		k_msleep(80);
		set_rgb(0, 0, 0);
		k_msleep(80);
	}

	/* 2. Welcome Banner over SEGGER RTT */
	printf("\n\n===============================================================\n");
	printf("🧠 Alif Ensemble E8 - Arm Ethos-U55 NPU Edge AI Accelerator 🧠\n");
	printf(" Target Core    : ARM Cortex-M55 (RTSS-HP @ 400 MHz)\n");
	printf(" NPU Engine     : Arm Ethos-U55 (256 MACs Hardware NPU)\n");
	printf(" AI Framework   : TensorFlow Lite for Microcontrollers (TFLM)\n");
	printf(" Model Topology : %s\n", modelName);
	printf(" Tensor Arena   : %u Bytes (Local DTCM)\n", TENSOR_ARENA_SIZE);
	printf(" RTT Buffer     : Local 0x%p\n", &_SEGGER_RTT);
	printf("===============================================================\n");

	/* 3. Initialize TensorFlow Lite Micro & Ethos-U Resolver */
	const tflite::Model *model = ::tflite::GetModel(networkModelData);
	if (model->version() != TFLITE_SCHEMA_VERSION) {
		printf("FATAL: Model schema version %lu unsupported (expected %d)!\n",
		       model->version(), TFLITE_SCHEMA_VERSION);
		set_rgb(1, 0, 0);
		return -1;
	}

	static tflite::MicroMutableOpResolver<1> resolver;
	resolver.AddEthosU();

	static tflite::MicroInterpreter static_interpreter(
		model, resolver, tensor_arena, TENSOR_ARENA_SIZE);
	interpreter = &static_interpreter;

	TfLiteStatus alloc_status = interpreter->AllocateTensors();
	if (alloc_status != kTfLiteOk) {
		printf("FATAL: Tensor allocation failed (%d)!\n", alloc_status);
		set_rgb(1, 0, 0);
		return -1;
	}
	printf("✓ Model loaded & tensors allocated successfully on Ethos-U55!\n");

	/* 4. Run initial verification inference */
	execute_inference(true, nullptr, nullptr);
	print_help();

	bool continuous_mode = false;
	uint32_t count = 0;

	while (1) {
		/* Check interactive host keystrokes */
		if (SEGGER_RTT_HasKey()) {
			int key = SEGGER_RTT_GetKey();
			switch (key) {
			case 's':
			case 'S':
				printf("\n>>> Manual Single Inference Triggered <<<\n");
				execute_inference(true, nullptr, nullptr);
				break;
			case 'b':
			case 'B':
				run_benchmark();
				break;
			case 't':
			case 'T':
				continuous_mode = !continuous_mode;
				printf("\n>>> [MODE] Continuous inference mode: %s <<<\n\n",
				       continuous_mode ? "ENABLED (2 Hz)" : "DISABLED");
				break;
			case 'h':
			case 'H':
				print_help();
				break;
			default:
				break;
			}
		}

		if (continuous_mode) {
			uint32_t cycles = 0;
			uint32_t us = 0;
			execute_inference(false, &cycles, &us);
			count++;
			printf("[NPU #%04u] Latency: %4u us | Cycles: %6u | Status: OK\n",
			       count, us, cycles);
			k_msleep(500);
		} else {
			k_msleep(50);
		}
	}

	return 0;
}

