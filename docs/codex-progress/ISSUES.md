# Open issues

## Compute dispatch

- `resource-upload-dispatch-writeback` remains expensive: 4.044 ms average in the latest
  controlled run, with a mean two-second-window P95 of 11.980 ms. Its nested
  `resource-bindings` and `buffer-views` stages need a non-overlapping attribution pass.
- Copy-HLE still averages 4.628 ms over every dispatch-stage sample, including cheap
  non-copy probes. Although eager copy reconciliation was removed for owned pages, resource
  preparation and later coherence points still dominate long-tail samples.
- Direct GPU submissions remain frequent (15,849 in the 66-second run). Submission batching
  must be measured by call site before changing ordering or visibility.
- GPU execution time is unknown because timestamp-query instrumentation is not present.

## Validation limits

- The two Bloodborne runs used cloned copies of the same isolated profile and equal duration,
  but no automated camera/save-state lock proves identical rendered frames. Do not infer an
  FPS change from these runs.
- The process was deliberately stopped after 66 seconds, so its `-1` exit status is expected
  and is not a crash result.
