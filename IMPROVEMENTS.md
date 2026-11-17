# Melhorias do bench PKES: medição e visual

Lista de tarefas derivadas de dois pontos: a análise dos logs de 2026-11-16 (`data/`) e do
código de `vehicle/`, `relays/` e `key/`; e as ideias de tela inspiradas no `deskrab`.

Cada item é uma tarefa independente. Quando uma tarefa era grande demais, ela foi quebrada
em duas (`a` e `b`). Nada disto foi compilado nem gravado nas placas ainda. A validação de
cada item é `idf.py build`, leitura do monitor serial ou o simulador de tela (V-17).

Convenções: cada item traz **Arquivos**, **Aceite** e **Depende de**. As telas continuam sem
acentos (regra do `CLAUDE.md`), a menos que V-16b mude isso.

---

## Parte 1: Medição

### A. Validade da rodada (fazer primeiro)

> **Status (2026-11-17):** implementado e compilado, sem validação em hardware ainda.
> Compilacao: a primeira rodada saiu com o alvo errado (sdkconfig local desatualizado: vehicle esp32s3, relays esp32c3). Recompilado com `esp32` em 2026-11-17: todos passam (vehicle, relays BLE A e B, selftest A e B).
> (um aviso antigo: `s_from_vehicle` não usado no selftest, já existia antes).
> **Os dois relays precisam ser regravados juntos:** o formato do túnel mudou
> (ver `relays/main/link.h`), e um relay antigo não conversa com um novo.
>
> Decisões tomadas na implementação:
> - **M-02:** em `TIMEOUT` o `rtt_us` vai como `-1`. No display, o card de falha virou
>   `Falha / tmo` com o valor `falhas / timeouts`.
> - **M-03:** o `seq` é um contador atribuído pelo Relay B a cada desafio (não os 4 B do nonce).
>   Frame B->A: `[seq:4][nonce:16]`. Frame A->B: `[seq:4][turnaround A:4][hmac:16]`.
> - **M-04:** resposta descartada aparece no log com o contador `atrasadas`. Ainda não tem
>   campo no display (vai com V-04/V-06).
> - **M-05:** veio no mesmo frame da M-03. A linha `CSVB` ganhou uma última coluna com o
>   turnaround de A.
> - **A verificar em hardware:** no timeout, o veículo chama `ble_gap_conn_rssi` e
>   `ble_gattc_write_flat` a partir da task do `esp_timer`. É o mesmo padrão que o timer de
>   rodada já usava, mas precisa ser confirmado com o NimBLE rodando.

- [x] **M-01. Timeout por rodada no veículo.** Hoje o próximo desafio só sai dentro do handler
  de notify. Se a resposta se perde, a campanha trava para sempre. Adicionar um timer de espera
  (constante `ROUND_TIMEOUT_MS`, sugestão 2 s) que marca a rodada como perdida e segue.
  **Arquivos:** `vehicle/main/main.c`. **Aceite:** com o relay desligado no meio da campanha, o
  veículo registra rodadas perdidas e continua, sem reiniciar. **Depende de:** nada.

- [x] **M-02. Resultado TIMEOUT separado de FALHA.** `FALHA` hoje é só HMAC errado. Acrescentar
  `TIMEOUT` (sem resposta no prazo) como resultado próprio, com contador próprio no CSV e na
  linha `===`. **Arquivos:** `vehicle/main/main.c`. **Aceite:** uma rodada perdida aparece como
  `TIMEOUT` no CSV e é contada à parte no resumo. **Depende de:** M-01.

- [x] **M-03. ID da rodada no frame de resposta do túnel.** `LINK_MSG_RESPONSE` leva só o HMAC
  de 16 B. Sem ID, a resposta é casada com o desafio pela ordem de chegada. Acrescentar 4 B do
  nonce (ou um contador) ao frame. **Arquivos:** `relays/main/link.h`, `relays/main/relay_a.c`,
  `relays/main/relay_b.c`. **Aceite:** cada frame de resposta traz o ID do desafio que o gerou.
  **Depende de:** nada.

- [x] **M-04. Relay B descarta resposta tardia.** Com M-03, B compara o ID recebido com o desafio
  pendente. Se não bater, descarta e conta em "atrasadas". Sem isso, uma resposta que chega depois
  do timeout cai na rodada seguinte e vira um `FALHA` falso. **Arquivos:** `relays/main/relay_b.c`.
  **Aceite:** com atraso forçado no túnel, o contador "atrasadas" sobe e o veículo não registra
  `FALHA`. **Depende de:** M-03.

