#!/usr/bin/env python3
"""Junta as linhas das campanhas e calcula as estatisticas (D-04a e D-04b).

Entrada: pastas de campanha (data/runs/<id>/, com capture_*.log) ou capturas avulsas
(logs antigos de data/). Cada entrada vira uma condicao (rotulo + distancia).

Junta por rodada, pelo nonce (4 bytes), as linhas do veiculo (CSV), do Relay A (CSVA) e
do Relay B (CSVB). Assim cada rodada e decomposta em pernas:
  veic<->B   = RTT do veiculo - turnaround de B   (BLE do veiculo ate B, ida e volta)
  tunel      = turnaround de B - turnaround de A  (ESP-NOW ida e volta + fila)
  A<->chave  = turnaround de A                    (BLE de A ate a chave + HMAC)

Estatisticas: contagens por resultado, IC de Wilson para a taxa de sucesso, mediana,
quartis, p90 e IC bootstrap 95 % da mediana. Entre condicoes: diferenca de medianas
(overhead) com IC bootstrap. Bootstrap com semente fixa, reproduzivel.

Uso (Python do venv do ESP-IDF com -I, ou qualquer Python 3):
  python -I tools/analyze_runs.py data/runs/<baseline> data/runs/<relay> --out data/analise
  python -I tools/analyze_runs.py data/baseline_20261116-220429.log data/relayed_20261116-220429.log

Saida: rodadas.csv (uma linha por rodada) e resumo.md (tabelas) na pasta --out.
"""

import argparse
import csv
import json
import math
import pathlib
import random
import re
import statistics
import sys

RE_VEH = re.compile(r"VEICULO: CSV,(.*)$")
RE_A = re.compile(r"RELAY_A: CSVA,(.*)$")
RE_B = re.compile(r"RELAY_B: CSVB,(.*)$")
RE_SUM = re.compile(r"VEICULO: (=== .*)$")
BOOTSTRAP_N = 2000
SEED = 20261117


def read_text(path):
    raw = pathlib.Path(path).read_bytes()
    if raw[:2] in (b"\xff\xfe", b"\xfe\xff"):          # UTF-16 (capturas antigas)
        text = raw.decode("utf-16")
    else:
        text = raw.decode("utf-8-sig", errors="replace")
    return text.replace("\r", "").splitlines()


def load_condition(path):
    """Devolve (nome, manifesto ou {}, lista de arquivos de captura)."""
    p = pathlib.Path(path)
    if p.is_dir():
        manifest_path = p / "manifest.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.exists() else {}
        files = sorted(p.glob("capture_*.log")) or sorted(p.glob("*.log"))
        name = manifest.get("label") or p.name
        return name, manifest, files
    return p.stem, {}, [p]


def parse_rows(lines):
    """Extrai as linhas CSV de um log. Aceita os formatos v1 e v2 do veiculo e o formato
    de relays (CSVB com 4, 5 ou 6 campos)."""
    veh, a_rows, b_rows, summaries = [], {}, {}, []
    for line in lines:
        m = RE_VEH.search(line)
        if m:
            f = m.group(1).split(",")
            if len(f) >= 10:          # v2: label,dist,rodada,nonce,t_tx,rtt,resultado,rssi,itvl,mtu
                veh.append(dict(label=f[0], dist=f[1], rodada=int(f[2]), nonce=f[3],
                                t_tx=int(f[4]), rtt=int(f[5]), res=f[6], rssi=int(f[7]),
                                itvl=int(f[8]), mtu=int(f[9])))
            elif len(f) >= 6:         # v1: label,rodada,nonce,rtt,resultado,rssi
                veh.append(dict(label=f[0], dist="-", rodada=int(f[1]), nonce=f[2],
                                t_tx=None, rtt=int(f[3]), res=f[4], rssi=int(f[5]),
                                itvl=None, mtu=None))
            continue
        m = RE_A.search(line)
        if m:
            f = m.group(1).split(",")
            a_rows[f[1]] = dict(seq=int(f[0]), turn_a=int(f[2]), rssi=int(f[3]),
                                txfalhas=int(f[4]) if len(f) >= 5 else None)
            continue
        m = RE_B.search(line)
        if m:
            f = m.group(1).split(",")
            b_rows[f[1]] = dict(seq=int(f[0]), turn_b=int(f[2]), rssi=int(f[3]),
                                turn_a=int(f[4]) if len(f) >= 5 else None,
                                txfalhas=int(f[5]) if len(f) >= 6 else None)
            continue
        m = RE_SUM.search(line)
        if m:
            summaries.append(m.group(1))
    return veh, a_rows, b_rows, summaries


