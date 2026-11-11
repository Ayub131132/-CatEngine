#include "../../Bcore/src/main/cpp/catEngine/cv/BoardDetectionEngine.h"
#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>

using namespace catengine::cv;

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

// Test 1: Coordinate Conversion Bidirectional Invariants
void test_coordinate_conversion() {
    std::cout << "\n[TEST 1] Coordinate Conversion Bidirectional Invariants..." << std::endl;

    CoordinateConverter conv;
    conv.setScreenDimensions(1080.0f, 2400.0f);
    // Table width = 1000px, top=700, bottom=1700
    conv.setBoardBounds(40.0f, 1040.0f, 1700.0f, 700.0f);

    TEST_ASSERT(conv.isCalibrated(), "Converter must be calibrated after setting bounds");

    // Test Center (0, 0)
    float sx = 0.0f, sy = 0.0f;
    conv.boardToScreen(0.0f, 0.0f, sx, sy);
    TEST_ASSERT(std::abs(sx - 540.0f) < 0.1f, "Board center X must map to Screen Center X (540)");
    TEST_ASSERT(std::abs(sy - 1200.0f) < 0.1f, "Board center Y must map to Screen Center Y (1200)");

    float bx = 0.0f, by = 0.0f;
    conv.screenToBoard(sx, sy, bx, by);
    TEST_ASSERT(std::abs(bx - 0.0f) < 0.001f, "Screen center must map back to Board center X");
    TEST_ASSERT(std::abs(by - 0.0f) < 0.001f, "Screen center must map back to Board center Y");

    // Test Corner Pocket (33.48, 33.48)
    conv.boardToScreen(33.48f, 33.48f, sx, sy);
    // Board +Y is UP, so screen Y should be LESS than centerY (< 1200)
    TEST_ASSERT(sy < 1200.0f, "Positive board Y must map to upper screen (lower pixel Y)");
    TEST_ASSERT(sx > 540.0f, "Positive board X must map to right screen (higher pixel X)");

    conv.screenToBoard(sx, sy, bx, by);
    TEST_ASSERT(std::abs(bx - 33.48f) < 0.001f, "Pocket X must map back exactly");
    TEST_ASSERT(std::abs(by - 33.48f) < 0.001f, "Pocket Y must map back exactly");

    std::cout << "    Board (33.48, 33.48) -> Screen (" << sx << ", " << sy << ") -> Reconstructed (" << bx << ", " << by << ")" << std::endl;
    TEST_PASS("Bidirectional coordinate transformations achieve sub-millimeter precision (< 0.01 mm).");
}

// Test 2: Color Classification Signatures (HSV)
void test_color_classification() {
    std::cout << "\n[TEST 2] HSV Color Classification Signatures..." << std::endl;

    BoardDetectionEngine engine;
    engine.getConverter().setScreenDimensions(500, 500);
    engine.getConverter().setCalibration(250, 250, 6.0f);

    const int w = 100, h = 100;
    std::vector<uint8_t> redBuffer(w * h * 4, 0);
    std::vector<uint8_t> whiteBuffer(w * h * 4, 0);
    std::vector<uint8_t> blackBuffer(w * h * 4, 0);

    // Fill synthetic image buffers
    for (int i = 0; i < w * h; i++) {
        // Red Queen (R=220, G=30, B=30, A=255)
        redBuffer[i * 4 + 0] = 220;
        redBuffer[i * 4 + 1] = 30;
        redBuffer[i * 4 + 2] = 30;
        redBuffer[i * 4 + 3] = 255;

        // White Puck (R=240, G=240, B=235, A=255)
        whiteBuffer[i * 4 + 0] = 240;
        whiteBuffer[i * 4 + 1] = 240;
        whiteBuffer[i * 4 + 2] = 235;
        whiteBuffer[i * 4 + 3] = 255;

        // Black Puck (R=35, G=35, B=35, A=255)
        blackBuffer[i * 4 + 0] = 35;
        blackBuffer[i * 4 + 1] = 35;
        blackBuffer[i * 4 + 2] = 35;
        blackBuffer[i * 4 + 3] = 255;
    }

    float puckRadiusPx = 1.95f * 6.0f; // 11.7px
    float strikerRadiusPx = 2.65f * 6.0f; // 15.9px

    PuckClass cRed = engine.classifyDisc(redBuffer.data(), w, h, w * 4, 50.0f, 50.0f, puckRadiusPx);
    PuckClass cWhite = engine.classifyDisc(whiteBuffer.data(), w, h, w * 4, 50.0f, 50.0f, puckRadiusPx);
    PuckClass cBlack = engine.classifyDisc(blackBuffer.data(), w, h, w * 4, 50.0f, 50.0f, puckRadiusPx);
    PuckClass cStriker = engine.classifyDisc(whiteBuffer.data(), w, h, w * 4, 50.0f, 50.0f, strikerRadiusPx);

    TEST_ASSERT(cRed == PuckClass::Queen, "Red pixel block must classify as Queen");
    TEST_ASSERT(cWhite == PuckClass::White, "Bright pixel block with puck radius must classify as White");
    TEST_ASSERT(cBlack == PuckClass::Black, "Dark pixel block must classify as Black");
    TEST_ASSERT(cStriker == PuckClass::Striker, "Entity with 15.9px radius must classify as Striker");

    TEST_PASS("HSV pixel classification reliably distinguishes Queen, White, Black pucks, and Striker.");
}

// Test 3: Frame Processing Throughput & Pocket Detection
void test_frame_processing_throughput() {
    std::cout << "\n[TEST 3] Frame Processing Latency & Pockets..." << std::endl;

    BoardDetectionEngine engine;
    const int w = 1080, h = 1920;
    std::vector<uint8_t> frameBuffer(w * h * 4, 128); // 1080x1920 RGBA frame

    BoardDetectionResult res = engine.processFrame(frameBuffer.data(), w, h, w * 4);

    TEST_ASSERT(res.boardDetected == true, "Board must be detected");
    TEST_ASSERT(res.corners.isValid == true, "Corner pockets must be valid");
    TEST_ASSERT(res.processingTimeUs >= 0, "Processing time must be recorded non-negative");

    std::cout << "    Full frame 1080x1920 analysis latency: " << res.processingTimeUs << " us (" 
              << (res.processingTimeUs / 1000.0) << " ms)" << std::endl;

    TEST_ASSERT(res.processingTimeUs < 10000, "Processing time must be under 10 ms for real-time 60 FPS operation");
    TEST_PASS("Full frame vision pipeline completes comfortably under 10 ms real-time budget.");
}

int main() {
    std::cout << "===========================================================" << std::endl;
    std::cout << "  CatEngine BoardDetectionEngine Standalone Unit Tests   " << std::endl;
    std::cout << "===========================================================" << std::endl;

    test_coordinate_conversion();
    test_color_classification();
    test_frame_processing_throughput();

    std::cout << "\n===========================================================" << std::endl;
    std::cout << "  TEST SUMMARY: " << g_testsPassed << " PASSED, " << g_testsFailed << " FAILED" << std::endl;
    std::cout << "===========================================================" << std::endl;

    return (g_testsFailed == 0) ? 0 : 1;
}
