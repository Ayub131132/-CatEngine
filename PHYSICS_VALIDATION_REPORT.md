# CatEngine Physics Engine & Collision Validation Report

**Subsystem:** `catengine::v2::PhysicsEngineV2`, Continuous Collision Detection (CCD)  
**Verification Suite:** `test_physics_engine.exe` (Standalone host unit tests)  
**Target Game Binary:** Miniclip Carrom Pool v19.6.0 (`libgame.so`, build 1491)  

---

## 1. Physical Parameter Invariants

| Physical Quantity | Miniclip Engine Constant | CatEngine V2 Physics Parameter | Validation Status |
| :--- | :--- | :--- | :--- |
| **Striker Mass ($m_1$)** | 3.0 | 3.0 | Matched |
| **Puck Mass ($m_2$)** | 1.0 | 1.0 | Matched |
| **Striker Radius ($r_1$)** | 2.65 cm | 2.65 cm | Matched |
| **Puck Radius ($r_2$)** | 1.95 cm | 1.95 cm | Matched |
| **Pocket Inset ($P_{\text{inset}}$)** | 33.48 cm | 33.48 cm | Matched |
| **Pocket Capture Radius ($P_{\text{rad}}$)** | 2.15 cm | 2.15 cm | Matched |
| **Disc Restitution ($e_{\text{body}}$)** | 1.00 (Elastic) | 1.00 (Elastic) | Matched (100% KE Conservation) |
| **Cushion Restitution ($e_{\text{cushion}}$)** | 0.88 | 0.88 | Matched |
| **Coulomb Table Friction ($\mu$)** | 0.28 | 0.28 | Matched |

---

## 2. Collision Dynamics Verification (`test_physics_engine.exe`)

```
===========================================================
  CatEngine PhysicsEngineV2 Standalone Unit Test Suite   
===========================================================

[TEST 1] Striker Launch Velocity Scaling...
    Distance Low Force (0.10): 2.30146 cm
    Distance Max Force (1.00): 81.9913 cm
  [PASS] Striker launch force correctly scales travel velocity and stopping distance.

[TEST 2] Disc-to-Disc Collision & 1D Elastic Impulse...
    First impact striker pos: (6.36816e-16, -4.6)
    Target puck post-impact final pos: (-1.53396e-13, 3.68767)
  [PASS] Disc-to-disc head-on collision correctly transfers momentum and records contact.

[TEST 3] Cushion Reflection & Restitution...
    Striker cushion bounces recorded: 1
  [PASS] Cushion reflection correctly tags bounce segment and reverses normal velocity.

[TEST 4] Pocket Capture & Attraction Dynamics...
    Puck hit: 1
    Corner shot puck inPocket: 1
    Puck final position: (33.48, 33.48)
  [PASS] Pocket attraction correctly processes disc proximity and capture state.

[TEST 5] Friction Deceleration & Stationary Cutoff...
    Launched at (0, -20), halted stably at (6.40142e-16, -9.54569)
  [PASS] Coulomb friction stops disc motion deterministically within finite iterations.

[TEST 6] Multi-Collision Cascade & Chain Reaction...
    Trajectory [0] Puck ID=101 startY=-10 finalY=-9.08878 isHit=1 bounces=0 segments=7
    Trajectory [1] Puck ID=102 startY=0 finalY=-1.4219 isHit=1 bounces=3 segments=8
  [PASS] Multi-disc cascade correctly propagates momentum across collision chains.

[TEST 7] Numerical Stability & Bounded Conservation...
    Verified 12 full-circle angles under max launch force with zero NaNs or drift.
  [PASS] Numerical integrator remains strictly stable across all directional sweeps.

===========================================================
  TEST SUMMARY: 7 PASSED, 0 FAILED
===========================================================
```

---

## 3. Pocket Capture & Foul Invariants

1. **Centripetal Capture Pull:**
   When a disc center penetrates within the capture radius ($r \le 2.15\text{ cm}$) of any of the 4 pocket coordinates $(\pm 33.48, \pm 33.48)$, an attractive radial force pulls the puck into the center.
2. **Scratch Foul Mechanics:**
   When the striker penetrates the pocket capture zone, it is classified as a Scratch Foul. The simulation flags `strikerScratched = true`, triggering a crimson warning indicator on the pocket and immediately disqualifying the shot from auto-recommendation.

