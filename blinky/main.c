#include "RTE_Components.h"
#include CMSIS_device_header

#include "Driver_IO.h"
#include "board_config.h"

#if defined(ENSEMBLE_SOC_E1C)
#include "pinconf.h"
#endif

#define _GET_DRIVER_REF(ref, peri, chan) \
    extern ARM_DRIVER_##peri Driver_##peri##chan; \
    static ARM_DRIVER_##peri * ref = &Driver_##peri##chan;
#define GET_DRIVER_REF(ref, peri, chan) _GET_DRIVER_REF(ref, peri, chan)

// RGB LED GPIO 控制介面指標
GET_DRIVER_REF(gpio_b, GPIO, BOARD_LEDRGB0_B_GPIO_PORT);
GET_DRIVER_REF(gpio_g, GPIO, BOARD_LEDRGB0_G_GPIO_PORT);
GET_DRIVER_REF(gpio_r, GPIO, BOARD_LEDRGB0_R_GPIO_PORT);

#define BOARD_LEDRGB_B_GPIO_PIN BOARD_LEDRGB0_B_GPIO_PIN
#define BOARD_LEDRGB_G_GPIO_PIN BOARD_LEDRGB0_G_GPIO_PIN
#define BOARD_LEDRGB_R_GPIO_PIN BOARD_LEDRGB0_R_GPIO_PIN

// 心跳計數器（可在除錯器中即時觀察數值遞增）
volatile uint32_t g_heartbeat_count = 0;

int main (void)
{
    // 1. 初始化 E8 DevKit 板載 Pinmux (引腳複用配置)
    int32_t ret = board_pins_config();
    if (ret != 0) {
        while(1); // 初始化失敗停留
    }

/* E1C 晶片相容處理 (若在 E8 則跳過) */
#if defined(ENSEMBLE_SOC_E1C)
    pinconf_set(BOARD_LEDRGB0_B_GPIO_PORT, BOARD_LEDRGB_B_GPIO_PIN, PINMUX_ALTERNATE_FUNCTION_0, PADCTRL_OUTPUT_DRIVE_STRENGTH_4MA);
#endif

    // 2. 初始化藍色 LED (GPIO 初始化 -> 供電 -> 設為輸出模式 -> 預設熄滅)
    gpio_b->Initialize(BOARD_LEDRGB_B_GPIO_PIN, NULL);
    gpio_b->PowerControl(BOARD_LEDRGB_B_GPIO_PIN, ARM_POWER_FULL);
    gpio_b->SetDirection(BOARD_LEDRGB_B_GPIO_PIN, GPIO_PIN_DIRECTION_OUTPUT);
    gpio_b->SetValue(BOARD_LEDRGB_B_GPIO_PIN, GPIO_PIN_OUTPUT_STATE_LOW);

    // 3. 初始化綠色 LED
    gpio_g->Initialize(BOARD_LEDRGB_G_GPIO_PIN, NULL);
    gpio_g->PowerControl(BOARD_LEDRGB_G_GPIO_PIN, ARM_POWER_FULL);
    gpio_g->SetDirection(BOARD_LEDRGB_G_GPIO_PIN, GPIO_PIN_DIRECTION_OUTPUT);
    gpio_g->SetValue(BOARD_LEDRGB_G_GPIO_PIN, GPIO_PIN_OUTPUT_STATE_LOW);

    // 4. 初始化紅色 LED
    gpio_r->Initialize(BOARD_LEDRGB_R_GPIO_PIN, NULL);
    gpio_r->PowerControl(BOARD_LEDRGB_R_GPIO_PIN, ARM_POWER_FULL);
    gpio_r->SetDirection(BOARD_LEDRGB_R_GPIO_PIN, GPIO_PIN_DIRECTION_OUTPUT);
    gpio_r->SetValue(BOARD_LEDRGB_R_GPIO_PIN, GPIO_PIN_OUTPUT_STATE_LOW);

    // 5. 設定 SysTick 定時中斷：設定為每秒觸發 2 次 (每 500ms 觸發一次，即 1Hz 閃爍頻率)
    SysTick_Config(SystemCoreClock / 2);

    // 6. 主迴圈：進入低功耗等待中斷 (WFI = Wait For Interrupt)
    while (1) {
        __WFI();
    }
}

/**
 * @brief SysTick 中斷服務函式 (每 500ms 執行一次)
 */
void SysTick_Handler (void)
{
    g_heartbeat_count++;

    // 翻轉紅色 LED (亮 500ms，滅 500ms)
    gpio_r->SetValue(BOARD_LEDRGB_R_GPIO_PIN, GPIO_PIN_OUTPUT_STATE_TOGGLE);

#if defined(RTSS_HE)
    // 若在 HE 核心執行，同步翻轉藍色 LED
    gpio_b->SetValue(BOARD_LEDRGB_B_GPIO_PIN, GPIO_PIN_OUTPUT_STATE_TOGGLE);
#endif
}

// Stubs to suppress missing stdio definitions for nosys
#define TRAP_RET_ZERO  {__BKPT(0); return 0;}
int _close(int val) TRAP_RET_ZERO
int _lseek(int val0, int val1, int val2) TRAP_RET_ZERO
int _read(int val0, char * val1, int val2) TRAP_RET_ZERO
int _write(int val0, char * val1, int val2) TRAP_RET_ZERO
int _fstat(int val0, void * val1) TRAP_RET_ZERO
int _isatty(int val0) TRAP_RET_ZERO
int _getpid(void) TRAP_RET_ZERO
int _kill(int val0, int val1) TRAP_RET_ZERO