- [x] **M-05. Relay A devolve o próprio turnaround.** Junto da resposta (M-03), A envia o
  `turnaround` que já mede (linha `CSVA`). Assim B consegue decompor o tempo sem depender de log.
  **Arquivos:** `relays/main/relay_a.c`, `relays/main/link.h`. **Aceite:** B recebe e registra o
  turnaround de A na mesma rodada. **Depende de:** M-03.

### B. Túnel ESP-NOW

> **Status (2026-11-17):** implementado e compilado para `esp32`; falta validar no hardware.
> M-06a: o MAC de cada relay sai no log de boot (`MAC desta placa`). M-06b: enquanto
> `RELAY_PEER_MAC` (em `relay_config.h`) estiver zerado, o enlace segue em broadcast, que nao
> tem ACK e nao mostra perdas. M-07: a ultima coluna de `CSVA` e `CSVB` e o contador
> cumulativo de falhas de envio. M-08: o log mostra `power save WiFi = NONE`.

- [x] **M-06a. Ler e mostrar o MAC de cada placa.** Imprimir o MAC no boot (`esp_wifi_get_mac`) e
  mostrar no display. **Arquivos:** `relays/main/link.c`, `relays/main/main.c`. **Aceite:** o MAC
  de A e de B aparece no log de boot. **Depende de:** nada.

- [x] **M-06b. Peers unicast.** Trocar o `BCAST` por MAC de peer fixo, configurado em
  `relay_config.h`. Assim o envio passa a ter ACK da camada MAC. **Arquivos:**
  `relays/main/link.c`, `relays/main/relay_config.h`. **Aceite:** frames vão só ao peer, sem
  tráfego de terceiros no canal. **Depende de:** M-06a.

- [x] **M-07. Contar falhas de envio do túnel.** Com broadcast, o callback de envio não reporta
  perda de forma útil. Com unicast (M-06b), o status vira informação real. Contar as falhas e
  expô-las no CSV e no resumo. **Arquivos:** `relays/main/link.c`, `relays/main/relay_a.c`,
  `relays/main/relay_b.c`. **Aceite:** uma falha de envio forçada (peer desligado) sobe o
  contador. **Depende de:** M-06b.

- [x] **M-08. Desligar o power save do Wi-Fi.** Configurar `esp_wifi_set_ps(WIFI_PS_NONE)` em
  `link_init` e registrar o modo ativo no log. O padrão pode adicionar latência. Medir o RTT do
  túnel antes e depois. **Arquivos:** `relays/main/link.c`. **Aceite:** o log mostra PS=NONE.
  **Depende de:** nada.

### C. Conexão BLE

> **Status (2026-11-17):** implementado e compilado para `esp32`; falta validar no hardware.
> Os parametros de `components/pkes_contract/ble_link_params.h` sao os **padroes do NimBLE**
> (30-50 ms, latencia 0, supervisao 2,56 s). Esta etapa nao muda o comportamento: so o torna
> explicito e registrado. Mudar esses valores afeta baseline e relays ao mesmo tempo. Linhas
> `CONN` e `MTU` no log; o intervalo entra no CSV v2. M-12 conta desconexoes (cada uma gera
> uma reconexao) e aparece no resumo `===`.

- [x] **M-09a. Header com parâmetros de conexão.** Criar
  `components/pkes_contract/ble_link_params.h` com intervalo mínimo e máximo, latência e timeout
  de supervisão. **Arquivos:** novo header. **Depende de:** nada.

- [x] **M-09b. Aplicar os parâmetros no connect.** Passar os parâmetros em `ble_gap_connect`
  (hoje é `NULL`, então vale o padrão do NimBLE, que não sabemos). **Arquivos:**
  `vehicle/main/main.c`, `relays/main/relay_a.c`. **Depende de:** M-09a.

- [x] **M-10. Logar os parâmetros negociados.** Na conexão e em cada `BLE_GAP_EVENT_CONN_UPDATE`,
  imprimir intervalo, latência e timeout efetivos numa linha `CONN,...`. **Arquivos:**
  `vehicle/main/main.c`, `relays/main/relay_a.c`. **Aceite:** a linha `CONN` aparece em toda
  conexão. **Depende de:** M-09b.

