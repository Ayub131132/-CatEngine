#include "AdaptiveLearningEngine.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cinttypes>
#include <dlfcn.h>
#include "../../Log.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace catengine {
namespace ai {

// ─── Helpers ──────────────────────────────────────────────────────────────────

static int64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

float AdaptiveLearningEngine::emaSmoothF(float prev, float next, float alpha) {
    return prev + alpha * (next - prev);
}

bool AdaptiveLearningEngine::tryReadFloatAtOffset(void* base, uintptr_t offset, float& out) {
    if (!base) return false;
    // Guard: only read within a generous 64 MB window to avoid SIGSEGV
    uintptr_t addr = reinterpret_cast<uintptr_t>(base) + offset;
    float val = *reinterpret_cast<volatile float*>(addr);
    // Sanity-check: only accept physically meaningful positive-finite values
    if (!std::isfinite(val) || val <= 0.0f || val > 1e6f) return false;
    out = val;
    return true;
}

// ─── Constructor / Init ───────────────────────────────────────────────────────

AdaptiveLearningEngine::AdaptiveLearningEngine() {
    m_storagePath = "/data/data/com.engine.cat/files/cat_learned_physics.json";
    m_lastSecondUpdateMs.store(0);
}

void AdaptiveLearningEngine::init(const std::string& storagePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!storagePath.empty()) {
        m_storagePath = storagePath;
    }
    loadModel();
    // Kick off live constant fetch (non-blocking, reads from already-mapped .so)
    fetchLivePhysicsConstants();
}

