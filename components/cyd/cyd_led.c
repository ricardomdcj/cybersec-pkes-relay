#include "cyd_led.h"
#include "board.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Brilho maximo (0-255): o LED da CYD e forte e fica de frente para quem esta perto
#define LED_MAX 40

static const int s_pins[3] = { PIN_LED_R, PIN_LED_G, PIN_LED_B };
static const ledc_channel_t s_ch[3] = { LEDC_CHANNEL_1, LEDC_CHANNEL_2, LEDC_CHANNEL_3 };

static volatile cyd_led_mode_t s_mode = CYD_LED_OFF;
static volatile uint32_t s_rgb;

static void write_rgb(uint32_t rgb, int level /* 0-255 */) {
    for (int i = 0; i < 3; i++) {
        int c = (rgb >> (16 - 8 * i)) & 0xFF;
        ledc_set_duty(LEDC_LOW_SPEED_MODE, s_ch[i], c * level * LED_MAX / (255 * 255));
        ledc_update_duty(LEDC_LOW_SPEED_MODE, s_ch[i]);
    }
}

static void led_task(void *arg) {
    while (1) {
        int64_t ms = esp_timer_get_time() / 1000;
        int level = 0;
        switch (s_mode) {
        case CYD_LED_OFF:   level = 0; break;
        case CYD_LED_SOLID: level = 255; break;
        case CYD_LED_BLINK: level = (ms / 400) % 2 ? 255 : 0; break;
        case CYD_LED_BREATHE: {
            int p = ms % 2400;                      // triangulo de 2,4 s
            int t = p < 1200 ? p : 2400 - p;
            level = 20 + t * 235 / 1200;
            level = level * level / 255;            // curva aproximada de percepcao
            break;
        }
        }
        write_rgb(s_rgb, level);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void cyd_led_init(void) {
    // Usa o timer 0 ja configurado pelo backlight (5 kHz, 8 bits)
    for (int i = 0; i < 3; i++) {
        ledc_channel_config_t ch = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel    = s_ch[i],
            .timer_sel  = LEDC_TIMER_0,
            .intr_type  = LEDC_INTR_DISABLE,
            .gpio_num   = s_pins[i],
            .duty       = 0,
            .hpoint     = 0,
            .flags.output_invert = 1,   // LED ligado ao 3V3, acende com nivel baixo
        };
        ESP_ERROR_CHECK(ledc_channel_config(&ch));
    }
    xTaskCreate(led_task, "led", 2048, NULL, 2, NULL);
}

void cyd_led_set(cyd_led_mode_t mode, uint32_t rgb) {
    s_rgb = rgb;
    s_mode = mode;
}
