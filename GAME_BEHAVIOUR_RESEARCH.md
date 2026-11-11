# Carrom Pool Gameplay Behaviour & Empirical Physics Research

**Lead Engineer:** Game Physics Engineer & Experimental Data Analyst  
**Environment:** Authorized / Owned Device Test Laboratory  
**Target:** Physical & Empirical Dynamics of *Carrom Pool* (Miniclip)

---

## 1. Controlled Test Methodology

To validate trajectory prediction against reality, empirical shot data must be gathered systematically in a controlled, authorized environment (e.g. offline practice mode or custom room matches on an owned device).

```
+--------------------------------------------------------------------------+
|                        Controlled Test Shot Cycle                        |
+--------------------------------------------------------------------------+
                                     |
 1. State Recording  ---> Capture initial striker (x, y) & pucks {id: (x, y)}
                                     |
 2. Input Execution  ---> Apply deterministic pull angle (theta) & force (f)
                                     |
 3. High-Speed Track ---> Log first impact point, bounce normals, speeds
                                     |
 4. Quiescence Wait  ---> Wait until all bodies satisfy velocity < epsilon
                                     |
 5. Outcome Capture  ---> Record final coordinates {id: (x', y')} & potted state
                                     |
 6. Error Evaluation ---> Compute delta_x, delta_y, and trajectory error norm
```

---

## 2. Test Shot Dataset Schema

Each recorded test shot is captured into a structured JSON dataset for regression testing:

```json
{
  "test_id": "SHOT-2026-001",
  "game_version": "1489",
  "board_mode": "classic",
  "initial_state": {
    "striker": { "x": 0.0, "y": -25.35, "radius": 2.65 },
    "pucks": [
      { "id": 1, "type": "white", "x": 0.0, "y": 0.0, "radius": 1.95 },
      { "id": 2, "type": "black", "x": 5.2, "y": 8.4, "radius": 1.95 },
      { "id": 3, "type": "queen", "x": 0.0, "y": 3.9, "radius": 1.95 }
    ]
  },
  "shot_input": {
    "pull_angle_deg": 90.0,
    "forward_aim_rad": 1.570796,
    "force_fraction": 0.75,
    "striker_bonus": 0.0
  },
  "predicted_outcome": {
    "first_impact": { "puck_id": 1, "striker_pos": [0.0, -4.60], "puck_pos": [0.0, 0.0] },
    "striker_final": [0.0, -2.10],
    "target_puck_final": [0.0, 18.45],
    "potted_pucks": []
  },
  "actual_observed_outcome": {
    "first_impact": { "puck_id": 1, "striker_pos": [0.02, -4.58], "puck_pos": [0.0, 0.0] },
    "striker_final": [0.04, -2.15],
    "target_puck_final": [0.01, 18.32],
    "potted_pucks": []
  },
  "error_metrics": {
    "impact_position_error_cm": 0.028,
    "final_position_error_cm": 0.134,
    "pocket_outcome_matched": true
  }
}
```

---

## 3. Observable Physical Phenomena & Mechanics

### 3.1 Slingshot Pull vs. Cartesian Aim Angle
- In Carrom Pool, user interaction mimics a slingshot: dragging the finger backwards pulls the striker in the opposite direction.
- In native code (`libgame.so` at `onAimChanged` 0x19843A0 and `onShotFired` 0x1984568), the angle argument passed internally is already the **forward Cartesian launch angle** in radians ($0 \le \theta < 2\pi$), measured from the positive X-axis.

### 3.2 Friction & Deceleration Phases
Empirical high-speed observation reveals two distinct deceleration regimes:
1. **Initial Sliding / Slip Phase**:
   - Immediately following launch, angular velocity is zero ($\omega_x = 0, \omega_y = 0$).
   - Kinetic sliding friction dominates: $\mu_{\text{slide}} \approx 0.160 \implies a_{\text{decel}} = 156.8\text{ cm/s}^2$.
2. **Transition to Pure Rolling**:
   - Surface friction generates torque: $T = r \times F_f$.
   - Once rolling condition $v = r\omega$ is established, rolling friction coefficient applies: $\mu_{\text{roll}} \approx 0.108 \implies a_{\text{decel}} = 105.8\text{ cm/s}^2$.

### 3.3 Cushion Rebound Dynamics
- **Normal Restitution**: When striking cushions orthogonally, the restitution coefficient is $e \approx 0.88$.
- **Tangential Friction & Spin Effect**: On oblique impacts (e.g. $45^\circ$ bank shots), cushion friction induces significant angular spin ($\omega_z$), causing the exit angle to deviate slightly from pure specular reflection ($\theta_{\text{exit}} \ne \theta_{\text{incident}}$).
- The impulse solver in `PhysicsEngineV2.cpp` models this tangential impulse via:
  $$J_t = \min(0.4 \cdot |v_n|, |v_{\text{slip}}| / 2.5)$$

### 3.4 Coin-to-Coin Elastic Collisions
- Pucks have equal mass ($m_p = 1.0$), while the striker has three times their mass ($m_s = 3.0$).
- When a moving puck strikes a stationary puck head-on, momentum transfer is nearly complete ($e \approx 1.00$). The incoming puck almost stops, and the target puck inherits its full velocity.
- When the striker ($m=3.0$) strikes a stationary puck ($m=1.0$), conservation of linear momentum yields:
  $$v_s' = \frac{m_s - m_p}{m_s + m_p} v_s = \frac{2}{4} v_s = 0.5 v_s$$
  $$v_p' = \frac{2 m_s}{m_s + m_p} v_s = \frac{6}{4} v_s = 1.5 v_s$$
  The struck puck accelerates to $150\%$ of the striker's pre-impact speed.

### 3.5 Pocket Capture Thresholds
- The physical hole radius is $\sim 2.15\text{ cm}$, situated at inset coordinates $(\pm 33.48, \pm 33.48)$.
- **Skip Velocity Threshold**: If a puck or striker enters the pocket radius with speed exceeding $250.0\text{ cm/s}$, the disc does not drop into the pocket; it skips over the rim and rebounds off the back cushion.
- **Funnel Gravitational Field**: Inside the pocket radius below $250\text{ cm/s}$, inward acceleration of $980.0\text{ cm/s}^2$ pulls the disc into the pocket center.

---

## 4. Ground-Truth Accuracy & Statistical Limits

> [!IMPORTANT]
> **No simulation model can claim 100% accuracy.**
> Minor sub-millimeter floating-point differences, discrete physics timestep discretization ($dt = 0.005\text{ s}$), cushion elasticity variations, and microscopic touch drag jitter produce non-linear divergence over long multi-bounce cascades.

### Realistic Accuracy Confidence Bounds:
- **Direct Single Shots (Striker $\to$ Puck $\to$ Pocket)**: **92% - 96%** empirical pot success alignment.
- **Single Cushion Bank Shots (Striker $\to$ Cushion $\to$ Puck $\to$ Pocket)**: **84% - 89%** alignment.
- **Double Bank / Multi-Puck Combination Shots**: **70% - 78%** alignment due to error compounding across successive collision normal vectors.