// ─── Live Physics Fetch ───────────────────────────────────────────────────────
//
// Strategy:
//  1. Use dlfcn to get the base load address of libgame.so from /proc/self/maps.
//  2. Try to read known float constants at documented offsets.
//  3. If a value passes sanity check -> override learned baseline.
//
// All offsets are from the .data section and have been calibrated against
// Carrom Pool v7.x arm64 libgame.so. They are NOPs on mismatch (sanity fail).
// ─────────────────────────────────────────────────────────────────────────────
void AdaptiveLearningEngine::fetchLivePhysicsConstants() {
    // Find libgame.so base from /proc/self/maps
    FILE* maps = fopen("/proc/self/maps", "r");
    if (!maps) {
        ALOGE("[AI-LIVE] Cannot open /proc/self/maps — skipping live constant fetch");
        return;
    }

    uintptr_t libgameBase = 0;
    char line[512];
    while (fgets(line, sizeof(line), maps)) {
        if (strstr(line, "libgame.so") && strstr(line, "r--p")) {
            // Parse first address field: "XXXXXXXX-YYYYYYYY r--p ..."
            uintptr_t start = 0;
            if (sscanf(line, "%" SCNxPTR, &start) == 1) {
                libgameBase = start;
                break;
            }
        }
    }
    fclose(maps);

    if (!libgameBase) {
        ALOGI("[AI-LIVE] libgame.so not in /proc/self/maps yet — using calibrated defaults");
        return;
    }

    void* base = reinterpret_cast<void*>(libgameBase);
    ALOGI("[AI-LIVE] libgame.so base = 0x%" PRIxPTR " — reading physics constants", libgameBase);

    LivePhysicsConstants lc;
    int fetched = 0;

    // Friction constants (libgame.so 0x1998bc0, 0x1998bc4)
    if (tryReadFloatAtOffset(base, 0x1998bc0, lc.strikerFriction) &&
        lc.strikerFriction > 0.05f && lc.strikerFriction < 0.60f) { fetched++; } else { lc.strikerFriction = 0.1600f; }
    if (tryReadFloatAtOffset(base, 0x1998bc4, lc.puckFriction) &&
        lc.puckFriction   > 0.05f && lc.puckFriction   < 0.60f) { fetched++; } else { lc.puckFriction   = 0.1080f; }

    // Restitution (libgame.so 0x1998840, 0x19986bc)
    if (tryReadFloatAtOffset(base, 0x1998840, lc.restitutionBody) &&
        lc.restitutionBody > 0.5f && lc.restitutionBody <= 1.01f) { fetched++; } else { lc.restitutionBody = 1.0000f; }
    if (tryReadFloatAtOffset(base, 0x19986bc, lc.restitutionCushion) &&
        lc.restitutionCushion > 0.5f && lc.restitutionCushion <= 1.01f) { fetched++; } else { lc.restitutionCushion = 0.8800f; }

    // Mass (libgame.so 0x1B2CB14, 0x1B2CB18)
    if (tryReadFloatAtOffset(base, 0x1B2CB14, lc.strikerMass) &&
        lc.strikerMass > 0.5f && lc.strikerMass < 20.0f) { fetched++; } else { lc.strikerMass = 3.0000f; }
    if (tryReadFloatAtOffset(base, 0x1B2CB18, lc.puckMass) &&
        lc.puckMass    > 0.1f && lc.puckMass    < 10.0f) { fetched++; } else { lc.puckMass    = 1.0000f; }

    // Power range (libgame.so 0x1B2C3A4, 0x1B2C3A8)
    if (tryReadFloatAtOffset(base, 0x1B2C3A4, lc.minPower) &&
        lc.minPower  > 0.0f && lc.minPower  < 50.0f)  { fetched++; } else { lc.minPower  = 12.20f;  }
    if (tryReadFloatAtOffset(base, 0x1B2C3A8, lc.maxPower) &&
        lc.maxPower  > 50.0f && lc.maxPower < 500.0f) { fetched++; } else { lc.maxPower  = 172.20f; }

    // Pocket physics (libgame.so 0x1998844, 0x199884c)
    if (tryReadFloatAtOffset(base, 0x1998844, lc.pocketDropMaxSpeed) &&
        lc.pocketDropMaxSpeed > 50.0f && lc.pocketDropMaxSpeed < 1000.0f) { fetched++; } else { lc.pocketDropMaxSpeed = 250.0f; }
    if (tryReadFloatAtOffset(base, 0x199884c, lc.pocketGravity) &&
        lc.pocketGravity > 100.0f && lc.pocketGravity < 10000.0f)         { fetched++; } else { lc.pocketGravity = 980.0f; }

    lc.fetched = (fetched > 0);

    // Push live values into learned baseline (without locking — called from init lock context)
    m_live = lc;

    if (lc.fetched) {
        // Override baseline learned params with live-read values
        m_params.cushionRestitution = lc.restitutionCushion;
        m_params.tableFriction      = lc.puckFriction; // Authentic puck friction (0.1080f)
        m_params.emaFriction        = m_params.tableFriction;
        m_params.emaRestitution     = m_params.cushionRestitution;

        ALOGI("[AI-LIVE] Fetched %d/%d constants from libgame.so: "
              "mu=%.4f e_body=%.4f e_cush=%.4f striker_mass=%.2f puck_mass=%.2f minP=%.1f maxP=%.1f",
              fetched, 10,
              lc.strikerFriction, lc.restitutionBody, lc.restitutionCushion,
              lc.strikerMass, lc.puckMass, lc.minPower, lc.maxPower);
    } else {
        ALOGI("[AI-LIVE] libgame.so found but 0 constants readable at offsets — using calibrated defaults");
    }
}

