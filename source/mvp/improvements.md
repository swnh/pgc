# Architectural Improvements & Edge Case Handling

This document identifies the critical edge cases currently missing from the hardware implementation and the necessary steps to implement them.

## 1. End-of-Stream Flushing (COMPLETED)
### Current Issue
The `chunk_mux` module currently only finalizes and outputs a 256-byte chunk when it is physically full (`aec_ptr >= bypass_ptr`).
### Consequence
At the end of a point cloud frame, the final partial chunk (containing the last few processed points) remains trapped in the internal `chunk_buf`. This results in a truncated `.bin` file that the decoder cannot read.
### Implementation
- Added `flush` input signal to the `chunk_mux` (chunk_mux.h:32).
- When `flush` is asserted, the state machine transitions to `FINALIZE`, pads remaining bits in bypass accumulator, and streams out non-empty segment.
- Added `flush_sig` signal in top.h, connected to both chunk_mux and testbench.
- Testbench asserts `flush_out` for 1 cycle when source is done and queue is empty.

## 2. Chunk Splicing (PENDING)
### Current Issue
Bypass data is written backwards from index 255. In a non-full chunk (truncated chunk at the end of a stream), there is a large gap between the AEC bytes (front) and Bypass bytes (back).
### Consequence
Standard TMC13 decoders expect bypass data to be aligned either at the end of a full 256-byte block or immediately following the AEC data in a spliced stream. If we concatenate two unspliced frames, the decoder will find zeros where it expects the bypass data of the first frame.
### Required Fix
- Implement `spliceChunkStreams` logic.
- Upon a `flush`, the module should optionally shift the backward-written bypass bytes forward so they are contiguous with the AEC bytes, effectively "shrinking" the final chunk to its minimal necessary size.
### Status
Not yet implemented. Current flush outputs partial chunk with gap intact.

## 3. Handshake Synchronization Tightening
### Current Issue
SystemC `sc_signal` writes take one delta-cycle/wait to propagate. Several modules (`context_modeler`, `arith_encoder`) initially had "shadowing" deadlocks where ready/valid signals were set and cleared in the same cycle.
### Required Fix
- Ensure all modules follow the "Wait-Before-Read" rule: `if (valid.read() && ready.read())`.
- Verify the testbench `sink` module can handle high-latency renormalization stalls in the Arithmetic Encoder without timing out.

## 4. Hardware Optimization (HLS)
- **Memory Partitioning**: The 512-entry Context Memory should be partitioned into multiple banks to allow simultaneous X, Y, and Z probability lookups, increasing throughput to 3 bits per cycle.
- **Renormalization Unrolling**: Replace the serial shift logic in the Arithmetic Encoder with a parallel priority encoder to perform renormalization in a single clock cycle.
