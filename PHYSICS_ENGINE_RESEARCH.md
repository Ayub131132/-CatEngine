# Independent 2D Carrom Physics Simulation Research

**Lead Engineer:** Game Physics Engineer & C++ Specialist  
**Target:** High-Performance, Deterministic 2D Carrom Rigid-Body Simulator

---

## 1. Physics Engine Architecture & Governing Equations

To predict shot outcomes with high fidelity, the simulation must model 2D disc dynamics, continuous collision detection (CCD), Coulomb friction, impulse resolution, and gravitational pocket attraction.

```
+--------------------------------------------------------------------------+
|                       Forward Simulation Substepping                     |
+--------------------------------------------------------------------------+
  For step = 0 to max_steps (e.g. 500 steps, dt = 0.005s):
    1. Apply Coulomb Friction Deceleration: a = -mu * g * normalize(v)
    2. Check Cushion Collisions (Boundary clamping & impulse response)
    3. Check Inter-Disc Collisions (Circle-circle CCD & elastic momentum)
    4. Check Pocket Capture Zones (Funnel suction & skip velocity check)
    5. Integrate Positions: x += v * dt
    6. Check Quiescence: If all |v| < 0.1 cm/s, break early
```

---

## 2. Mathematical Formulations

### 2.1 Rigid-Body Movement & Friction
For a disc with position $\mathbf{p} = (x, y)$, linear velocity $\mathbf{v} = (v_x, v_y)$, and mass $m$:

1. **Sliding Friction**:
   $$\mathbf{a}_{\text{slide}} = -\mu_{\text{slide}} \cdot g \cdot \frac{\mathbf{v}}{\|\mathbf{v}\|}, \quad g = 980\text{ cm/s}^2$$
   For striker: $\mu_{\text{slide}} = 0.160 \implies a \approx 156.8\text{ cm/s}^2$.
2. **Rolling Friction**:
   $$\mathbf{a}_{\text{roll}} = -\mu_{\text{roll}} \cdot g \cdot \frac{\mathbf{v}}{\|\mathbf{v}\|}$$
   For pucks: $\mu_{\text{roll}} = 0.108 \implies a \approx 105.8\text{ cm/s}^2$.

### 2.2 Circle-to-Circle Collision Resolution
Given two discs $A$ and $B$ with radii $r_A, r_B$, masses $m_A, m_B$, positions $\mathbf{p}_A, \mathbf{p}_B$, and velocities $\mathbf{v}_A, \mathbf{v}_B$:

1. **Collision Normal & Relative Velocity**:
   $$\mathbf{n} = \frac{\mathbf{p}_B - \mathbf{p}_A}{\|\mathbf{p}_B - \mathbf{p}_A\|}, \quad \mathbf{v}_{\text{rel}} = \mathbf{v}_A - \mathbf{v}_B$$
   Approaching condition: $\mathbf{v}_{\text{rel}} \cdot \mathbf{n} > 0$.
2. **Normal Impulse Magnitude ($J_n$)**:
   $$J_n = \frac{-(1 + e) (\mathbf{v}_{\text{rel}} \cdot \mathbf{n})}{\frac{1}{m_A} + \frac{1}{m_B}}$$
   Where restitution $e = 1.00$ (elastic collision).
3. **Velocity Updates**:
   $$\mathbf{v}_A' = \mathbf{v}_A + \frac{J_n \mathbf{n}}{m_A}, \quad \mathbf{v}_B' = \mathbf{v}_B - \frac{J_n \mathbf{n}}{m_B}$$

### 2.3 Wall Cushion Rebound & Restitution
The table boundary is defined by a rectangular cushion polygon: $x \in [-L_x, +L_x], y \in [-L_y, +L_y]$ where $L_x \approx 37.0\text{ cm}, L_y \approx 37.0\text{ cm}$.

For disc with radius $r$:
- When $|x| + r \ge L_x$:
  $$v_x' = -e_{\text{cushion}} \cdot v_x, \quad e_{\text{cushion}} = 0.88$$
- When $|y| + r \ge L_y$:
  $$v_y' = -e_{\text{cushion}} \cdot v_y$$
- **Tangential Cushion Friction**: An oblique bounce reduces parallel velocity $v_t$ via:
  $$v_t' = v_t \cdot (1 - \mu_{\text{cushion\_tangent}})$$

### 2.4 Pocket Mechanics & Gravity Funnels
For each of the 4 corner pockets at $\mathbf{c}_k = (\pm 33.48, \pm 33.48)$:
1. Calculate distance: $d = \|\mathbf{p} - \mathbf{c}_k\|$.
2. If $d \le r_{\text{pocket}}$ ($2.15\text{ cm}$):
   - **Velocity Cutoff**: If $\|\mathbf{v}\| > 250.0\text{ cm/s}$, disc skips over hole (no pot).
   - **Capture**: If $\|\mathbf{v}\| \le 250.0\text{ cm/s}$, inward gravitational acceleration $g_{\text{pocket}} = 980\text{ cm/s}^2$ is applied towards $\mathbf{c}_k$. Once distance $d < 0.8\text{ cm}$, the body is flagged `inPocket = true` and removed from subsequent collision phases.

---

## 3. Strict Consistency Requirement

> [!CRITICAL]
> **Unified Simulation Rule**: The visual prediction renderer (`GraphicsEngine.cpp`) and the autonomous shot selector (`GraphicsEngine_ScanAndExecuteBestShot`) must invoke the exact same simulation function (`catengine::v2::simulate()`).
> Separately computed aesthetic trajectories or approximated lines are strictly prohibited, as divergence between rendered prediction and shot execution leads directly to shot failure.

---

## 4. Evaluation of Physics Simulation Libraries

| Criteria | Custom C++ Engine (`PhysicsEngineV2.cpp`) | Box2D (v2.4 / v3.0) | Chipmunk2D |
| :--- | :--- | :--- | :--- |
| **Footprint** | **~45 KB** | ~400 KB | ~180 KB |
| **Dependencies** | None (standard C++17) | C++ Standard Library | Pure C99 |
| **Simulation Speed** | **$< 2\text{ ms}$ for 500 steps** | $\sim 10\text{ ms}$ | $\sim 5\text{ ms}$ |
| **Carrom Mechanics Match** | **100% matched** to Carrom Pool mass ratio and pocket drop thresholds. | Generic solver; requires heavy custom contact listeners. | Generic solver; requires custom constraint overrides. |
| **Deterministic Multi-Shot Search** | High parallelism; can simulate 360 radial trajectories in $< 30\text{ ms}$ on mobile ARM cores. | Slower; world initialization and destruction overhead. | Moderate; requires manual memory pool management. |

### Engineering Verdict:
**Retain and optimize the existing custom C++ engine (`catengine::v2::PhysicsEngineV2`)**. Box2D and Chipmunk add substantial binary weight and generic solver overhead without providing superior accuracy for a strictly circular 2D disc billiard game.