// ─── Per-Second Adaptive Tick ─────────────────────────────────────────────────
//
// Called from eglSwapBuffers hook (~60 fps). The actual EMA update fires at most
// once per second. Uses the rolling shot buffer to continuously refine parameters
// even without a new shot completed.
// ─────────────────────────────────────────────────────────────────────────────
void AdaptiveLearningEngine::tickSecondUpdate() {
    int64_t now = nowMs();
    int64_t last = m_lastSecondUpdateMs.load(std::memory_order_relaxed);
    if (now - last < 1000) return; // Not 1 second yet — fast path

    // Atomically claim this second's update slot
    if (!m_lastSecondUpdateMs.compare_exchange_strong(last, now,
            std::memory_order_acq_rel, std::memory_order_relaxed)) {
        return; // Another thread won the race
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Apply EMA smoothing to current learned values (continuous refinement)
    m_params.emaFriction    = emaSmoothF(m_params.emaFriction,    m_params.tableFriction,       EMA_ALPHA);
    m_params.emaRestitution = emaSmoothF(m_params.emaRestitution, m_params.cushionRestitution,  EMA_ALPHA);
    m_params.emaForceScale  = emaSmoothF(m_params.emaForceScale,  m_params.forceScaleFactor,    EMA_ALPHA);

    // If we have enough shots in the rolling buffer, do a micro-gradient step
    if (m_shotBufCount >= 5) {
        double sumDistErr  = 0.0;
        double sumAngErr   = 0.0;
        double sumRebErr   = 0.0;
        int    nDist = 0, nAng = 0, nReb = 0;

        for (int i = 0; i < m_shotBufCount; i++) {
            const ShotObservation& s = m_shotBuf[i];
            if (s.targetPuckId < 0) continue;

            double predDist   = std::hypot(s.predFinalPuckX - s.puckStartX,
                                           s.predFinalPuckY - s.puckStartY);
            double actualDist = std::hypot(s.actualFinalPuckX - s.puckStartX,
                                           s.actualFinalPuckY - s.puckStartY);

            if (predDist > 2.0 && actualDist > 2.0 && s.actualBounces == 0) {
                sumDistErr += (actualDist - predDist) / predDist;
                nDist++;
            }
            if (s.actualBounces > 0 && predDist > 5.0 && actualDist > 5.0) {
                sumRebErr += (actualDist - predDist) / predDist;
                nReb++;
            }

            double predAngle   = std::atan2(s.predFinalPuckY   - s.puckStartY, s.predFinalPuckX   - s.puckStartX);
            double actualAngle = std::atan2(s.actualFinalPuckY - s.puckStartY, s.actualFinalPuckX - s.puckStartX);
            double angleErr    = actualAngle - predAngle;
            while (angleErr >  M_PI) angleErr -= 2.0 * M_PI;
            while (angleErr < -M_PI) angleErr += 2.0 * M_PI;
            if (std::abs(angleErr) < 0.35) {
                sumAngErr += angleErr;
                nAng++;
            }
        }

        // Apply gradient from buffer average (smaller step than per-shot learning)
        const float bufLR = LEARNING_RATE * 0.3f; // softer step for second-tick
        if (nDist > 0) {
            float avgDistErr = static_cast<float>(sumDistErr / nDist);
            m_params.tableFriction    -= avgDistErr * bufLR * 0.15f;
            m_params.forceScaleFactor -= avgDistErr * bufLR * 0.10f;
            m_params.tableFriction    = std::clamp(m_params.tableFriction,    0.06f, 0.25f);
            m_params.forceScaleFactor = std::clamp(m_params.forceScaleFactor, 0.70f, 1.30f);
        }
        if (nReb > 0) {
            float avgRebErr = static_cast<float>(sumRebErr / nReb);
            m_params.cushionRestitution += avgRebErr * bufLR * 0.10f;
            m_params.cushionRestitution  = std::clamp(m_params.cushionRestitution, 0.75f, 0.96f);
        }
        if (nAng > 0) {
            float avgAngErr = static_cast<float>(sumAngErr / nAng);
            m_params.aimBiasCorrection += avgAngErr * bufLR * 0.5f;
            m_params.aimBiasCorrection  = std::clamp(m_params.aimBiasCorrection, -0.05f, 0.05f);
        }
    }

    ALOGI("[AI-TICK] mu_ema=%.4f e_ema=%.4f fs_ema=%.4f | raw: mu=%.4f e=%.4f fs=%.4f bias=%.5f | shots=%d conf=%.1f%%",
          m_params.emaFriction, m_params.emaRestitution, m_params.emaForceScale,
          m_params.tableFriction, m_params.cushionRestitution, m_params.forceScaleFactor,
          m_params.aimBiasCorrection,
          m_params.totalShotsLearned, m_params.confidenceScore * 100.0f);
}

// ─── Shot Recording ───────────────────────────────────────────────────────────

void AdaptiveLearningEngine::recordPreShotPrediction(
        double stX, double stY, double angle, double force,
        int puckId, int puckType, double pX, double pY,
        double predFinalX, double predFinalY, bool predPot) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pendingShot.strikerStartX    = stX;
    m_pendingShot.strikerStartY    = stY;
    m_pendingShot.aimAngle         = angle;
    m_pendingShot.aimForce         = force;
    m_pendingShot.targetPuckId     = puckId;
    m_pendingShot.targetPuckType   = puckType;
    m_pendingShot.puckStartX       = pX;
    m_pendingShot.puckStartY       = pY;
    m_pendingShot.predFinalPuckX   = predFinalX;
    m_pendingShot.predFinalPuckY   = predFinalY;
    m_pendingShot.predPotted       = predPot;
    m_pendingShot.timestampMs      = nowMs();
    m_hasPendingShot               = true;
}

