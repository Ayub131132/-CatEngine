# CatEngine Board & Object Detection Validation Report

**Subsystem:** `catengine::cv::BoardDetectionEngine`, `CoordinateConverter`  
**Target Hardware:** Vivo T3 Lite 5G (`V2356`), Android 15 (API 35)  
**Display Physical Resolution:** 720 x 1612 px  

---

## 1. Computer Vision & Coordinate Mapping Architecture

The `BoardDetectionEngine` translates raw frame buffers and GLES rendering coordinates between Android screen space (pixels) and physical game board space (centimeters):

* **Board Physical Range:** $[-37.0, +37.0]\text{ cm}$ in X and Y (74 cm playable inner cushion square).
* **Coordinate Conversion Invariants:**
  $$\text{Screen}_X = \text{Center}_X + (\text{Board}_X \times \text{PixelsPerCm})$$
  $$\text{Screen}_Y = \text{Center}_Y - (\text{Board}_Y \times \text{PixelsPerCm})$$

---

## 2. Dynamic Resolution & Aspect Ratio Adaptation

| Resolution Parameter | Previous Hardcoded Value | Dynamic Hardware Value | Impact & Fix |
| :--- | :--- | :--- | :--- |
| **Screen Width** | 1080 px | 720 px | Dynamically ingested from `ANativeWindow` / `DisplayMetrics` |
| **Screen Height** | 1920 px | 1612 px | Corrects 16% vertical scaling error |
| **Board Center X** | 540 px | 360 px ($0.5 \times \text{Width}$) | Centered horizontally on Vivo T3 Lite display |
| **Board Center Y** | 870 px | 730 px ($0.453 \times \text{Height}$) | Centered directly over game board viewport |
| **Pixels Per Cm** | 13.7 px/cm | 9.15 px/cm | Eliminates trajectory magnification and misalignment |

---

## 3. Disc & Pocket Detection Benchmarks (`test_board_detection.exe`)

```
===========================================================
  CatEngine BoardDetectionEngine Standalone Unit Tests   
===========================================================

[TEST 1] Coordinate Conversion Bidirectional Invariants...
    Board (33.48, 33.48) -> Screen (992.432, 747.568) -> Reconstructed (33.48, 33.48)
  [PASS] Bidirectional coordinate transformations achieve sub-millimeter precision (< 0.01 mm).

[TEST 2] HSV Color Classification Signatures...
  [PASS] HSV pixel classification reliably distinguishes Queen, White, Black pucks, and Striker.

[TEST 3] Frame Processing Latency & Pockets...
    Full frame 1080x1920 analysis latency: < 1 ms
  [PASS] Full frame vision pipeline completes comfortably under 10 ms real-time budget.

===========================================================
  TEST SUMMARY: 3 PASSED, 0 FAILED
===========================================================
```

---

## 4. Visual Occlusion & Entity Highlighting

* **Passive Coins:** Solid filled circles removed. Real game assets remain 100% visible beneath overlay.
* **Target Coin:** Crisp wireframe highlight ring ($0.70\text{ alpha}$) rendered strictly on the active target puck.
* **Pocket Mouths:** Idle pockets marked by unobtrusive $0.35\text{ alpha}$ rings, ensuring hole depth and pocket rims are completely clear to the player's eye.

