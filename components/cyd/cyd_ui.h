#pragma once
#include <stdint.h>
#include <stdbool.h>

/*
 * UI de status compartilhada por todos os papeis da bancada (key, vehicle, relay A/B).
 *
 * Uma tela unica, view-only (sem toque), no estilo funcional-limpo do deskrab:
 *   - cabecalho colorido com o nome do papel e um mascote animado no canto;
 *   - um banner grande com o ESTADO atual (e a cor dele, espelhada no LED RGB);
 *   - uma grade de cards rotulo/valor (ate 6) com as metricas do papel;
 *   - ate duas linhas hexadecimais (nonce / resposta) no rodape.
 *
 * O mascote representa o dispositivo (chave, carro ou antena de relay) e tem animacoes
 * basicas: balanca de leve (bob), pisca os olhos e da um "pulo" a cada cyd_ui_pulse()
 * (chamado a cada rodada/evento, para a tela ter vida).
 *
 * Todas as funcoes sao seguras para chamar de qualquer task: elas pegam o lock do LVGL.
 */

typedef enum {
    CYD_MASCOT_KEY,     // chave digital
    CYD_MASCOT_CAR,     // veiculo
    CYD_MASCOT_RELAY,   // relay (antena)
} cyd_mascot_t;

typedef struct {
    const char  *title;      // ex.: "VEICULO"
    uint32_t     accent;     // cor do cabecalho/mascote, 0xRRGGBB
    cyd_mascot_t mascot;
    const char  *fields[6];  // rotulos dos cards; use NULL para terminar antes de 6
    const char  *hex0;       // rotulo da 1a linha hex (NULL = nenhuma linha hex)
    const char  *hex1;       // rotulo da 2a linha hex (NULL = so uma)
} cyd_ui_cfg_t;

// Sobe display + LED e monta a tela. Chame depois de cyd_display_init()/cyd_led_init().
void cyd_ui_init(const cyd_ui_cfg_t *cfg);

// Banner de estado + cor; a cor vira o LED (respirando) e tinge o banner.
void cyd_ui_set_state(const char *text, uint32_t color);

// Valor do card idx (0..n-1), estilo printf.
void cyd_ui_set_field(int idx, const char *fmt, ...);

// Mostra 'n' bytes em hex na linha idx (0 ou 1). data NULL ou n<=0 limpa a linha.
void cyd_ui_set_hex(int idx, const uint8_t *data, int n);

// Da um "pulo" no mascote (reacao a um evento, ex.: rodada concluida).
void cyd_ui_pulse(void);