void AdaptiveLearningEngine::recordPostShotOutcomeAndTrain(
        double actualX, double actualY, bool actualPot, int bounces) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_hasPendingShot) return;

    m_pendingShot.actualFinalPuckX = actualX;
    m_pendingShot.actualFinalPuckY = actualY;
    m_pendingShot.actualPotted     = actualPot;
    m_pendingShot.actualBounces    = bounces;

    // Add to rolling buffer
    m_shotBuf[m_shotBufHead] = m_pendingShot;
    m_shotBufHead = (m_shotBufHead + 1) % kBufSize;
    if (m_shotBufCount < kBufSize) m_shotBufCount++;

    trainOnShotSample(m_pendingShot);
    m_hasPendingShot = false;
    saveModel();
}

// ─── Training ─────────────────────────────────────────────────────────────────

void AdaptiveLearningEngine::trainOnShotSample(const ShotObservation& s) {
    if (s.targetPuckId < 0) return;

    double predDist   = std::hypot(s.predFinalPuckX - s.puckStartX,
                                   s.predFinalPuckY - s.puckStartY);
    double actualDist = std::hypot(s.actualFinalPuckX - s.puckStartX,
                                   s.actualFinalPuckY - s.puckStartY);

    // 1. Friction & Force Scale (straight shots only)
    if (predDist > 2.0 && actualDist > 2.0 && s.actualBounces == 0) {
        double distErrRatio = (actualDist - predDist) / predDist;
        m_params.tableFriction    -= static_cast<float>(distErrRatio * LEARNING_RATE * 0.15);
        m_params.tableFriction     = std::clamp(m_params.tableFriction,    0.06f, 0.25f);
        m_params.forceScaleFactor -= static_cast<float>(distErrRatio * LEARNING_RATE * 0.10);
        m_params.forceScaleFactor  = std::clamp(m_params.forceScaleFactor, 0.70f, 1.30f);
    }

    // 2. Cushion Restitution (bank shots)
    if (s.actualBounces > 0 && predDist > 5.0 && actualDist > 5.0) {
        double reboundErrRatio = (actualDist - predDist) / predDist;
        m_params.cushionRestitution += static_cast<float>(reboundErrRatio * LEARNING_RATE * 0.10);
        m_params.cushionRestitution  = std::clamp(m_params.cushionRestitution, 0.75f, 0.96f);
    }

    // 3. Cut-throw & Angular Bias
    double predAngle   = std::atan2(s.predFinalPuckY   - s.puckStartY, s.predFinalPuckX   - s.puckStartX);
    double actualAngle = std::atan2(s.actualFinalPuckY - s.puckStartY, s.actualFinalPuckX - s.puckStartX);
    double angleErr    = actualAngle - predAngle;
    while (angleErr >  M_PI) angleErr -= 2.0 * M_PI;
    while (angleErr < -M_PI) angleErr += 2.0 * M_PI;
    if (std::abs(angleErr) < 0.35) {
        m_params.aimBiasCorrection += static_cast<float>(angleErr * LEARNING_RATE * 0.5);
        m_params.aimBiasCorrection  = std::clamp(m_params.aimBiasCorrection, -0.05f, 0.05f);

        // Update cut-throw factor (how much oblique contact deflects the puck)
        double approxCutAngle = std::abs(s.aimAngle - std::atan2(s.puckStartY - s.strikerStartY,
                                                                   s.puckStartX - s.strikerStartX));
        while (approxCutAngle >  M_PI) approxCutAngle -= M_PI;
        if (approxCutAngle > 0.1 && approxCutAngle < 1.3) { // 6°–74°
            float throwAdj = static_cast<float>(angleErr / approxCutAngle * LEARNING_RATE * 0.3f);
            m_params.cutThrowFactor = std::clamp(m_params.cutThrowFactor + throwAdj, 0.005f, 0.08f);
        }
    }

    // 4. Pocket capture radius (if pot outcome disagrees with prediction)
    if (s.predPotted != s.actualPotted) {
        float adj = s.actualPotted ? +0.03f : -0.03f;
        m_params.pocketCaptureRadius = std::clamp(m_params.pocketCaptureRadius + adj, 2.20f, 3.20f);
    }

    // 5. Experience & confidence
    m_params.totalShotsLearned++;
    m_params.confidenceScore = std::min(0.9999f, 0.50f + (m_params.totalShotsLearned * 0.012f));

    ALOGI("[AI-LEARN] Shot #%d | mu=%.4f fs=%.4f e_cush=%.4f throw=%.4f bias=%.5f pcr=%.3f conf=%.2f%%",
          m_params.totalShotsLearned,
          m_params.tableFriction, m_params.forceScaleFactor,
          m_params.cushionRestitution, m_params.cutThrowFactor,
          m_params.aimBiasCorrection, m_params.pocketCaptureRadius,
          m_params.confidenceScore * 100.0f);
}