- [x] **M-11. Logar MTU e PHY.** Negociar o MTU e registrar MTU e PHY na linha `CONN`. **Arquivos:**
  `vehicle/main/main.c`, `relays/main/relay_a.c`, `relays/main/relay_b.c`. **Depende de:** M-10.

- [x] **M-12. Motivo de desconexão e contador de reconexões.** O log já mostra `reason=520` no
  início (provavelmente timeout de conexão, 0x08 do HCI). Registrar o motivo e contar as
  reconexões no CSV ou no resumo. **Arquivos:** `vehicle/main/main.c`, `relays/main/relay_a.c`,
  `relays/main/relay_b.c`, `key/main/main.c`. **Depende de:** nada.

### D. Cabeçalho da execução e rotulagem

> **Status (2026-11-17):** implementado e compilado para `esp32`; falta validar no hardware.
> Mudancas de comportamento que afetam a bancada:
> - `RUN label=<rotulo> dist=<metros>` e `START` sao comandos na UART0 (mesma porta do OTA).
> - A campanha **nao comeca mais sozinha**: envie `START` depois de `inscrito em notify`. Uma
>   queda interrompe a campanha; e preciso `START` de novo apos reconectar.
> - Sem `RUN`, o rotulo fica `run` e a distancia `-`, com aviso no log.
> - A grade de rodadas e fixa: uma a cada 1 s, contada a partir do envio anterior.
> - CSV v2 (`CSVHDR v2`): `label,dist_m,rodada,nonce,t_tx_us,rtt_us,resultado,rssi_dbm,itvl_1_25ms,mtu`.
> - `RUN_META` sai no boot com o commit do momento do configure. Depois de commitar, rode
>   `idf.py reconfigure` antes do build, senao a linha mostra o commit anterior.
> - Ferramentas: `tools/capture_run.py` (captura e manifesto) e `tools/analyze_runs.py` (junta
>   por nonce, separa as pernas e calcula estatistica). O analisador foi testado com as
>   capturas antigas e com um log sintetico v2. A captura real ainda nao foi testada.

- [x] **M-13a. Metadados de build.** Gerar no CMake o commit git e a data de build
  (`-DRUN_GIT_COMMIT=...`). Imprimir a linha `RUN_META` no boot, junto do SHA256 do ELF.
  **Arquivos:** `vehicle/CMakeLists.txt`, `vehicle/main/main.c`. **Depende de:** nada.

- [x] **M-13b. Replicar `RUN_META` nos relays e na chave.** Mesma linha em `relays/` e `key/`.
  **Arquivos:** `relays/main/main.c`, `key/main/main.c`. **Depende de:** M-13a.

- [x] **M-14a. Comando serial de execução.** Interpretar uma linha
  `RUN label=relay dist=3.0` vinda do UART0. O `serial_ota` já lê linhas `OTA:*` nesse canal.
  **Arquivos:** `components/serial_ota/serial_ota.c` ou um parser novo. **Depende de:** nada.

- [x] **M-14b. Usar rótulo e distância da execução.** Trocar o `RUN_LABEL` de build pelo valor do
  comando, no CSV, no resumo e no display. Sem comando, usar o padrão e avisar no log.
  **Arquivos:** `vehicle/main/main.c`. **Depende de:** M-14a.

- [x] **M-15. Início sob comando.** A campanha só começa com `START` (ou logo após `RUN`). Assim
  não sobram rodadas de aquecimento sem marcação. **Arquivos:** `vehicle/main/main.c`.
  **Depende de:** M-14a.

- [x] **M-16. Grade fixa de rodadas.** Agendar o desafio a cada `INTERVAL_MS` a partir do início,
  em vez de 1 s depois de cada notify. Assim o período não deriva com o RTT. **Arquivos:**
  `vehicle/main/main.c`. **Depende de:** M-01.

- [x] **M-17. Timestamp absoluto no CSV.** Incluir `t_tx` (µs desde o boot) em cada linha, para
  cruzar com os logs e montar uma linha do tempo. **Arquivos:** `vehicle/main/main.c`.
  **Depende de:** nada.

- [x] **M-18. Schema CSV v2.** Fixar as colunas (rótulo, distância, rodada, nonce, `t_tx_us`,
  `rtt_us`, resultado, RSSI, e o que mais for necessário) no `CSVHDR` e documentar. **Arquivos:**
  `vehicle/main/main.c`. **Depende de:** M-02, M-14b, M-17.

