#include "cyd_ui.h"
#include "cyd_display.h"
#include "cyd_led.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "esp_timer.h"

/* ---- paleta e medidas --------------------------------------------------- */

#define COL_BG        0x101014   /* fundo quase preto   */
#define COL_CARD      0x1e1e26   /* fundo dos cards     */
#define COL_LABEL     0x8a8a9a   /* rotulos (cinza)     */
#define COL_VALUE     0xf2f2f6   /* valores (branco)    */
#define COL_FACE      0xf4f4f4   /* rosto do mascote    */
#define COL_INK       0x202028   /* olhos/boca          */

#define CARD_W  150
#define CARD_H  44

/* ---- widgets persistentes ----------------------------------------------- */

static lv_obj_t *s_state_box, *s_state_lbl;
static lv_obj_t *s_field_val[6];
static int       s_n_fields;
static lv_obj_t *s_hex_lbl[2];
static const char *s_hex_tag[2];

/* mascote */
static lv_obj_t *s_mascot, *s_eye[2], *s_ant_tip;
static int       s_base_y;        /* y de repouso do mascote */
static int       s_tick;          /* contador do timer de animacao */
static int       s_hop;           /* energia do "pulo", decai a cada tick */
static uint32_t  s_accent;

/* barra de progresso (V-05) e cor do estado guardada para o pisca do LED (V-10) */
static lv_obj_t *s_bar;
static uint32_t  s_state_color = 0x33333a;
static esp_timer_handle_t s_flash_timer;

/* indicadores do cabecalho (V-06) e linha de estatistica (V-08) */
static lv_obj_t *s_tag_lbl;
static lv_obj_t *s_link_dot[2], *s_link_lbl[2];
static lv_obj_t *s_stats_lbl;

/* mascote: expressao temporaria (V-09a) e balao de resultado (V-09b) */
static lv_obj_t *s_mouth;
typedef enum { EXPR_NORMAL, EXPR_OK, EXPR_FALHA, EXPR_TIMEOUT } expr_t;
static volatile expr_t s_expr = EXPR_NORMAL;
static volatile int    s_expr_until;
static lv_obj_t       *s_badge;
static volatile int    s_badge_until;

/* sparkline das rodadas (V-07a/b). Valores do grafico em decimos de ms (cabem em int16). */
#define SPARK_N  30
#define SPARK_W  102
#define SPARK_H  18
#define SPARK_X  212
#define SPARK_Y  178
static lv_obj_t          *s_spark, *s_band, *s_rug[SPARK_N];
static lv_chart_series_t *s_ser;
static int64_t s_win[SPARK_N];        /* us; -1 = timeout, -2 = vazio */
static int64_t s_band_lo, s_band_hi;  /* us; hi <= lo = sem faixa */
static bool    s_spark_on;
#define COL_LINK_OK   0x1AA260
#define COL_LINK_DOWN 0xD64545

/* ---- helpers ------------------------------------------------------------ */

static lv_obj_t *mk_card(lv_obj_t *parent, const char *label) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, CARD_W, CARD_H);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(card, lv_color_hex(COL_CARD), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 6, 0);
    lv_obj_set_style_pad_all(card, 5, 0);

    lv_obj_t *l = lv_label_create(card);
    lv_label_set_text(l, label);
    lv_obj_set_style_text_color(l, lv_color_hex(COL_LABEL), 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *v = lv_label_create(card);
    lv_label_set_text(v, "-");
    lv_obj_set_style_text_color(v, lv_color_hex(COL_VALUE), 0);
    lv_obj_set_style_text_font(v, &lv_font_montserrat_20, 0);
    lv_obj_align(v, LV_ALIGN_BOTTOM_LEFT, 0, 2);
    return v;
}

/* ---- mascote ------------------------------------------------------------ */

static lv_obj_t *mk_dot(lv_obj_t *parent, int d, uint32_t color) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_size(o, d, d);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    return o;
}

