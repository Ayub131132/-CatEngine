#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <atomic>
#include <mutex>

namespace catengine {
namespace ai {

struct ShotObservation {
    double strikerStartX = 0.0;
    double strikerStartY = 0.0;
    double aimAngle = 0.0;
    double aimForce = 1.0;
    
    int targetPuckId = -1;
    int targetPuckType = 1;
    double puckStartX = 0.0;
    double puckStartY = 0.0;
    
    // Predicted
    double predFinalPuckX = 0.0;
    double predFinalPuckY = 0.0;
    bool predPotted = false;
    
    // Observed actual outcome
    double actualFinalPuckX = 0.0;
    double actualFinalPuckY = 0.0;
    bool actualPotted = false;
    int actualBounces = 0;
};

struct LearnedParameters {
    float cushionRestitution = 0.88f;
    float tableFriction = 0.28f;
    float cutThrowFactor = 0.02f;
    float pocketCaptureRadius = 2.65f;
    float aimBiasCorrection = 0.00f; // Angular bias in radians
    float forceScaleFactor = 1.00f;  // Real-time learned force scaling factor
    int totalShotsLearned = 0;
    float confidenceScore = 0.50f;
};

class AdaptiveLearningEngine {
public:
    static AdaptiveLearningEngine& getInstance() {
        static AdaptiveLearningEngine instance;
        return instance;
    }

    void init(const std::string& storagePath);
    
    // Call right before shot execution to record the predicted intent
    void recordPreShotPrediction(double stX, double stY, double angle, double force,
                                 int puckId, int puckType, double pX, double pY,
                                 double predFinalX, double predFinalY, bool predPot);

    // Call after all discs settle to record the ground truth outcome and train the model
    void recordPostShotOutcomeAndTrain(double actualX, double actualY, bool actualPot, int bounces);

    // Online gradient descent step
    void trainOnShotSample(const ShotObservation& sample);

    // Access current learned parameters
    LearnedParameters getLearnedParameters() const;

    // Apply real-time learned correction to aim angle
    double getCorrectedAimAngle(double rawAimAngle, double cutAngleDeg, double distanceCm) const;

    void saveModel();
    void loadModel();

private:
    AdaptiveLearningEngine();
    ~AdaptiveLearningEngine() = default;

    mutable std::mutex m_mutex;
    std::string m_storagePath;
    LearnedParameters m_params;
    ShotObservation m_pendingShot;
    bool m_hasPendingShot = false;
    
    const float LEARNING_RATE = 0.045f;
};

} // namespace ai
} // namespace catengine

