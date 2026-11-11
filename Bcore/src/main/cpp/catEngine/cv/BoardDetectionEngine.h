#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <cmath>

namespace catengine {
namespace cv {

enum class PuckClass {
    Unknown = -1,
    Striker = 0,
    White = 1,
    Black = 2,
    Queen = 3
};

struct DetectedEntity {
    int id = -1;
    PuckClass puckClass = PuckClass::Unknown;
    float screenX = 0.0f;     // Screen pixels
    float screenY = 0.0f;
    float screenRadius = 0.0f;
    float boardX = 0.0f;      // Board cm [-37.0, +37.0]
    float boardY = 0.0f;
    float boardRadius = 1.95f;
    float confidence = 0.0f;  // [0.0, 1.0]
};

struct BoardCorners {
    float topLeftX = 0.0f, topLeftY = 0.0f;
    float topRightX = 0.0f, topRightY = 0.0f;
    float bottomLeftX = 0.0f, bottomLeftY = 0.0f;
    float bottomRightX = 0.0f, bottomRightY = 0.0f;
    bool isValid = false;
};

struct BoardDetectionResult {
    bool boardDetected = false;
    float boardConfidence = 0.0f;
    BoardCorners corners;
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;
    float boardCenterScreenX = 0.0f;
    float boardCenterScreenY = 0.0f;
    float pixelsPerCm = 0.0f;
    DetectedEntity striker;
    bool strikerFound = false;
    std::vector<DetectedEntity> pucks;
    int64_t processingTimeUs = 0;
};

class CoordinateConverter {
public:
    CoordinateConverter();
    void setScreenDimensions(float width, float height);
    void setBoardBounds(float left, float right, float bottom, float top);
    void setCalibration(float centerX, float centerY, float pixelsPerCm);
    
    // Board [-37.0, +37.0] cm -> Screen pixels
    void boardToScreen(float bx, float by, float& outSx, float& outSy) const;
    
    // Screen pixels -> Board [-37.0, +37.0] cm
    void screenToBoard(float sx, float sy, float& outBx, float& outBy) const;

    float getPixelsPerCm() const { return m_pixelsPerCm; }
    float getCenterX() const { return m_centerX; }
    float getCenterY() const { return m_centerY; }
    bool isCalibrated() const { return m_isCalibrated; }

private:
    float m_screenWidth = 1080.0f;
    float m_screenHeight = 2400.0f;
    float m_centerX = 540.0f;
    float m_centerY = 1200.0f;
    float m_pixelsPerCm = 14.0f;
    bool m_isCalibrated = false;
};

class BoardDetectionEngine {
public:
    BoardDetectionEngine();
    ~BoardDetectionEngine();

    // Process raw RGBA pixel buffer and output structured board state
    BoardDetectionResult processFrame(const uint8_t* rgbaPixels, int width, int height, int strideBytes);

    // Fast disc detection using color thresholding & geometric circle centroiding
    std::vector<DetectedEntity> detectDiscs(const uint8_t* rgbaPixels, int width, int height, int strideBytes);

    // Corner pocket detection from radial dark circular wells
    BoardCorners detectPocketCorners(const uint8_t* rgbaPixels, int width, int height, int strideBytes);

    // Classification of detected disc by HSV color signature
    PuckClass classifyDisc(const uint8_t* rgbaPixels, int width, int height, int strideBytes, float cx, float cy, float radius);

    CoordinateConverter& getConverter() { return m_converter; }
    const CoordinateConverter& getConverter() const { return m_converter; }

private:
    CoordinateConverter m_converter;
};

} // namespace cv
} // namespace catengine

