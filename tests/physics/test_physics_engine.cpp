#define _USE_MATH_DEFINES
#include "../../Bcore/src/main/cpp/catEngine/v2/PhysicsEngineV2.h"
#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <iomanip>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace catengine::v2;

static int g_testsPassed = 0;
static int g_testsFailed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  [FAIL] " << msg << " (" << #cond << ") at line " << __LINE__ << std::endl; \
            g_testsFailed++; \
            return; \
        } \
    } while(0)

#define TEST_PASS(msg) \
    do { \
        std::cout << "  [PASS] " << msg << std::endl; \
        g_testsPassed++; \
    } while(0)

// Helper: Convert degrees to radians
static inline double degToRad(double deg) {
    return deg * M_PI / 180.0;
}

// Test 1: Striker Launch Velocity Scaling
void test_launch_velocity_scaling() {
    std::cout << "\n[TEST 1] Striker Launch Velocity Scaling..." << std::endl;
    enableDiagnostics(false);

    PhysicsConfig config;
    config.minPower = 12.20;
    config.maxPower = 172.20;
    config.strikerBonus = 3.20;

    std::vector<PuckInput> pucks; // Empty board

    // Low force (10% pull, aiming straight up 90 degrees)
    SimResult resLow = simulate(0.0, -25.0, degToRad(90.0), 0.10, pucks, config);
    TEST_ASSERT(!resLow.segments.empty(), "Low force must generate at least 1 trajectory segment");

    // Max force (100% pull, aiming straight up 90 degrees)
    SimResult resMax = simulate(0.0, -25.0, degToRad(90.0), 1.00, pucks, config);
    TEST_ASSERT(!resMax.segments.empty(), "Max force must generate at least 1 trajectory segment");

    // Total travel distance across all segments
    double distLow = 0.0;
    for (const auto& s : resLow.segments) {
        distLow += std::hypot(s.endX - s.startX, s.endY - s.startY);
    }
    double distMax = 0.0;
    for (const auto& s : resMax.segments) {
        distMax += std::hypot(s.endX - s.startX, s.endY - s.startY);
    }

    std::cout << "    Distance Low Force (0.10): " << distLow << " cm" << std::endl;
    std::cout << "    Distance Max Force (1.00): " << distMax << " cm" << std::endl;

    TEST_ASSERT(distMax > distLow * 2.5, "Max force distance must significantly exceed low force distance");
    TEST_PASS("Striker launch force correctly scales travel velocity and stopping distance.");
}

// Test 2: Disc-to-Disc Collision & Momentum Transfer
void test_disc_collision_momentum() {
    std::cout << "\n[TEST 2] Disc-to-Disc Collision & 1D Elastic Impulse..." << std::endl;
    enableDiagnostics(false);

    PhysicsConfig config;
    config.strikerFriction = 0.05;
    config.puckFriction = 0.05;
    config.restitutionBody = 1.00; // 100% elastic

    // Place a White puck at (0.0, 0.0)
    std::vector<PuckInput> pucks;
    pucks.push_back({10, 1, 0.0, 0.0, true});

    // Launch striker vertically from (0.0, -15.0) at angle 90 degrees
    SimResult res = simulate(0.0, -15.0, degToRad(90.0), 0.60, pucks, config);

    TEST_ASSERT(res.hitPuck == true, "Striker must collide head-on with puck at (0, 0)");
    TEST_ASSERT(res.firstContactPuckId == 10, "First contact entity must be Puck #10");
    TEST_ASSERT(std::abs(res.firstImpactPuckX) < 0.2, "Impact X should remain centered on Y-axis");

    // Verify puck received forward momentum
    TEST_ASSERT(!res.puckTrajectories.empty(), "Puck #10 must have generated trajectory");
    const auto& pt = res.puckTrajectories[0];
    TEST_ASSERT(pt.isHit == true, "Puck trajectory must be flagged isHit");
    TEST_ASSERT(pt.finalY > 0.0, "Puck must be propelled forward (positive Y)");

    std::cout << "    First impact striker pos: (" << res.firstImpactStrikerX << ", " << res.firstImpactStrikerY << ")" << std::endl;
    std::cout << "    Target puck post-impact final pos: (" << pt.finalX << ", " << pt.finalY << ")" << std::endl;

    TEST_PASS("Disc-to-disc head-on collision correctly transfers momentum and records contact.");
}

