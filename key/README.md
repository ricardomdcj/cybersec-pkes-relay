# key — a chave digital (peripheral BLE em CYD)

Porte do antigo app Android `phoneaskey/` para uma **CYD (ESP32-2432S028R)**. Peripheral BLE +
servidor GATT (NimBLE) que anuncia o serviço da chave, recebe o nonce em CHALLENGE, calcula
`HMAC-SHA256(PSK, nonce)` truncado em `RESPONSE_LEN` e notifica em RESPONSE.

O contrato (UUIDs, tamanhos, PSK) e o HMAC vêm do componente compartilhado
`components/pkes_contract` — a mesma fonte usada pelo `vehicle/`.

## Display

Mostra o estado (anunciando/conectado), quantos desafios respondeu, o tempo de cálculo+notify
e o último nonce/resposta em hex, com um **mascote de chave** (âmbar) no canto. Camada
`components/cyd` (ST7789 + LVGL + LED RGB). View-only, sem toque.

## Build / flash / monitor

```
idf.py set-target esp32     # CYD = ESP32 classico
idf.py build flash monitor
```

> Gravação na CYD: o auto-reset do esptool não entra em download mode (erro `0x13`).
> Segure **BOOT**, toque **RST**, solte BOOT e grave com `--before no-reset` (ver CLAUDE.md).

## Saída esperada no monitor

```
I cyd_display: ST7789 + LVGL prontos
I KEY: chave iniciada (peripheral BLE + display CYD)
I KEY: anunciando a chave; aguardando o veiculo
I KEY: desafio 1 (16 B) respondido em NNN us
```
