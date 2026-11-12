# relays — Relay A e Relay B (esboço)

Dois relays BLE ligados por ESP-NOW, um único projeto ESP-IDF selecionando o papel por `#define`.

```
Celular(chave) ─BLE─► Relay A (central) ─ESP-NOW─► Relay B (periph) ─BLE─► Veículo
       HMAC ◄──────────────────── nonce ◄──────────────── nonce ◄──────────── nonce
```

- **Relay A** (`relay_a.c`): central, perto do celular. Recebe o nonce por ESP-NOW, escreve na chave, devolve o HMAC.
- **Relay B** (`relay_b.c`): peripheral, perto do veículo. Clona o serviço da chave; repassa o desafio do veículo e notifica a resposta.
- **Enlace** (`link.c`): ESP-NOW em canal fixo. **Já funciona.**
- **Contrato** (`ble_contract.h`): espelho de `BleContract.kt` / `vehicle`.

## Estado

- [x] Estrutura do projeto e seleção de papel (`relay_config.h`)
- [x] Enlace ESP-NOW A↔B + autoteste que mede o RTT do túnel
- [ ] BLE central do Relay A (reaproveitar `vehicle/main.c`) — marcado `TODO`
- [ ] BLE peripheral do Relay B (espelhar `KeyPeripheral.kt`) — marcado `TODO`

## Milestone 1: validar o túnel (sem BLE)

Com `RELAY_LINK_SELFTEST 1` (padrão em `relay_config.h`), o Relay B dispara um desafio
por segundo e mede o tempo de ida e volta pelo ESP-NOW.

O `sdkconfig` atual foi gerado sem BT. Para pegar os `sdkconfig.defaults` novos, apague-o
e reconfigure:

```
cd C:\Users\Ricardo\Desktop\Mestrado\cybersec-pkes-relay\relays
del sdkconfig

REM --- Relay A (ESP32-S3) ---  edite relay_config.h: RELAY_ROLE = RELAY_ROLE_A
idf.py set-target esp32s3
idf.py build flash monitor

REM --- Relay B (ESP32-C3) ---  edite relay_config.h: RELAY_ROLE = RELAY_ROLE_B
idf.py set-target esp32c3
idf.py fullclean
idf.py build flash monitor
```

Com A e B ligados, o monitor do B deve mostrar:

```
I RELAY_B: [selfteste] originando desafios pelo tunel a cada 1 s
I RELAY_B: [selfteste] resposta recebida (16 B); RTT tunel=NNNN us
```

Esse `RTT tunel` é o overhead do enlace ESP-NOW — um dado que já entra no artigo.

## Milestone 2: BLE nas duas pontas

Depois do túnel validado, preenche-se o `TODO(BLE)` de cada relay e põe-se
`RELAY_LINK_SELFTEST 0`. Aí o par completo (celular → A → B → veículo) roda o
desafio-resposta real, e comparamos o RTT com o baseline honesto do `vehicle`.
