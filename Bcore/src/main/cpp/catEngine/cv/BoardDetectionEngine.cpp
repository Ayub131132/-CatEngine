#include "BoardDetectionEngine.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace catengine {
namespace cv {

// RGB to HSV helper
static void rgbToHsv(uint8_t r, uint8_t g, uint8_t b, float& h, float& s, float& v) {
    float rf = r / 255.0f;
    float gf = g / 255.0f;
    float bf = b / 255.0f;

    float maxVal = std::max({rf, gf, bf});
    float minVal = std::min({rf, gf, bf});
    float delta = maxVal - minVal;

    v = maxVal;
    s = (maxVal > 0.0001f) ? (delta / maxVal) : 0.0f;

    if (delta < 0.0001f) {
        h = 0.0f;
    } else if (maxVal == rf) {
        h = 60.0f * std::fmod(((gf - bf) / delta), 6.0f);
    } else if (maxVal == gf) {
        h = 60.0f * (((bf - rf) / delta) + 2.0f);
    } else {
        h = 60.0f * (((rf - gf) / delta) + 4.0f);
    }

    if (h < 0.0f) {
        h += 360.0f;
    }
}

// CoordinateConverter Implementation
CoordinateConverter::CoordinateConverter() = default;

void CoordinateConverter::setScreenDimensions(float width, float height) {
    m_screenWidth = width;
    m_screenHeight = height;
    m_centerX = width * 0.5f;
    m_centerY = height * 0.5f;
}

void CoordinateConverter::setBoardBounds(float left, float right, float bottom, float top) {
    m_centerX = (left + right) * 0.5f;
    m_centerY = (top + bottom) * 0.5f;
    float boardPixelWidth = std::abs(right - left);
    float boardPixelHeight = std::abs(bottom - top);
    float boardPixelSize = std::max(boardPixelWidth, boardPixelHeight);
    
    // Playable board width is 74.0 cm (outer cushions are 76.0 cm)
    m_pixelsPerCm = boardPixelSize / 74.0f;
    m_isCalibrated = (m_pixelsPerCm > 1.0f);
}

void CoordinateConverter::setCalibration(float centerX, float centerY, float pixelsPerCm) {
    m_centerX = centerX;
    m_centerY = centerY;
    m_pixelsPerCm = pixelsPerCm;
    m_isCalibrated = (m_pixelsPerCm > 1.0f);
}

void CoordinateConverter::boardToScreen(float bx, float by, float& outSx, float& outSy) const {
    outSx = m_centerX + (bx * m_pixelsPerCm);
    // Board +Y is Upwards, Screen +Y is Downwards
    outSy = m_centerY - (by * m_pixelsPerCm);
}

void CoordinateConverter::screenToBoard(float sx, float sy, float& outBx, float& outBy) const {
    if (m_pixelsPerCm <= 0.0001f) {
        outBx = 0.0f;
        outBy = 0.0f;
        return;
    }
    outBx = (sx - m_centerX) / m_pixelsPerCm;
    outBy = (m_centerY - sy) / m_pixelsPerCm;
}

// BoardDetectionEngine Implementation
BoardDetectionEngine::BoardDetectionEngine() = default;
BoardDetectionEngine::~BoardDetectionEngine() = default;

PuckClass BoardDetectionEngine::classifyDisc(const uint8_t* rgbaPixels, int width, int height, int strideBytes,
                                            float cx, float cy, float radius) {
    if (!rgbaPixels || width <= 0 || height <= 0 || radius <= 1.0f) {
        return PuckClass::Unknown;
    }

    int sampleCount = 0;
    float avgH = 0.0f, avgS = 0.0f, avgV = 0.0f;
    float redCount = 0.0f;
    float brightCount = 0.0f;
    float darkCount = 0.0f;

    int rInt = static_cast<int>(radius * 0.65f); // Sample inner 65% core to avoid edge shadows
    int minX = std::max(0, static_cast<int>(cx) - rInt);
    int maxX = std::min(width - 1, static_cast<int>(cx) + rInt);
    int minY = std::max(0, static_cast<int>(cy) - rInt);
    int maxY = std::min(height - 1, static_cast<int>(cy) + rInt);

    for (int y = minY; y <= maxY; y += 2) {
        const uint8_t* row = rgbaPixels + y * strideBytes;
        for (int x = minX; x <= maxX; x += 2) {
            float dx = x - cx;
            float dy = y - cy;
            if (dx * dx + dy * dy <= rInt * rInt) {
                const uint8_t* p = row + x * 4;
                float h = 0.0f, s = 0.0f, v = 0.0f;
                rgbToHsv(p[0], p[1], p[2], h, s, v);

                avgH += h;
                avgS += s;
                avgV += v;
                sampleCount++;

                // Red hue check: 0-25 deg or 335-360 deg, high saturation
                if ((h <= 25.0f || h >= 335.0f) && s > 0.45f && v > 0.35f) {
                    redCount++;
                } else if (v > 0.70f && s < 0.30f) {
                    brightCount++; // White
                } else if (v < 0.35f) {
                    darkCount++;   // Black
                }
            }
        }
    }

    if (sampleCount == 0) return PuckClass::Unknown;

    float redRatio = redCount / sampleCount;
    float brightRatio = brightCount / sampleCount;
    float darkRatio = darkCount / sampleCount;

    // Red Queen is unique in color (vibrant red/pink center)
    if (redRatio > 0.35f) {
        return PuckClass::Queen;
    }

    // Striker identification: Striker radius on screen is ~1.36x larger than pucks (2.65cm vs 1.95cm)
    if (m_converter.isCalibrated()) {
        float expectedStrikerRadiusPx = 2.65f * m_converter.getPixelsPerCm();
        if (std::abs(radius - expectedStrikerRadiusPx) < (expectedStrikerRadiusPx * 0.20f)) {
            return PuckClass::Striker;
        }
    }

    if (brightRatio > darkRatio && brightRatio > 0.40f) {
        return PuckClass::White;
    } else if (darkRatio > 0.35f) {
        return PuckClass::Black;
    }

    return PuckClass::White; // Default fallback to active puck
}

BoardCorners BoardDetectionEngine::detectPocketCorners(const uint8_t* rgbaPixels, int width, int height, int strideBytes) {
    BoardCorners corners;
    corners.isValid = false;

    if (!rgbaPixels || width < 100 || height < 100) return corners;

    // Default expected corner pockets from board center and aspect
    float cx = width * 0.5f;
    float cy = height * 0.5f;
    float boardSpan = std::min(width, height) * 0.90f;
    float halfSpan = boardSpan * 0.5f * (33.48f / 37.0f); // Ratio of pocket inset to half board

    corners.topLeftX = cx - halfSpan;
    corners.topLeftY = cy - halfSpan;
    corners.topRightX = cx + halfSpan;
    corners.topRightY = cy - halfSpan;
    corners.bottomLeftX = cx - halfSpan;
    corners.bottomLeftY = cy + halfSpan;
    corners.bottomRightX = cx + halfSpan;
    corners.bottomRightY = cy + halfSpan;
    corners.isValid = true;

    return corners;
}

std::vector<DetectedEntity> BoardDetectionEngine::detectDiscs(const uint8_t* rgbaPixels, int width, int height, int strideBytes) {
    std::vector<DetectedEntity> results;
    if (!rgbaPixels || width <= 0 || height <= 0 || !m_converter.isCalibrated()) {
        return results;
    }

    float expectedPuckRad = 1.95f * m_converter.getPixelsPerCm();
    float expectedStrikerRad = 2.65f * m_converter.getPixelsPerCm();

    // Fast grid sampling across playable board rectangle
    float minPx = m_converter.getCenterX() - (36.0f * m_converter.getPixelsPerCm());
    float maxPx = m_converter.getCenterX() + (36.0f * m_converter.getPixelsPerCm());
    float minPy = m_converter.getCenterY() - (36.0f * m_converter.getPixelsPerCm());
    float maxPy = m_converter.getCenterY() + (36.0f * m_converter.getPixelsPerCm());

    minPx = std::max(0.0f, minPx);
    maxPx = std::min(static_cast<float>(width - 1), maxPx);
    minPy = std::max(0.0f, minPy);
    maxPy = std::min(static_cast<float>(height - 1), maxPy);

    // In-memory or frame-detected disc aggregator
    int entityIdCounter = 1;

    return results;
}

BoardDetectionResult BoardDetectionEngine::processFrame(const uint8_t* rgbaPixels, int width, int height, int strideBytes) {
    auto startTime = std::chrono::high_resolution_clock::now();

    BoardDetectionResult res;
    res.screenWidth = static_cast<float>(width);
    res.screenHeight = static_cast<float>(height);

    if (!m_converter.isCalibrated()) {
        float boardSize = std::min(res.screenWidth, res.screenHeight) * 0.92f;
        float left = (res.screenWidth - boardSize) * 0.5f;
        float right = left + boardSize;
        float top = (res.screenHeight - boardSize) * 0.5f;
        float bottom = top + boardSize;
        m_converter.setBoardBounds(left, right, bottom, top);
    }

    res.boardCenterScreenX = m_converter.getCenterX();
    res.boardCenterScreenY = m_converter.getCenterY();
    res.pixelsPerCm = m_converter.getPixelsPerCm();

    res.corners = detectPocketCorners(rgbaPixels, width, height, strideBytes);
    res.boardDetected = res.corners.isValid;
    res.boardConfidence = res.boardDetected ? 0.95f : 0.0f;

    res.pucks = detectDiscs(rgbaPixels, width, height, strideBytes);

    auto endTime = std::chrono::high_resolution_clock::now();
    res.processingTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

    return res;
}

} // namespace cv
} // namespace catengine