static void build_mascot(lv_obj_t *scr, cyd_mascot_t kind, uint32_t accent) {
    s_mascot = lv_obj_create(scr);
    lv_obj_set_size(s_mascot, 58, 58);
    lv_obj_clear_flag(s_mascot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(s_mascot, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_mascot, 0, 0);
    lv_obj_set_style_pad_all(s_mascot, 0, 0);
    lv_obj_align(s_mascot, LV_ALIGN_TOP_RIGHT, -6, 2);

    /* rosto: quadrado arredondado claro */
    lv_obj_t *face = lv_obj_create(s_mascot);
    lv_obj_set_size(face, 44, 38);
    lv_obj_clear_flag(face, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(face, lv_color_hex(COL_FACE), 0);
    lv_obj_set_style_bg_opa(face, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(face, 11, 0);
    lv_obj_set_style_border_width(face, 2, 0);
    lv_obj_set_style_border_color(face, lv_color_hex(accent), 0);
    lv_obj_set_style_pad_all(face, 0, 0);
    lv_obj_align(face, LV_ALIGN_BOTTOM_MID, 0, 0);

    s_eye[0] = mk_dot(face, 8, COL_INK);
    s_eye[1] = mk_dot(face, 8, COL_INK);
    lv_obj_align(s_eye[0], LV_ALIGN_CENTER, -9, -3);
    lv_obj_align(s_eye[1], LV_ALIGN_CENTER,  9, -3);

    lv_obj_t *mouth = lv_obj_create(face);
    lv_obj_set_size(mouth, 14, 4);
    lv_obj_clear_flag(mouth, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(mouth, lv_color_hex(COL_INK), 0);
    lv_obj_set_style_bg_opa(mouth, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(mouth, 2, 0);
    lv_obj_set_style_border_width(mouth, 0, 0);
    lv_obj_align(mouth, LV_ALIGN_CENTER, 0, 10);
    s_mouth = mouth;

    /* acessorio que identifica o papel */
    switch (kind) {
    case CYD_MASCOT_KEY: {
        /* anel + haste de uma chave acima da cabeca */
        lv_obj_t *ring = lv_obj_create(s_mascot);
        lv_obj_set_size(ring, 14, 14);
        lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(ring, 3, 0);
        lv_obj_set_style_border_color(ring, lv_color_hex(accent), 0);
        lv_obj_align(ring, LV_ALIGN_TOP_MID, -7, 0);
        lv_obj_t *stem = lv_obj_create(s_mascot);
        lv_obj_set_size(stem, 3, 10);
        lv_obj_clear_flag(stem, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(stem, lv_color_hex(accent), 0);
        lv_obj_set_style_bg_opa(stem, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(stem, 0, 0);
        lv_obj_align(stem, LV_ALIGN_TOP_MID, 2, 6);
        break;
    }
    case CYD_MASCOT_CAR: {
        /* duas rodas sob o rosto */
        lv_obj_t *w0 = mk_dot(s_mascot, 12, COL_INK);
        lv_obj_t *w1 = mk_dot(s_mascot, 12, COL_INK);
        lv_obj_align(w0, LV_ALIGN_BOTTOM_MID, -12, 2);
        lv_obj_align(w1, LV_ALIGN_BOTTOM_MID,  12, 2);
        break;
    }
    case CYD_MASCOT_RELAY: {
        /* antena com ponta que pisca na cor do papel */
        lv_obj_t *mast = lv_obj_create(s_mascot);
        lv_obj_set_size(mast, 3, 16);
        lv_obj_clear_flag(mast, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(mast, lv_color_hex(accent), 0);
        lv_obj_set_style_bg_opa(mast, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(mast, 0, 0);
        lv_obj_align(mast, LV_ALIGN_TOP_MID, 0, 0);
        s_ant_tip = mk_dot(s_mascot, 8, accent);
        lv_obj_align(s_ant_tip, LV_ALIGN_TOP_MID, 0, -2);
        break;
    }
    }
}

/* Timer unico (roda sob o lock do LVGL): bob + piscar + pulo. */
static void mascot_tick(lv_timer_t *t) {
    (void) t;
    s_tick++;

    /* bob: onda triangular, amplitude 5, periodo ~27 ticks (~1,6 s) */
    int p = s_tick % 27;
    int tri = p < 14 ? p : 27 - p;
    int bob = tri * 5 / 13;

    int y = s_base_y - bob - s_hop;
    lv_obj_align(s_mascot, LV_ALIGN_TOP_RIGHT, -6, y);
    if (s_hop > 0) s_hop -= 3;
    if (s_hop < 0) s_hop = 0;

    /* piscar: ~a cada 45 ticks (~2,7 s), olhos fechados por 2 ticks */
    bool closed = (s_tick % 45) >= 43;

    /* expressao temporaria (V-09a): volta ao normal quando o prazo passa */
    if (s_expr != EXPR_NORMAL && s_tick >= s_expr_until) s_expr = EXPR_NORMAL;
    int eye_h = closed ? 2 : 8, mouth_w = 14, mouth_h = 4, mouth_dy = 10;
    uint32_t eye_col = COL_INK;
    switch (s_expr) {
    case EXPR_OK:      eye_h = 2;  mouth_w = 18; mouth_h = 6; mouth_dy = 9;  break;  /* sorriso */
    case EXPR_FALHA:   eye_col = 0xD64545; mouth_w = 14; mouth_h = 2; mouth_dy = 11; break;
    case EXPR_TIMEOUT: eye_h = 10; mouth_w = 6;  mouth_h = 6; mouth_dy = 9;  break;  /* surpreso */
    default: break;
    }
    lv_obj_set_height(s_eye[0], eye_h);
    lv_obj_set_height(s_eye[1], eye_h);
    lv_obj_set_style_bg_color(s_eye[0], lv_color_hex(eye_col), 0);
    lv_obj_set_style_bg_color(s_eye[1], lv_color_hex(eye_col), 0);
    lv_obj_set_size(s_mouth, mouth_w, mouth_h);
    lv_obj_align(s_mouth, LV_ALIGN_CENTER, 0, mouth_dy);

    /* balao de resultado (V-09b): some quando o prazo passa */
    if (s_badge && !lv_obj_has_flag(s_badge, LV_OBJ_FLAG_HIDDEN) && s_tick >= s_badge_until)
        lv_obj_add_flag(s_badge, LV_OBJ_FLAG_HIDDEN);

    /* ponta da antena pisca (so no relay) */
    if (s_ant_tip) {
        bool on = (s_tick / 8) % 2;
        lv_obj_set_style_bg_opa(s_ant_tip, on ? LV_OPA_COVER : LV_OPA_40, 0);
    }
}

/* ---- API ---------------------------------------------------------------- */

void cyd_ui_init(const cyd_ui_cfg_t *cfg) {
    s_accent = cfg->accent;
    cyd_display_lock();

    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* cabecalho */
    lv_obj_t *hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, 320, 40);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(cfg->accent), 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_align(hdr, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_t *title = lv_label_create(hdr);
    lv_label_set_text(title, cfg->title);
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 4, 0);

    /* V-06: modo da campanha e dois indicadores de enlace, entre o titulo e o mascote
     * (o mascote ocupa x 256..314 e nao pode ser coberto). Escondidos por padrao. */
    s_tag_lbl = lv_label_create(hdr);
    lv_obj_set_style_text_color(s_tag_lbl, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(s_tag_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(s_tag_lbl, LV_ALIGN_LEFT_MID, 116, 0);
    lv_obj_add_flag(s_tag_lbl, LV_OBJ_FLAG_HIDDEN);
    static const char *link_names[2] = { "BLE", "ESP" };
    for (int i = 0; i < 2; i++) {
        int x = 182 + i * 36;
        s_link_dot[i] = lv_obj_create(hdr);
        lv_obj_set_size(s_link_dot[i], 8, 8);
        lv_obj_clear_flag(s_link_dot[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(s_link_dot[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(s_link_dot[i], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(s_link_dot[i], lv_color_hex(COL_LINK_DOWN), 0);
        lv_obj_set_style_border_width(s_link_dot[i], 0, 0);
        lv_obj_set_style_pad_all(s_link_dot[i], 0, 0);
        lv_obj_align(s_link_dot[i], LV_ALIGN_LEFT_MID, x, 0);
        lv_obj_add_flag(s_link_dot[i], LV_OBJ_FLAG_HIDDEN);

        s_link_lbl[i] = lv_label_create(hdr);
        lv_label_set_text(s_link_lbl[i], link_names[i]);
        lv_obj_set_style_text_color(s_link_lbl[i], lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(s_link_lbl[i], &lv_font_montserrat_12, 0);
        lv_obj_align(s_link_lbl[i], LV_ALIGN_LEFT_MID, x + 11, 0);
        lv_obj_add_flag(s_link_lbl[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* banner de estado */
    s_state_box = lv_obj_create(scr);
    /* 240 px (e nao 308): o mascote ocupa x 256..314 em y 2..60 e cobria o fim do banner (V-03). */
    lv_obj_set_size(s_state_box, 240, 30);
    lv_obj_clear_flag(s_state_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_state_box, lv_color_hex(0x33333a), 0);
    lv_obj_set_style_bg_opa(s_state_box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_state_box, 6, 0);
    lv_obj_set_style_border_width(s_state_box, 0, 0);
    lv_obj_set_style_pad_all(s_state_box, 0, 0);
    lv_obj_align(s_state_box, LV_ALIGN_TOP_LEFT, 6, 44);
    s_state_lbl = lv_label_create(s_state_box);
    lv_label_set_text(s_state_lbl, "iniciando");
    lv_obj_set_style_text_color(s_state_lbl, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(s_state_lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(s_state_lbl, LV_ALIGN_LEFT_MID, 8, 0);

    /* barra de progresso fina entre o banner (y 74) e a grade (y 80); escondida por padrao */
    s_bar = lv_bar_create(scr);
    lv_obj_set_size(s_bar, 308, 4);
    lv_obj_align(s_bar, LV_ALIGN_TOP_LEFT, 6, 76);
    lv_bar_set_range(s_bar, 0, 100);
    lv_bar_set_value(s_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_bar, lv_color_hex(0x33333a), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_bar, lv_color_hex(cfg->accent), LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_bar, 2, LV_PART_INDICATOR);
    lv_obj_add_flag(s_bar, LV_OBJ_FLAG_HIDDEN);

    /* grade de cards (flex, 2 por linha) */
    lv_obj_t *grid = lv_obj_create(scr);
    lv_obj_set_size(grid, 312, 110);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_style_pad_row(grid, 6, 0);
    lv_obj_set_style_pad_column(grid, 6, 0);
    lv_obj_align(grid, LV_ALIGN_TOP_LEFT, 4, 80);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    s_n_fields = 0;
    for (int i = 0; i < 6 && cfg->fields[i]; i++) {
        s_field_val[i] = mk_card(grid, cfg->fields[i]);
        s_n_fields++;
    }

    /* linhas hex no rodape */
    const char *tags[2] = { cfg->hex0, cfg->hex1 };
    for (int i = 0; i < 2; i++) {
        s_hex_tag[i] = tags[i];
        s_hex_lbl[i] = NULL;
        if (!tags[i]) continue;
        lv_obj_t *l = lv_label_create(scr);
        lv_label_set_text_fmt(l, "%s -", tags[i]);
        lv_obj_set_style_text_color(l, lv_color_hex(COL_LABEL), 0);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
        lv_obj_align(l, LV_ALIGN_BOTTOM_LEFT, 6, i == 0 ? -22 : -4);
        s_hex_lbl[i] = l;
    }

    /* V-08: linha de estatistica abaixo da grade (cards terminam em y 174) */
    s_stats_lbl = lv_label_create(scr);
    lv_label_set_text(s_stats_lbl, "");
    lv_obj_set_style_text_color(s_stats_lbl, lv_color_hex(COL_VALUE), 0);
    lv_obj_set_style_text_font(s_stats_lbl, &lv_font_montserrat_12, 0);
    lv_label_set_long_mode(s_stats_lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(s_stats_lbl, 200);                   /* deixa x 212..314 para a sparkline */
    lv_obj_align(s_stats_lbl, LV_ALIGN_TOP_LEFT, 6, SPARK_Y + 2);

    /* V-07: faixa do baseline (atras do grafico), sparkline e marcas de timeout (rug) */
    s_band = lv_obj_create(scr);
    lv_obj_set_size(s_band, SPARK_W, SPARK_H);
    lv_obj_clear_flag(s_band, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_band, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_opa(s_band, LV_OPA_20, 0);
    lv_obj_set_style_radius(s_band, 0, 0);
    lv_obj_set_style_border_width(s_band, 0, 0);
    lv_obj_set_style_pad_all(s_band, 0, 0);
    lv_obj_set_pos(s_band, SPARK_X, SPARK_Y);
    lv_obj_add_flag(s_band, LV_OBJ_FLAG_HIDDEN);

    s_spark = lv_chart_create(scr);
    lv_obj_set_size(s_spark, SPARK_W, SPARK_H);
    lv_obj_set_pos(s_spark, SPARK_X, SPARK_Y);
    lv_chart_set_type(s_spark, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_spark, SPARK_N);
    lv_chart_set_div_line_count(s_spark, 0, 0);
    lv_chart_set_update_mode(s_spark, LV_CHART_UPDATE_MODE_SHIFT);
    lv_obj_set_style_bg_opa(s_spark, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_spark, 0, 0);
    lv_obj_set_style_pad_all(s_spark, 0, 0);
    lv_obj_set_style_line_width(s_spark, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(s_spark, 0, LV_PART_INDICATOR);       /* sem marcadores nos pontos */
    s_ser = lv_chart_add_series(s_spark, lv_color_hex(cfg->accent), LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_add_flag(s_spark, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < SPARK_N; i++) {
        s_rug[i] = lv_obj_create(scr);
        lv_obj_set_size(s_rug[i], 2, 2);
        lv_obj_clear_flag(s_rug[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(s_rug[i], lv_color_hex(0xD64545), 0);
        lv_obj_set_style_bg_opa(s_rug[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(s_rug[i], 0, 0);
        lv_obj_set_style_border_width(s_rug[i], 0, 0);
        lv_obj_set_style_pad_all(s_rug[i], 0, 0);
        lv_obj_set_pos(s_rug[i], SPARK_X + i * SPARK_W / SPARK_N, SPARK_Y + SPARK_H - 2);
        lv_obj_add_flag(s_rug[i], LV_OBJ_FLAG_HIDDEN);
    }
    for (int i = 0; i < SPARK_N; i++) s_win[i] = -2;
    s_band_lo = s_band_hi = 0;
    s_spark_on = false;

    /* V-06/V-09b: balao de resultado sobre o banner, a esquerda do mascote (x 256..314) */
    s_badge = lv_label_create(scr);
    lv_label_set_text(s_badge, "");
    lv_obj_set_style_text_font(s_badge, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_badge, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_color(s_badge, lv_color_hex(0x1AA260), 0);
    lv_obj_set_style_bg_opa(s_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_badge, 6, 0);
    lv_obj_set_style_pad_hor(s_badge, 6, 0);
    lv_obj_set_style_pad_ver(s_badge, 3, 0);
    lv_obj_align(s_badge, LV_ALIGN_TOP_LEFT, 136, 46);
    lv_obj_add_flag(s_badge, LV_OBJ_FLAG_HIDDEN);

    build_mascot(scr, cfg->mascot, cfg->accent);
    s_base_y = 2;
    s_tick = s_hop = 0;
    lv_timer_create(mascot_tick, 60, NULL);

    cyd_display_unlock();

    cyd_ui_set_state("iniciando", 0x33333a);
}

void cyd_ui_set_state(const char *text, uint32_t color) {
    s_state_color = color;
    cyd_display_lock();
    if (s_state_lbl) lv_label_set_text(s_state_lbl, text);
    if (s_state_box) lv_obj_set_style_bg_color(s_state_box, lv_color_hex(color), 0);
    cyd_display_unlock();
    /* espelha no LED RGB: respira na cor do estado */
    cyd_led_set(CYD_LED_BREATHE, color);
}

void cyd_ui_set_field(int idx, const char *fmt, ...) {
    if (idx < 0 || idx >= s_n_fields) return;
    char buf[48];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    cyd_display_lock();
    lv_label_set_text(s_field_val[idx], buf);
    cyd_display_unlock();
}

void cyd_ui_set_hex(int idx, const uint8_t *data, int n) {
    if (idx < 0 || idx > 1 || !s_hex_lbl[idx]) return;
    char buf[80];
    int w = snprintf(buf, sizeof buf, "%s ", s_hex_tag[idx]);
    if (!data || n <= 0) {
        snprintf(buf + w, sizeof buf - w, "-");
    } else {
        if (n > 16) n = 16;   /* cabe na linha */
        for (int i = 0; i < n && w < (int) sizeof buf - 3; i++)
            w += snprintf(buf + w, sizeof buf - w, "%02x", data[i]);
    }
    cyd_display_lock();
    lv_label_set_text(s_hex_lbl[idx], buf);
    cyd_display_unlock();
}

void cyd_ui_pulse(void) {
    s_hop = 16;   /* o timer de animacao faz o pulo decair */
}

/* ---- expressao, balao e sparkline (V-07, V-09) ------------------------- */

void cyd_ui_expression(cyd_expr_t e, int ms) {
    cyd_display_lock();
    s_expr = (expr_t) e;
    s_expr_until = s_tick + ms / 60;     /* o timer do mascote roda a cada 60 ms */
    cyd_display_unlock();
}

void cyd_ui_badge(const char *text, uint32_t color, int ms) {
    cyd_display_lock();
    if (s_badge) {
        lv_label_set_text(s_badge, text ? text : "");
        lv_obj_set_style_bg_color(s_badge, lv_color_hex(color), 0);
        lv_obj_clear_flag(s_badge, LV_OBJ_FLAG_HIDDEN);
        s_badge_until = s_tick + ms / 60;
    }
    cyd_display_unlock();
}

/* Recalcula a escala (em decimos de ms), a faixa e as marcas de timeout. Chamar com o lock. */
static void spark_redraw(void)
{
    int64_t lo = 0, hi = 0;
    bool any = false;
    for (int i = 0; i < SPARK_N; i++) {
        if (s_win[i] < 0) continue;
        int64_t v = s_win[i] / 100;
        if (!any) { lo = hi = v; any = true; }
        if (v < lo) lo = v;
        if (v > hi) hi = v;
    }
    bool band = s_spark_on && s_band_hi > s_band_lo;
    if (band) {
        int64_t bl = s_band_lo / 100, bh = s_band_hi / 100;
        if (!any) { lo = bl; hi = bh; any = true; }
        if (bl < lo) lo = bl;
        if (bh > hi) hi = bh;
    }
    if (!any) { lo = 0; hi = 1; }
    if (hi == lo) { lo -= 50; hi += 50; }          /* 5 ms de folga para linha reta */
    int64_t pad = (hi - lo) / 10;
    lo -= pad;
    hi += pad;
    lv_chart_set_range(s_spark, LV_CHART_AXIS_PRIMARY_Y, (lv_coord_t) lo, (lv_coord_t) hi);

    int64_t span = hi - lo;
    if (band) {
        int64_t bl = s_band_lo / 100, bh = s_band_hi / 100;
        int y0 = SPARK_Y + (int) ((hi - bh) * SPARK_H / span);
        int y1 = SPARK_Y + (int) ((hi - bl) * SPARK_H / span);
        if (y0 < SPARK_Y) y0 = SPARK_Y;
        if (y1 > SPARK_Y + SPARK_H) y1 = SPARK_Y + SPARK_H;
        if (y1 <= y0) y1 = y0 + 1;
        lv_obj_set_pos(s_band, SPARK_X, y0);
        lv_obj_set_height(s_band, y1 - y0);
        lv_obj_clear_flag(s_band, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_band, LV_OBJ_FLAG_HIDDEN);
    }

    for (int i = 0; i < SPARK_N; i++) {
        if (s_spark_on && s_win[i] == -1) lv_obj_clear_flag(s_rug[i], LV_OBJ_FLAG_HIDDEN);
        else                              lv_obj_add_flag(s_rug[i], LV_OBJ_FLAG_HIDDEN);
    }
}

void cyd_ui_spark_reset(void) {
    cyd_display_lock();
    for (int i = 0; i < SPARK_N; i++) s_win[i] = -2;
    if (s_spark) {
        lv_chart_set_all_value(s_spark, s_ser, LV_CHART_POINT_NONE);
        lv_obj_clear_flag(s_spark, LV_OBJ_FLAG_HIDDEN);
    }
    s_spark_on = true;
    spark_redraw();
    cyd_display_unlock();
}

void cyd_ui_spark_push(int64_t us) {
    cyd_display_lock();
    if (s_spark) {
        for (int i = 0; i < SPARK_N - 1; i++) s_win[i] = s_win[i + 1];
        s_win[SPARK_N - 1] = us < 0 ? -1 : us;
        lv_chart_set_next_value(s_spark, s_ser,
                                us < 0 ? LV_CHART_POINT_NONE : (lv_coord_t) (us / 100));
        spark_redraw();
    }
    cyd_display_unlock();
}

void cyd_ui_spark_band(int64_t lo_us, int64_t hi_us) {
    cyd_display_lock();
    s_band_lo = lo_us;
    s_band_hi = hi_us;
    spark_redraw();
    cyd_display_unlock();
}

/* ---- cabecalho e estatistica (V-06, V-08) ------------------------------ */

void cyd_ui_set_tag(const char *text) {
    cyd_display_lock();
    if (s_tag_lbl) {
        if (text && *text) {
            lv_label_set_text(s_tag_lbl, text);
            lv_obj_clear_flag(s_tag_lbl, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_tag_lbl, LV_OBJ_FLAG_HIDDEN);
        }
    }
    cyd_display_unlock();
}

void cyd_ui_set_link(cyd_link_t which, int state) {
    if ((int) which < 0 || (int) which > 1) return;
    cyd_display_lock();
    if (s_link_dot[which]) {
        if (state < 0) {
            lv_obj_add_flag(s_link_dot[which], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_link_lbl[which], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_set_style_bg_color(s_link_dot[which],
                                      lv_color_hex(state ? COL_LINK_OK : COL_LINK_DOWN), 0);
            lv_obj_clear_flag(s_link_dot[which], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_link_lbl[which], LV_OBJ_FLAG_HIDDEN);
        }
    }
    cyd_display_unlock();
}

void cyd_ui_set_stats(const char *text) {
    cyd_display_lock();
    if (s_stats_lbl) lv_label_set_text(s_stats_lbl, text ? text : "");
    cyd_display_unlock();
}

/* ---- valores com unidade (V-02) ---------------------------------------- */

void cyd_fmt_us(char *buf, size_t cap, int64_t us) {
    if (us < 0) {
        snprintf(buf, cap, "-");
    } else if (us >= 1000000) {
        snprintf(buf, cap, "%lld.%02lld s", (long long) (us / 1000000),
                 (long long) ((us % 1000000) / 10000));
    } else {
        snprintf(buf, cap, "%lld.%01lld ms", (long long) (us / 1000),
                 (long long) ((us % 1000) / 100));
    }
}

/* ---- pisca do LED por resultado (V-10) --------------------------------- */

static void flash_end_cb(void *arg) {
    cyd_led_set(CYD_LED_BREATHE, s_state_color);   /* volta a respirar na cor do estado */
}

void cyd_ui_flash(uint32_t color, int ms) {
    cyd_led_set(CYD_LED_BLINK, color);
    if (!s_flash_timer) {
        const esp_timer_create_args_t a = { .callback = flash_end_cb, .name = "led_flash" };
        if (esp_timer_create(&a, &s_flash_timer) != ESP_OK) return;
    }
    esp_timer_stop(s_flash_timer);
    esp_timer_start_once(s_flash_timer, (uint64_t) ms * 1000);
}

/* ---- barra de progresso (V-05) ----------------------------------------- */

void cyd_ui_set_progress(int cur, int total) {
    cyd_display_lock();
    if (s_bar) {
        if (total <= 0) {
            lv_obj_add_flag(s_bar, LV_OBJ_FLAG_HIDDEN);
        } else {
            int pct = cur * 100 / total;
            if (pct < 0) pct = 0;
            if (pct > 100) pct = 100;
            lv_obj_clear_flag(s_bar, LV_OBJ_FLAG_HIDDEN);
            lv_bar_set_value(s_bar, pct, LV_ANIM_ON);
        }
    }
    cyd_display_unlock();
}
