#pragma once
// CYD ESP32-2432S028 pins (micro-USB + USB-C), ST7789 display, XPT2046 touch.
// Copied from the deskrab bench. The PKES bench uses only the display and the LED
// (view-only), so the touch is declared here but not initialized.

// Display (HSPI). MISO is unused: GPIO12 is a strapping pin
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
// Panel orientation/colors: adjust if the image comes out mirrored or with swapped colors
#define LCD_SWAP_XY     1
#define LCD_MIRROR_X    1
#define LCD_MIRROR_Y    0
#define LCD_INVERT      0
#define LCD_BGR         0

// RGB LED (active low)
#define PIN_LED_R       4
#define PIN_LED_G       16
#define PIN_LED_B       17