### E. Captura e análise (ferramentas no PC)

- [ ] **D-01. Padronizar o encoding dos logs.** Os arquivos de `data/` estão em UTF-16
  (baseline) e UTF-8 com BOM (relayed), e isso quebra o `grep`. Converter para UTF-8 sem BOM e
  registrar isso num README da pasta. **Arquivos:** `data/`. **Depende de:** nada.

- [x] **D-02. Script de captura `tools/capture_run.py`.** Captura não interativa com pyserial (o
  mesmo procedimento do `TESTING.md` §6), gravando UTF-8 em
  `data/runs/<data>_<rotulo>/capture_<porta>.log`. Rodar com o Python do venv do ESP-IDF e `-I`.
  **Depende de:** nada.

- [x] **D-03. Manifesto da execução.** O mesmo script gera `manifest.json` com rótulo, distância,
  data, commit e SHA, lidos das linhas `RUN_META` e `RUN`. **Arquivos:** `tools/capture_run.py`.
  **Depende de:** D-02, M-13a, M-14b.

- [x] **D-04a. Junção por nonce.** `tools/analyze_runs.py` junta as linhas do veículo, do Relay A
  e do Relay B pelo nonce e gera uma tabela por perna (veículo↔B, túnel, A↔chave). **Depende
  de:** D-01, M-05.

- [x] **D-04b. Estatísticas.** Mediana, IQR, p90, IC bootstrap do overhead e taxa de sucesso por
  distância com IC de Wilson. Saída em CSV e Markdown. **Depende de:** D-04a.

- [ ] **D-05a. Gráfico de série temporal.** RTT por rodada, com cor por resultado (OK, FALHA,
  TIMEOUT) e marcação das trocas de condição. Matplotlib. **Depende de:** D-04a.

- [ ] **D-05b. Histograma e boxplot baseline × relay.** **Depende de:** D-04b.

- [ ] **D-06. Atualizar o `TESTING.md`.** O documento ainda diz 20 rodadas, celular como chave,
  S3 e C3 como placas. Atualizar para ROUNDS atual, placas CYD, chave CYD, comandos novos e
  schema CSV v2. **Depende de:** M-18, D-03.

---

## Parte 2: Visual

### Inspiração no deskrab (anotada)

| Elemento no deskrab | Onde está | Como usar no PKES |
|---|---|---|
| Faixas de y fixas: topo 0–27, cena 28–107, texto 112–168, dados 172–230 | `deskrab/.claude/skills/cyd-animation/references/layout.md` | Mesma divisão, documentada (V-01). Hoje as posições estão espalhadas em `cyd_ui.c`. |
| Barra de topo com status à direita | `deskrab/main/ui.c`, `layout.md` | Modo e indicadores de enlace (V-06). |
| Mascote que reage a eventos (`event = true`) | `deskrab/main/skits.c` (`s_defs`) | Reação a OK, FALHA e TIMEOUT (V-09a, V-09b). |
| Balão `say()` e nuvem de pensamento | `skits.c`, `layout.md` | Balão com o RTT da rodada (V-09b). |
| Cena de fundo que conta a história | `deskrab/main/scene.c`, README "Adding a scenario" | Carro e chave na distância real (V-14a, V-14b). |
| Efeito contínuo ligado ao estado | `deskrab/main/fx.c` | Pacote andando no enlace, com tempo real (V-12b). |
| LED que respira na cor do estado | `deskrab/main/led.c` | Pulso por resultado (V-10). |
| Simulador que desenha o painel em BMP | `deskrab/tools/sim/`, `CLAUDE.md` "Screen simulator" | Pré-visualizar e gravar telas sem placa (V-17). |
| Fontes com acentos geradas a partir do Montserrat | `deskrab/tools/gen_latin_fonts.py` | Opcional (V-16). |
| Pipeline de vídeo | `deskrab/tools/anuncio/` (`telas.py`, `compose.py`, `video.py`) | Montagem do vídeo (V-18). |

### Tarefas

