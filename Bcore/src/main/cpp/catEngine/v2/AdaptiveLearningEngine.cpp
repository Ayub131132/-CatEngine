#include "AdaptiveLearningEngine.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include "../../Log.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace catengine {
namespace ai {

AdaptiveLearningEngine::AdaptiveLearningEngine() {
    m_storagePath = "/data/data/com.engine.cat/files/cat_learned_physics.json";
}

void AdaptiveLearningEngine::init(const std::string& storagePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!storagePath.empty()) {
        m_storagePath = storagePath;
    }
    loadModel();
}

void AdaptiveLearningEngine::recordPreShotPrediction(double stX, double stY, double angle, double force,
                                                     int puckId, int puckType, double pX, double pY,
                                                     double predFinalX, double predFinalY, bool predPot) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pendingShot.strikerStartX = stX;
    m_pendingShot.strikerStartY = stY;
    m_pendingShot.aimAngle = angle;
    m_pendingShot.aimForce = force;
    m_pendingShot.targetPuckId = puckId;
    m_pendingShot.targetPuckType = puckType;
    m_pendingShot.puckStartX = pX;
    m_pendingShot.puckStartY = pY;
    m_pendingShot.predFinalPuckX = predFinalX;
    m_pendingShot.predFinalPuckY = predFinalY;
    m_pendingShot.predPotted = predPot;
    m_hasPendingShot = true;
}

void AdaptiveLearningEngine::recordPostShotOutcomeAndTrain(double actualX, double actualY, bool actualPot, int bounces) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_hasPendingShot) return;

    m_pendingShot.actualFinalPuckX = actualX;
    m_pendingShot.actualFinalPuckY = actualY;
    m_pendingShot.actualPotted = actualPot;
    m_pendingShot.actualBounces = bounces;

    trainOnShotSample(m_pendingShot);
    m_hasPendingShot = false;
    saveModel();
}

void AdaptiveLearningEngine::trainOnShotSample(const ShotObservation& s) {
    if (s.targetPuckId < 0) return;

    double predDist = std::hypot(s.predFinalPuckX - s.puckStartX, s.predFinalPuckY - s.puckStartY);
    double actualDist = std::hypot(s.actualFinalPuckX - s.puckStartX, s.actualFinalPuckY - s.puckStartY);

    // 1. Friction & Force Scale adaptation:
    // If actual travel exceeded prediction, table friction is lower than modeled and/or force scale was higher
    if (predDist > 2.0 && actualDist > 2.0 && s.actualBounces == 0) {
        double distErrRatio = (actualDist - predDist) / predDist;
        // Adjust friction opposite to distance error
        m_params.tableFriction -= static_cast<float>(distErrRatio * LEARNING_RATE * 0.15);
        m_params.tableFriction = std::clamp(m_params.tableFriction, 0.16f, 0.40f);

        // Adjust force scale factor proportional to distance error
        m_params.forceScaleFactor -= static_cast<float>(distErrRatio * LEARNING_RATE * 0.10);
        m_params.forceScaleFactor = std::clamp(m_params.forceScaleFactor, 0.70f, 1.30f);
    }

    // 2. Cushion Restitution adaptation (for bank shots)
    if (s.actualBounces > 0 && predDist > 5.0 && actualDist > 5.0) {
        double reboundErrRatio = (actualDist - predDist) / predDist;
        m_params.cushionRestitution += static_cast<float>(reboundErrRatio * LEARNING_RATE * 0.10);
        m_params.cushionRestitution = std::clamp(m_params.cushionRestitution, 0.75f, 0.96f);
    }

    // 3. Cut-induced throw & aim angular bias adaptation
    double predAngle = std::atan2(s.predFinalPuckY - s.puckStartY, s.predFinalPuckX - s.puckStartX);
    double actualAngle = std::atan2(s.actualFinalPuckY - s.puckStartY, s.actualFinalPuckX - s.puckStartX);
    double angleErr = actualAngle - predAngle;
    while (angleErr > M_PI) angleErr -= 2.0 * M_PI;
    while (angleErr < -M_PI) angleErr += 2.0 * M_PI;

    if (std::abs(angleErr) < 0.35) { // Reject noisy collision cascades
        m_params.aimBiasCorrection += static_cast<float>(angleErr * LEARNING_RATE * 0.5);
        m_params.aimBiasCorrection = std::clamp(m_params.aimBiasCorrection, -0.05f, 0.05f);
    }

    // 4. Update experience count and confidence
    m_params.totalShotsLearned++;
    m_params.confidenceScore = std::min(0.99f, 0.50f + (m_params.totalShotsLearned * 0.015f));

    ALOGI("[AI LEARNING] Shot #%d learned! mu=%.3f, forceScale=%.3f, e=%.3f, bias=%.4f rad, Confidence=%.1f%%",
          m_params.totalShotsLearned, m_params.tableFriction, m_params.forceScaleFactor,
          m_params.cushionRestitution, m_params.aimBiasCorrection, m_params.confidenceScore * 100.0f);
}