def join_rounds(veh, a_rows, b_rows):
    """Uma linha por rodada do veiculo, com as pernas dos relays quando existem."""
    out = []
    for v in veh:
        a = a_rows.get(v["nonce"])
        b = b_rows.get(v["nonce"])
        rec = dict(v)
        rec["turn_b_us"] = b["turn_b"] if b else None
        rec["turn_a_us"] = (b["turn_a"] if b and b["turn_a"] is not None
                            else (a["turn_a"] if a else None))
        rec["txfalhas_b"] = b["txfalhas"] if b else None
        rec["txfalhas_a"] = a["txfalhas"] if a else None
        if rec["turn_b_us"] is not None and rec["turn_a_us"] is not None:
            rec["tunel_us"] = rec["turn_b_us"] - rec["turn_a_us"]
        else:
            rec["tunel_us"] = None
        if rec["turn_b_us"] is not None and v["res"] != "TIMEOUT" and v["rtt"] > 0:
            rec["veic_b_us"] = v["rtt"] - rec["turn_b_us"]
        else:
            rec["veic_b_us"] = None
        out.append(rec)
    return out


# ---------- estatistica (stdlib) --------------------------------------------

def quantile(xs, q):
    """Quantil com interpolacao linear (igual ao padrao do numpy)."""
    s = sorted(xs)
    if not s:
        return None
    pos = (len(s) - 1) * q
    lo, hi = math.floor(pos), math.ceil(pos)
    return s[lo] + (s[hi] - s[lo]) * (pos - lo)


def wilson(k, n, z=1.959964):
    if n == 0:
        return None, None
    p = k / n
    den = 1 + z * z / n
    centro = (p + z * z / (2 * n)) / den
    marg = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / den
    return max(0.0, centro - marg), min(1.0, centro + marg)


def boot_median_ci(xs, seed=SEED):
    if len(xs) < 2:
        return None, None
    rng = random.Random(seed)
    n = len(xs)
    meds = sorted(statistics.median(rng.choices(xs, k=n)) for _ in range(BOOTSTRAP_N))
    return meds[int(0.025 * BOOTSTRAP_N)], meds[int(0.975 * BOOTSTRAP_N)]


def boot_diff_ci(a, b, seed=SEED):
    """IC 95 % bootstrap da diferenca de medianas (b - a), amostras independentes."""
    if len(a) < 2 or len(b) < 2:
        return None, None
    rng = random.Random(seed)
    diffs = []
    for _ in range(BOOTSTRAP_N):
        ra = rng.choices(a, k=len(a))
        rb = rng.choices(b, k=len(b))
        diffs.append(statistics.median(rb) - statistics.median(ra))
    diffs.sort()
    return diffs[int(0.025 * BOOTSTRAP_N)], diffs[int(0.975 * BOOTSTRAP_N)]


def fmt_ms(us):
    return "-" if us is None else f"{us / 1000:.1f}"


def describe(xs):
    if not xs:
        return None
    med = statistics.median(xs)
    lo, hi = boot_median_ci(xs)
    return dict(n=len(xs), min=min(xs), med=med, q1=quantile(xs, .25), q3=quantile(xs, .75),
                p90=quantile(xs, .90), max=max(xs), mean=statistics.mean(xs),
                sd=statistics.pstdev(xs) if len(xs) > 1 else 0.0,
                med_ci=(lo, hi))


# ---------- relatorio ---------------------------------------------------------

def condition_report(name, rounds, manifest):
    res_counts = {"OK": 0, "FALHA": 0, "TIMEOUT": 0}
    for r in rounds:
        res_counts[r["res"]] = res_counts.get(r["res"], 0) + 1
    n = len(rounds)
    answered = [r["rtt"] for r in rounds if r["res"] != "TIMEOUT" and r["rtt"] > 0]
    ok_rtt = [r["rtt"] for r in rounds if r["res"] == "OK" and r["rtt"] > 0]
    lo, hi = wilson(res_counts["OK"], n)
    legs = {
        "veic<->B": [r["veic_b_us"] for r in rounds if r["veic_b_us"] is not None],
        "tunel": [r["tunel_us"] for r in rounds if r["tunel_us"] is not None],
        "A<->chave": [r["turn_a_us"] for r in rounds if r["turn_a_us"] is not None],
    }
    return dict(name=name, manifest=manifest, n=n, counts=res_counts,
                sucesso=(res_counts["OK"] / n if n else None), sucesso_ci=(lo, hi),
                rtt=describe(answered), rtt_ok=ok_rtt, answered=answered,
                legs={k: describe(v) for k, v in legs.items() if v})