> **Status (2026-11-17):** implementado e compilado para `esp32` (vehicle, key, relays BLE A e B,
> selftest A e B); nao visto na tela, falta conferir na placa. Sem simulador (V-17) ainda.
> - V-02: `cyd_fmt_us()` mostra `91.2 ms` e `1.20 s`. Usado no RTT do veiculo, no turnaround do
>   Relay B e no `Calc` da chave (rotulo `Calc us` virou `Calc`).
> - V-03: banner de estado com 240 px (era 308), para nao cobrir o mascote em x 256-314.
> - V-04: o card do Relay B mostra o turnaround no modo BLE (rotulo `Turnaround`); no selftest
>   continua o RTT do tunel (rotulo `RTT tunel`).
> - V-05: barra de progresso fina abaixo do banner, e o card OK mostra `12 (100%)`. O card
>   `Falha / tmo` continua com `falhas / timeouts`.
> - V-10: o LED pisca por resultado (verde OK, vermelho FALHA, amarelo TIMEOUT) por 1,2 s e
>   volta a respirar. Limite: na ultima rodada, a troca para "fim" apaga o pisca antes de aparecer.

- [ ] **V-01. Mapa de layout do PKES.** Escrever `components/cyd/LAYOUT.md` com as faixas de y e a
  posição de cada widget, no formato do `layout.md` do deskrab. **Aceite:** todos os widgets de
  `cyd_ui.c` estão listados com coordenadas. **Depende de:** nada.

- [x] **V-02. Valores com unidade.** Função `cyd_fmt_us()` que mostra `91.2 ms` e, acima de 1 s,
  `1.20 s`. Hoje o veículo mostra `97545` (µs crus) e a chave mostra `Calc us`. Usar em veículo,
  relays e chave. **Arquivos:** `components/cyd/cyd_ui.c`, `cyd_ui.h`. **Depende de:** nada.

- [x] **V-03. Mascote sobre o banner.** O mascote (58×58, em x 256–314 e y 2–60) cobre o fim do
  banner de estado (y 44–74). Reduzir o mascote ou encurtar o banner. Conferir com V-17b ou
  foto da placa. **Arquivos:** `components/cyd/cyd_ui.c`. **Depende de:** V-01.

- [x] **V-04. Relay B mostra o RTT no modo BLE.** O campo "RTT us" só é atualizado no autoteste
  (`relays/main/relay_b.c`, perto da linha 333). No modo BLE fica "-" para sempre. Mostrar o
  turnaround de B. **Arquivos:** `relays/main/relay_b.c`. **Depende de:** nada.

- [x] **V-05. Progresso e taxa de sucesso.** Barra de progresso (`12/50`) e contadores com
  porcentagem (`OK 12 (100%)`, `FALHA 0`, `TIMEOUT 0`). **Depende de:** M-02, V-02.

- [ ] **V-06. Barra de topo com estado.** Modo (`BASELINE` ou `RELAY`), indicador BLE e indicador
  ESP-NOW, à direita, no estilo da barra de topo do deskrab. **Depende de:** V-01, M-14b.

- [ ] **V-07a. Sparkline das últimas 30 rodadas.** Gráfico de linha (`lv_chart`) com as últimas 30
  rodadas. Medir o uso do pool do LVGL depois (o deskrab usa 72 KB e fica perto de 40%). **Depende
  de:** V-02.

- [ ] **V-07b. Faixa de referência e marcas de perda.** Sombrear a faixa min–máx do baseline no
  gráfico e marcar TIMEOUT com um traço vermelho. **Depende de:** V-07a, M-02.

- [ ] **V-08. Estatística ao vivo.** Linha `min 83  med 91  max 98 ms`, atualizada a cada rodada.
  **Depende de:** V-02.

- [ ] **V-09a. Expressões do mascote.** Olhos em arco para OK, olhos em X e boca reta para FALHA,
  um "?" para TIMEOUT. Usar as poses de `build_mascot` em `cyd_ui.c`. **Depende de:** V-01.

- [ ] **V-09b. Balão com o resultado.** Mostrar `91 ms OK` ao lado do mascote por cerca de 1 s,
  como o `say()` do deskrab. **Depende de:** V-09a.

- [x] **V-10. LED por rodada.** Verde curto para OK, vermelho para FALHA, amarelo para TIMEOUT, e
  depois volta ao respiro da cor do estado (`cyd_led_set`). **Depende de:** M-02.

- [ ] **V-11a. Histograma em RAM.** Guardar os RTTs da campanha e desenhar um histograma de 10
  barras. **Depende de:** V-02.

