// PKES screen simulator: runs a script of cyd_ui calls on the PC and writes 320x240 BMP frames.
//
// Usage: pkes_sim <script.txt> [outdir]        (outdir defaults to "out"; it must exist)
//
// The UI is the real components/cyd/cyd_ui.c, built with LVGL 8.3. Only the hardware (display,
// LED, timers) is simulated, see sim_hal.c. The script drives the UI API directly: the firmware
// logic (BLE, ESP-NOW, the round timer) needs the radio and does not run here.
//
// Script commands (one per line; '#' starts a comment; text runs to the end of the line):
//   init <key|vehicle|relay_a|relay_b>   bring up the UI of a role (once per run)
//   verbose <on|off>                     print LED calls with the virtual time
//   wait <ms>                            advance the virtual clock
//   shot <file.bmp>                      write the current frame to <outdir>/<file.bmp>
//   tag <text|->                         header mode (-  hides it)
//   link <ble|esp> <-1|0|1>              header indicator: -1 hidden, 0 down, 1 up
//   state <hex colour> <text>            state banner and colour
//   field <idx> <text>                   value of card idx (0..5)
//   hex <idx> <hex bytes|->              hex line 0 or 1
//   stats <text|->                       statistics line
//   progress <cur> <total>               progress bar (total 0 hides it)
//   pulse                                mascot hop
//   flash <hex colour> <ms>              LED blink, then back to the state colour
//   expr <ok|fail|timeout|normal> <ms>   mascot expression
//   badge <hex colour> <ms> <text>       balloon over the banner
//   spark <reset|timeout|us>             sparkline: start a campaign, a timeout, or an RTT
//   band <lo_us> <hi_us>                 baseline band (hi <= lo turns it off)
//   linkpulse <0|1> [text|-]             relay link: 0 = B->A challenge, 1 = A->B response
//   stack <a_us> <b_us>                  stacked bar (negative hides it)
//   scene <dist_x10> <ms>                car and key scene (dist_x10 = tenths of a metre)
//   splash <ms> <subtitle>               boot splash
//   summary <bins>|<legend>|<line>|...   final summary with a histogram (bins: comma separated)
//   grid                                 back to the card grid
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "cyd_ui.h"
#include "cyd_display.h"
#include "cyd_led.h"
#include "sim_hal.h"

// Copies of the cyd_ui_cfg_t of the firmwares (key/main/main.c, vehicle/main/main.c,
// relays/main/relay_a.c and relays/main/relay_b.c). Keep them in sync when a card changes.
// relay_b uses the BLE-mode label "Turnaround"; the self-test build shows "tunnel RTT".
static const cyd_ui_cfg_t CFG_KEY = {
    .title = "KEY", .accent = 0xF4B400, .mascot = CYD_MASCOT_KEY,
    .fields = { "Challenges", "Calc", NULL }, .hex0 = "nonce", .hex1 = "resp",
};
static const cyd_ui_cfg_t CFG_VEHICLE = {
    .title = "VEHICLE", .accent = 0x2962FF, .mascot = CYD_MASCOT_CAR,
    .fields = { "Round", "RTT", "OK", "Fail / TO", NULL }, .hex0 = "nonce", .hex1 = "resp",
};
static const cyd_ui_cfg_t CFG_RELAY_A = {
    .title = "RELAY A", .accent = 0x7C4DFF, .mascot = CYD_MASCOT_RELAY,
    .fields = { "From tunnel", "From key", "Key", NULL }, .hex0 = "nonce", .hex1 = NULL,
};
static const cyd_ui_cfg_t CFG_RELAY_B = {
    .title = "RELAY B", .accent = 0x00B8D4, .mascot = CYD_MASCOT_RELAY,
    .fields = { "From car", "From tunnel", "Turnaround", NULL }, .hex0 = "resp", .hex1 = NULL,
};

static int s_ui_ready;

// ---- parsing helpers -------------------------------------------------------------------------

// Next whitespace-separated token; terminates it in place. NULL at the end of the line.
static int is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

static char *next_tok(char **p) {
    char *s = *p;
    while (is_space(*s)) s++;
    if (!*s) { *p = s; return NULL; }
    char *e = s;
    while (*e && !is_space(*e)) e++;
    if (*e) *e++ = 0;
    *p = e;
    return s;
}

// The rest of the line, without the line break and trailing spaces.
static char *rest_of(char **p) {
    char *s = *p;
    while (*s == ' ' || *s == '\t') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ' || e[-1] == '\t')) *--e = 0;
    *p = e;
    return s;
}

