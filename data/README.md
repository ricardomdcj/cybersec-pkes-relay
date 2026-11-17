# data/ — captured runs

Raw captures from the bench. The original files are not modified.

| File | What it is | Encoding |
|---|---|---|
| `baseline_20261116-220429.log` | honest baseline: vehicle and key, no relays (captured 2026-11-16 22:04:29) | UTF-16 with BOM |
| `relayed_20261116-220429.log` | same session, full chain: key, Relay A, Relay B, vehicle | UTF-8 with BOM |

Both were captured with the firmware of that day: label `run`, 20 rounds per capture, CSV v1 with the
old Portuguese tags (`VEICULO`, `FALHA`). The analysis tools accept those tags.

- `utf8/`: UTF-8 copies (no BOM, line endings unchanged) of the two files above, made with
  `tools/recode_logs.py`. The text is the same as the originals, and `tools/analyze_runs.py` gives
  identical output from either set. Use these copies for `grep` and `diff`.
- `analysis/`: output of `tools/analyze_runs.py` (`rounds.csv`, `summary.md`) and of
  `tools/plot_runs.py` (`rtt_timeseries.svg`, `rtt_histogram.svg`, `rtt_boxplot.svg`), built from `utf8/`.
- `runs/`: campaigns of the current firmware, one folder per run with `manifest.json`, created by
  `tools/capture_run.py` when a campaign is captured.