- [ ] **V-11b. Tela de resumo final.** Ao fim da campanha, alternar entre o resumo (OK%, mediana,
  IQR) e o histograma a cada 5 s. **Depende de:** V-11a, V-08.

- [ ] **V-12a. Desenho do enlace no relay.** Dois nós (A e B) com uma linha tracejada entre eles,
  na faixa de cena (y 28–107). **Depende de:** V-01.

- [ ] **V-12b. Pacote animado com tempo real.** Um ponto percorre a linha durante o tempo medido
  do trecho (por exemplo, 22 ms no túnel). A velocidade é a real, não uma animação fixa.
  **Depende de:** V-12a, M-05.

- [ ] **V-13. Barra empilhada da última rodada.** O Relay B mostra uma barra dividida em túnel e
  A↔chave, com o total. **Depende de:** M-05, V-02.

- [ ] **V-14a. Cena do carro e da chave.** Desenho estático de carro e chave na faixa de cena, no
  estilo das cenas do deskrab. **Depende de:** V-01.

- [ ] **V-14b. Distância na cena.** Posicionar a chave proporcional à distância informada no
  comando `RUN`, com escala e limite. **Depende de:** V-14a, M-14b.

- [ ] **V-15. Splash de identidade.** Na inicialização, título, mascote e o commit curto de
  `RUN_META`, por 2 s. **Depende de:** M-13a.

- [ ] **V-16a. Fontes com acentos (opcional).** Gerar fontes Latin-1 com o
  `gen_latin_fonts.py` do deskrab. **Depende de:** nada.

- [ ] **V-16b. Textos com acentos (opcional).** Trocar os textos de tela para português correto e
  atualizar a regra do `CLAUDE.md`. **Depende de:** V-16a.

- [ ] **V-17a. Harness do simulador no PKES.** Portar `deskrab/tools/sim/` para o PKES: stubs de
  HAL e script de build, com renderização do `cyd_ui`. **Depende de:** V-01.

- [ ] **V-17b. Roteiro e gravação de frames.** Script de cenário (estado, rodadas, distâncias) que
  grava um BMP de 320×240 por tela. Serve para conferir o layout (V-03) sem placa. **Depende de:**
  V-17a.

- [ ] **V-18. Roteiro de vídeo e montagem.** Capturar as cenas do roteiro (abaixo) com V-17b e
  montar com `deskrab/tools/anuncio/`. **Depende de:** V-17b, V-09b, V-11b, V-12b, V-14b.

---

## Mockups anotados

Telas em 320×240, simplificadas. O texto dentro delas é o que apareceria na tela (ASCII).

### Veículo (proposta)

