# components — código compartilhado da bancada

Componentes ESP-IDF reaproveitados por `key/`, `vehicle/` e `relays/`. Cada projeto os
enxerga porque seu `CMakeLists.txt` raiz faz:

```cmake
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components")
```

## `pkes_contract/`

Fonte única do protocolo GATT da bancada (antes duplicado à mão em três lugares):

- `ble_contract.h` — UUIDs do serviço/características, `NONCE_LEN`/`RESPONSE_LEN` e a PSK.
- `pkes_hmac.c/.h` — `HMAC-SHA256(PSK, nonce)` truncado em `RESPONSE_LEN`. Usa **PSA Crypto**
  no mbedTLS 4 (IDF v6.1) e a API MD legada no mbedTLS 3 (v5.4). Chame `pkes_hmac_init()` uma
  vez no `app_main`. Só `key/` (calcula) e `vehicle/` (confere) usam; os relays não.

`REQUIRES bt mbedtls` (o header usa os tipos de UUID do NimBLE).

## `cyd/`

Camada de apresentação da CYD (ESP32-2432S028R), portada da bancada **deskrab**:

- `board.h` — pinos do display ST7789, do touch XPT2046 e do LED RGB.
- `cyd_display.c/.h` — ST7789 por SPI + LVGL 8.3 (buffers DMA, task do LVGL, lock/unlock).
- `cyd_led.c/.h` — LED RGB (off/solid/blink/breathe), espelha o estado.
- `cyd_ui.c/.h` — tela de status comum: cabeçalho colorido, banner de estado, grade de cards
  rótulo/valor, linhas hex e um **mascote animado** por papel (bob, piscar, "pulo" por evento).
- `idf_component.yml` — puxa `lvgl/lvgl ~8.3` (component manager).

A bancada é **view-only** (sem controle por toque): o touch do deskrab não foi portado.
`REQUIRES lvgl__lvgl esp_lcd ...`.
