# Computer Vision & Board Perception Research

**Lead Engineer:** Senior Computer Vision & Mobile Graphics Engineer  
**Target:** Optical Board Localization & Disc Tracking for *Carrom Pool*

---

## 1. Vision Perception System Architecture

For standalone testing environments where in-process memory introspection is not permitted or desired, an optical Computer Vision (CV) perception pipeline is designed to extract game state directly from display frames.

```
+--------------------------------------------------------------------------+
|                        Raw Display Frame (RGBA8888)                      |
|                  (Captured via glReadPixels or MediaProjection)          |
+--------------------------------------------------------------------------+
                                     |
                                     v
+--------------------------------------------------------------------------+
| 1. Table Rectification & Homography (OpenCV)                             |
|    - Find 4 outer corner pocket markers / circular dark wells            |
|    - Compute 3x3 Perspective Transform Matrix (H)                        |
|    - Warp Perspective to Canonical 76.0 cm x 76.0 cm Orthogonal Canvas   |
+--------------------------------------------------------------------------+
                                     |
                                     v
+--------------------------------------------------------------------------+
| 2. Color Space Segmentation & Disc Candidate Extraction                  |
|    - Convert RGB to LAB and HSV Color Spaces                             |
|    - White Pucks: High L channel, low S (Saturation)                     |
|    - Black Pucks: Low L channel (L < 40)                                 |
|    - Queen: High Saturation, Red hue window (H in [0, 10] U [170, 180])   |
|    - Striker: Verified by radius (r_striker = 1.36 * r_puck)             |
+--------------------------------------------------------------------------+
                                     |
                                     v
+--------------------------------------------------------------------------+
| 3. Sub-Pixel Circle Fitting & State Synthesis                            |
|    - HoughCircles or Contour Moments (cv::fitEllipse / minEnclosingCircle)|
|    - Filter by radius bounds (striker ~2.65 cm, puck ~1.95 cm)           |
|    - Map detected centroids (u, v) back to Table Space (x, y in cm)      |
+--------------------------------------------------------------------------+
```

---

## 2. Coordinate Transformation (Screen $\leftrightarrow$ Board Space)

### 2.1 The Homography Mapping
Given screen pixel coordinates $\mathbf{x}_{\text{screen}} = (u, v, 1)^T$ and canonical board coordinates $\mathbf{X}_{\text{board}} = (x, y, 1)^T$ (in centimeters):

$$\mathbf{x}_{\text{screen}} \sim \mathbf{H} \cdot \mathbf{X}_{\text{board}}$$

Where $\mathbf{H}$ is a $3 \times 3$ projective homography matrix computed from the 4 pocket coordinates:
- Top-Left: $(-33.48, +33.48)$
- Top-Right: $(+33.48, +33.48)$
- Bottom-Right: $(+33.48, -33.48)$
- Bottom-Left: $(-33.48, -33.48)$

Using OpenCV:
```cpp
cv::Mat H = cv::getPerspectiveTransform(boardCorners, screenPocketPoints);
cv::Mat invH = H.inv();
```

To map any screen point $(u, v)$ to board space $(x, y)$:
$$\begin{pmatrix} X \\ Y \\ W \end{pmatrix} = \mathbf{H}^{-1} \begin{pmatrix} u \\ v \\ 1 \end{pmatrix}, \quad x = \frac{X}{W}, \quad y = \frac{Y}{W}$$

---

## 3. Disc Classification & Separation

| Disc Type | Color Segmentation (HSV / LAB) | Geometric Constraint | Verification Logic |
| :--- | :--- | :--- | :--- |
| **White Puck** | $L^* > 75$, $S < 35$ | Radius $r \in [1.80, 2.10]\text{ cm}$ | Minimum 1 puck, maximum 9. |
| **Black Puck** | $L^* < 35$, $V < 45$ | Radius $r \in [1.80, 2.10]\text{ cm}$ | Minimum 1 puck, maximum 9. |
| **Red Queen** | $H \in [0, 12] \cup [168, 180]$, $S > 120$ | Radius $r \in [1.80, 2.10]\text{ cm}$ | Exactly 0 or 1 puck. |
| **Striker** | Multi-colored / custom skin | **Radius $r \approx 2.65\text{ cm}$** ($36\%$ larger than pucks) | Located along baseline $y \approx \pm 25.35\text{ cm}$. |

---

## 4. Evaluation of CV Libraries for Android

| Library | Platform / Language | Latency (ARM64) | Robustness | Memory Footprint | Recommendation |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **OpenCV Android SDK (4.10+)** | C++ NDK / Java Wrapper | **$6 - 12\text{ ms}$** | High with Homography & Color Masking. Sensitive to heavy glare. | $\sim 15\text{ MB}$ (strip unused modules to $\sim 4\text{ MB}$) | **Primary Recommendation**. Industry standard, NEON-optimized. |
| **TFLite YOLOv8-Nano (INT8)** | C++ / TFLite GPU Delegate | $12 - 20\text{ ms}$ | Very High. Invariant to skins, glare, and table animations. | $\sim 6\text{ MB}$ | **Secondary Recommendation** (ML fallback for complex board skins). |
| **BoofCV** | Pure Java/Kotlin | $30 - 55\text{ ms}$ | Moderate. Lacks native SIMD acceleration. | $\sim 3\text{ MB}$ | Not recommended (causes frame pacing hitches). |

---

## 5. Handling Environmental Variances

1. **Resolution & Aspect Ratios**:
   - Android devices range from 16:9 ($1080 \times 1920$) to 20:9 ($1080 \times 2400$).
   - Homography normalization completely eliminates resolution and aspect ratio dependencies by mapping the table to a fixed $1024 \times 1024$ virtual frame buffer before detection.
2. **Dynamic Lighting & Table Skins**:
   - Carrom Pool features diverse cosmetic board skins (marble, neon, wood grain).
   - Adaptive histogram equalization (CLAHE) on the L-channel suppresses table highlights and shadows.
   - For heavily textured boards, the striker and discs are segmented via background subtraction against an empty table reference image.