```
+------------------------------------------------+
| VEICULO                [BASELINE]  BLE  ESPNOW |  [1]
+------------------------------------------------+
| medindo                             12 / 50    |  [2]
| [##############--------------------]           |
|                                                |
|  RTT  91.2 ms                       (carinha)  |  [3] [9]
|  .--.__.--'`--.__.--.__.--'`--.__.--.          |  [4]
|  faixa do baseline 83-98 ms                    |  [5]
|                                                |
|  min 83   med 91   max 98 ms                   |  [6]
|  OK 12 (100%)   FALHA 0   TIMEOUT 0            |  [7]
|  nonce 7c1dbaa5   resp 7c1dbaa5...             |  [8]
+------------------------------------------------+
```

- **[1]** Barra de topo com modo e enlaces (V-06).
- **[2]** Estado e progresso (V-05).
- **[3]** Valor grande com unidade (V-02).
- **[4]** Sparkline das últimas 30 rodadas (V-07a).
- **[5]** Faixa do baseline e marcas de perda (V-07b).
- **[6]** Estatística ao vivo (V-08).
- **[7]** Contadores com porcentagem (V-05).
- **[8]** Linhas hex, como hoje.
- **[9]** Mascote com expressão do último resultado (V-09a, V-09b).

### Relay B (proposta)

```
+------------------------------------------------+
| RELAY B                 [RELAY]  BLE  ESPNOW   |  [1]
+------------------------------------------------+
|  (veic)  o - - - - - - - - - o  (B)            |  [2]
|                pacote 22 ms                    |
|  [== tunel 22 ms ==][======= A-chave 192 ms ===]|  [3]
|  turnaround B 214 ms   atrasadas 0   falhas 0  |  [4]
|  nonce c728bac7                                |
+------------------------------------------------+
```

- **[1]** Barra de topo (V-06).
- **[2]** Enlace desenhado com pacote animado no tempo real (V-12a, V-12b).
- **[3]** Barra empilhada da última rodada (V-13). Os valores são os medidos: túnel 22 ms e
  A↔chave 192 ms na rodada 1 do run de 2026-11-16.
- **[4]** Turnaround de B (V-04) e contadores de M-04 e M-07.

### Chave (proposta)

Mesma estrutura do veículo, com o mascote de chave. O campo `Calc us` passa a `Calc ms` (V-02).
Expressões do mascote seguem V-09a.

---

## Roteiro de cenas para o vídeo

Cada cena mostra números medidos. As animações usam tempos reais, para que a tela sirva de
prova, não de decoração.

1. **Abertura.** Splash com identidade e commit (V-15). Carro e chave na mesma tela, à
   distância zero (V-14a).
2. **Linha de base.** Veículo com a chave direta: RTT em torno de 91 ms, gráfico estável, LED
   verde (V-02, V-07a, V-10).
3. **Com relé.** Relays A e B: pacote andando no enlace e barra empilhada (V-12, V-13). O
   veículo continua marcando `OK`, com RTT maior.
4. **Distância.** A cena mostra a chave longe, e o veículo ainda marca `OK` (V-14b). Esse é o
   argumento do artigo: o relé encaminha a identidade, não a distância.
5. **Resultado.** Histograma baseline × relay na tela de resumo (V-11b).

---

## Ordem sugerida

1. Validade da rodada: M-01 a M-05. São a base de qualquer número novo.
2. Ferramentas de captura e análise: D-01 a D-04b. Permitem tratar os dados sem depender de placa.
3. Túnel e BLE: M-06a a M-12. Mexem em firmware, mas não mudam a campanha em andamento.
4. Visual de baixo custo: V-01 a V-05, V-08, V-10. Aproveitam os dados que já existem.
5. Visual com dados novos: V-06, V-07, V-09, V-11 a V-14. Dependem dos itens anteriores.
6. Simulador e vídeo: V-17, V-18, com V-16 como opcional.

---

## Pausa: o que ficou de fora de proposito

- **D-01 (encoding dos logs antigos):** nao converti as capturas de `data/`. O analisador le
  UTF-8 e UTF-16, entao nao bloqueia nada. Reescrever dado capturado so depois de voce confirmar.
- **D-05 (graficos):** precisa de `matplotlib`, que nao esta instalado no Python do ESP-IDF nem
  no do sistema. Instalar muda o ambiente, entao espero sua decisao: instalar, ou gerar SVG puro.
- **D-06 (atualizar `TESTING.md`):** o procedimento novo (`RUN`, `START`, CSV v2, MAC do peer)
  deve ser escrito depois da primeira campanha no hardware, porque a validacao pode mudar algo.
- **Bloco V (visual):** envolve escolhas de estilo que sao suas, e o simulador (V-17) e um
  port grande do `tools/sim` do deskrab. Vale alinhar a direcao antes.
- **Validacao no hardware:** os itens acima ainda nao rodaram numa placa. Ordem sugerida:
  gravar vehicle, key e relays; ler `MAC desta placa` dos relays e colar em `RELAY_PEER_MAC`
  (A aponta para B e B para A), recompilar e regravar os relays; depois uma campanha de baseline
  e uma de relay com `tools/capture_run.py --port COMx --label <rotulo> --dist <m> --start`.

---

## Checkpoints (branches locais)

Cada checkpoint e um commit num branch `ckpt/...`. Cada branch parte do anterior, então um branch
antigo é sempre um estado completo do código. Só crio checkpoint depois de compilar para `esp32`.
Nada vai para o remoto sem pedido. Os branches ficam locais.

| Branch | Conteudo | Estado |
|---|---|---|
| `ckpt/01-rodada-ble-ferramentas` | M-01 a M-18, D-02, D-03, D-04a, D-04b | compilado para esp32, nao gravado |
| `ckpt/02-visual-basico` | V-02, V-03, V-04, V-05, V-10 | compilado para esp32, nao gravado, nao visto na tela |

Regra para voltar: ao gravar, teste do branch mais novo para o mais antigo. Se o mais novo
falhar, volte um branch, grave e teste de novo. Os itens M-01 a M-05 estao no mesmo commit que os
demais (o `vehicle/main/main.c` mudou de uma vez), entao nao ha como reverter so eles.