static int parse_hex(const char *s, uint8_t *out, int max) {
    int n = 0;
    while (s[0] && s[1] && n < max) {
        char b[3] = { s[0], s[1], 0 };
        out[n++] = (uint8_t)strtoul(b, NULL, 16);
        s += 2;
    }
    return n;
}

static const cyd_ui_cfg_t *role_cfg(const char *role) {
    if (!strcmp(role, "key")) return &CFG_KEY;
    if (!strcmp(role, "vehicle")) return &CFG_VEHICLE;
    if (!strcmp(role, "relay_a")) return &CFG_RELAY_A;
    if (!strcmp(role, "relay_b")) return &CFG_RELAY_B;
    return NULL;
}

// ---- one script line -------------------------------------------------------------------------

#define FAIL(...) do { fprintf(stderr, "line %d: ", ln); fprintf(stderr, __VA_ARGS__); \
                       fputc('\n', stderr); return 1; } while (0)
#define ARG_STR(var, what) char *var = next_tok(&p); if (!var) FAIL("missing %s", what)
#define ARG_INT(var, what) char *var##_s = next_tok(&p); if (!var##_s) FAIL("missing %s", what); \
                           int var = atoi(var##_s)
#define ARG_HEX(var, what) char *var##_s = next_tok(&p); if (!var##_s) FAIL("missing %s", what); \
                           uint32_t var = (uint32_t)strtoul(var##_s, NULL, 16)

static int run_line(char *line, int ln, const char *outdir) {
    char *p = line;
    char *cmd = next_tok(&p);
    if (!cmd || cmd[0] == '#') return 0;

    if (!strcmp(cmd, "init")) {
        ARG_STR(role, "role");
        if (s_ui_ready) FAIL("init already done in this run");
        const cyd_ui_cfg_t *cfg = role_cfg(role);
        if (!cfg) FAIL("unknown role '%s' (key, vehicle, relay_a, relay_b)", role);
        cyd_ui_init(cfg);
        s_ui_ready = 1;
        return 0;
    }
    if (!strcmp(cmd, "verbose")) {
        ARG_STR(v, "on|off");
        g_sim_verbose = !strcmp(v, "on");
        return 0;
    }
    if (!strcmp(cmd, "wait")) {
        ARG_INT(ms, "ms");
        sim_advance_ms(ms);
        return 0;
    }
    if (!strcmp(cmd, "shot")) {
        ARG_STR(name, "file name");
        char path[512];
        snprintf(path, sizeof path, "%s/%s", outdir, name);
        if (sim_write_bmp(path) != 0) FAIL("cannot write %s", path);
        printf("wrote %s\n", path);
        return 0;
    }

    if (!s_ui_ready) FAIL("'%s' used before init", cmd);

    if (!strcmp(cmd, "tag")) {
        char *t = rest_of(&p);
        cyd_ui_set_tag(!strcmp(t, "-") ? NULL : t);
        return 0;
    }
    if (!strcmp(cmd, "link")) {
        ARG_STR(which, "ble|esp");
        ARG_INT(st, "state");
        cyd_ui_set_link(!strcmp(which, "esp") ? CYD_LINK_ESPNOW : CYD_LINK_BLE, st);
        return 0;
    }
    if (!strcmp(cmd, "state")) {
        ARG_HEX(color, "colour");
        char *t = rest_of(&p);
        cyd_ui_set_state(t, color);
        return 0;
    }
    if (!strcmp(cmd, "field")) {
        ARG_INT(idx, "index");
        char *t = rest_of(&p);
        cyd_ui_set_field(idx, "%s", t);
        return 0;
    }
    if (!strcmp(cmd, "hex")) {
        ARG_INT(idx, "index");
        char *h = next_tok(&p);
        uint8_t buf[32];
        int n = 0;
        if (h && strcmp(h, "-")) n = parse_hex(h, buf, sizeof buf);
        cyd_ui_set_hex(idx, n ? buf : NULL, n);
        return 0;
    }
    if (!strcmp(cmd, "stats")) {
        char *t = rest_of(&p);
        cyd_ui_set_stats(!strcmp(t, "-") ? NULL : t);
        return 0;
    }
    if (!strcmp(cmd, "progress")) {
        ARG_INT(cur, "cur");
        ARG_INT(total, "total");
        cyd_ui_set_progress(cur, total);
        return 0;
    }
    if (!strcmp(cmd, "pulse")) {
        cyd_ui_pulse();
        return 0;
    }
    if (!strcmp(cmd, "flash")) {
        ARG_HEX(color, "colour");
        ARG_INT(ms, "ms");
        cyd_ui_flash(color, ms);
        return 0;
    }
    if (!strcmp(cmd, "expr")) {
        ARG_STR(e, "expression");
        ARG_INT(ms, "ms");
        cyd_expr_t x;
        if (!strcmp(e, "ok")) x = CYD_EXPR_OK;
        else if (!strcmp(e, "fail")) x = CYD_EXPR_FAIL;
        else if (!strcmp(e, "timeout")) x = CYD_EXPR_TIMEOUT;
        else if (!strcmp(e, "normal")) x = CYD_EXPR_NORMAL;
        else FAIL("unknown expression '%s'", e);
        cyd_ui_expression(x, ms);
        return 0;
    }
    if (!strcmp(cmd, "badge")) {
        ARG_HEX(color, "colour");
        ARG_INT(ms, "ms");
        char *t = rest_of(&p);
        cyd_ui_badge(t, color, ms);
        return 0;
    }
    if (!strcmp(cmd, "spark")) {
        ARG_STR(v, "reset|timeout|us");
        if (!strcmp(v, "reset")) cyd_ui_spark_reset();
        else if (!strcmp(v, "timeout")) cyd_ui_spark_push(-1);
        else cyd_ui_spark_push((int64_t)strtoll(v, NULL, 10));
        return 0;
    }
    if (!strcmp(cmd, "band")) {
        ARG_STR(lo, "lo_us");
        ARG_STR(hi, "hi_us");
        cyd_ui_spark_band((int64_t)strtoll(lo, NULL, 10), (int64_t)strtoll(hi, NULL, 10));
        return 0;
    }
    if (!strcmp(cmd, "linkpulse")) {
        ARG_INT(dir, "0|1");
        char *t = rest_of(&p);
        cyd_ui_link_pulse(dir, (t[0] == '\0' || !strcmp(t, "-")) ? NULL : t);
        return 0;
    }
    if (!strcmp(cmd, "stack")) {
        ARG_STR(a, "a_us");
        ARG_STR(b, "b_us");
        cyd_ui_stack_bar((int64_t)strtoll(a, NULL, 10), (int64_t)strtoll(b, NULL, 10));
        return 0;
    }
    if (!strcmp(cmd, "scene")) {
        ARG_INT(dist, "dist_x10");
        ARG_INT(ms, "ms");
        cyd_ui_scene(dist, ms);
        return 0;
    }
    if (!strcmp(cmd, "splash")) {
        ARG_INT(ms, "ms");
        char *t = rest_of(&p);
        cyd_ui_splash(t, ms);
        return 0;
    }
    if (!strcmp(cmd, "summary")) {
        char *t = rest_of(&p);
        char *parts[12];
        int np = 0;
        for (char *s = strtok(t, "|"); s && np < 12; s = strtok(NULL, "|")) parts[np++] = s;
        if (np < 2) FAIL("summary needs <bins>|<legend>|<line>...");
        uint16_t bins[10];
        int nbins = 0;
        for (char *s = parts[0]; *s && nbins < 10;) {
            bins[nbins++] = (uint16_t)strtoul(s, &s, 10);
            if (*s == ',') s++;
        }
        static char text[512];
        text[0] = 0;
        for (int i = 2; i < np; i++) {
            if (i > 2) strcat(text, "\n");
            strncat(text, parts[i], sizeof text - strlen(text) - 2);
        }
        cyd_ui_summary_show(text, bins, nbins, parts[1]);
        return 0;
    }
    if (!strcmp(cmd, "grid")) {
        cyd_ui_summary_hide();
        return 0;
    }

    FAIL("unknown command '%s'", cmd);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: pkes_sim <script.txt> [outdir]\n");
        return 2;
    }
    const char *outdir = argc > 2 ? argv[2] : "out";

    FILE *f = fopen(argv[1], "r");
    if (!f) { perror(argv[1]); return 2; }

    cyd_display_init();
    cyd_led_init();

    char line[512];
    int ln = 0, rc = 0;
    while (fgets(line, sizeof line, f)) {
        ln++;
        if (run_line(line, ln, outdir) != 0) { rc = 1; break; }
    }
    fclose(f);
    return rc;
}
