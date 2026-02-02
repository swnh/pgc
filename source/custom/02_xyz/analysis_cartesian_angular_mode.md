# Analysis: Cartesian Coordinates in Angular Mode - Why Reconstruction is Distorted

## Executive Summary

Your modification attempts to use Cartesian coordinates instead of spherical coordinates in angular mode. While encoding appears to work (bitstream is generated), the **decoder cannot properly reconstruct** the point cloud because the encoder and decoder have inconsistent coordinate interpretations.

---

## Issue #1: `beginSph[nodeIdx]` is Never Set for First Point in Duplicate Group

### Location
`generateGeomPredictionTreeAngular()` - Lines 1332, 1349-1351

### Code (Current)
```cpp
const auto carPos = curPoint - origin;
// ...
// propagate converted coordinates over duplicate points
for (int i = nodeIdx + 1; i < nodeIdxN; i++)
  beginSph[i] = carPos;
```

### Problem
- `beginSph[nodeIdx]` (the **first** point in each group) is **never assigned**
- Only duplicate points (`nodeIdx + 1` to `nodeIdxN - 1`) get the value
- `beginSph[nodeIdx]` contains **uninitialized/garbage data**

### Impact
- When `encodeTree()` uses `srcPts[nodeIdx]` (which is `beginSph`), it reads garbage values
- All predictions and residuals are computed from garbage → distorted reconstruction

### Fix Required
```cpp
const auto carPos = curPoint - origin;
beginSph[nodeIdx] = carPos;  // ADD THIS LINE - set first point!
// propagate converted coordinates over duplicate points
for (int i = nodeIdx + 1; i < nodeIdxN; i++)
  beginSph[i] = carPos;
```

---

## Issue #2: Reconstruction Uses Invalid Prediction (Likely Secondary Issue)

### Location
`encodeTree()` - Lines 1106, 1117-1120

### Code (Current)
```cpp
best.prediction = origin + _sphToCartesian(point);  // Line 1106
best.residual = reconPts[nodeIdx] - best.prediction;
// ...
reconPts[nodeIdx] = best.prediction + best.residual;  // Line 1120
```

### Problem
- `point` is now Cartesian `(x, y, z)`, but `_sphToCartesian()` expects spherical `(r, phi, theta)`
- Result: `best.prediction` becomes garbage after the `_sphToCartesian()` call
- Since `residual2` is disabled, `best.residual` is set to 0 (line 1113)
- Final: `reconPts[nodeIdx] = garbage + 0 = garbage`

### Your Assumption Was Incorrect
You assumed that since `residual2` is disabled, the garbage prediction doesn't matter. **This is wrong** because:
- The reconstructed point `reconPts[nodeIdx]` is written back at line 1120
- The garbage `best.prediction` **directly becomes the final reconstruction**

### Flow When `residual2` is Disabled
```
best.prediction = origin + _sphToCartesian(point)  // garbage
best.residual = reconPts[nodeIdx] - best.prediction  // calculated
best.residual = 0  // overwritten because residual2 disabled
reconPts[nodeIdx] = best.prediction + best.residual  // garbage + 0 = garbage!
```

### Fix Required
Either:
1. Skip the entire block (lines 1073-1115) when using Cartesian mode
2. Or compute `best.prediction` correctly:
```cpp
best.prediction = origin + point;  // point is already Cartesian, don't convert
```

---

## Issue #3: Decoder Decodes Spherical But Tries to Reconstruct Cartesian

### The Fundamental Problem
The **decoder** (`geometry_predictive_decoder.cpp`) still:
1. Decodes the residuals as spherical coordinates components
2. Applies `SphericalToCartesian` conversion to get final Cartesian points

Your encoder:
1. Encodes Cartesian coordinate residuals as if they were spherical
2. The decoder interprets them incorrectly

### Why This Causes Distortion
| Component | Encoder Interpretation | Decoder Interpretation |
|-----------|------------------------|------------------------|
| residual[0] | X coordinate | Radius (r) |
| residual[1] | Y coordinate | Azimuth (phi) |
| residual[2] | Z coordinate | Laser index (theta) |

The decoder will apply `SphericalToCartesian()` to values that are already Cartesian → complete distortion.

### Fix Required
You must **also modify the decoder** to:
1. Skip the `SphericalToCartesian` conversion when your custom mode is active
2. Interpret residuals as Cartesian components directly

---

## Summary of Required Fixes

| Priority | Location | Issue | Fix |
|----------|----------|-------|-----|
| **CRITICAL** | Line 1332 | `beginSph[nodeIdx]` never assigned | Add `beginSph[nodeIdx] = carPos;` |
| **CRITICAL** | Line 1106 | Invalid `_sphToCartesian()` corrupts reconstruction | Change to `origin + point` or skip block |
| **CRITICAL** | Decoder | Decoder still uses spherical interpretation | Modify decoder to match encoder |

---

## Quick Test to Confirm Issue #1

Add this debug print before line 1591:
```cpp
std::cerr << "beginSph[" << i << "] = " 
          << beginSph[i][0] << ", " 
          << beginSph[i][1] << ", " 
          << beginSph[i][2] << std::endl;
```

If you see garbage/large values for the first points, Issue #1 is confirmed.

---

## Recommendation

For your Cartesian-in-angular-mode approach to work:
1. Fix the `beginSph[nodeIdx]` assignment (immediate)
2. Either:
   - Disable the entire block at lines 1073-1115, OR
   - Fix line 1106 to use Cartesian directly
3. Create a matching decoder modification (required for proper reconstruction)