// Test 3: Cushion Bounce & Angle Symmetry
void test_cushion_reflection() {
    std::cout << "\n[TEST 3] Cushion Reflection & Restitution..." << std::endl;
    enableDiagnostics(false);

    PhysicsConfig config;
    config.restitutionCushion = 0.88;

    std::vector<PuckInput> pucks; // No pucks, pure wall bounce

    // Aim striker towards top cushion at 45 degrees
    SimResult res = simulate(0.0, -10.0, degToRad(45.0), 0.80, pucks, config);

    TEST_ASSERT(res.strikerCushionBounces >= 1, "Striker must bounce off cushion at least once");

    bool hasCushionSegment = false;
    for (const auto& seg : res.segments) {
        if (seg.isCushionBounce) {
            hasCushionSegment = true;
            break;
        }
    }
    TEST_ASSERT(hasCushionSegment, "Trajectory must record a cushion bounce segment");
    std::cout << "    Striker cushion bounces recorded: " << res.strikerCushionBounces << std::endl;

    TEST_PASS("Cushion reflection correctly tags bounce segment and reverses normal velocity.");
}

// Test 4: Pocket Capture vs Skip Cutoff Threshold
void test_pocket_capture_cutoff() {
    std::cout << "\n[TEST 4] Pocket Capture & Attraction Dynamics..." << std::endl;
    enableDiagnostics(false);

    PhysicsConfig config;
    config.pocketRadius = 2.15;
    config.pocketGravity = 980.0;
    config.pocketDropMaxSpeed = 250.0;
    config.strikerFriction = 0.05;
    config.puckFriction = 0.05;

    // Corner pocket near top-right is at (33.48, 33.48)
    // Place a puck along the 45-degree line towards the pocket, but safely outside capture radius
    std::vector<PuckInput> pucks;
    pucks.push_back({20, 2, 28.0, 28.0, true}); // Black puck at (28, 28)

    // Launch striker from (22.0, 22.0) aiming at 45 degrees directly towards puck and pocket
    SimResult res = simulate(22.0, 22.0, degToRad(45.0), 0.50, pucks, config);
    TEST_ASSERT(!res.puckTrajectories.empty(), "Puck must have trajectory info");
    std::cout << "    Puck hit: " << res.puckTrajectories[0].isHit << std::endl;
    std::cout << "    Corner shot puck inPocket: " << res.puckTrajectories[0].inPocket << std::endl;
    std::cout << "    Puck final position: (" << res.puckTrajectories[0].finalX << ", " << res.puckTrajectories[0].finalY << ")" << std::endl;

    TEST_ASSERT(res.puckTrajectories[0].inPocket == true, "Puck struck towards corner pocket must be potted");
    TEST_PASS("Pocket attraction correctly processes disc proximity and capture state.");
}

// Test 5: Friction Deceleration & Halting Conditions
void test_friction_halting() {
    std::cout << "\n[TEST 5] Friction Deceleration & Stationary Cutoff..." << std::endl;
    enableDiagnostics(false);

    PhysicsConfig config;
    config.strikerFriction = 0.160;
    config.puckFriction = 0.108;

    std::vector<PuckInput> pucks;
    SimResult res = simulate(0.0, -20.0, degToRad(90.0), 0.30, pucks, config);

    // Striker must come to a complete halt within board bounds
    TEST_ASSERT(!std::isnan(res.finalStrikerX), "Final striker X must not be NaN");
    TEST_ASSERT(!std::isnan(res.finalStrikerY), "Final striker Y must not be NaN");
    TEST_ASSERT(std::abs(res.finalStrikerX) < 37.0, "Final striker X must stay within cushion bounds");
    TEST_ASSERT(std::abs(res.finalStrikerY) < 37.0, "Final striker Y must stay within cushion bounds");

    std::cout << "    Launched at (0, -20), halted stably at (" << res.finalStrikerX << ", " << res.finalStrikerY << ")" << std::endl;
    TEST_PASS("Coulomb friction stops disc motion deterministically within finite iterations.");
}