double AdaptiveLearningEngine::getCorrectedAimAngle(double rawAimAngle, double cutAngleDeg, double distanceCm) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    double corrected = rawAimAngle + m_params.aimBiasCorrection;

    // Cut-throw compensation: oblique cut shots have minor friction throw
    if (std::abs(cutAngleDeg) > 5.0 && std::abs(cutAngleDeg) < 70.0) {
        double throwOffset = (cutAngleDeg / 90.0) * m_params.cutThrowFactor;
        corrected += throwOffset;
    }

    return corrected;
}

LearnedParameters AdaptiveLearningEngine::getLearnedParameters() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_params;
}

void AdaptiveLearningEngine::saveModel() {
    try {
        std::ofstream ofs(m_storagePath, std::ios::trunc);
        if (ofs.is_open()) {
            ofs << "{\n";
            ofs << "  \"totalShotsLearned\": " << m_params.totalShotsLearned << ",\n";
            ofs << "  \"confidenceScore\": " << m_params.confidenceScore << ",\n";
            ofs << "  \"cushionRestitution\": " << m_params.cushionRestitution << ",\n";
            ofs << "  \"tableFriction\": " << m_params.tableFriction << ",\n";
            ofs << "  \"cutThrowFactor\": " << m_params.cutThrowFactor << ",\n";
            ofs << "  \"pocketCaptureRadius\": " << m_params.pocketCaptureRadius << ",\n";
            ofs << "  \"aimBiasCorrection\": " << m_params.aimBiasCorrection << ",\n";
            ofs << "  \"forceScaleFactor\": " << m_params.forceScaleFactor << "\n";
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
            if (colon != std::string::npos) {
                try {
                    std::string valStr = line.substr(colon + 1);
                    valStr.erase(std::remove(valStr.begin(), valStr.end(), ','), valStr.end());
                    valStr.erase(std::remove(valStr.begin(), valStr.end(), ' '), valStr.end());
                    valStr.erase(std::remove(valStr.begin(), valStr.end(), '\r'), valStr.end());
                    valStr.erase(std::remove(valStr.begin(), valStr.end(), '\n'), valStr.end());

                    if (line.find("totalShotsLearned") != std::string::npos) {
                        m_params.totalShotsLearned = std::stoi(valStr);
                    } else if (line.find("confidenceScore") != std::string::npos) {
                        m_params.confidenceScore = std::stof(valStr);
                    } else if (line.find("cushionRestitution") != std::string::npos) {
                        m_params.cushionRestitution = std::stof(valStr);
                    } else if (line.find("tableFriction") != std::string::npos) {
                        m_params.tableFriction = std::stof(valStr);
                    } else if (line.find("cutThrowFactor") != std::string::npos) {
                        m_params.cutThrowFactor = std::stof(valStr);
                    } else if (line.find("pocketCaptureRadius") != std::string::npos) {
                        m_params.pocketCaptureRadius = std::stof(valStr);
                    } else if (line.find("aimBiasCorrection") != std::string::npos) {
                        m_params.aimBiasCorrection = std::stof(valStr);
                    } else if (line.find("forceScaleFactor") != std::string::npos) {
                        m_params.forceScaleFactor = std::stof(valStr);
                    }
                } catch (...) {}
            }
        }
        ifs.close();
        ALOGI("[AI LEARNING] Loaded existing model: %d shots, mu=%.3f, e=%.3f, conf=%.1f%%",
              m_params.totalShotsLearned, m_params.tableFriction, m_params.cushionRestitution,
              m_params.confidenceScore * 100.0f);
    } catch (...) {}
}

} // namespace ai
} // namespace catengine

