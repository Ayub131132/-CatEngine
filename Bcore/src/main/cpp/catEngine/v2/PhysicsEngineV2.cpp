#define _USE_MATH_DEFINES
#include "PhysicsEngineV2.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include <chrono>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef __ANDROID__
#include <android/log.h>
#ifndef LOG_TAG
#define LOG_TAG "CatEngine-V2"
#endif
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#include <cstdio>
#define ALOGI(...) do { printf("[INFO] " __VA_ARGS__); printf("\n"); } while(0)
#define ALOGW(...) do { printf("[WARN] " __VA_ARGS__); printf("\n"); } while(0)
#define ALOGE(...) do { fprintf(stderr, "[ERROR] " __VA_ARGS__); fprintf(stderr, "\n"); } while(0)
#endif

namespace catengine {
namespace v2 {

static bool s_diagnosticsEnabled = true;

void enableDiagnostics(bool enable) {
    s_diagnosticsEnabled = enable;
}

bool isDiagnosticsEnabled() {
    return s_diagnosticsEnabled;
}

struct BodyInternal {
    int id = -1;
    int puckType = 0; // 0 = Striker, 1 = White, 2 = Black, 3 = Queen
    double px = 0.0;
    double py = 0.0;
    double vx = 0.0;
    double vy = 0.0;
    double wx = 0.0; // Surface rolling velocity X: omega_y * R (cm/s)
    double wy = 0.0; // Surface rolling velocity Y: -omega_x * R (cm/s)
    double wz = 0.0; // Authentic vertical angular velocity / spin omega_z (rad/s, libgame.so + 0x70)
    double r_omega = 0.0; // Surface rolling speed: R * Omega (cm/s)
    double distTraveled = 0.0; // Total cumulative distance traveled from launch (cm)
    double mass = 1.0;
    double radius = 1.95;
    int chainLevel = -1; // 0 = striker, 1 = primary hit puck, 2 = secondary cascade
    double initX = 0.0;
    double initY = 0.0;
    double segStartX = 0.0;
    double segStartY = 0.0;
    bool inPocket = false;
    bool inPocketDrop = false; // Authentic pocket sinking transition (libgame.so state == 2, 0x1997e8c)
    double pocketDropTime = 0.0; // Sinking time accumulator (libgame.so +0xb8)
    double lastPocketDistSq = 999.0;
    int pocketIndex = -1;
    double pocketCenterX = 0.0;
    double pocketCenterY = 0.0;
    double lastSegAngle = 0.0;
    int hitByEntityId = -1; // -1 = None, 0 = Striker, >0 = Puck ID
    double hitSpeed = 0.0;
    int cushionBounces = 0;
    std::vector<TrajectorySegment> segments;
};

static inline bool resolveDiscDiscContact(BodyInternal& bi, BodyInternal& bj,
                                          const PhysicsConfig& config,
                                          SimResult& result,
                                          bool recordSegments) {
    if (bi.inPocket || bi.inPocketDrop || bj.inPocket || bj.inPocketDrop) return false;

    double cdx = bi.px - bj.px;
    double cdy = bi.py - bj.py;
    double dist = std::hypot(cdx, cdy);
    double minDist = bi.radius + bj.radius;
    if (dist < 1e-6) {
        cdx = 1.0; cdy = 0.0; dist = 1e-6;
    }

    double nx = cdx / dist; // Unit normal pointing from j to i
    double ny = cdy / dist;

    // 1. Overlap separation by exact inverse mass ratio (libgame.so 0x1998840)
    double delta = minDist - dist;
    if (delta > 0.0) {
        double shift = delta / dist;
        double m1 = bi.mass;
        double m2 = bj.mass;
        double mSum = m1 + m2;
        bi.px += cdx * shift * (m2 / mSum);
        bi.py += cdy * shift * (m2 / mSum);
        bj.px -= cdx * shift * (m1 / mSum);
        bj.py -= cdy * shift * (m1 / mSum);
    }

    // 2. Relative normal velocity (bi relative to bj):
    // v1n = bi.v . n, v2n = bj.v . n
    double v1n = bi.vx * nx + bi.vy * ny;
    double v2n = bj.vx * nx + bj.vy * ny;
    double relNorm = v1n - v2n;

    // If bodies are separating along the contact normal, do not apply attractive impulse
    if (relNorm >= -1e-6) {
        return false;
    }

    // 3. Orthonormal tangent vector t = (-ny, nx)
    double tx = -ny;
    double ty = nx;
    double v1t = bi.vx * tx + bi.vy * ty;
    double v2t = bj.vx * tx + bj.vy * ty;

    // 4. Elastic Normal Momentum Exchange
    double m1 = bi.mass;
    double m2 = bj.mass;
    double mSum = m1 + m2;
    double mEffNorm = (m1 * m2) / mSum;
    double e = (config.restitutionBody > 0.0 && config.restitutionBody <= 1.0) ? config.restitutionBody : 1.00;

    // Total normal impulse magnitude Jn > 0 (acting on bi along +n, bj along -n)
    double Jn = (1.0 + e) * mEffNorm * (-relNorm);

    // 5. Tangential Contact Slip & Friction Impulse (Cut-Throw)
    // Contact point on bi is at -bi.radius * n; on bj is at +bj.radius * n.
    // Tangential contact velocities:
    // v_ci,t = v1t - bi.wz * bi.radius
    // v_cj,t = v2t + bj.wz * bj.radius
    double v_slip = (v1t - v2t) - (bi.wz * bi.radius + bj.wz * bj.radius);

    // Rotational inertia factor: R^2 / I = 2.5 / m (libgame.so 0x1998a8c rim-weighted disc)
    // Tangential effective mass: mEffTang = mEffNorm / 3.5
    double mEffTang = mEffNorm / 3.5;
    double Jt_stick = -mEffTang * v_slip;

    // Coulomb friction impulse limit: Jt_max = mu_contact * Jn
    double muContact = (config.cutThrowFactor > 0.0) ? config.cutThrowFactor : 0.040;
    double Jt_max = muContact * Jn;
    double Jt = std::clamp(Jt_stick, -Jt_max, Jt_max);

    // 6. Apply equal and opposite impulses to both bodies (Newton's 3rd Law & Conservation of Momentum)
    // Normal impulse: +Jn along n on bi, -Jn along n on bj
    // Tangential impulse: +Jt along t on bi, -Jt along t on bj
    bi.vx += (Jn * nx + Jt * tx) / m1;
    bi.vy += (Jn * ny + Jt * ty) / m1;
    bi.wz -= (2.5 * Jt) / (m1 * bi.radius);

    bj.vx -= (Jn * nx + Jt * tx) / m2;
    bj.vy -= (Jn * ny + Jt * ty) / m2;
    bj.wz -= (2.5 * Jt) / (m2 * bj.radius);

    bi.r_omega = std::hypot(bi.vx, bi.vy);
    bj.r_omega = std::hypot(bj.vx, bj.vy);

    bi.lastSegAngle = std::atan2(bi.vy, bi.vx);
    bj.lastSegAngle = std::atan2(bj.vy, bj.vx);

    double spd1 = std::hypot(bi.vx, bi.vy);
    double spd2 = std::hypot(bj.vx, bj.vy);

    // 7. Track impact speed and causality
    if (bi.chainLevel >= 0 && bj.chainLevel == -1) {
        bj.hitByEntityId = (bi.id == -1) ? 0 : bi.id;
        bj.hitSpeed = spd1;
    } else if (bj.chainLevel >= 0 && bi.chainLevel == -1) {
        bi.hitByEntityId = (bj.id == -1) ? 0 : bj.id;
        bi.hitSpeed = spd2;
    } else {
        if (spd1 >= spd2) {
            if (bj.hitByEntityId == -1) {
                bj.hitByEntityId = (bi.id == -1) ? 0 : bi.id;
                bj.hitSpeed = spd1;
            }
        } else {
            if (bi.hitByEntityId == -1) {
                bi.hitByEntityId = (bj.id == -1) ? 0 : bj.id;
                bi.hitSpeed = spd2;
            }
        }
    }

    // 8. Chain level assignment
    if (bi.chainLevel >= 0 && bj.chainLevel == -1) {
        bj.chainLevel = bi.chainLevel + 1;
        if (bj.segments.empty()) {
            bj.segStartX = bj.px;
            bj.segStartY = bj.py;
        }
    } else if (bj.chainLevel >= 0 && bi.chainLevel == -1) {
        bi.chainLevel = bj.chainLevel + 1;
        if (bi.segments.empty()) {
            bi.segStartX = bi.px;
            bi.segStartY = bi.py;
        }
    }

    // 9. Striker first impact record
    if (bi.id == -1 && !result.hasFirstContact) {
        result.hasFirstContact = true;
        result.firstImpactStrikerX = static_cast<float>(bi.px);
        result.firstImpactStrikerY = static_cast<float>(bi.py);
        result.firstImpactPuckX    = static_cast<float>(bj.px);
        result.firstImpactPuckY    = static_cast<float>(bj.py);
        result.firstContactPuckId  = bj.id;

        if (recordSegments) {
            TrajectorySegment seg;
            seg.startX = static_cast<float>(bi.segStartX);
            seg.startY = static_cast<float>(bi.segStartY);
            seg.endX   = static_cast<float>(bi.px);
            seg.endY   = static_cast<float>(bi.py);
            seg.isCushionBounce = (bi.segments.size() > 0);
            seg.isPuckHit = true;
            bi.segments.push_back(seg);
            bi.segStartX = bi.px;
            bi.segStartY = bi.py;
        }
    } else if (recordSegments) {
        if (bi.id == -1 || bi.chainLevel >= 1) {
            double distI = std::hypot(bi.px - bi.segStartX, bi.py - bi.segStartY);
            if (distI > 0.10) {
                TrajectorySegment seg;
                seg.startX = static_cast<float>(bi.segStartX);
                seg.startY = static_cast<float>(bi.segStartY);
                seg.endX   = static_cast<float>(bi.px);
                seg.endY   = static_cast<float>(bi.py);
                seg.isCushionBounce = false;
                seg.isPuckHit = true;
                bi.segments.push_back(seg);
                bi.segStartX = bi.px;
                bi.segStartY = bi.py;
            }
        }
        if (bj.id != -1 && bj.chainLevel >= 1) {
            double distJ = std::hypot(bj.px - bj.segStartX, bj.py - bj.segStartY);
            if (distJ > 0.10) {
                TrajectorySegment seg;
                seg.startX = static_cast<float>(bj.segStartX);
                seg.startY = static_cast<float>(bj.segStartY);
                seg.endX   = static_cast<float>(bj.px);
                seg.endY   = static_cast<float>(bj.py);
                seg.isCushionBounce = false;
                seg.isPuckHit = true;
                bj.segments.push_back(seg);
                bj.segStartX = bj.px;
                bj.segStartY = bj.py;
            }
        }
    }

    return true;
}

static inline void resolveCushionBounce(BodyInternal& b, double thetaCushion,
                                        const PhysicsConfig& config,
                                        bool recordSegments) {
    if (b.inPocket || b.inPocketDrop) return;

    double cosTh = std::cos(thetaCushion);
    double sinTh = std::sin(thetaCushion);

    double vt = b.vx * cosTh - b.vy * sinTh;
    double vn = b.vx * sinTh + b.vy * cosTh;

    // Contact point tangential slip: v_slip = vt - wz * R (libgame.so 0x1998b90 FMSUB)
    double v_slip = vt - b.wz * b.radius;
    double sgnSlip = (v_slip > 0.0) ? 1.0 : -1.0;
    double Jt = std::min(0.40 * std::abs(vn), std::abs(v_slip) / 2.5);
    double delta_vt = -sgnSlip * Jt;

    // Authentic TableLogic + 0x1b0 table cushion damping (libgame.so 0x1998bc4 & 0x1998bcc)
    double e_body = (config.restitutionBody > 0.0) ? config.restitutionBody : 1.00;
    double vn_prime = -vn * e_body * config.restitutionCushion;
    double vt_prime = vt + delta_vt;
    b.wz -= (2.5 * delta_vt) / b.radius;

    // Rotated velocities back into world coordinates (libgame.so 0x1998c10 & 0x1998c14)
    b.vx = cosTh * vt_prime + sinTh * vn_prime;
    b.vy = cosTh * vn_prime - sinTh * vt_prime;

    b.r_omega = std::hypot(b.vx, b.vy);
    b.lastSegAngle = std::atan2(b.vy, b.vx);
    b.cushionBounces++;

    if (recordSegments) {
        double dCush = std::hypot(b.px - b.segStartX, b.py - b.segStartY);
        if (dCush > 0.12) {
            TrajectorySegment seg;
            seg.startX = static_cast<float>(b.segStartX);
            seg.startY = static_cast<float>(b.segStartY);
            seg.endX   = static_cast<float>(b.px);
            seg.endY   = static_cast<float>(b.py);
            seg.isCushionBounce = true;
            seg.isPuckHit = false;
            b.segments.push_back(seg);
            b.segStartX = b.px;
            b.segStartY = b.py;
        }
    }
}

static inline bool isPuckAlreadyPotted(double x, double y, double inset, double captureRad) {
    if (std::isnan(x) || std::isnan(y)) return true;
    double ax = std::abs(x);
    double ay = std::abs(y);
    if (ax > 38.0 || ay > 38.0) return true;
    if (ax > 29.0 && ay > 29.0) {
        double dx = ax - inset;
        double dy = ay - inset;
        if (std::hypot(dx, dy) < captureRad) return true;
    }
    return false;
}

SimResult simulate(double startX, double startY, double shotAngle, double forceVal,
                   const std::vector<PuckInput>& pucks,
                   const PhysicsConfig& config,
                   int maxSteps) {
    SimResult result;

    // 1. Initial striker launch calculation (libgame.so 0x1B2C3A4)
    // Pure linear interpolation matching authentic TableLogic::launchStriker:
    // v0 = minPower + f * (maxPower - minPower)
    // When f == 1.0 (full pull), striker power bonus is applied if available.
    double f = std::clamp(forceVal, 0.0, 1.0);
    double minP = config.minPower;
    double maxP = config.maxPower;

    double v0 = minP + f * (maxP - minP);
    if (f >= 0.999 && config.strikerBonus > 0.0) {
        v0 += config.strikerBonus;
    }
    v0 *= config.powerMultiplier;

    // 2. Setup rigid bodies
    std::vector<BodyInternal> bodies;
    bodies.reserve(pucks.size() + 1);

    // Striker (Body 0)
    // Note: libgame.so (0x1B2C474-0x1B2C484) initializes striker angular velocities
    // to zero (wx = 0, wy = 0, wz = 0) at launch. The striker starts in pure sliding/slip,
    // which triggers the authentic 196 cm/s^2 slip deceleration until rolling begins.
    BodyInternal striker;
    striker.id = -1;
    striker.puckType = 0;
    striker.px = startX;
    striker.py = startY;
    striker.vx = v0 * std::cos(shotAngle);
    striker.vy = v0 * std::sin(shotAngle);
    striker.mass = config.strikerMass;
    striker.radius = config.strikerRadius;
    striker.chainLevel = 0;
    striker.wz = 0.0;
    striker.wx = 0.0;
    striker.wy = 0.0;
    striker.initX = startX;
    striker.initY = startY;
    striker.segStartX = startX;
    striker.segStartY = startY;
    striker.lastSegAngle = shotAngle;
    bodies.push_back(striker);

    // Add table pucks
    for (const auto& p : pucks) {
        if (!p.active) continue;
        if (isPuckAlreadyPotted(p.x, p.y, config.pocketInset, config.pocketRadius)) continue;

        BodyInternal pb;
        pb.id = p.id;
        pb.puckType = p.puckType;
        pb.px = p.x;
        pb.py = p.y;
        pb.vx = 0.0;
        pb.vy = 0.0;
        pb.mass = config.puckMass;
        pb.radius = config.puckRadius;
        pb.chainLevel = -1;
        pb.wz = 0.0;
        pb.initX = p.x;
        pb.initY = p.y;
        pb.segStartX = p.x;
        pb.segStartY = p.y;
        pb.lastSegAngle = 0.0;
        bodies.push_back(pb);
    }

    // 4 Corner Pocket Centers
    const double pocketCenters[4][2] = {
        {  config.pocketInset,  config.pocketInset }, // Pocket 0: Top-Right
        { -config.pocketInset,  config.pocketInset }, // Pocket 1: Top-Left
        {  config.pocketInset, -config.pocketInset }, // Pocket 2: Bottom-Right
        { -config.pocketInset, -config.pocketInset }  // Pocket 3: Bottom-Left
    };

    // Setup authentic cushion segments and rounded jaw vertices
    struct CushionDef {
        Point2D A;
        Point2D B;
        double tangentAngle;
    };

    std::vector<CushionDef> cushions;
    std::vector<Point2D> jawVertices;

    if (config.cushionVertices.size() >= 3) {
        // Custom polygon from caller
        const size_t nv = config.cushionVertices.size();
        for (size_t k = 0; k < nv; k++) {
            const auto& pA = config.cushionVertices[k];
            const auto& pB = config.cushionVertices[(k + 1) % nv];
            double dx = pB.x - pA.x;
            double dy = pB.y - pA.y;
            cushions.push_back({ pA, pB, -std::atan2(dy, dx) });
            jawVertices.push_back(pA);
        }
    } else {
        // Default authentic Carrom Pool table: 4 flat cushions with open corner pockets
        const double h = config.boardHalfSize;       // 37.00 cm
        const double c = config.pocketJawCutoff;     // 32.80 cm
        cushions = {
            { { -c,  h }, {  c,  h }, 0.0 },              // Top cushion: normal points DOWN
            { {  h,  c }, {  h, -c }, -M_PI / 2.0 },      // Right cushion: normal points LEFT
            { {  c, -h }, { -c, -h }, M_PI },             // Bottom cushion: normal points UP
            { { -h, -c }, { -h,  c }, M_PI / 2.0 }        // Left cushion: normal points RIGHT
        };
        // 8 cushion jaw corners (endpoints of the 4 cushions)
        jawVertices = {
            { -c,  h }, {  c,  h },
            {  h,  c }, {  h, -c },
            {  c, -h }, { -c, -h },
            { -h, -c }, { -h,  c }
        };
    }

    const double dt = 0.001; // Authentic 1000 Hz integration step (libgame.so 0x12ea830)
    const double gDecel = 980.0;
    const double minSpeed = 0.15; // Stop moving when velocity falls below 0.15 cm/s

    // Main Simulation Loop
    for (int step = 0; step < maxSteps; step++) {
        bool anyMotion = false;
        for (const auto& b : bodies) {
            if (!b.inPocket && std::hypot(b.vx, b.vy) > minSpeed) {
                anyMotion = true;
                break;
            }
        }
        if (!anyMotion) break;

        double timeLeft = dt;
        int subStep = 0;
        const int maxSubSteps = 64;

        while (timeLeft > 1e-6 && subStep < maxSubSteps) {
            subStep++;

            double earliestT = timeLeft;
            int eventType = 0; // 0 = none, 1 = disc-disc contact, 2 = cushion bounce
            size_t eventI = 0, eventJ = 0;
            double eventCushionNormalAngle = 0.0;

            // -------------------------------------------------------------
            // A. Continuous Collision Detection (CCD): Disc vs Disc (libgame.so 0x19985e0)
            // -------------------------------------------------------------
            for (size_t i = 0; i < bodies.size(); i++) {
                const auto& bi = bodies[i];
                if (bi.inPocket || bi.inPocketDrop) continue;

                for (size_t j = i + 1; j < bodies.size(); j++) {
                    const auto& bj = bodies[j];
                    if (bj.inPocket || bj.inPocketDrop) continue;

                    if (std::hypot(bi.vx, bi.vy) <= minSpeed && std::hypot(bj.vx, bj.vy) <= minSpeed) {
                        continue;
                    }

                    double rSum = bi.radius + bj.radius;
                    double rx = bj.px - bi.px;
                    double ry = bj.py - bi.py;
                    double rSq = rx * rx + ry * ry;

                    // Relative velocity: v = V_j - V_i
                    double vx = bj.vx - bi.vx;
                    double vy = bj.vy - bi.vy;

                    double A = vx * vx + vy * vy;
                    if (A <= 1e-9) continue;

                    // Dot product: r . v (negative when approaching)
                    double dotRV = rx * vx + ry * vy;
                    double C = rSq - (rSum * rSum);

                    if (C <= 0.0) {
                        // Already touching or overlapping
                        if (dotRV < -1e-6) {
                            if (0.0 < earliestT) {
                                earliestT = 0.0;
                                eventType = 1;
                                eventI = i;
                                eventJ = j;
                            }
                        }
                    } else if (dotRV < -1e-6) {
                        // Approaching each other: quadratic A*t^2 + 2*dotRV*t + C = 0
                        double disc = dotRV * dotRV - A * C;
                        if (disc >= 0.0) {
                            double tHit = (-dotRV - std::sqrt(disc)) / A;
                            if (tHit >= 0.0 && tHit < earliestT) {
                                earliestT = tHit;
                                eventType = 1;
                                eventI = i;
                                eventJ = j;
                            }
                        }
                    }
                }
            }

            // -------------------------------------------------------------
            // B. Authentic Cushion Segment & Corner CCD (libgame.so 0x19984cc & 0x19985e0)
            // -------------------------------------------------------------
            for (size_t i = 0; i < bodies.size(); i++) {
                const auto& b = bodies[i];
                if (b.inPocket || b.inPocketDrop) continue;
                if (std::hypot(b.vx, b.vy) <= minSpeed) continue;

                // 1. Check straight cushion segments
                for (const auto& cush : cushions) {
                    const auto& A = cush.A;
                    const auto& B = cush.B;

                    double dx = B.x - A.x;
                    double dy = B.y - A.y;
                    double L = std::hypot(dx, dy);
                    if (L < 1e-6) continue;

                    double cross = dx * b.vy - dy * b.vx;
                    if (std::abs(cross) < 1e-9) continue;

                    double invL = 1.0 / L;
                    double nx = dy * invL;
                    double ny = -dx * invL;

                    double vNorm = nx * b.vx + ny * b.vy;
                    if (vNorm >= 0.0) continue;

                    double px_edge = b.px - nx * b.radius;
                    double py_edge = b.py - ny * b.radius;

                    double Dx = px_edge - A.x;
                    double Dy = py_edge - A.y;

                    double u = (Dx * b.vy - Dy * b.vx) / cross;
                    if (u >= 0.0 && u <= 1.0) {
                        double tHit = (dx * Dy - dy * Dx) / (-cross);
                        if (tHit >= 0.0 && tHit < earliestT) {
                            earliestT = tHit;
                            eventType = 2; // Segment bounce
                            eventI = i;
                            eventCushionNormalAngle = cush.tangentAngle;
                        }
                    }
                }

                // 2. Check rounded cushion jaw corners
                for (const auto& V : jawVertices) {
                    double rx = V.x - b.px;
                    double ry = V.y - b.py;
                    double B_corner = -2.0 * (rx * b.vx + ry * b.vy);
                    if (B_corner < 0.0) {
                        double A_corner = b.vx * b.vx + b.vy * b.vy;
                        double C_corner = rx * rx + ry * ry;
                        double rSq = b.radius * b.radius;
                        double dMinCornerSq = C_corner + (B_corner * B_corner) / (-4.0 * A_corner);
                        if (dMinCornerSq < rSq) {
                            double discCorner = B_corner * B_corner - 4.0 * A_corner * (C_corner - rSq);
                            if (discCorner >= 0.0) {
                                double tHitCorner = (-B_corner - std::sqrt(discCorner)) / (2.0 * A_corner);
                                if (tHitCorner >= 0.0 && tHitCorner < earliestT) {
                                    earliestT = tHitCorner;
                                    eventType = 2; // Jaw corner bounce
                                    eventI = i;
                                    double cnx = (b.px + b.vx * tHitCorner) - V.x;
                                    double cny = (b.py + b.vy * tHitCorner) - V.y;
                                    eventCushionNormalAngle = -std::atan2(cnx, -cny);
                                }
                            }
                        }
                    }
                }
            }

            // Immediate multi-body cluster contact solver:
            // When multiple pucks touch or overlap at t <= 1e-8, resolve all contacting pairs
            // iteratively so momentum transfers throughout the cluster in a single substep.
            if (earliestT <= 1e-8 && eventType == 1) {
                for (int iter = 0; iter < 10; ++iter) {
                    bool anyImpulse = false;
                    for (size_t i = 0; i < bodies.size(); i++) {
                        if (bodies[i].inPocket || bodies[i].inPocketDrop) continue;
                        for (size_t j = i + 1; j < bodies.size(); j++) {
                            if (bodies[j].inPocket || bodies[j].inPocketDrop) continue;
                            double cdx = bodies[i].px - bodies[j].px;
                            double cdy = bodies[i].py - bodies[j].py;
                            double dSq = cdx * cdx + cdy * cdy;
                            double rSum = bodies[i].radius + bodies[j].radius;
                            if (dSq <= (rSum + 0.05) * (rSum + 0.05)) {
                                if (resolveDiscDiscContact(bodies[i], bodies[j], config, result, true)) {
                                    anyImpulse = true;
                                }
                            }
                        }
                    }
                    if (!anyImpulse && iter > 0) break;
                }
                continue;
            }

            // -------------------------------------------------------------
            // Advance bodies to earliestT using Authentic Two-Phase Friction
            // -------------------------------------------------------------
            double advanceDt = earliestT;
            if (advanceDt > 1e-8) {
                // 1. Advance positions first using exact velocities solved in CCD
                for (auto& b : bodies) {
                    if (b.inPocket) continue;
                    double spd = std::hypot(b.vx, b.vy);
                    if (spd > minSpeed) {
                        b.px += b.vx * advanceDt;
                        b.py += b.vy * advanceDt;
                        b.distTraveled += spd * advanceDt;

                        // Corner Pocket Attraction & Drop Entry (libgame.so 0x1B2CB14 & 0x1997e8c)
                        for (int p = 0; p < 4; p++) {
                            double dx = pocketCenters[p][0] - b.px;
                            double dy = pocketCenters[p][1] - b.py;
                            double dSq = dx * dx + dy * dy;
                            // Physical pocket capture threshold:
                            // Striker radius is 2.65 cm, puck radius is 1.95 cm.
                            double dropRadius = (b.id == -1) ? config.pocketRadius : std::min(static_cast<double>(config.pocketRadius), 2.35);
                            if (dSq <= dropRadius * dropRadius) {
                                double curSpd = std::hypot(b.vx, b.vy);
                                double dotVel = dx * b.vx + dy * b.vy;
                                bool movingTowards = (dSq <= 1.40 * 1.40) || (dotVel > 0.0);
                                bool speedValid = (curSpd <= config.pocketDropMaxSpeed);
                                bool canDrop = movingTowards && speedValid;
                                if (canDrop && !b.inPocketDrop) {
                                    b.inPocketDrop = true;
                                    b.pocketIndex = p;
                                    b.pocketCenterX = pocketCenters[p][0];
                                    b.pocketCenterY = pocketCenters[p][1];
                                    b.pocketDropTime = 0.0;
                                    b.lastPocketDistSq = dSq;
                                    break;
                                }
                            }
                        }
                    }
                }

                // 2. Authentic Carrom Pool Pocket Sinking & Funnel Damping (libgame.so 0x1997f64 & 0x1998050)
                for (auto& b : bodies) {
                    if (b.inPocket || !b.inPocketDrop) continue;
                    b.pocketDropTime += advanceDt;

                    // Authentic rolling step damping inside pocket: 0.970000 per 1ms substep (0x1997f64)
                    double stepDamp = std::pow(0.970000, advanceDt / 0.001);
                    b.vx *= stepDamp;
                    b.vy *= stepDamp;

                    // Moving away check: if moving away from hole center, extra 0.850000 damping (0x1998050)
                    double dx = b.pocketCenterX - b.px;
                    double dy = b.pocketCenterY - b.py;
                    double curDistSq = dx * dx + dy * dy;
                    if (curDistSq > b.lastPocketDistSq) {
                        double skidDamp = std::pow(0.850000, advanceDt / 0.001);
                        b.vx *= skidDamp;
                        b.vy *= skidDamp;
                    }
                    b.lastPocketDistSq = curDistSq;

                    // Inward gravitational funnel suction (libgame.so 0x1B2CB14)
                    double curDist = std::sqrt(curDistSq);
                    if (curDist > 1e-4) {
                        double pull = config.pocketGravity * 0.5 * advanceDt;
                        b.vx += (dx / curDist) * pull;
                        b.vy += (dy / curDist) * pull;
                    }

                    // Terminal capture condition (libgame.so 0x1998088 / 0x1998240):
                    // When settled within 0.20 cm, or time >= 0.200s, or speed drops below minSpeed
                    double spd = std::hypot(b.vx, b.vy);
                    if (curDistSq <= 0.20 * 0.20 || b.pocketDropTime >= 0.200 || spd < minSpeed) {
                        b.inPocket = true;
                        b.inPocketDrop = false;
                        b.px = b.pocketCenterX;
                        b.py = b.pocketCenterY;
                        b.vx = 0.0;
                        b.vy = 0.0;
                        b.wx = 0.0;
                        b.wy = 0.0;
                        b.wz = 0.0;

                        TrajectorySegment seg;
                        seg.startX = static_cast<float>(b.segStartX);
                        seg.startY = static_cast<float>(b.segStartY);
                        seg.endX   = static_cast<float>(b.px);
                        seg.endY   = static_cast<float>(b.py);
                        seg.isCushionBounce = (b.segments.size() > 0);
                        seg.isPuckHit = false;
                        b.segments.push_back(seg);
                        b.segStartX = b.px;
                        b.segStartY = b.py;
                    }
                }

                // 3. Authentic Carrom Pool Ground Friction & Slip Solver (libgame.so 0x19986bc)
                for (auto& b : bodies) {
                    if (b.inPocket || b.inPocketDrop) continue;
                    double curSpd = std::hypot(b.vx, b.vy);
                    if (curSpd > minSpeed) {
                        // Contact point slip velocity (libgame.so 0x1998700 & 0x1998704)
                        // v_cx = vx + wy * R, v_cy = vy - wx * R
                        double v_cx = b.vx + b.wy * b.radius;
                        double v_cy = b.vy - b.wx * b.radius;
                        double v_slip = std::hypot(v_cx, v_cy);

                        // Time to stop slipping: t_slip = v_slip / 686.0 (libgame.so 0x1998718, where 1/686.0 = 0.0014577259475218659)
                        double t_slip = v_slip * 0.0014577259475218659;
                        double t_s = std::min(advanceDt, t_slip);

                        if (v_slip > 1e-6 && t_s > 0.0) {
                            // Slip deceleration impulse: 196.0 * t_s (libgame.so 0x1998744)
                            double dv_slip = 196.0 * t_s;
                            double slipRatio = dv_slip / v_slip;
                            b.vx -= v_cx * slipRatio;
                            b.vy -= v_cy * slipRatio;
                            b.wx += (2.5 * v_cy * slipRatio) / b.radius;
                            b.wy -= (2.5 * v_cx * slipRatio) / b.radius;
                        }

                        // Pure rolling Coulomb deceleration for remaining time (libgame.so 0x199878c - 0x19987d4)
                        if (advanceDt > t_slip) {
                            double dt_roll = advanceDt - t_slip;
                            double spd = std::hypot(b.vx, b.vy);
                            double mu = (b.id == -1) ? config.strikerFriction : config.puckFriction;
                            if (mu < 0.0001) mu = 0.0001;
                            double dv_roll = mu * 980.0 * dt_roll;
                            if (spd <= dv_roll) {
                                b.vx = 0.0;
                                b.vy = 0.0;
                                b.wx = 0.0;
                                b.wy = 0.0;
                            } else {
                                double scale = (spd - dv_roll) / spd;
                                b.vx *= scale;
                                b.vy *= scale;
                                b.wx = b.vy / b.radius;
                                b.wy = -b.vx / b.radius;
                            }
                        }

                        // Vertical spin decay: (mu / 5.0) * 980.0 * advanceDt (libgame.so 0x19987f4 - 0x199881c)
                        if (std::abs(b.wz) > 1e-4) {
                            double mu = (b.id == -1) ? config.strikerFriction : config.puckFriction;
                            double dwz = (mu / 5.0) * 980.0 * advanceDt;
                            if (std::abs(b.wz) <= dwz) {
                                b.wz = 0.0;
                            } else {
                                b.wz -= std::copysign(dwz, b.wz);
                            }
                        }
                        b.r_omega = std::hypot(b.vx, b.vy);
                    } else {
                        // Strict stationary cutoff enforcement (libgame.so minSpeed = 0.15 cm/s)
                        b.vx = 0.0;
                        b.vy = 0.0;
                        b.wx = 0.0;
                        b.wy = 0.0;
                        b.wz = 0.0;
                    }
                }

                // 3. Smooth Curving Trajectory Tracking
                // When surface slip curves the direction by >= 1.0 deg and moved >= 3.5 cm, record intermediate vertex
                for (auto& b : bodies) {
                    if (b.inPocket) continue;
                    double spd = std::hypot(b.vx, b.vy);
                    if (spd > minSpeed) {
                        double curAngle = std::atan2(b.vy, b.vx);
                        double angleDiff = std::abs(curAngle - b.lastSegAngle);
                        if (angleDiff > 3.141592653589793) angleDiff = 2.0 * 3.141592653589793 - angleDiff;
                        double dSinceSeg = std::hypot(b.px - b.segStartX, b.py - b.segStartY);
                        if (dSinceSeg >= 3.5 && angleDiff >= 0.017) {
                            TrajectorySegment seg;
                            seg.startX = static_cast<float>(b.segStartX);
                            seg.startY = static_cast<float>(b.segStartY);
                            seg.endX   = static_cast<float>(b.px);
                            seg.endY   = static_cast<float>(b.py);
                            seg.isCushionBounce = false;
                            seg.isPuckHit = false;
                            b.segments.push_back(seg);
                            b.segStartX = b.px;
                            b.segStartY = b.py;
                            b.lastSegAngle = curAngle;
                        }
                    }
                }
            }

            timeLeft -= advanceDt;

            // -------------------------------------------------------------
            // Resolve Collision Event
            // -------------------------------------------------------------
            if (eventType == 1) {
                resolveDiscDiscContact(bodies[eventI], bodies[eventJ], config, result, true);
            } else if (eventType == 2) {
                resolveCushionBounce(bodies[eventI], eventCushionNormalAngle, config, true);
            } else {
                break;
            }
        }

        // If timeLeft remains after subSteps, advance positions with micro-steps and overlap depenetration
        if (timeLeft > 1e-6) {
            const double microDt = std::min(timeLeft, 0.00025);
            int microSteps = static_cast<int>(std::ceil(timeLeft / microDt));
            for (int ms = 0; ms < microSteps; ms++) {
                double curMicro = std::min(microDt, timeLeft - ms * microDt);
                if (curMicro <= 1e-7) break;
                for (auto& b : bodies) {
                    if (b.inPocket) continue;
                    double spd = std::hypot(b.vx, b.vy);
                    if (spd > minSpeed) {
                        b.px += b.vx * curMicro;
                        b.py += b.vy * curMicro;
                        b.distTraveled += spd * curMicro;
                    }
                }
                for (size_t i = 0; i < bodies.size(); i++) {
                    if (bodies[i].inPocket) continue;
                    for (size_t j = i + 1; j < bodies.size(); j++) {
                        if (bodies[j].inPocket) continue;
                        double cdx = bodies[i].px - bodies[j].px;
                        double cdy = bodies[i].py - bodies[j].py;
                        double dist = std::hypot(cdx, cdy);
                        double minDist = bodies[i].radius + bodies[j].radius;
                        if (dist < minDist && dist > 1e-6) {
                            double delta = minDist - dist;
                            double shift = delta / dist;
                            double mSum = bodies[i].mass + bodies[j].mass;
                            bodies[i].px += cdx * shift * (bodies[j].mass / mSum);
                            bodies[i].py += cdy * shift * (bodies[j].mass / mSum);
                            bodies[j].px -= cdx * shift * (bodies[i].mass / mSum);
                            bodies[j].py -= cdy * shift * (bodies[i].mass / mSum);
                        }
                    }
                }
            }
        }
    }

    // Finalize any bodies that settled in pocket drop funnel
    for (auto& b : bodies) {
        if (b.inPocketDrop) {
            b.inPocket = true;
            b.inPocketDrop = false;
            b.px = b.pocketCenterX;
            b.py = b.pocketCenterY;
            b.vx = 0.0;
            b.vy = 0.0;
            TrajectorySegment seg;
            seg.startX = static_cast<float>(b.segStartX);
            seg.startY = static_cast<float>(b.segStartY);
            seg.endX   = static_cast<float>(b.px);
            seg.endY   = static_cast<float>(b.py);
            seg.isCushionBounce = (b.segments.size() > 0);
            seg.isPuckHit = false;
            b.segments.push_back(seg);
            b.segStartX = b.px;
            b.segStartY = b.py;
        }
    }

    // Finalize pending segments for bodies still in flight
    for (auto& b : bodies) {
        if (!b.inPocket) {
            double d = std::hypot(b.px - b.segStartX, b.py - b.segStartY);
            if ((b.id != -1 && b.chainLevel >= 1 && d > 0.01) || (b.segments.empty() && d > 0.15) || (!b.segments.empty() && d > 0.10)) {
                TrajectorySegment seg;
                seg.startX = static_cast<float>(b.segStartX);
                seg.startY = static_cast<float>(b.segStartY);
                seg.endX   = static_cast<float>(b.px);
                seg.endY   = static_cast<float>(b.py);
                seg.isCushionBounce = (b.segments.size() > 0);
                seg.isPuckHit = (b.chainLevel >= 1 && b.segments.empty());
                b.segments.push_back(seg);
            }
        }
    }

    const double boardLimit = config.boardHalfSize - 1.95; // Table cushion visual boundary (35.05 cm)

    // Collect Striker results
    result.segments = bodies[0].segments;
    for (auto& s : result.segments) {
        s.startX = static_cast<float>(std::clamp((double)s.startX, -boardLimit, boardLimit));
        s.startY = static_cast<float>(std::clamp((double)s.startY, -boardLimit, boardLimit));
        s.endX   = static_cast<float>(std::clamp((double)s.endX,   -boardLimit, boardLimit));
        s.endY   = static_cast<float>(std::clamp((double)s.endY,   -boardLimit, boardLimit));
    }
    double stX = bodies[0].px;
    double stY = bodies[0].py;
    if (!bodies[0].inPocket) {
        stX = std::clamp(stX, -boardLimit, boardLimit);
        stY = std::clamp(stY, -boardLimit, boardLimit);
    }
    result.finalStrikerX = static_cast<float>(stX);
    result.finalStrikerY = static_cast<float>(stY);
    result.strikerInPocket = bodies[0].inPocket;
    result.strikerPocketIndex = bodies[0].pocketIndex;
    result.strikerCushionBounces = bodies[0].cushionBounces;
    result.hitPuck = result.hasFirstContact;

    // Collect Object Puck results (all pucks on table)
    for (size_t i = 1; i < bodies.size(); i++) {
        const auto& b = bodies[i];
        PuckTrajectoryInfo traj;
        traj.puckIndex = b.id;
        traj.puckType = b.puckType;
        traj.startX = static_cast<float>(b.initX);
        traj.startY = static_cast<float>(b.initY);
        double pkX = b.px;
        double pkY = b.py;
        if (!b.inPocket) {
            pkX = std::clamp(pkX, -boardLimit, boardLimit);
            pkY = std::clamp(pkY, -boardLimit, boardLimit);
        }
        traj.finalX = static_cast<float>(pkX);
        traj.finalY = static_cast<float>(pkY);
        traj.inPocket = b.inPocket;
        traj.pocketIndex = b.pocketIndex;
        traj.pocketCenterX = static_cast<float>(b.pocketCenterX);
        traj.pocketCenterY = static_cast<float>(b.pocketCenterY);
        traj.segments = b.segments;
        traj.isHit = (b.chainLevel >= 1) || (b.hitByEntityId >= 0) || (std::hypot(traj.finalX - traj.startX, traj.finalY - traj.startY) > 0.02);
        traj.hitByEntityId = b.hitByEntityId;
        traj.hitSpeed = static_cast<float>(b.hitSpeed);
        traj.cushionBounces = b.cushionBounces;
        traj.hasDeflection = false;

        if (traj.segments.empty() && (traj.isHit || traj.inPocket)) {
            double dHit = std::hypot(traj.finalX - traj.startX, traj.finalY - traj.startY);
            if (dHit > 0.01) {
                TrajectorySegment seg;
                seg.startX = traj.startX;
                seg.startY = traj.startY;
                seg.endX   = traj.finalX;
                seg.endY   = traj.finalY;
                seg.isCushionBounce = false;
                seg.isPuckHit = true;
                traj.segments.push_back(seg);
            }
        }
        for (auto& s : traj.segments) {
            s.startX = static_cast<float>(std::clamp((double)s.startX, -boardLimit, boardLimit));
            s.startY = static_cast<float>(std::clamp((double)s.startY, -boardLimit, boardLimit));
            s.endX   = static_cast<float>(std::clamp((double)s.endX,   -boardLimit, boardLimit));
            s.endY   = static_cast<float>(std::clamp((double)s.endY,   -boardLimit, boardLimit));
        }
        result.puckTrajectories.push_back(traj);
    }

    if (s_diagnosticsEnabled) {
        static int s_v2SimCounter = 0;
        if (++s_v2SimCounter % 25 == 0 || result.hasFirstContact) {
            logDiagnosticReport(result, startX, startY, shotAngle, forceVal);
        }
    }

    return result;
}

void logDiagnosticReport(const SimResult& sim, double startX, double startY, double angle, double force) {
    ALOGI("[CatEngine-V2] [SIM] Striker=(%.2f, %.2f) Angle=%.1f deg Force=%.2f | Segs=%zu FirstHit=%d PuckId=%d FirstImpact=(%.2f, %.2f) | Pucks=%zu Pocketed=%d",
          startX, startY, angle * 180.0 / M_PI, force,
          sim.segments.size(), sim.hasFirstContact ? 1 : 0, sim.firstContactPuckId,
          sim.firstImpactStrikerX, sim.firstImpactStrikerY,
          sim.puckTrajectories.size(), sim.strikerInPocket ? 1 : 0);
    for (const auto& p : sim.puckTrajectories) {
        if (p.inPocket) {
            ALOGI("[CatEngine-V2] [POCKET-POT] Puck #%d (Type %d) potted into Pocket #%d (%.2f, %.2f)",
                  p.puckIndex, p.puckType, p.pocketIndex, p.pocketCenterX, p.pocketCenterY);
        }
    }
}

} // namespace v2
} // namespace catengine