def write_outputs(out_dir, conds, all_rounds):
    out_dir.mkdir(parents=True, exist_ok=True)
    cols = ["condicao", "label", "dist_m", "rodada", "nonce", "t_tx_us", "resultado",
            "rtt_us", "rssi_dbm", "itvl_1_25ms", "mtu", "turn_b_us", "turn_a_us",
            "tunel_us", "veic_b_us", "txfalhas_a", "txfalhas_b"]
    with open(out_dir / "rodadas.csv", "w", encoding="utf-8", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(cols)
        for r in all_rounds:
            w.writerow([r["condicao"], r["label"], r["dist"], r["rodada"], r["nonce"],
                        r["t_tx"], r["res"], r["rtt"], r["rssi"], r["itvl"], r["mtu"],
                        r["turn_b_us"], r["turn_a_us"], r["tunel_us"], r["veic_b_us"],
                        r["txfalhas_a"], r["txfalhas_b"]])

    md = ["# Resumo das campanhas", ""]
    md += ["| Condicao | Dist (m) | Rodadas | OK | FALHA | TIMEOUT | Sucesso (IC 95 %) |",
           "|---|---|---|---|---|---|---|"]
    for c in conds:
        lo, hi = c["sucesso_ci"]
        suc = "-" if c["sucesso"] is None else f"{c['sucesso']*100:.1f} % ({lo*100:.1f}-{hi*100:.1f})"
        md.append(f"| {c['name']} | {c['manifest'].get('dist_m', '-')} | {c['n']} | "
                  f"{c['counts']['OK']} | {c['counts']['FALHA']} | {c['counts']['TIMEOUT']} | {suc} |")
    md += ["", "## RTT do veiculo (rodadas respondidas, ms)", "",
           "| Condicao | n | min | Q1 | mediana (IC 95 %) | Q3 | p90 | max | media | dp |",
           "|---|---|---|---|---|---|---|---|---|---|"]
    for c in conds:
        d = c["rtt"]
        if not d:
            md.append(f"| {c['name']} | 0 | | | | | | | | |")
            continue
        ci = d["med_ci"]
        md.append(f"| {c['name']} | {d['n']} | {fmt_ms(d['min'])} | {fmt_ms(d['q1'])} | "
                  f"{fmt_ms(d['med'])} ({fmt_ms(ci[0])}-{fmt_ms(ci[1])}) | {fmt_ms(d['q3'])} | "
                  f"{fmt_ms(d['p90'])} | {fmt_ms(d['max'])} | {fmt_ms(d['mean'])} | {fmt_ms(d['sd'])} |")

    md += ["", "## Pernas da rodada (mediana, ms)", "",
           "| Condicao | veic<->B | tunel | A<->chave |", "|---|---|---|---|"]
    for c in conds:
        cell = []
        for leg in ("veic<->B", "tunel", "A<->chave"):
            d = c["legs"].get(leg)
            cell.append("-" if not d else f"{fmt_ms(d['med'])} (IQR {fmt_ms(d['q1'])}-{fmt_ms(d['q3'])})")
        md.append(f"| {c['name']} | " + " | ".join(cell) + " |")

    base = conds[0]
    if len(conds) > 1 and base["rtt"]:
        md += ["", f"## Overhead em relacao a '{base['name']}' (mediana, ms)", "",
               "| Condicao | diferenca de medianas (IC 95 %) |", "|---|---|"]
        for c in conds[1:]:
            if not c["rtt"]:
                continue
            lo, hi = boot_diff_ci(base["answered"], c["answered"])
            diff = statistics.median(c["answered"]) - statistics.median(base["answered"])
            md.append(f"| {c['name']} | {fmt_ms(diff)} ({fmt_ms(lo)} a {fmt_ms(hi)}) |")

    md += ["", "Notas: rodadas TIMEOUT ficam fora das estatisticas de RTT e entram na taxa de "
           "sucesso. IC da mediana e da diferenca por bootstrap (2000 reamostragens, semente "
           f"{SEED}). IC da taxa por Wilson 95 %."]
    (out_dir / "resumo.md").write_text("\n".join(md) + "\n", encoding="utf-8")
    return "\n".join(md)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("entradas", nargs="+", help="pastas de campanha ou arquivos de captura")
    ap.add_argument("--out", default=None, help="pasta de saida (padrao: analise/ ao lado da primeira entrada)")
    args = ap.parse_args(argv)

    conds, all_rounds = [], []
    for entrada in args.entradas:
        name, manifest, files = load_condition(entrada)
        lines = []
        for f in files:
            lines += read_text(f)
        veh, a_rows, b_rows, summaries = parse_rows(lines)
        rounds = join_rounds(veh, a_rows, b_rows)
        if not rounds:
            print(f"aviso: nenhuma linha CSV do veiculo em {entrada}", file=sys.stderr)
            continue
        for r in rounds:
            r["condicao"] = name
            r["dist"] = manifest.get("dist_m") if r["dist"] in (None, "-") else r["dist"]
        conds.append(condition_report(name, rounds, manifest))
        all_rounds += rounds

    if not conds:
        print("nenhuma condicao com dados", file=sys.stderr)
        return 1

    out_dir = pathlib.Path(args.out) if args.out else pathlib.Path(args.entradas[0]).resolve().parent / "analise"
    print(write_outputs(out_dir, conds, all_rounds))
    print(f"\nsaidas em: {out_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