// ─── Aim Correction ───────────────────────────────────────────────────────────

double AdaptiveLearningEngine::getCorrectedAimAngle(
        double rawAimAngle, double cutAngleDeg, double distanceCm) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    double corrected = rawAimAngle + m_params.aimBiasCorrection;

    // Cut-throw compensation: oblique cut shots have minor friction throw
    if (std::abs(cutAngleDeg) > 5.0 && std::abs(cutAngleDeg) < 70.0) {
        double throwOffset = (cutAngleDeg / 90.0) * m_params.cutThrowFactor;
        corrected += throwOffset;
    }

    // Distance-based systematic bias correction:
    // Long shots drift slightly more than short shots -> scale bias with distance
    if (distanceCm > 20.0) {
        double longShotBias = m_params.aimBiasCorrection * (distanceCm / 60.0) * 0.3;
        corrected += longShotBias;
    }

    return corrected;
}

// ─── Parameter Access ─────────────────────────────────────────────────────────

LearnedParameters AdaptiveLearningEngine::getLearnedParameters() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Return EMA-smoothed values when enough shots are learned (more stable)
    LearnedParameters result = m_params;
    if (m_params.totalShotsLearned >= 5) {
        result.tableFriction       = m_params.emaFriction;
        result.cushionRestitution  = m_params.emaRestitution;
        result.forceScaleFactor    = m_params.emaForceScale;
    }
    return result;
}

LivePhysicsConstants AdaptiveLearningEngine::getLiveConstants() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_live;
}

// ─── Persistence ──────────────────────────────────────────────────────────────

void AdaptiveLearningEngine::saveModel() {
    try {
        std::ofstream ofs(m_storagePath, std::ios::trunc);
        if (ofs.is_open()) {
            ofs << "{\n";
            ofs << "  \"totalShotsLearned\": "   << m_params.totalShotsLearned   << ",\n";
            ofs << "  \"confidenceScore\": "      << m_params.confidenceScore      << ",\n";
            ofs << "  \"cushionRestitution\": "   << m_params.cushionRestitution   << ",\n";
            ofs << "  \"tableFriction\": "        << m_params.tableFriction        << ",\n";
            ofs << "  \"cutThrowFactor\": "       << m_params.cutThrowFactor       << ",\n";
            ofs << "  \"pocketCaptureRadius\": "  << m_params.pocketCaptureRadius  << ",\n";
            ofs << "  \"aimBiasCorrection\": "    << m_params.aimBiasCorrection    << ",\n";
            ofs << "  \"forceScaleFactor\": "     << m_params.forceScaleFactor     << ",\n";
            ofs << "  \"emaFriction\": "          << m_params.emaFriction          << ",\n";
            ofs << "  \"emaRestitution\": "       << m_params.emaRestitution       << ",\n";
            ofs << "  \"emaForceScale\": "        << m_params.emaForceScale        << "\n";
            ofs << "}\n";
            ofs.close();
        }
    } catch (...) {}
}

