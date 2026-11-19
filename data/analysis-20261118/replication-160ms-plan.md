# Replication of the 160 ms bound: plan, fixed before the campaigns

Date: 2026-11-19. Firmware: M-19, commit b79a463 (one random start offset per campaign).
Layout: car to key 2.24 m, relay A to relay B about 2 m, the same as the M-19 and held-out campaigns.

## Question

The 160 ms row in the held-out analysis was chosen after the held-out maximum (150.6 ms) was seen
(`heldout-150ms-plan.md`, section 7 of `cutoff.md`), so it has no clean test yet. Does a bound of 160 ms,
fixed now before any new campaign, reject no honest round and accept few relayed rounds on campaigns
that were not used to choose it?

## Fixed before the campaigns

- T = 160 ms. It is not changed after the new campaigns are seen.
- Ten direct baseline campaigns (label `baseline-rep`) and ten relayed campaigns (unicast, default TX,
  label `relay-rep-ab200`), alternating, with the same scripts and the same firmware as the M-19 campaigns.
- A campaign whose bring-up aborts before the capture is moved to `data/runs/_invalid/aborted/`, and the
  run is repeated, up to three attempts per campaign. Repeats are counted on the abort, never on an RTT.
- Exclusions: only the existing rules. A relayed session without relay lines goes to `data/runs/_invalid/`.
  No campaign is dropped for a high RTT.

## Criteria

1. Honest false rejects at 160 ms: replication honest rounds above T or unanswered. Expected: 0.
   A count above 0 means the bound does not hold as a no-false-reject limit on these campaigns. The
   replication honest maximum is then reported, and a higher bound is evaluated as a separate row, not as a
   replacement.
2. Relayed accepts at 160 ms: replication relayed rounds answered at or below T, as a count and a share with
   a 95 % Wilson interval. No pass threshold: the share depends on the connection phase of each campaign,
   and it is reported as found.

Expectation, not a criterion: the relayed share between 0 and 4 %. The in-sample set gave 1.5 % and the
held-out set 1.4 % at 160 ms.

Analysis: `tools/analyze_cutoff.py`, section 9, on the replication campaigns only.
