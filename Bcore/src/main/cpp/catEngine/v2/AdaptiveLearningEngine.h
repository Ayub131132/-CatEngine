#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cstdint>

namespace catengine {
namespace ai {

// ─── Live Physics Constants (auto-fetched from libgame.so on startup) ─────────
struct LivePhysicsConstants {
    bool  fetched              = false;
    float strikerFriction      = 0.1600f; // libgame.so 0x1998bc0
    float puckFriction         = 0.1080f; // libgame.so 0x1998bc4
    float restitutionBody      = 1.0000f; // libgame.so 0x1998840
    float restitutionCushion   = 0.8800f; // libgame.so 0x19986bc
    float strikerMass          = 3.0000f; // libgame.so 0x1B2CB14
    float puckMass             = 1.0000f; // libgame.so 0x1B2CB18
    float minPower             = 12.20f;  // libgame.so 0x1B2C3A4
    float maxPower             = 172.20f; // libgame.so 0x1B2C3A8
    float pocketDropMaxSpeed   = 180.0f;  // libgame.so 0x1998844
    float pocketGravity        = 980.0f;  // libgame.so 0x199884c
    float contactFriction      = 0.0500f; // libgame.so 0x199885c
};

struct ShotObservation {
    double strikerStartX    = 0.0;
    double strikerStartY    = 0.0;
    double aimAngle         = 0.0;
    double aimForce         = 1.0;

    int    targetPuckId     = -1;
    int    targetPuckType   = 1;
    double puckStartX       = 0.0;
    double puckStartY       = 0.0;

    // Predicted
    double predFinalPuckX   = 0.0;
    double predFinalPuckY   = 0.0;
    bool   predPotted       = false;

    // Observed actual outcome
    double actualFinalPuckX = 0.0;
    double actualFinalPuckY = 0.0;
    bool   actualPotted     = false;
    int    actualBounces    = 0;

    // Timing metadata
    int64_t timestampMs     = 0;
};

struct LearnedParameters {
    float cushionRestitution  = 0.88f;
    float tableFriction       = 0.28f;
    float cutThrowFactor      = 0.02f;
    float pocketCaptureRadius = 2.65f;
    float aimBiasCorrection   = 0.00f; // Angular bias in radians
    float forceScaleFactor    = 1.00f; // Real-time learned force scaling
    float contactFriction     = 0.05f; // Disc-disc contact friction
    int   totalShotsLearned   = 0;
    float confidenceScore     = 0.50f;
    // Per-second EMA smoothing state (not persisted):
    float emaFriction         = 0.28f;
    float emaRestitution      = 0.88f;
    float emaForceScale       = 1.00f;
};

class AdaptiveLearningEngine {
public:
    static AdaptiveLearningEngine& getInstance() {
        static AdaptiveLearningEngine instance;
        return instance;
    }

    // ── Initialisation ──────────────────────────────────────────────────────
    void init(const std::string& storagePath);

    // Auto-fetch real physics constants from libgame.so using /proc/self/maps
    // and dlfcn. Call once after the EGL hook is installed.
    void fetchLivePhysicsConstants();

    // ── Per-frame tick (called from eglSwapBuffers hook, ~60 fps) ──────────
    // Runs actual EMA update only once per second.
    void tickSecondUpdate();

    // ── Shot learning ───────────────────────────────────────────────────────
    void recordPreShotPrediction(double stX, double stY, double angle, double force,
                                 int puckId, int puckType, double pX, double pY,
                                 double predFinalX, double predFinalY, bool predPot);

    void recordPostShotOutcomeAndTrain(double actualX, double actualY,
                                       bool actualPot, int bounces);

    void trainOnShotSample(const ShotObservation& sample);

    // ── Parameter access ────────────────────────────────────────────────────
    LearnedParameters    getLearnedParameters() const;
    LivePhysicsConstants getLiveConstants()      const;

    double getCorrectedAimAngle(double rawAimAngle,
                                double cutAngleDeg,
                                double distanceCm) const;

    // ── Persistence ─────────────────────────────────────────────────────────
    void saveModel();
    void loadModel();

private:
    AdaptiveLearningEngine();
    ~AdaptiveLearningEngine() = default;

    // helpers
    bool  tryReadFloatAtOffset(void* base, uintptr_t offset, float& out);
    float emaSmoothF(float prev, float next, float alpha);

    mutable std::mutex    m_mutex;
    std::string           m_storagePath;
    LearnedParameters     m_params;
    LivePhysicsConstants  m_live;
    ShotObservation       m_pendingShot;
    bool                  m_hasPendingShot = false;

    // Per-second update state
    std::atomic<int64_t>  m_lastSecondUpdateMs{0};

    // Rolling shot buffer for second-granularity refinement (ring buffer, 30 entries)
    static constexpr int  kBufSize = 30;
    ShotObservation       m_shotBuf[kBufSize];
    int                   m_shotBufHead = 0;
    int                   m_shotBufCount = 0;

    const float LEARNING_RATE = 0.045f;
    const float EMA_ALPHA     = 0.15f; // EMA weight per second update
};

} // namespace ai
} // namespace catengine