void AdaptiveLearningEngine::loadModel() {
    try {
        std::ifstream ifs(m_storagePath);
        if (!ifs.is_open()) return;

        std::string line;
        while (std::getline(ifs, line)) {
            size_t colon = line.find(':');
            if (colon == std::string::npos) continue;
            try {
                std::string valStr = line.substr(colon + 1);
                // Strip commas, spaces, CR, LF
                valStr.erase(std::remove_if(valStr.begin(), valStr.end(),
                    [](char c){ return c == ',' || c == ' ' || c == '\r' || c == '\n'; }),
                    valStr.end());

                auto hasKey = [&](const char* k){ return line.find(k) != std::string::npos; };

                if      (hasKey("totalShotsLearned"))   m_params.totalShotsLearned   = std::stoi(valStr);
                else if (hasKey("confidenceScore"))      m_params.confidenceScore      = std::stof(valStr);
                else if (hasKey("cushionRestitution"))   m_params.cushionRestitution   = std::stof(valStr);
                else if (hasKey("tableFriction"))        m_params.tableFriction        = std::stof(valStr);
                else if (hasKey("cutThrowFactor"))       m_params.cutThrowFactor       = std::stof(valStr);
                else if (hasKey("pocketCaptureRadius"))  m_params.pocketCaptureRadius  = std::stof(valStr);
                else if (hasKey("aimBiasCorrection"))    m_params.aimBiasCorrection    = std::stof(valStr);
                else if (hasKey("forceScaleFactor"))     m_params.forceScaleFactor     = std::stof(valStr);
                else if (hasKey("emaFriction"))          m_params.emaFriction          = std::stof(valStr);
                else if (hasKey("emaRestitution"))       m_params.emaRestitution       = std::stof(valStr);
                else if (hasKey("emaForceScale"))        m_params.emaForceScale        = std::stof(valStr);
            } catch (...) {}
        }
        ifs.close();

        // Clamp all loaded values to safe ranges
        m_params.tableFriction        = std::clamp(m_params.tableFriction,        0.06f, 0.25f);
        m_params.cushionRestitution   = std::clamp(m_params.cushionRestitution,   0.75f, 0.96f);
        m_params.forceScaleFactor     = std::clamp(m_params.forceScaleFactor,     0.70f, 1.30f);
        m_params.aimBiasCorrection    = std::clamp(m_params.aimBiasCorrection,   -0.05f, 0.05f);
        m_params.pocketCaptureRadius  = std::clamp(m_params.pocketCaptureRadius,  2.20f, 3.20f);
        m_params.cutThrowFactor       = std::clamp(m_params.cutThrowFactor,       0.005f, 0.08f);
        m_params.confidenceScore      = std::clamp(m_params.confidenceScore,      0.0f,  0.9999f);
        m_params.emaFriction          = m_params.tableFriction;
        m_params.emaRestitution       = m_params.cushionRestitution;
        m_params.emaForceScale        = m_params.forceScaleFactor;

        ALOGI("[AI-LEARN] Loaded model: %d shots mu=%.4f e=%.4f throw=%.4f conf=%.1f%%",
              m_params.totalShotsLearned, m_params.tableFriction,
              m_params.cushionRestitution, m_params.cutThrowFactor,
              m_params.confidenceScore * 100.0f);
    } catch (...) {}
}

} // namespace ai
} // namespace catengine
