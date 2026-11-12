#pragma once

#include <vector>
#include <cmath>
#include <cstdint>
#include <string>

namespace catengine {
namespace v2 {

struct TrajectorySegment {
    float startX = 0.0f;
    float startY = 0.0f;
    float endX = 0.0f;
    float endY = 0.0f;
    bool isCushionBounce = false;
    bool isPuckHit = false;
};

struct PuckTrajectoryInfo {
    int puckIndex = -1;
    int puckType = 1; // 1 = White, 2 = Black, 3 = Red (Queen)
    float startX = 0.0f;
    float startY = 0.0f;
    float finalX = 0.0f;
    float finalY = 0.0f;
    bool inPocket = false;
    int pocketIndex = -1;
    float pocketCenterX = 0.0f;
    float pocketCenterY = 0.0f;
    std::vector<TrajectorySegment> segments;
    bool hasDeflection = false;
    TrajectorySegment deflectSegment;
    bool isHit = false;
    int hitByEntityId = -1; // -1 = None, 0 = Striker, >0 = Puck ID
    float hitSpeed = 0.0f;
    int cushionBounces = 0;
};

struct SimResult {
    std::vector<TrajectorySegment> segments;
    float finalStrikerX = 0.0f;
    float finalStrikerY = 0.0f;
    bool hasFirstContact = false;
    float firstImpactStrikerX = 0.0f;
    float firstImpactStrikerY = 0.0f;
    float firstImpactPuckX = 0.0f;
    float firstImpactPuckY = 0.0f;
    int firstContactPuckId = -1;
    bool hitPuck = false;
    bool strikerInPocket = false;
    int strikerPocketIndex = -1;
    int strikerCushionBounces = 0;
    std::vector<PuckTrajectoryInfo> puckTrajectories;
};

struct Point2D {
    double x = 0.0;
    double y = 0.0;
};

struct PuckInput {
    int id = -1;
    int puckType = 1;
    double x = 0.0;
    double y = 0.0;
    bool active = false;
};

struct PhysicsConfig {
    double strikerRadius = 2.65;
    double puckRadius = 1.95;
    double pocketInset = 33.48;
    double pocketRadius = 2.15;
    double minPower = 12.20;
    double maxPower = 172.20;
    double powerCurve = 1.0;
    double strikerBonus = 3.20;
    double powerMultiplier = 1.0;

    // Physical friction (Coulomb boric acid powder on polished wood)
    // Carrom Pool libgame.so telemetry-calibrated:
    // Striker (larger sliding surface): mu = 0.1600 (a_decel = 156.8 cm/s^2 matching actual travel distance)
    double strikerFriction = 0.1600; // mu for striker - matches Carrom Pool runtime deceleration
    double puckFriction = 0.1080;    // mu for pucks - matches Carrom Pool runtime (libgame.so PuckLogic+0x80)

    // Restitution & Contact Friction
    double restitutionBody = 1.00;        // Inter-disc restitution (Authentic Carrom Pool libgame.so: 1.00 pure elastic)
    double restitutionCushion = 0.88;     // Cushion rebound restitution (Authentic Carrom Pool libgame.so: 0.88)
    // Striker mass: Authentic Carrom Pool libgame.so mass ratio is 3.00 : 1.00
    double strikerMass = 3.00;            // Authentic carrom striker mass ratio (3.00 : 1.00)
    double puckMass = 1.00;
    double cutThrowFactor = 0.020;        // Oblique cut throw deflection factor (libgame.so 0x199885c)
    // NOTE: contactFriction and cushionTangentFriction removed — they were UNUSED in simulate().
    // Tangential friction is handled inline by the spin impulse solver: min(0.4*|vn|, |v_slip|/2.5)

    // Pocket Mechanics (Authentic libgame.so + 0x1B2CB14)
    double pocketDropMaxSpeed = 250.0;    // Speed cutoff: above this, puck skips/bounces past pocket hole (cm/s)
    double pocketGravity = 980.0;         // Inward gravitational funnel suction acceleration (cm/s^2)

    // Table boundaries
    double boardHalfSize = 37.00;         // Half width of board (cm)
    double pocketJawCutoff = 32.80;       // Extent of flat cushion (cm)

    // Authentic Cushion Polygon Vertices (from libgame.so TableLogic + 0x198)
    std::vector<Point2D> cushionVertices;
};

// Continuous Collision Detection (CCD) Physics Simulation
SimResult simulate(double startX, double startY, double shotAngle, double forceVal,
                   const std::vector<PuckInput>& pucks,
                   const PhysicsConfig& config,
                   int maxSteps = 2500);

void enableDiagnostics(bool enable);
bool isDiagnosticsEnabled();
void logDiagnosticReport(const SimResult& sim, double startX, double startY, double angle, double force);

} // namespace v2
} // namespace catengine
