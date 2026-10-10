# Open issues

## Compute dispatch

- `resource-upload-dispatch-writeback` remains expensive: 4.044 ms average in the latest
  controlled run, with a mean two-second-window P95 of 11.980 ms. Its nested
  `resource-bindings` and `buffer-views` stages need a non-overlapping attribution pass.
- Copy-HLE still averages 4.628 ms over every dispatch-stage sample, including cheap
  non-copy probes. Although eager copy reconciliation was removed for owned pages, resource
  preparation and later coherence points still dominate long-tail samples.
- Copy submission batching reduced copy queue submissions by 33.8%, but direct submissions
  remain essentially unchanged. Further batching requires call-site dependency evidence.
- D3D12 timestamps show compute execution averaging about 0.0064 ms in the measured scene;
  shader execution is not the cause of the multi-millisecond dispatch wall time.
- Snapshot comparison, uploads and later GPU-owned readback still process roughly 5-6 GB in
  the 40-second steady-state window. The guest buffer ranges that cause these large coherence
  transfers need to be attributed before narrowing dirty ranges.

## Validation limits

- The two Bloodborne runs used cloned copies of the same isolated profile and equal duration,
  but no automated camera/save-state lock proves identical rendered frames. Do not infer an
  FPS change from these runs.
- The process was deliberately stopped after 66 seconds, so its `-1` exit status is expected
  and is not a crash result.
