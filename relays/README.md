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
- [x] BLE central do Relay A (`relay_a.c`): scan/conexão/descoberta/notify portados do
      `vehicle/main.c`, sem gerar nonce nem conferir HMAC (é um cano burro)
- [x] BLE peripheral do Relay B (`relay_b.c`): servidor GATT NimBLE espelhando o
      `KeyPeripheral.kt` (serviço clonado, CHALLENGE write, RESPONSE notify)
- [ ] Teste da cadeia completa no hardware (celular → A → B → veículo) — ver Milestone 2

## Milestone 1: validar o túnel (sem BLE)

Com `RELAY_LINK_SELFTEST 1` (padrão em `relay_config.h`), o Relay B dispara um desafio
por segundo e mede o tempo de ida e volta pelo ESP-NOW.

Se existir um `sdkconfig` antigo gerado sem BT, apague-o para pegar os `sdkconfig.defaults`
novos. A partir da pasta `relays/` (o papel e o modo podem ir na linha de comando com `-D`,
sem editar os headers):

```
del sdkconfig

REM --- Relay A (ESP32-S3) ---
idf.py set-target esp32s3
idf.py -DRELAY_ROLE=1 build flash monitor

REM --- Relay B (ESP32-C3) ---
idf.py set-target esp32c3
idf.py fullclean
idf.py -DRELAY_ROLE=2 build flash monitor
```

Com A e B ligados, o monitor do B deve mostrar:

```
I RELAY_B: [selfteste] originando desafios pelo tunel a cada 1 s
I RELAY_B: [selfteste] resposta recebida (16 B); RTT tunel=NNNN us
```

Esse `RTT tunel` é o overhead do enlace ESP-NOW — um dado que já entra no artigo.

## Milestone 2: BLE nas duas pontas

O `TODO(BLE)` de cada relay está implementado. Com `RELAY_LINK_SELFTEST 0` o par completo
(celular → A → B → veículo) roda o desafio-resposta real, e comparamos o RTT com o baseline
honesto do `vehicle`.

- **Relay A** (`relay_a.c`): central. Varre pelo `SVC_UUID`, conecta na chave, descobre
  CHALLENGE/RESPONSE, habilita notify. Desafio do túnel → escreve em CHALLENGE (com resposta,
  igual ao `vehicle`); notify da chave → devolve pelo túnel. Não gera nonce nem confere HMAC.
- **Relay B** (`relay_b.c`): peripheral. Servidor GATT NimBLE anunciando o `SVC_UUID` clonado;
  write do veículo em CHALLENGE → manda pelo túnel; resposta do túnel → notifica RESPONSE.

Em ambos, o frame que chega do túnel (callback do ESP-NOW, que roda na task do WiFi) é
enfileirado e a chamada GATT acontece numa task worker — o custo dessa passagem entra no
overhead medido.

### Como rodar (um binário por papel)

```
del sdkconfig

REM --- Relay A (ESP32-S3) ---
idf.py set-target esp32s3
idf.py -DRELAY_ROLE=1 -DRELAY_LINK_SELFTEST=0 build flash monitor

REM --- Relay B (ESP32-C3) ---
idf.py set-target esp32c3
idf.py fullclean
idf.py -DRELAY_ROLE=2 -DRELAY_LINK_SELFTEST=0 build flash monitor
```

Com a cadeia de pé, o monitor do **veículo** deve voltar a logar rodadas `OK` (o HMAC
relayado confere) e um `=== BASELINE ... ===` cujo RTT agora inclui o salto do relay — é
esse número que se compara com o baseline honesto. Nos relays o monitor mostra:

```
I RELAY_B: desafio do veiculo (16 B) -> tunel
I RELAY_A: HMAC da chave (16 B) -> tunel
I RELAY_B: resposta (16 B) notificada ao veiculo
```

No arranque há uma corrida boba: se o veículo escrever um desafio antes do Relay A ter
conectado na chave, o Relay A descarta aquele desafio (log `... a chave nao esta pronta`) e
o veículo simplesmente perde aquela rodada — as 20 rodadas dão folga até a cadeia subir.

## Toolchain

O projeto foi escrito para ESP-IDF v5.4, mas também **compila no v6.1** (a máquina de
desenvolvimento atual). A assinatura do callback de envio do ESP-NOW mudou no v5.4, então o
`link.c` seleciona a forma certa por `ESP_IDF_VERSION`. No v6.1 o `main/CMakeLists.txt`
precisa do `REQUIRES bt esp_wifi esp_netif esp_event nvs_flash esp_timer`.

> Espaço em flash: em modo BLE o binário passa de 1 MB (BT+WiFi+NimBLE juntos) e estoura a
> partição de app padrão — no ESP32-C3 (Relay B) por poucos KB. Por isso o `sdkconfig.defaults`
> já usa `CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y` (~1.5 MB de app, cabe em 2 MB de flash);
> com ela sobra ~31% na partição. Se trocar os defaults, apague o `sdkconfig` para reaplicar.
