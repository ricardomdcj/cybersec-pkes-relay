#pragma once
// Pinos da CYD ESP32-2432S028 (micro-USB + USB-C), display ST7789, touch XPT2046.
// Copiado da bancada deskrab; a bancada PKES usa so o display e o LED (view-only),
// entao o touch fica declarado mas nao e inicializado.

// Display (HSPI). MISO nao e usado: GPIO12 e pino de strapping
#define LCD_HOST        SPI2_HOST
#define PIN_LCD_SCLK    14
#define PIN_LCD_MOSI    13
#define PIN_LCD_CS      15
#define PIN_LCD_DC      2
#define PIN_LCD_RST     -1
#define PIN_LCD_BL      21
#define LCD_PCLK_HZ     (40 * 1000 * 1000)

#define LCD_W           320
#define LCD_H           240
// Orientacao/cores do painel: ajustar se a imagem sair espelhada ou com cores trocadas
#define LCD_SWAP_XY     1
#define LCD_MIRROR_X    1
#define LCD_MIRROR_Y    0
#define LCD_INVERT      0
#define LCD_BGR         0

// LED RGB (ativo em nivel baixo)
#define PIN_LED_R       4
#define PIN_LED_G       16
#define PIN_LED_B       17
