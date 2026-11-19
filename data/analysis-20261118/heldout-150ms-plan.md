# Held-out check of the 150 ms bound: plan, fixed before the campaigns

Date: 2026-11-19. Firmware: M-19, commit b79a463 (one random start offset per campaign).
Layout: car to key 2.24 m, relay A to relay B about 2 m, the same as the M-19 campaigns
(car to relay B 12 cm, relay A to key 12 cm).

## Question

The bound T = 150 ms was read from the eight M-19 baseline and the eight M-19 relayed campaigns
(`cutoff.md`, sections 5 and 6, commit 2980f64). Does it hold on campaigns it was not read from?

## Fixed before the campaigns

- T = 150 ms. It is not changed after the new campaigns are seen.
- Ten direct baseline campaigns (label `baseline-ho`) and ten relayed campaigns (unicast, default TX,
  label `relay-ho-ab200`), alternating, with the same scripts and the same firmware as the M-19 campaigns.
- Exclusions: only the existing rules. A relayed session without relay lines goes to `data/runs/_invalid/`,
  and an aborted bring-up goes to `data/runs/_invalid/aborted/`. No campaign is dropped for a high RTT.

## Criteria

1. Honest false rejects: held-out honest rounds with no answer or an RTT above 150 ms. Expected: 0.
   A count above 0 means the bound fails as a false-reject limit out of sample. The held-out honest
   maximum is then reported, and a higher bound is evaluated as a separate row, not as a replacement.
2. Relayed accepts: held-out relayed rounds answered at or below 150 ms, as a count and as a share with a
   95 % Wilson interval. No pass threshold is set for this share, because it depends on the connection
   phase of each campaign. It is reported as found.

Expectation, not a criterion: the relayed share between 0 and 3 %, because one of the eight M-19
campaigns reached 115 ms, and the other seven did not go below 164.6 ms.

Analysis: `tools/analyze_cutoff.py`, section 7, on the held-out campaigns only.
