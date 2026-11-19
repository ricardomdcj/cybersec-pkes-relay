# Campaign scripts

These scripts run one campaign from the Windows bench, as used for the 2026-11-18 data
(`data/runs/README.md`). They assume the bench's ports: COM5 relay A, COM6 key, COM7 relay B and
COM8 vehicle, and the ESP-IDF v6.1 Python environment under `C:\Espressif\tools\python\v6.1\venv`.

- `run_baseline.ps1 -Label baseline -Dist <m>`: holds both relays in reset, logs the key passively and
  runs the vehicle campaign (50 rounds) on COM8. Relays stay silent, so the vehicle talks to the key
  directly.
- `run_relay.ps1 -Label <label> -Dist <m> [-TxPow 8]`: holds the vehicle in reset, reboots relays A and B,
  waits until the key confirms relay A's connection (resetting relay A up to three times), optionally
  sets TXPOW on both relays, then runs the vehicle campaign. It warns when the campaign has no relay lines.
- `hold_and_log.py`: holds a port in reset (RTS asserted) or reads it passively (DTR and RTS idle).
- `log_ports.py`: passive logger for COM5, COM6 and COM7. It accepts `--pulse` for a reset at start and a
  command file where each line is `PORT text` (or `PORT PULSE`).

Both `.ps1` scripts close the deskrab bridge before each campaign, because it probes the COM ports. The
campaign folders go to `data/runs/`. After a run, add it to the index with `python -I tools/run_index.py`.

Example:

    powershell -NoProfile -ExecutionPolicy Bypass -File tools\campaign\run_relay.ps1 -Label relay -Dist 0.52