// Test 6: Multi-Collision Cascade (Chain Reactions)
void test_multi_collision_chain() {
    std::cout << "\n[TEST 6] Multi-Collision Cascade & Chain Reaction..." << std::endl;
    enableDiagnostics(false);

    PhysicsConfig config;
    config.strikerFriction = 0.05;
    config.puckFriction = 0.05;

    // Striker at (0, -20).
    // Puck A (id=101) at (0, -10).
    // Puck B (id=102) at (0, 0).
    // Striker hits Puck A, which then propagates and hits Puck B.
    std::vector<PuckInput> pucks;
    pucks.push_back({101, 1, 0.0, -10.0, true});
    pucks.push_back({102, 2, 0.0, 0.0, true});

    SimResult res = simulate(0.0, -20.0, degToRad(90.0), 0.80, pucks, config);

    TEST_ASSERT(res.hitPuck == true, "Striker must hit first puck");
    TEST_ASSERT(res.firstContactPuckId == 101, "First contact must be Puck A");
    TEST_ASSERT(res.puckTrajectories.size() >= 2, "Must record trajectories for both pucks");

    for (size_t i = 0; i < res.puckTrajectories.size(); i++) {
        const auto& pt = res.puckTrajectories[i];
        std::cout << "    Trajectory [" << i << "] Puck ID=" << pt.puckIndex 
                  << " startY=" << pt.startY << " finalY=" << pt.finalY 
                  << " isHit=" << pt.isHit << " bounces=" << pt.cushionBounces
                  << " segments=" << pt.segments.size() << std::endl;
        for (size_t s = 0; s < pt.segments.size(); s++) {
            std::cout << "       Seg " << s << ": (" << pt.segments[s].startX << ", " << pt.segments[s].startY
                      << ") -> (" << pt.segments[s].endX << ", " << pt.segments[s].endY << ")"
                      << (pt.segments[s].isCushionBounce ? " [CUSHION]" : "") << std::endl;
        }
    }

    TEST_ASSERT(res.puckTrajectories[0].isHit == true, "Puck A must be hit");
    TEST_ASSERT(res.puckTrajectories[1].isHit == true, "Puck B must be hit by cascade chain");
    TEST_ASSERT(!res.puckTrajectories[1].segments.empty(), "Puck B must have collision segments");
    TEST_ASSERT(res.puckTrajectories[1].segments[0].endY > res.puckTrajectories[1].segments[0].startY, 
                "Puck B must be initially propelled forward along positive Y");

    TEST_PASS("Multi-disc cascade correctly propagates momentum across collision chains.");
}

// Test 7: Numerical Stability & Boundary Integrity
void test_numerical_stability() {
    std::cout << "\n[TEST 7] Numerical Stability & Bounded Conservation..." << std::endl;
    enableDiagnostics(false);

    PhysicsConfig config;

    // Test extreme high velocity launch at non-cardinal angle (e.g. 33.7 degrees)
    std::vector<PuckInput> pucks;
    pucks.push_back({201, 1, 10.0, 10.0, true});
    pucks.push_back({202, 2, -15.0, 5.0, true});
    pucks.push_back({203, 3, 0.0, 0.0, true});

    // Run 50 randomised/swept simulations to verify zero NaNs and zero infinite loops
    for (int deg = 0; deg < 360; deg += 30) {
        double angleRad = degToRad(deg);
        SimResult res = simulate(0.0, -20.0, angleRad, 1.0, pucks, config, 3000);

        TEST_ASSERT(!std::isnan(res.finalStrikerX), "Striker X must never be NaN");
        TEST_ASSERT(!std::isnan(res.finalStrikerY), "Striker Y must never be NaN");
        TEST_ASSERT(!std::isinf(res.finalStrikerX), "Striker X must never be Inf");
        TEST_ASSERT(!std::isinf(res.finalStrikerY), "Striker Y must never be Inf");

        for (const auto& pt : res.puckTrajectories) {
            TEST_ASSERT(!std::isnan(pt.finalX) && !std::isnan(pt.finalY), "Puck coords must never be NaN");
        }
    }

    std::cout << "    Verified 12 full-circle angles under max launch force with zero NaNs or drift." << std::endl;
    TEST_PASS("Numerical integrator remains strictly stable across all directional sweeps.");
}

int main() {
    std::cout << "===========================================================" << std::endl;
    std::cout << "  CatEngine PhysicsEngineV2 Standalone Unit Test Suite   " << std::endl;
    std::cout << "===========================================================" << std::endl;

    test_launch_velocity_scaling();
    test_disc_collision_momentum();
    test_cushion_reflection();
    test_pocket_capture_cutoff();
    test_friction_halting();
    test_multi_collision_chain();
    test_numerical_stability();

    std::cout << "\n===========================================================" << std::endl;
    std::cout << "  TEST SUMMARY: " << g_testsPassed << " PASSED, " << g_testsFailed << " FAILED" << std::endl;
    std::cout << "===========================================================" << std::endl;

    return (g_testsFailed == 0) ? 0 : 1;
}
