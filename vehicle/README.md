# vehicle — verificador BLE (baseline honesto)

Central BLE em ESP-IDF (NimBLE) que reproduz o par legítimo com a chave (app `phoneaskey`)
e mede o **RTT do desafio-resposta sem relay**. Esse número é a linha de base contra a qual
o custo do relay será comparado nos marcos seguintes.

## Protocolo (espelha `BleContract.kt`)

1. Varre pelo serviço `8f2a0001-…` e conecta na chave.
2. Descobre as características, habilita *notify* na `8f2a0003` (response).
3. Escreve um nonce de 16 B na `8f2a0002` (challenge).
4. Recebe a notify e confere `HMAC-SHA256(PSK, nonce)` truncado em 16 B.
5. Mede o RTT (escrita → notify) e repete `ROUNDS` vezes.

UUIDs, PSK e formato ficam em `main/ble_contract.h` — **precisa ser idêntico ao app**.

## Build / flash / monitor

Ambiente ESP-IDF v5.4 já instalado em `C:\Users\Ricardo\esp\v5.4`.

```
idf.py set-target esp32s3   # placa que você tem para testar agora
idf.py build flash monitor
```

> O endpoint "veículo" definitivo será um ESP32 clássico; nesse dia é só
> `idf.py set-target esp32` e recompilar. A lógica NimBLE é a mesma.

## Saída esperada no monitor

```
I VEICULO: escaneando pelo servico da chave...
I VEICULO: chave encontrada, conectando...
I VEICULO: conectado (handle 1); descobrindo servico
I VEICULO: inscrito em notify; iniciando 20 rodadas
I VEICULO: rodada 1/20: OK  RTT=NNNN us
...
I VEICULO: === BASELINE n=20 ok=20 falha=0 | RTT us: min=.. avg=.. max=.. ===
```

`OK` confirma que a chave provou posse do segredo e vivacidade (o nonce muda a cada rodada).
Ajuste `ROUNDS` e `INTERVAL_MS` no topo de `main/main.c`.
