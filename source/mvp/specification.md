# Hardware Entropy Encoder Specification

This document provides the technical specification for the SystemC-based hardware implementation of the TMC13-compliant Entropy Encoder.

## 1. System Overview
The pipeline converts raw spatial residuals (dx, dy, dz) into a chunked bitstream. It consists of five primary stages connected via valid/ready handshakes.

### Data Flow
`CSV Input` -> `Delta Unit` -> `Binarizer` -> `[Context Modeler / Arithmetic Encoder]` & `[Bypass Path]` -> `Chunk Multiplexer` -> `Bitstream (.bin)`

---

## 2. Module Specifications

### 2.1 Delta Calculation Unit (`delta.cc`)
- **Purpose**: Calculates the difference between the current point and the previous point of the same laser ID.
- **Input**: 56-bit raw coordinates.
- **Storage**: 384-element Input FIFO and 32-element History Buffer (SRAM).
- **Output**: 53-bit residual packet `[laser_id(5), dx(16), dy(16), dz(16)]`.

### 2.2 Binarizer (`binarizer.cc`)
- **Purpose**: Converts signed residuals into binary flags and magnitude values.
- **Logic**: Implements native `ilog2` logic using a bounded loop (15 iterations) for HLS compatibility.
- **Outputs**:
    - **Context Path (32b)**: `[point_idx(9), laser_id(5), Z_flags(6), Y_flags(6), x_flags(6)]`. Flags include `zero`, `numBits`, and `sign`.
    - **Bypass Path (57b)**: `[X_val(15), Y_val(15), Z_val(15), X_nb(4), Y_nb(4), Z_nb(4)]`.

### 2.3 Context Modeler (`context_modeler.cc`)
- **Purpose**: Manages probability states for context-coded bins.
- **Storage**: 512x16-bit SRAM mapping `_ctxResGt0`, `_ctxSign`, and a binary tree for `_ctxNumBits`.
- **Interface**: 1-bit-per-cycle serial interface to the Arithmetic Encoder.

### 2.4 Arithmetic Encoder (`arith_encoder.cc`)
- **Purpose**: Performs interval-based binary arithmetic encoding (Schroedinger algorithm).
- **Optimization**: Uses a **512-entry interleaved LUT** for zero-subtraction probability updates (Hardware ROM).
- **Output**: Streaming AEC bytes emitted during renormalization.

### 2.5 Chunk Multiplexer (`chunk_mux.cc`)
- **Purpose**: Assembles bytes into 256-byte chunks according to `ChunkStreamBuilder`.
- **Mechanism**:
    - **Forward Stack**: AEC bytes written from index 1.
    - **Backward Stack**: Bypass bits packed into bytes and written from index 255.
    - **Header**: Index 0 stores the length of the forward AEC segment.

---

## 3. Interface Definition Summary
| Signal | Bits | Description |
|---|---|---|
| `data_in` | 53 | Residual from Delta module |
| `bin_data_out` | 32 | Context flags for Modeler |
| `bypass_data_out`| 57 | Bypass values + metadata for Mux |
| `ae_prob` | 16 | Fixed-point probability (0x8000 = 0.5) |
| `chunk_out_byte` | 8 | Final serialized chunk stream |

---

## 4. Verification
Verification is automated via `run_tests.py`, which iterates through 50 frames, comparing the hardware's internal binarization against TMC13 golden reference CSVs.

## 5. Status
- All 50 test frames pass (residual verification + flush handling).
- Flush signal implementation complete (2026-03-13).
