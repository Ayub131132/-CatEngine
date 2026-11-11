#define TAG "CatEngine"
#include "../Log.h"
#include "GraphicsEngine.h"
#include "v2/PhysicsEngineV2.h"
#include "v2/AdaptiveLearningEngine.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <dlfcn.h>
#include <cmath>
#include <mutex>
#include <atomic>
#include <vector>
#include <algorithm>
#include <cstring>
#include <thread>
#include <chrono>
#include "Dobby/dobby.h"

#define GRAPHICS_TAG "CatEngine"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const float kBoardHalfSize       = 37.0f;
static const float kBaselineY           = -25.35f;
static const float kOpponentBaselineY   = 25.35f;

static std::atomic<float> s_strikerRadius(2.65f);
static std::atomic<float> s_puckRadius(1.95f);
static std::atomic<float> s_pocketRadius(2.65f);
std::atomic<float> s_pocketInset(33.48f);
static std::atomic<bool>  s_hasUserPocketRadius(false);
static std::atomic<float> s_maxTravelDistance(150.0f);
static std::atomic<float> s_guidelineThickness(0.45f);
static std::atomic<int>   s_colorTheme(0);
static std::atomic<bool>  s_showPrediction(true);
static std::atomic<bool>  s_showPocket(true);
static std::atomic<float> s_lineTransparency(1.0f);
static std::atomic<bool>  s_keepLineAfterShot(true);
static std::atomic<double> s_freezeBaselineX(0.0);
static std::atomic<bool>  s_waitingForHandMove(false);

static std::atomic<uint32_t> s_customStrikerColor(0xFFFFFFFF);
static std::atomic<uint32_t> s_customMyPuckColor(0xFF2699FF);
static std::atomic<uint32_t> s_customOpponentPuckColor(0xFFFFD91A);
static std::atomic<bool>     s_hasCustomColors(false);

struct BodyColor {
    float r, g, b, a;
};

// Color scheme requested by user:
// Striker: WHITE
// White puck (Player): BLUE
// Black puck (Opponent): YELLOW
// Red puck (Queen): RED
// Pocket Pot Success: ELECTRIC NEON EMERALD (#00FF66)
static const BodyColor kStrikerColor   = { 1.00f, 1.00f, 1.00f, 0.95f }; // White for striker
static const BodyColor kWhitePuckColor = { 0.15f, 0.60f, 1.00f, 0.95f }; // Blue for white puck
static const BodyColor kBlackPuckColor = { 1.00f, 0.85f, 0.10f, 0.95f }; // Yellow for black puck
static const BodyColor kRedPuckColor   = { 1.00f, 0.15f, 0.15f, 0.95f }; // Red for red puck
static const BodyColor kPotColor       = { 0.00f, 1.00f, 0.40f, 0.98f }; // Electric Neon Emerald for potted pucks & target pocket

static BodyColor argbToBodyColor(uint32_t argb) {
    BodyColor c;
    float a = ((argb >> 24) & 0xFF) / 255.0f;
    if (a <= 0.01f) a = 0.95f;
    c.a = a;
    c.r = ((argb >> 16) & 0xFF) / 255.0f;
    c.g = ((argb >> 8) & 0xFF) / 255.0f;
    c.b = (argb & 0xFF) / 255.0f;
    return c;
}

static BodyColor getStrikerColor() {
    if (s_hasCustomColors.load()) {
        return argbToBodyColor(s_customStrikerColor.load());
    }
    return kStrikerColor;
}

static BodyColor getEntityColor(int entityId, int puckType = 1) {
    if (puckType == 0 || entityId == -1) {
        return getStrikerColor();
    } else if (puckType == 3) {
        return kRedPuckColor;   // Red Queen
    } else if (puckType == 2) {
        if (s_hasCustomColors.load()) {
            return argbToBodyColor(s_customOpponentPuckColor.load());
        }
        return kBlackPuckColor; // Yellow for black puck
    } else if (puckType == 1) {
        if (s_hasCustomColors.load()) {
            return argbToBodyColor(s_customMyPuckColor.load());
        }
        return kWhitePuckColor; // Blue for white puck
    }
    return kWhitePuckColor;
}

static std::atomic<bool>  s_showBorder(false);
static std::atomic<bool>  s_matchActive(false);
static std::atomic<bool>  s_hasManualBounds(false);
static std::atomic<float> s_manualL(0.0f);
static std::atomic<float> s_manualR(0.0f);
static std::atomic<float> s_manualB(0.0f);
static std::atomic<float> s_manualT(0.0f);

static std::atomic<bool>  s_hasEngineCalibration(false);
static std::atomic<float> s_autoCenterX(0.0f);
static std::atomic<float> s_autoCenterY(0.0f);
static std::atomic<float> s_autoScale(1.0f);

// Native Table Affine Transform (a, b, c, d, tx, ty) from TableNode / _tableContainer
static std::atomic<bool>   s_hasNativeAffine(false);
static std::atomic<double> s_nativeAffineA(1.0);
static std::atomic<double> s_nativeAffineB(0.0);
static std::atomic<double> s_nativeAffineC(0.0);
static std::atomic<double> s_nativeAffineD(1.0);
static std::atomic<double> s_nativeAffineTx(0.0);
static std::atomic<double> s_nativeAffineTy(0.0);

static std::atomic<float> s_baselineMinX(-23.85f);
static std::atomic<float> s_baselineMaxX( 23.85f);

// Review freeze: retains previous shot lines/circles until next shot pull
static std::atomic<bool>  s_isReviewFrozen(false);

// Carrom Pool libgame.so telemetry-matched defaults:
// StrikerFriction=0.1600 (yields authentic travel distance matching game physics), StrikerMass=3.00 (MassRatio=3.00:1)
static std::atomic<double> s_strikerFriction(0.1600);
static std::atomic<double> s_puckFriction(0.1080);
static std::atomic<double> s_restitutionBody(1.0000);
static std::atomic<double> s_pocketDropMaxSpeed(180.0);
static std::atomic<double> s_pocketGravity(980.0);
static std::atomic<double> s_restitutionCushion(0.8800);
static std::atomic<double> s_strikerMass(3.0000);
static std::atomic<double> s_puckMass(1.0000);
static std::atomic<double> s_powerMultiplier(1.0000);

static std::atomic<double> s_minPower(12.20);
static std::atomic<double> s_maxPower(172.20);
static std::atomic<double> s_strikerBonus(3.20);

static std::atomic<bool>   s_userManualPhysics(false);
static std::atomic<double> s_contactFriction(0.050);
static std::atomic<double> s_cushionTangentFriction(0.900);

static std::mutex s_posMutex;
static std::mutex s_polyMutex;
static std::vector<catengine::v2::Point2D> s_cushionPolygon;

void GraphicsEngine_SetCushionPolygon(const double* coords, int pointCount) {
    if (!coords || pointCount < 3) return;
    std::lock_guard<std::mutex> lock(s_polyMutex);
    s_cushionPolygon.clear();
    s_cushionPolygon.reserve(pointCount);
    for (int i = 0; i < pointCount; i++) {
        catengine::v2::Point2D pt;
        pt.x = coords[i * 2];
        pt.y = coords[i * 2 + 1];
        s_cushionPolygon.push_back(pt);
    }
    ALOGI("[CatEngine] Cushion polygon updated with %d vertices from TableLogic!", pointCount);
}
static double s_strikerX = 0.0;
static double s_strikerY = kBaselineY;
static bool s_hasValidStriker = true;

static std::atomic<double> s_aimAngle(M_PI / 2.0);
static std::atomic<double> s_aimForce(1.0);
static std::atomic<bool>   s_isAiming(false);
static std::atomic<bool>   s_isShotExecuting(false);
static std::atomic<bool>   s_autoShotEnabled(false);
static std::atomic<bool>   s_fastFirstShotMode(false);
static std::atomic<double> s_scanLiveX(0.0);
static std::atomic<double> s_scanLiveY(0.0);
static std::atomic<double> s_scanLiveAngle(0.0);
static std::atomic<bool>   s_isActivelySearching(false);
static std::atomic<int64_t> s_shotReleasedTimeMs(0);
static std::atomic<int>    s_targetColorMode(0); // 0 = Auto, 1 = White Only, 2 = Black Only, 3 = Any Puck
static std::atomic<int>    s_autoDetectedColor(0); // 0 = Undecided/Auto, 1 = White, 2 = Black, 3 = Any Puck

extern "C" bool CatEngine_GetLiveStrikerPos(double* outX, double* outY);
extern "C" float CatEngine_GetTurnRemainingSeconds();

struct TablePuckEntry {
    int id = -1;
    double x = 0.0;
    double y = 0.0;
    bool active = false;
    int puckType = 1;
};
static std::mutex s_puckMutex;
static std::vector<TablePuckEntry> s_puckList;

// TrajectorySegment, PuckTrajectoryInfo, AimSimulationResult are defined in GraphicsEngine.h

static std::mutex           s_persistedSimMutex;
static AimSimulationResult  s_persistedSim;
static bool                 s_hasPersistedSim = false;
static std::atomic<bool>    s_isShotInProgress(false);
static std::atomic<int64_t> s_shotFiredTimeMs(0);
static std::atomic<double>  s_frozenLaunchX(0.0);
static std::atomic<double>  s_frozenLaunchY(0.0);
static std::atomic<int64_t> s_turnScanStartTimeMs(0);
static std::atomic<bool> s_hasCachedTier2(false);
static std::atomic<bool> s_hasCachedTier3(false);

bool GraphicsEngine_IsShotInProgress() {
    return s_isShotInProgress.load();
}

void GraphicsEngine_ResetTurnScanTimer() {
    s_turnScanStartTimeMs.store(0);
    s_hasCachedTier2.store(false);
    s_hasCachedTier3.store(false);
}

int64_t GraphicsEngine_GetTurnScanStartTimeMs() {
    return s_turnScanStartTimeMs.load();
}

typedef EGLBoolean (*eglSwapBuffers_fn)(EGLDisplay dpy, EGLSurface surface);
static eglSwapBuffers_fn orig_eglSwapBuffers = nullptr;

static bool   s_glInitialized = false;
static GLuint s_lineProgram = 0;
static GLint  s_aLinePos = -1;
static GLint  s_aLineCoord = -1;
static GLint  s_uLineMVP = -1;
static GLint  s_uLineColor = -1;
static GLint  s_uLineLength = -1;
static GLint  s_uLineHalfWidth = -1;
static GLint  s_uLineFeather = -1;

static GLuint s_circleProgram = 0;
static GLint  s_aCirclePos = -1;
static GLint  s_aCircleCoord = -1;
static GLint  s_uCircleMVP = -1;
static GLint  s_uCircleColor = -1;
static GLint  s_uCircleInnerRadius = -1;
static GLint  s_uCircleOuterRadius = -1;
static GLint  s_uCircleFeather = -1;

static const char* kLineVertexShader =
    "attribute vec2 a_Pos;\n"
    "attribute vec2 a_LocalCoord;\n"
    "uniform mat4 u_MVP;\n"
    "varying vec2 v_LocalCoord;\n"
    "void main() {\n"
    "    v_LocalCoord = a_LocalCoord;\n"
    "    gl_Position = u_MVP * vec4(a_Pos, 0.0, 1.0);\n"
    "}\n";

static const char* kLineFragmentShader =
    "#extension GL_OES_standard_derivatives : enable\n"
    "precision mediump float;\n"
    "uniform vec4 u_Color;\n"
    "uniform float u_Length;\n"
    "uniform float u_HalfWidth;\n"
    "uniform float u_Feather;\n"
    "varying vec2 v_LocalCoord;\n"
    "void main() {\n"
    "    float x = v_LocalCoord.x;\n"
    "    float y = v_LocalCoord.y;\n"
    "    float dx = max(0.0, max(-x, x - u_Length));\n"
    "    float dy = abs(y);\n"
    "    float dist = length(vec2(dx, dy));\n"
    "    float fw = u_Feather;\n"
    "#ifdef GL_OES_standard_derivatives\n"
    "    fw = max(fwidth(dist) * 0.5, u_Feather * 0.25);\n"
    "#endif\n"
    "    float alpha = 1.0 - smoothstep(max(0.0, u_HalfWidth - fw), u_HalfWidth + fw, dist);\n"
    "    gl_FragColor = vec4(u_Color.rgb, u_Color.a * alpha);\n"
    "}\n";

static const char* kCircleVertexShader =
    "attribute vec2 a_Pos;\n"
    "attribute vec2 a_LocalCoord;\n"
    "uniform mat4 u_MVP;\n"
    "varying vec2 v_LocalCoord;\n"
    "void main() {\n"
    "    v_LocalCoord = a_LocalCoord;\n"
    "    gl_Position = u_MVP * vec4(a_Pos, 0.0, 1.0);\n"
    "}\n";

static const char* kCircleFragmentShader =
    "#extension GL_OES_standard_derivatives : enable\n"
    "precision mediump float;\n"
    "uniform vec4 u_Color;\n"
    "uniform float u_InnerRadius;\n"
    "uniform float u_OuterRadius;\n"
    "uniform float u_Feather;\n"
    "varying vec2 v_LocalCoord;\n"
    "void main() {\n"
    "    float r = length(v_LocalCoord);\n"
    "    float fw = u_Feather;\n"
    "#ifdef GL_OES_standard_derivatives\n"
    "    fw = max(fwidth(r) * 0.5, u_Feather * 0.25);\n"
    "#endif\n"
    "    float alphaOuter = 1.0 - smoothstep(max(0.0, u_OuterRadius - fw), u_OuterRadius + fw, r);\n"
    "    float alphaInner = (u_InnerRadius > 0.01) ? smoothstep(max(0.0, u_InnerRadius - fw), u_InnerRadius + fw, r) : 1.0;\n"
    "    float alpha = alphaOuter * alphaInner;\n"
    "    gl_FragColor = vec4(u_Color.rgb, u_Color.a * alpha);\n"
    "}\n";

static GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char info[512];
        glGetShaderInfoLog(shader, sizeof(info), nullptr, info);
        ALOGE("[GraphicsEngine] Shader compile error: %s", info);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static void initGLES() {
    if (s_glInitialized) return;

    GLuint vsLine = compileShader(GL_VERTEX_SHADER, kLineVertexShader);
    GLuint fsLine = compileShader(GL_FRAGMENT_SHADER, kLineFragmentShader);
    if (vsLine && fsLine) {
        s_lineProgram = glCreateProgram();
        glAttachShader(s_lineProgram, vsLine);
        glAttachShader(s_lineProgram, fsLine);
        glLinkProgram(s_lineProgram);
        s_aLinePos       = glGetAttribLocation(s_lineProgram, "a_Pos");
        s_aLineCoord     = glGetAttribLocation(s_lineProgram, "a_LocalCoord");
        s_uLineMVP       = glGetUniformLocation(s_lineProgram, "u_MVP");
        s_uLineColor     = glGetUniformLocation(s_lineProgram, "u_Color");
        s_uLineLength    = glGetUniformLocation(s_lineProgram, "u_Length");
        s_uLineHalfWidth = glGetUniformLocation(s_lineProgram, "u_HalfWidth");
        s_uLineFeather   = glGetUniformLocation(s_lineProgram, "u_Feather");
    }

    GLuint vsCirc = compileShader(GL_VERTEX_SHADER, kCircleVertexShader);
    GLuint fsCirc = compileShader(GL_FRAGMENT_SHADER, kCircleFragmentShader);
    if (vsCirc && fsCirc) {
        s_circleProgram = glCreateProgram();
        glAttachShader(s_circleProgram, vsCirc);
        glAttachShader(s_circleProgram, fsCirc);
        glLinkProgram(s_circleProgram);
        s_aCirclePos         = glGetAttribLocation(s_circleProgram, "a_Pos");
        s_aCircleCoord       = glGetAttribLocation(s_circleProgram, "a_LocalCoord");
        s_uCircleMVP         = glGetUniformLocation(s_circleProgram, "u_MVP");
        s_uCircleColor       = glGetUniformLocation(s_circleProgram, "u_Color");
        s_uCircleInnerRadius = glGetUniformLocation(s_circleProgram, "u_InnerRadius");
        s_uCircleOuterRadius = glGetUniformLocation(s_circleProgram, "u_OuterRadius");
        s_uCircleFeather     = glGetUniformLocation(s_circleProgram, "u_Feather");
    }

    s_glInitialized = (s_lineProgram != 0 && s_circleProgram != 0);
    if (s_glInitialized) {
        ALOGI("[CatEngine] [GraphicsEngine] GLES shaders initialized successfully! LineProg=%u, CircleProg=%u", s_lineProgram, s_circleProgram);
    } else {
        ALOGE("[CatEngine] [GraphicsEngine] FAILED to initialize GLES shaders! LineProg=%u, CircleProg=%u", s_lineProgram, s_circleProgram);
    }
}

static void buildUniversalTableMatrix(float* m, float screenWidth, float screenHeight, float centerX, float centerY, float scale) {
    memset(m, 0, 16 * sizeof(float));
    m[0]  =  (2.0f * scale) / screenWidth;
    m[5]  =  (2.0f * scale) / screenHeight;
    m[10] = -1.0f;
    m[12] =  (2.0f * centerX / screenWidth)  - 1.0f;
    m[13] =  (2.0f * centerY / screenHeight) - 1.0f;
    m[15] =  1.0f;
}

// Authentic Cocos2d 2D Affine Transform -> OpenGL NDC Projection Matrix
// Maps table cm coordinates directly to screen NDC using game's nodeToWorldTransform
static void buildNativeAffineTableMatrix(float* m, float screenWidth, float screenHeight,
                                         double a, double b, double c, double d, double tx, double ty) {
    memset(m, 0, 16 * sizeof(float));
    m[0]  = static_cast<float>((2.0 * a) / screenWidth);
    m[1]  = static_cast<float>((2.0 * b) / screenHeight);
    m[4]  = static_cast<float>((2.0 * c) / screenWidth);
    m[5]  = static_cast<float>((2.0 * d) / screenHeight);
    m[10] = -1.0f;
    m[12] = static_cast<float>((2.0 * tx / screenWidth)  - 1.0);
    m[13] = static_cast<float>((2.0 * ty / screenHeight) - 1.0);
    m[15] =  1.0f;
}

static void drawTableLine(const float* mvp, float feather, float x1, float y1, float x2, float y2, float thickness, float r, float g, float b, float a, bool clipToCushions = true) {
    if (!s_lineProgram) return;

    if (clipToCushions) {
        // Strict boundary clipping to prevent guideline lines from escaping the table boundary
        const float clipBound = kBoardHalfSize - 1.95f;
        float dx_c = x2 - x1;
        float dy_c = y2 - y1;
        float p[4] = {-dx_c, dx_c, -dy_c, dy_c};
        float q[4] = {x1 - (-clipBound), clipBound - x1, y1 - (-clipBound), clipBound - y1};
        float u1 = 0.0f;
        float u2 = 1.0f;
        for (int i = 0; i < 4; i++) {
            if (p[i] == 0.0f) {
                if (q[i] < 0.0f) return;
            } else {
                float t = q[i] / p[i];
                if (p[i] < 0.0f) {
                    if (t > u2) return;
                    if (t > u1) u1 = t;
                } else {
                    if (t < u1) return;
                    if (t < u2) u2 = t;
                }
            }
        }
        if (u1 > u2) return;
        float origX1 = x1, origY1 = y1;
        x1 = origX1 + u1 * dx_c;
        y1 = origY1 + u1 * dy_c;
        x2 = origX1 + u2 * dx_c;
        y2 = origY1 + u2 * dy_c;
    }

    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = std::hypot(dx, dy);
    if (len < 0.05f) return;

    float ux = dx / len;
    float uy = dy / len;
    float nx = -uy;
    float ny = ux;

    float halfW = thickness * 0.5f;
    float pad = halfW + feather * 1.5f;

    float vx0 = x1 - ux * pad - nx * pad;
    float vy0 = y1 - uy * pad - ny * pad;
    float vx1 = x2 + ux * pad - nx * pad;
    float vy1 = y2 + uy * pad - ny * pad;
    float vx2 = x1 - ux * pad + nx * pad;
    float vy2 = y1 - uy * pad + ny * pad;
    float vx3 = x2 + ux * pad + nx * pad;
    float vy3 = y2 + uy * pad + ny * pad;

    float posData[8] = {
        vx0, vy0,
        vx1, vy1,
        vx2, vy2,
        vx3, vy3
    };

    float coordData[8] = {
        -pad, -pad,
        len + pad, -pad,
        -pad,  pad,
        len + pad,  pad
    };

    glUseProgram(s_lineProgram);
    glUniformMatrix4fv(s_uLineMVP, 1, GL_FALSE, mvp);
    glUniform4f(s_uLineColor, r, g, b, a);
    glUniform1f(s_uLineLength, len);
    glUniform1f(s_uLineHalfWidth, halfW);
    glUniform1f(s_uLineFeather, feather);

    glEnableVertexAttribArray(s_aLinePos);
    glVertexAttribPointer(s_aLinePos, 2, GL_FLOAT, GL_FALSE, 0, posData);

    glEnableVertexAttribArray(s_aLineCoord);
    glVertexAttribPointer(s_aLineCoord, 2, GL_FLOAT, GL_FALSE, 0, coordData);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(s_aLinePos);
    glDisableVertexAttribArray(s_aLineCoord);
}

// Authentic continuous mitered polyline triangulation (Parity with Snake Engine SE)
struct MCPointFloat {
    float x;
    float y;
};

static void drawTablePolyline(const float* mvp, float feather, const std::vector<MCPointFloat>& points,
                              float thickness, float r, float g, float b, float a) {
    if (!s_lineProgram || points.size() < 2) return;

    // Filter duplicate or collinear micro-jitter points
    std::vector<MCPointFloat> cleanPts;
    cleanPts.reserve(points.size());
    for (const auto& pt : points) {
        if (!cleanPts.empty()) {
            float dx = pt.x - cleanPts.back().x;
            float dy = pt.y - cleanPts.back().y;
            if (std::hypot(dx, dy) < 0.05f) continue;
        }
        cleanPts.push_back(pt);
    }
    if (cleanPts.size() < 2) return;

    if (cleanPts.size() == 2) {
        drawTableLine(mvp, feather, cleanPts[0].x, cleanPts[0].y, cleanPts[1].x, cleanPts[1].y,
                      thickness, r, g, b, a, false);
        return;
    }

    size_t n = cleanPts.size();
    float halfW = thickness * 0.5f;
    float pad = halfW + feather * 1.5f;

    std::vector<MCPointFloat> segDirs(n - 1);
    std::vector<MCPointFloat> segNorms(n - 1);
    std::vector<float> segLens(n - 1);
    float totalLen = 0.0f;

    for (size_t i = 0; i < n - 1; i++) {
        float dx = cleanPts[i + 1].x - cleanPts[i].x;
        float dy = cleanPts[i + 1].y - cleanPts[i].y;
        float len = std::hypot(dx, dy);
        if (len < 1e-4f) len = 1e-4f;
        segLens[i] = len;
        totalLen += len;
        float ux = dx / len;
        float uy = dy / len;
        segDirs[i] = { ux, uy };
        segNorms[i] = { -uy, ux };
    }

    std::vector<float> posData;
    std::vector<float> coordData;
    posData.reserve((n + 2) * 4);
    coordData.reserve((n + 2) * 4);

    // 1. Start Cap: extended backward by pad
    {
        float ux = segDirs[0].x;
        float uy = segDirs[0].y;
        float nx = segNorms[0].x;
        float ny = segNorms[0].y;
        float p0x = cleanPts[0].x - ux * pad;
        float p0y = cleanPts[0].y - uy * pad;

        posData.push_back(p0x - nx * pad);
        posData.push_back(p0y - ny * pad);
        coordData.push_back(-pad);
        coordData.push_back(-pad);

        posData.push_back(p0x + nx * pad);
        posData.push_back(p0y + ny * pad);
        coordData.push_back(-pad);
        coordData.push_back(pad);
    }

    // 2. Point 0
    {
        float nx = segNorms[0].x;
        float ny = segNorms[0].y;
        posData.push_back(cleanPts[0].x - nx * pad);
        posData.push_back(cleanPts[0].y - ny * pad);
        coordData.push_back(0.0f);
        coordData.push_back(-pad);

        posData.push_back(cleanPts[0].x + nx * pad);
        posData.push_back(cleanPts[0].y + ny * pad);
        coordData.push_back(0.0f);
        coordData.push_back(pad);
    }

    // 3. Intermediate Joints (1 to n - 2) with Miter Normal
    float currentDist = 0.0f;
    for (size_t i = 1; i < n - 1; i++) {
        currentDist += segLens[i - 1];
        float n1x = segNorms[i - 1].x, n1y = segNorms[i - 1].y;
        float n2x = segNorms[i].x, n2y = segNorms[i].y;

        // Miter normal: (n1 + n2) / (1 + dot(n1, n2))
        float dot = n1x * n2x + n1y * n2y;
        float denom = 1.0f + dot;
        float mx = 0.0f, my = 0.0f;
        if (denom > 1e-3f) {
            mx = (n1x + n2x) / denom;
            my = (n1y + n2y) / denom;
        } else {
            mx = n1x;
            my = n1y;
        }

        // Clamp miter limit to 2.0 to prevent spikes at acute angles
        float miterLen = std::hypot(mx, my);
        if (miterLen > 2.0f) {
            float inv = 2.0f / miterLen;
            mx *= inv;
            my *= inv;
        }

        float offX = mx * pad;
        float offY = my * pad;

        posData.push_back(cleanPts[i].x - offX);
        posData.push_back(cleanPts[i].y - offY);
        coordData.push_back(currentDist);
        coordData.push_back(-pad);

        posData.push_back(cleanPts[i].x + offX);
        posData.push_back(cleanPts[i].y + offY);
        coordData.push_back(currentDist);
        coordData.push_back(pad);
    }

    // 4. Point n - 1
    {
        float nx = segNorms[n - 2].x;
        float ny = segNorms[n - 2].y;
        posData.push_back(cleanPts[n - 1].x - nx * pad);
        posData.push_back(cleanPts[n - 1].y - ny * pad);
        coordData.push_back(totalLen);
        coordData.push_back(-pad);

        posData.push_back(cleanPts[n - 1].x + nx * pad);
        posData.push_back(cleanPts[n - 1].y + ny * pad);
        coordData.push_back(totalLen);
        coordData.push_back(pad);
    }

    // 5. End Cap: extended forward by pad
    {
        float ux = segDirs[n - 2].x;
        float uy = segDirs[n - 2].y;
        float nx = segNorms[n - 2].x;
        float ny = segNorms[n - 2].y;
        float pNx = cleanPts[n - 1].x + ux * pad;
        float pNy = cleanPts[n - 1].y + uy * pad;

        posData.push_back(pNx - nx * pad);
        posData.push_back(pNy - ny * pad);
        coordData.push_back(totalLen + pad);
        coordData.push_back(-pad);

        posData.push_back(pNx + nx * pad);
        posData.push_back(pNy + ny * pad);
        coordData.push_back(totalLen + pad);
        coordData.push_back(pad);
    }

    glUseProgram(s_lineProgram);
    glUniformMatrix4fv(s_uLineMVP, 1, GL_FALSE, mvp);
    glUniform4f(s_uLineColor, r, g, b, a);
    glUniform1f(s_uLineLength, totalLen);
    glUniform1f(s_uLineHalfWidth, halfW);
    glUniform1f(s_uLineFeather, feather);

    glEnableVertexAttribArray(s_aLinePos);
    glVertexAttribPointer(s_aLinePos, 2, GL_FLOAT, GL_FALSE, 0, posData.data());

    glEnableVertexAttribArray(s_aLineCoord);
    glVertexAttribPointer(s_aLineCoord, 2, GL_FLOAT, GL_FALSE, 0, coordData.data());

    glDrawArrays(GL_TRIANGLE_STRIP, 0, static_cast<GLsizei>(posData.size() / 2));

    glDisableVertexAttribArray(s_aLinePos);
    glDisableVertexAttribArray(s_aLineCoord);
}

static void drawTableRing(const float* mvp, float feather, float cx, float cy, float outerR, float innerR, float r, float g, float b, float a) {
    if (!s_circleProgram) return;

    float pad = outerR + feather * 1.5f;

    float posData[8] = {
        cx - pad, cy - pad,
        cx + pad, cy - pad,
        cx - pad, cy + pad,
        cx + pad, cy + pad
    };

    float coordData[8] = {
        -pad, -pad,
         pad, -pad,
        -pad,  pad,
         pad,  pad
    };

    glUseProgram(s_circleProgram);
    glUniformMatrix4fv(s_uCircleMVP, 1, GL_FALSE, mvp);
    glUniform4f(s_uCircleColor, r, g, b, a);
    glUniform1f(s_uCircleInnerRadius, innerR);
    glUniform1f(s_uCircleOuterRadius, outerR);
    glUniform1f(s_uCircleFeather, feather);

    glEnableVertexAttribArray(s_aCirclePos);
    glVertexAttribPointer(s_aCirclePos, 2, GL_FLOAT, GL_FALSE, 0, posData);

    glEnableVertexAttribArray(s_aCircleCoord);
    glVertexAttribPointer(s_aCircleCoord, 2, GL_FLOAT, GL_FALSE, 0, coordData);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(s_aCirclePos);
    glDisableVertexAttribArray(s_aCircleCoord);
}

static void drawTableCircle(const float* mvp, float feather, float cx, float cy, float radius, float r, float g, float b, float a) {
    drawTableRing(mvp, feather, cx, cy, radius, 0.0f, r, g, b, a);
}

struct PhysBody {
    int id = -1;
    int puckType = 1;
    double px = 0.0;
    double py = 0.0;
    double vx = 0.0;
    double vy = 0.0;
    double wx = 0.0;
    double wy = 0.0;
    double mass = 1.0;
    double radius = 1.95;
    bool inPocket = false;
    int pocketIndex = -1;
    double pocketCenterX = 0.0;
    double pocketCenterY = 0.0;
    int chainLevel = -1;
    double initX = 0.0;
    double initY = 0.0;
    double segStartX = 0.0;
    double segStartY = 0.0;
    double lastDirX = 0.0;
    double lastDirY = 0.0;
    double distTraveled = 0.0;
    bool firstHitDone = false;
    bool isHit = false;
    int hitByEntityId = -1;
    double hitSpeed = 0.0;
    double omega = 0.0; // authentic angular spin (rad/s)
    std::vector<TrajectorySegment> segments;
};

static inline bool isPuckPottedOrInvalid(double x, double y, double inset = 33.48, double holeRad = 2.65) {
    if (std::isnan(x) || std::isnan(y)) return true;
    double ax = std::abs(x);
    double ay = std::abs(y);
    if (ax > 36.5 || ay > 36.5) return true;
    if (ax > 26.0 && ay > 26.0) {
        double dx = ax - inset;
        double dy = ay - inset;
        if (std::hypot(dx, dy) <= holeRad) return true;
    }
    return false;
}

static inline bool isInsidePocketHole(double x, double y, double inset, double holeRad) {
    if (std::isnan(x) || std::isnan(y)) return true;
    double ax = std::abs(x);
    double ay = std::abs(y);
    if (ax > 36.5 || ay > 36.5) return true;
    if (ax > 26.0 && ay > 26.0) {
        double dx = ax - inset;
        double dy = ay - inset;
        if (std::hypot(dx, dy) <= holeRad) return true;
    }
    return false;
}

static AimSimulationResult simulateStrikerTrajectoryUniversal(double startX, double startY, double shotAngle, double forceVal, int maxSimSteps = 4000) {
    AimSimulationResult sim;

    const float stRadius = s_strikerRadius.load();
    const float pkRadius = s_puckRadius.load();
    const float pkInset  = s_pocketInset.load();
    float pkRad          = s_pocketRadius.load();

    double fClamped = std::clamp(forceVal, 0.0, 1.0);
    double minP = s_minPower.load();
    double maxP = s_maxPower.load();
    double bonus = s_strikerBonus.load();

    // Pure linear interpolation matching authentic TableLogic::launchStriker (libgame.so 0x1B2C3A4):
    double v0 = minP + fClamped * (maxP - minP);
    if (fClamped >= 0.999 && bonus > 0.0) {
        v0 += bonus;
    }
    v0 *= s_powerMultiplier.load();

    // Incorporate real-time AI adaptive physics parameters
    auto learnedAI = catengine::ai::AdaptiveLearningEngine::getInstance().getLearnedParameters();
    if (learnedAI.totalShotsLearned > 0) {
        v0 *= learnedAI.forceScaleFactor;
        if (learnedAI.pocketCaptureRadius > pkRad) {
            pkRad = learnedAI.pocketCaptureRadius;
        }
    }

    // 2. Setup bodies
    std::vector<PhysBody> bodies;
    bodies.reserve(20);

    // Body 0: Striker
    PhysBody striker;
    striker.id = -1;
    striker.puckType = 0;
    striker.px = startX;
    striker.py = startY;
    striker.vx = v0 * std::cos(shotAngle);
    striker.vy = v0 * std::sin(shotAngle);
    striker.mass = s_strikerMass.load();
    striker.radius = stRadius;
    striker.chainLevel = 0;
    striker.initX = startX;
    striker.initY = startY;
    striker.segStartX = startX;
    striker.segStartY = startY;
    double stSpd = std::hypot(striker.vx, striker.vy);
    striker.lastDirX = (stSpd > 1e-4) ? (striker.vx / stSpd) : 0.0;
    striker.lastDirY = (stSpd > 1e-4) ? (striker.vy / stSpd) : 0.0;
    bodies.push_back(striker);

    {
        std::lock_guard<std::mutex> lock(s_puckMutex);
        for (const auto& pe : s_puckList) {
            if (!pe.active) continue;
            if (isPuckPottedOrInvalid(pe.x, pe.y, pkInset, pkRad)) continue;

            PhysBody pb;
            pb.id = pe.id;
            pb.puckType = pe.puckType;
            pb.px = pe.x;
            pb.py = pe.y;
            pb.vx = 0.0;
            pb.vy = 0.0;
            pb.mass = s_puckMass.load();
            pb.radius = pkRadius;
            pb.chainLevel = -1;
            pb.initX = pe.x;
            pb.initY = pe.y;
            pb.segStartX = pe.x;
            pb.segStartY = pe.y;
            bodies.push_back(pb);
        }
    }

    const double dt = 0.001; // Authentic 1000 Hz CCD step (libgame.so 0x12ea830)
    const double gDecel = 980.0;
    const double e_body = s_restitutionBody.load();
    double e_cush = std::clamp(s_restitutionCushion.load(), 0.05, 0.99);
    if (learnedAI.totalShotsLearned > 0) {
        e_cush = std::clamp(static_cast<double>(learnedAI.cushionRestitution), 0.05, 0.99);
    }

    const double pocketCenters[4][2] = {
        {  pkInset,  pkInset },
        { -pkInset,  pkInset },
        {  pkInset, -pkInset },
        { -pkInset, -pkInset }
    };

    for (int step = 0; step < maxSimSteps; step++) {
        bool anyMotion = false;
        for (const auto& b : bodies) {
            if (!b.inPocket && std::hypot(b.vx, b.vy) > 0.15) {
                anyMotion = true;
                break;
            }
        }
        if (!anyMotion) break;

        double timeLeft = dt;
        int subStep = 0;
        const int maxSubSteps = 4; // allow up to 4 micro-events per dt tick

        while (timeLeft > 1e-6 && subStep < maxSubSteps) {
            subStep++;

            // 1. Continuous Collision Detection (CCD):
            // Find earliest collision time across all disc pairs and cushion edges
            double earliestT = timeLeft;
            int eventType = 0; // 0 = none, 1 = disc-disc edge contact, 2 = cushion edge contact
            size_t eventI = 0, eventJ = 0;
            int cushionWall = 0; // 1 = Right, 2 = Left, 3 = Top, 4 = Bottom

            // A. Check disc-to-disc edge-to-edge contact (Analytic TOI Quadratic)
            for (size_t i = 0; i < bodies.size(); i++) {
                const auto& bi = bodies[i];
                if (bi.inPocket) continue;

                for (size_t j = i + 1; j < bodies.size(); j++) {
                    const auto& bj = bodies[j];
                    if (bj.inPocket) continue;

                    // If neither body is moving, skip
                    if (std::hypot(bi.vx, bi.vy) <= 0.15 && std::hypot(bj.vx, bj.vy) <= 0.15) continue;

                    double rSum = bi.radius + bj.radius;
                    double rx = bj.px - bi.px;
                    double ry = bj.py - bi.py;
                    double rSq = rx * rx + ry * ry;

                    // Relative velocity: V_j - V_i
                    double vx = bj.vx - bi.vx;
                    double vy = bj.vy - bi.vy;

                    // Contact equation: ||r + v * t||^2 = rSum^2
                    // A*t^2 + 2*B*t + C = 0
                    double A = vx * vx + vy * vy;
                    double B = rx * vx + ry * vy; // dot(r, v)
                    double C = rSq - (rSum * rSum);

                    if (C <= 0.0) {
                        // Already touching or overlapping
                        if (B < -1e-6) {
                            // Approaching each other
                            if (0.0 < earliestT) {
                                earliestT = 0.0;
                                eventType = 1;
                                eventI = i;
                                eventJ = j;
                            }
                        }
                    } else if (B < -1e-6 && A > 1e-8) {
                        // Moving towards each other
                        double disc = B * B - A * C;
                        if (disc >= 0.0) {
                            double tHit = (-B - std::sqrt(disc)) / A;
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

            // B. Check cushion edge contact (CCD) - authentic table boundary
            for (size_t i = 0; i < bodies.size(); i++) {
                const auto& b = bodies[i];
                if (b.inPocket) continue;
                if (std::hypot(b.vx, b.vy) <= 0.15) continue;

                double boundLimit = kBoardHalfSize - b.radius;

                // Authentic pocket jaw cutoff: cushion only exists between pocket openings
                const double pocketJawCutoff = 32.80;

                // Right cushion (x = +boundLimit)
                if (b.vx > 1e-4) {
                    double tc = (b.px >= boundLimit) ? 0.0 : ((boundLimit - b.px) / b.vx);
                    if (tc >= 0.0 && tc < earliestT) {
                        double yc = b.py + b.vy * tc;
                        if (std::abs(yc) <= pocketJawCutoff) {
                            earliestT = tc;
                            eventType = 2;
                            eventI = i;
                            cushionWall = 1; // Right
                        }
                    }
                } else if (b.vx < -1e-4) {
                    // Left cushion (x = -boundLimit)
                    double tc = (b.px <= -boundLimit) ? 0.0 : ((-boundLimit - b.px) / b.vx);
                    if (tc >= 0.0 && tc < earliestT) {
                        double yc = b.py + b.vy * tc;
                        if (std::abs(yc) <= pocketJawCutoff) {
                            earliestT = tc;
                            eventType = 2;
                            eventI = i;
                            cushionWall = 2; // Left
                        }
                    }
                }

                // Top cushion (y = +boundLimit)
                if (b.vy > 1e-4) {
                    double tc = (b.py >= boundLimit) ? 0.0 : ((boundLimit - b.py) / b.vy);
                    if (tc >= 0.0 && tc < earliestT) {
                        double xc = b.px + b.vx * tc;
                        if (std::abs(xc) <= pocketJawCutoff) {
                            earliestT = tc;
                            eventType = 2;
                            eventI = i;
                            cushionWall = 3; // Top
                        }
                    }
                } else if (b.vy < -1e-4) {
                    // Bottom cushion (y = -boundLimit)
                    double tc = (b.py <= -boundLimit) ? 0.0 : ((-boundLimit - b.py) / b.vy);
                    if (tc >= 0.0 && tc < earliestT) {
                        double xc = b.px + b.vx * tc;
                        if (std::abs(xc) <= pocketJawCutoff) {
                            earliestT = tc;
                            eventType = 2;
                            eventI = i;
                            cushionWall = 4; // Bottom
                        }
                    }
                }
            }

            // 2. Advance all moving bodies to earliestT
            double advanceDt = earliestT;
            if (advanceDt > 1e-8) {
                // Advance positions first with exact velocities
                for (auto& b : bodies) {
                    if (b.inPocket) continue;
                    double spd = std::hypot(b.vx, b.vy);
                    if (spd > 0.15) {
                        b.px += b.vx * advanceDt;
                        b.py += b.vy * advanceDt;
                        b.distTraveled += spd * advanceDt;
                        double boundLimit = kBoardHalfSize - b.radius;
                        b.px = std::clamp(b.px, -boundLimit, boundLimit);
                        b.py = std::clamp(b.py, -boundLimit, boundLimit);

                        // Check corner pocket capture (strictly within visual pocket detection circle)
                        for (int p = 0; p < 4; p++) {
                            double dP = std::hypot(b.px - pocketCenters[p][0], b.py - pocketCenters[p][1]);
                            if (dP <= pkRad) {
                                b.inPocket = true;
                                b.pocketIndex = p;
                                b.pocketCenterX = pocketCenters[p][0];
                                b.pocketCenterY = pocketCenters[p][1];
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
                                break;
                            }
                        }
                    }
                }

                // Ground Coulomb friction deceleration: a = mu * 980.0
                for (auto& b : bodies) {
                    if (b.inPocket) continue;
                    double curSpd = std::hypot(b.vx, b.vy);
                    if (curSpd > 0.15) {
                        double muTable = (b.id == -1) ? s_strikerFriction.load() : s_puckFriction.load();
                        if (learnedAI.totalShotsLearned > 0) {
                            muTable = (b.id == -1) ? (learnedAI.tableFriction * 1.48) : learnedAI.tableFriction;
                        }
                        if (muTable < 0.0001) muTable = 0.0001;
                        double dv = muTable * gDecel * advanceDt;
                        if (curSpd <= dv) {
                            b.vx = 0.0;
                            b.vy = 0.0;
                            b.wx = 0.0;
                            b.wy = 0.0;
                        } else {
                            double scale = (curSpd - dv) / curSpd;
                            b.vx *= scale;
                            b.vy *= scale;
                            b.wx = b.vy / b.radius;
                            b.wy = -b.vx / b.radius;
                        }
                    } else {
                        b.vx = 0.0;
                        b.vy = 0.0;
                        b.wx = 0.0;
                        b.wy = 0.0;
                    }
                }
            }

            timeLeft -= advanceDt;

            // 3. Resolve collision at exact edge contact point
            if (eventType == 1) {
                auto& bi = bodies[eventI];
                auto& bj = bodies[eventJ];
                if (!bi.inPocket && !bj.inPocket) {
                    double cdx = bj.px - bi.px;
                    double cdy = bj.py - bi.py;
                    double dist = std::hypot(cdx, cdy);
                    double minDist = bi.radius + bj.radius;
                    if (dist < 1e-6) {
                        cdx = 1.0; cdy = 0.0; dist = 1e-6;
                    }

                    double nx = cdx / dist;
                    double ny = cdy / dist;

                    double wi = 1.0 / bi.mass;
                    double wj = 1.0 / bj.mass;
                    double wSum = wi + wj;

                    // Separation to guarantee zero overlap and prevent numeric sticking
                    if (dist < minDist) {
                        double overlap = minDist - dist;
                        bi.px -= nx * (overlap + 1e-4) * (wi / wSum);
                        bi.py -= ny * (overlap + 1e-4) * (wi / wSum);
                        bj.px += nx * (overlap + 1e-4) * (wj / wSum);
                        bj.py += ny * (overlap + 1e-4) * (wj / wSum);
                    }

                    double relVx = bi.vx - bj.vx;
                    double relVy = bi.vy - bj.vy;
                    double relNorm = relVx * nx + relVy * ny;

                    if (relNorm > 0.0) {
                        // Authentic 1D elastic momentum exchange (libgame.so 0x1998840)
                        double tx = -ny;
                        double ty = nx;

                        // Project velocities onto collision normal and tangent
                        double v1n = bi.vx * nx + bi.vy * ny;
                        double v1t = bi.vx * tx + bi.vy * ty;
                        double v2n = bj.vx * nx + bj.vy * ny;
                        double v2t = bj.vx * tx + bj.vy * ty;

                        double m1 = bi.mass;
                        double m2 = bj.mass;
                        double mSum = m1 + m2;

                        // Pure elastic 1D momentum conservation exchange
                        double v1n_new = ((m1 - m2) * v1n + 2.0 * m2 * v2n) / mSum;
                        double v2n_new = (2.0 * m1 * v1n + (m2 - m1) * v2n) / mSum;

                        // Tangential velocity is 100% preserved in linear coordinates
                        double v1t_new = v1t;
                        double v2t_new = v2t;

                        // Reconstruct post-collision Cartesian velocities
                        bi.vx = v1n_new * nx + v1t_new * tx;
                        bi.vy = v1n_new * ny + v1t_new * ty;
                        bj.vx = v2n_new * nx + v2t_new * tx;
                        bj.vy = v2n_new * ny + v2t_new * ty;

                        // Record hit telemetry data
                        bj.isHit = true;
                        bj.hitByEntityId = (bi.id == -1) ? 0 : bi.id;
                        bj.hitSpeed = std::hypot(bj.vx, bj.vy);

                        // Authentic spin transfer (libgame.so 0x1998a8c)
                        double v_slip_i = bi.omega * bi.radius - v1t;
                        double Jt_i = std::min(0.40 * std::abs(v1n), std::abs(v_slip_i) / 2.5);
                        double sgnSlip_i = (v_slip_i > 0.0) ? 1.0 : ((v_slip_i < 0.0) ? -1.0 : 0.0);
                        bi.omega += sgnSlip_i * (2.5 * Jt_i / bi.radius);

                        double v_slip_j = bj.omega * bj.radius - v2t;
                        double Jt_j = std::min(0.40 * std::abs(v2n), std::abs(v_slip_j) / 2.5);
                        double sgnSlip_j = (v_slip_j > 0.0) ? 1.0 : ((v_slip_j < 0.0) ? -1.0 : 0.0);
                        bj.omega += sgnSlip_j * (2.5 * Jt_j / bj.radius);

                        // Collision chain level propagation
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

                        // Striker first impact record (EXACT PERIMETER CONTACT POINT)
                        if (bi.id == -1 && !sim.hasFirstContact) {
                            sim.hasFirstContact = true;
                            sim.firstImpactStrikerX = static_cast<float>(bi.px);
                            sim.firstImpactStrikerY = static_cast<float>(bi.py);
                            sim.firstImpactPuckX    = static_cast<float>(bj.px);
                            sim.firstImpactPuckY    = static_cast<float>(bj.py);
                            sim.firstContactPuckId  = bj.id;

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
                        } else if (bi.id == -1) {
                            // Striker subsequent puck hit
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
                        } else if (bi.chainLevel >= 1) {
                            // Puck bi was moving and collided with another body:
                            // Split trajectory at the exact impact point!
                            double distI = std::hypot(bi.px - bi.segStartX, bi.py - bi.segStartY);
                            if (distI > 0.15) {
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

                        // Symmetrically handle bj if bj was a moving puck hitting another body:
                        // Split trajectory at the exact impact point so deflection starts from impact point!
                        if (bj.chainLevel >= 1 && bj.id != -1) {
                            double distJ = std::hypot(bj.px - bj.segStartX, bj.py - bj.segStartY);
                            if (distJ > 0.15) {
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
                }
            } else if (eventType == 2) {
                // Cushion bounce with authentic velocity loss & dynamic angle coupling (libgame.so 0x1998b2c)
                auto& b = bodies[eventI];
                if (!b.inPocket) {
                    double boundLimit = kBoardHalfSize - b.radius;
                    double e_eff = e_cush * 0.92; // Effective normal restitution (0.78 * 0.92 = 0.7176)
                    
                    if (cushionWall == 1) { // Right wall (x = +boundLimit)
                        double vn = b.vx;
                        double vt = -b.vy;
                        // Authentic tangential slip in cushion frame (libgame.so 0x1998b90): vt - omega * R
                        double v_slip = vt - b.omega * b.radius;
                        double sgnSlip = (v_slip > 0.0) ? 1.0 : ((v_slip < 0.0) ? -1.0 : 0.0);
                        double Jt = std::min(0.40 * std::abs(vn), std::abs(v_slip) / 2.5);
                        double v_prime_n = -vn * e_eff;
                        double v_prime_t = vt - sgnSlip * (Jt / 2.5);
                        b.omega += sgnSlip * (2.5 * Jt / b.radius) - 0.54 * vn / b.radius;
                        b.vx = v_prime_n;
                        b.vy = -v_prime_t * 0.85; // Rubber absorbs 15% tangential sliding energy
                        b.px = boundLimit;
                    } else if (cushionWall == 2) { // Left wall (x = -boundLimit)
                        double vn = -b.vx;
                        double vt = b.vy;
                        double v_slip = vt - b.omega * b.radius;
                        double sgnSlip = (v_slip > 0.0) ? 1.0 : ((v_slip < 0.0) ? -1.0 : 0.0);
                        double Jt = std::min(0.40 * std::abs(vn), std::abs(v_slip) / 2.5);
                        double v_prime_n = -vn * e_eff;
                        double v_prime_t = vt - sgnSlip * (Jt / 2.5);
                        b.omega += sgnSlip * (2.5 * Jt / b.radius) - 0.54 * vn / b.radius;
                        b.vx = -v_prime_n;
                        b.vy = v_prime_t * 0.85; // Rubber absorbs 15% tangential sliding energy
                        b.px = -boundLimit;
                    } else if (cushionWall == 3) { // Top wall (y = +boundLimit)
                        double vn = b.vy;
                        double vt = b.vx;
                        double v_slip = vt - b.omega * b.radius;
                        double sgnSlip = (v_slip > 0.0) ? 1.0 : ((v_slip < 0.0) ? -1.0 : 0.0);
                        double Jt = std::min(0.40 * std::abs(vn), std::abs(v_slip) / 2.5);
                        double v_prime_n = -vn * e_eff;
                        double v_prime_t = vt - sgnSlip * (Jt / 2.5);
                        b.omega += sgnSlip * (2.5 * Jt / b.radius) - 0.54 * vn / b.radius;
                        b.vx = v_prime_t * 0.85; // Rubber absorbs 15% tangential sliding energy
                        b.vy = v_prime_n;
                        b.py = boundLimit;
                    } else if (cushionWall == 4) { // Bottom wall (y = -boundLimit)
                        double vn = -b.vy;
                        double vt = -b.vx;
                        double v_slip = vt - b.omega * b.radius;
                        double sgnSlip = (v_slip > 0.0) ? 1.0 : ((v_slip < 0.0) ? -1.0 : 0.0);
                        double Jt = std::min(0.40 * std::abs(vn), std::abs(v_slip) / 2.5);
                        double v_prime_n = -vn * e_eff;
                        double v_prime_t = vt - sgnSlip * (Jt / 2.5);
                        b.omega += sgnSlip * (2.5 * Jt / b.radius) - 0.54 * vn / b.radius;
                        b.vx = -v_prime_t * 0.85; // Rubber absorbs 15% tangential sliding energy
                        b.vy = -v_prime_n;
                        b.py = -boundLimit;
                    }

                    b.wx = b.vx;
                    b.wy = b.vy;

                    double dCush = std::hypot(b.px - b.segStartX, b.py - b.segStartY);
                    if (dCush > 0.15) {
                        TrajectorySegment seg;
                        seg.startX = static_cast<float>(std::clamp(b.segStartX, -boundLimit, boundLimit));
                        seg.startY = static_cast<float>(std::clamp(b.segStartY, -boundLimit, boundLimit));
                        seg.endX   = static_cast<float>(std::clamp(b.px, -boundLimit, boundLimit));
                        seg.endY   = static_cast<float>(std::clamp(b.py, -boundLimit, boundLimit));
                        seg.isCushionBounce = true;
                        seg.isPuckHit = false;
                        b.segments.push_back(seg);
                        b.segStartX = b.px;
                        b.segStartY = b.py;
                    }
                }
            } else {
                // No collision event in remaining time step
                break;
            }
        }
    }

    for (auto& b : bodies) {
        if (!b.inPocket) {
            double boundLimit = kBoardHalfSize - b.radius;
            double d = std::hypot(b.px - b.segStartX, b.py - b.segStartY);
            if ((b.segments.empty() && d > 0.50) || (!b.segments.empty() && d > 0.35)) {
                TrajectorySegment seg;
                seg.startX = static_cast<float>(std::clamp(b.segStartX, -boundLimit, boundLimit));
                seg.startY = static_cast<float>(std::clamp(b.segStartY, -boundLimit, boundLimit));
                seg.endX   = static_cast<float>(std::clamp(b.px, -boundLimit, boundLimit));
                seg.endY   = static_cast<float>(std::clamp(b.py, -boundLimit, boundLimit));
                seg.isCushionBounce = (b.segments.size() > 0);
                seg.isPuckHit = false;
                b.segments.push_back(seg);
            }
        }
    }

    sim.segments = bodies[0].segments;
    sim.finalStrikerX = static_cast<float>(bodies[0].px);
    sim.finalStrikerY = static_cast<float>(bodies[0].py);
    sim.strikerInPocket = bodies[0].inPocket;
    sim.strikerPocketIndex = bodies[0].pocketIndex;
    sim.hitPuck = sim.hasFirstContact;

    for (size_t i = 1; i < bodies.size(); i++) {
        const auto& b = bodies[i];
        double totalTravel = std::hypot(b.px - b.initX, b.py - b.initY);

        if (b.chainLevel >= 1 && (totalTravel >= 0.35 || !b.segments.empty() || b.inPocket)) {
            PuckTrajectoryInfo traj;
            traj.puckIndex = b.id;
            traj.puckType = b.puckType;
            traj.startX = static_cast<float>(b.initX);
            traj.startY = static_cast<float>(b.initY);
            traj.finalX = static_cast<float>(b.px);
            traj.finalY = static_cast<float>(b.py);
            traj.inPocket = b.inPocket;
            traj.pocketIndex = b.pocketIndex;
            traj.pocketCenterX = static_cast<float>(b.pocketCenterX);
            traj.pocketCenterY = static_cast<float>(b.pocketCenterY);
            traj.segments = b.segments;
            traj.hasDeflection = false;
            traj.hitSpeed = static_cast<float>(b.hitSpeed);
            traj.hitByEntityId = b.hitByEntityId;
            traj.isHit = b.isHit;
            sim.puckTrajectories.push_back(traj);
        }
    }

    return sim;
}

#define USE_PHYSICS_ENGINE_V2 1

static AimSimulationResult simulateStrikerTrajectoryV2(double startX, double startY, double shotAngle, double forceVal, int maxSimSteps = 5000) {
    catengine::v2::PhysicsConfig cfg;
    cfg.strikerRadius = s_strikerRadius.load();
    cfg.puckRadius = s_puckRadius.load();
    cfg.pocketInset = s_pocketInset.load();
    cfg.pocketRadius = s_pocketRadius.load();
    cfg.minPower = s_minPower.load();
    cfg.maxPower = s_maxPower.load();
    cfg.strikerBonus = s_strikerBonus.load();
    cfg.powerMultiplier = s_powerMultiplier.load();

    cfg.strikerFriction = s_strikerFriction.load();
    cfg.puckFriction = s_puckFriction.load();
    cfg.restitutionBody = s_restitutionBody.load();
    cfg.restitutionCushion = s_restitutionCushion.load();
    cfg.strikerMass = s_strikerMass.load();
    cfg.puckMass = s_puckMass.load();
    cfg.pocketDropMaxSpeed = s_pocketDropMaxSpeed.load();
    cfg.pocketGravity = s_pocketGravity.load();

    // Incorporate real-time AI adaptive physics parameters
    auto learnedAI = catengine::ai::AdaptiveLearningEngine::getInstance().getLearnedParameters();
    if (learnedAI.totalShotsLearned > 0) {
        cfg.powerMultiplier *= learnedAI.forceScaleFactor;
        cfg.strikerFriction = learnedAI.tableFriction * 1.48; // Striker has higher friction ratio
        cfg.puckFriction = learnedAI.tableFriction;
        cfg.restitutionCushion = learnedAI.cushionRestitution;
        if (learnedAI.pocketCaptureRadius > cfg.pocketRadius) {
            cfg.pocketRadius = learnedAI.pocketCaptureRadius;
        }
    }

    {
        std::lock_guard<std::mutex> lock(s_polyMutex);
        cfg.cushionVertices = s_cushionPolygon;
    }

    std::vector<catengine::v2::PuckInput> pucks;
    {
        std::lock_guard<std::mutex> lock(s_puckMutex);
        pucks.reserve(s_puckList.size());
        for (const auto& pe : s_puckList) {
            catengine::v2::PuckInput pi;
            pi.id = pe.id;
            pi.puckType = pe.puckType;
            pi.x = pe.x;
            pi.y = pe.y;
            pi.active = pe.active;
            pucks.push_back(pi);
        }
    }

    catengine::v2::SimResult v2Res = catengine::v2::simulate(startX, startY, shotAngle, forceVal, pucks, cfg, maxSimSteps);

    AimSimulationResult sim;
    sim.finalStrikerX = v2Res.finalStrikerX;
    sim.finalStrikerY = v2Res.finalStrikerY;
    sim.hasFirstContact = v2Res.hasFirstContact;
    sim.firstImpactStrikerX = v2Res.firstImpactStrikerX;
    sim.firstImpactStrikerY = v2Res.firstImpactStrikerY;
    sim.firstImpactPuckX = v2Res.firstImpactPuckX;
    sim.firstImpactPuckY = v2Res.firstImpactPuckY;
    sim.firstContactPuckId = v2Res.firstContactPuckId;
    sim.hitPuck = v2Res.hitPuck;
    sim.strikerInPocket = v2Res.strikerInPocket;
    sim.strikerPocketIndex = v2Res.strikerPocketIndex;
    sim.strikerCushionBounces = v2Res.strikerCushionBounces;

    sim.segments.reserve(v2Res.segments.size());
    for (const auto& s : v2Res.segments) {
        TrajectorySegment ts;
        ts.startX = s.startX;
        ts.startY = s.startY;
        ts.endX = s.endX;
        ts.endY = s.endY;
        ts.isCushionBounce = s.isCushionBounce;
        ts.isPuckHit = s.isPuckHit;
        sim.segments.push_back(ts);
    }

    sim.puckTrajectories.reserve(v2Res.puckTrajectories.size());
    for (const auto& pt : v2Res.puckTrajectories) {
        PuckTrajectoryInfo pi;
        pi.puckIndex = pt.puckIndex;
        pi.puckType = pt.puckType;
        pi.startX = pt.startX;
        pi.startY = pt.startY;
        pi.finalX = pt.finalX;
        pi.finalY = pt.finalY;
        pi.inPocket = pt.inPocket;
        pi.pocketIndex = pt.pocketIndex;
        pi.pocketCenterX = pt.pocketCenterX;
        pi.pocketCenterY = pt.pocketCenterY;
        pi.hasDeflection = pt.hasDeflection;
        pi.isHit = pt.isHit;
        pi.hitByEntityId = pt.hitByEntityId;
        pi.hitSpeed = pt.hitSpeed;
        pi.cushionBounces = pt.cushionBounces;
        pi.segments.reserve(pt.segments.size());
        for (const auto& s : pt.segments) {
            TrajectorySegment ts;
            ts.startX = s.startX;
            ts.startY = s.startY;
            ts.endX = s.endX;
            ts.endY = s.endY;
            ts.isCushionBounce = s.isCushionBounce;
            ts.isPuckHit = s.isPuckHit;
            pi.segments.push_back(ts);
        }
        sim.puckTrajectories.push_back(pi);
    }

    return sim;
}

static std::atomic<int> s_physicsEngineMode(0); // 0 = Snake Engine CCD (Default), 1 = In-Engine Native

void GraphicsEngine_SetPhysicsEngineMode(int mode) {
    s_physicsEngineMode.store(mode);
    ALOGI("[CatEngine] Physics Engine Mode updated to: %d (%s)", mode, (mode == 1 ? "In-Engine Native Stepper" : "Snake Engine CCD"));
}

int GraphicsEngine_GetPhysicsEngineMode() {
    return s_physicsEngineMode.load();
}

static inline AimSimulationResult simulateStrikerTrajectory(double startX, double startY, double shotAngle, double forceVal, int maxSimSteps = 5000) {
    int mode = s_physicsEngineMode.load();
    if (mode == 1 && CatEngine_CanSimulateNative()) {
        AimSimulationResult nativeRes = CatEngine_SimulateInEngineNative(startX, startY, shotAngle, forceVal, maxSimSteps);
        if (!nativeRes.segments.empty()) {
            return nativeRes;
        }
    }
#if USE_PHYSICS_ENGINE_V2
    return simulateStrikerTrajectoryV2(startX, startY, shotAngle, forceVal, maxSimSteps);
#else
    return simulateStrikerTrajectoryUniversal(startX, startY, shotAngle, forceVal, maxSimSteps);
#endif
}

static EGLBoolean my_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    int width  = viewport[2];
    int height = viewport[3];

    EGLint surfW = 0, surfH = 0;
    if (dpy != EGL_NO_DISPLAY && surface != EGL_NO_SURFACE) {
        eglQuerySurface(dpy, surface, EGL_WIDTH, &surfW);
        eglQuerySurface(dpy, surface, EGL_HEIGHT, &surfH);
    }
    if (surfW > 0 && surfH > 0) {
        width  = surfW;
        height = surfH;
    }

    if (!s_glInitialized && width > 0 && height > 0) {
        initGLES();
    }

    bool matchActive = s_matchActive.load() && CatEngine_IsMatchAlive();

    // STRICT MATCH ACTIVE GATE:
    // When a match is NOT active, do not show lines, table border, pucks, striker, pockets, or anything at all!
    if (!matchActive) {
        if (orig_eglSwapBuffers) {
            return orig_eglSwapBuffers(dpy, surface);
        }
        return EGL_TRUE;
    }

    bool showBorder = s_showBorder.load();

    static int64_t s_lastPuckSyncMs = 0;
    int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    if (nowMs - s_lastPuckSyncMs >= 33) {
        s_lastPuckSyncMs = nowMs;
        CatEngine_UpdateTablePucks();
    }

    if (s_glInitialized && s_lineProgram != 0 && s_circleProgram != 0) {

        // State Preservation
        GLint prevProgram = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);

        GLint prevVBO = 0;
        GLint prevIBO = 0;
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prevVBO);
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &prevIBO);

        GLboolean blendWasOn    = glIsEnabled(GL_BLEND);
        GLboolean depthWasOn    = glIsEnabled(GL_DEPTH_TEST);
        GLboolean cullWasOn     = glIsEnabled(GL_CULL_FACE);
        GLboolean scissorWasOn  = glIsEnabled(GL_SCISSOR_TEST);

        GLint prevBlendSrcRGB = 0, prevBlendDstRGB = 0;
        GLint prevBlendSrcAlpha = 0, prevBlendDstAlpha = 0;
        glGetIntegerv(GL_BLEND_SRC_RGB, &prevBlendSrcRGB);
        glGetIntegerv(GL_BLEND_DST_RGB, &prevBlendDstRGB);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &prevBlendSrcAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &prevBlendDstAlpha);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_SCISSOR_TEST);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glViewport(0, 0, width, height);

        // Sync viewport framebuffer dimensions with engine
        CatEngine_SetViewportDimensions(width, height);

        // 1. Compute Center & Scale for Universal Table Matrix
        // Naturally centered on the screen (width * 0.5f, height * 0.5f)
        float boardCenterX = width * 0.5f;
        float boardCenterY = height * 0.5f;
        float boardScale   = (width * 0.94144f) / (2.0f * kBoardHalfSize);

        float tableMVP[16];
        bool calibrated = false;

        if (s_hasManualBounds.load()) {
            float rawL = s_manualL.load();
            float rawR = s_manualR.load();
            float rawB = s_manualB.load();
            float rawT = s_manualT.load();
            float left   = std::min(rawL, rawR);
            float right  = std::max(rawL, rawR);
            float topY_android    = std::min(rawB, rawT);
            float bottomY_android = std::max(rawB, rawT);
            float cx = (left + right) * 0.5f;
            float cy_android = (topY_android + bottomY_android) * 0.5f;
            // Invert Android screen Y into OpenGL viewport Y:
            float cy = height - cy_android;
            float sc = std::min(right - left, bottomY_android - topY_android) / (2.0f * kBoardHalfSize);
            if (sc > 3.0f && cx >= width * 0.15f && cx <= width * 0.85f) {
                boardCenterX = cx;
                boardCenterY = cy;
                boardScale   = sc;
                calibrated   = true;
            }
        }
        if (!calibrated && s_hasEngineCalibration.load()) {
            float cx = s_autoCenterX.load();
            float cy = s_autoCenterY.load();
            float sc = s_autoScale.load();
            if (sc > 5.0f && cx >= width * 0.35f && cx <= width * 0.65f) {
                boardCenterX = cx;
                boardCenterY = cy;
                boardScale   = sc;
                calibrated   = true;
            }
        }

        buildUniversalTableMatrix(tableMVP, static_cast<float>(width), static_cast<float>(height),
                                  boardCenterX, boardCenterY, boardScale);

        float feather = (boardScale > 0.1f) ? (1.2f / boardScale) : 0.045f;

        if (showBorder) {
            float th = 0.85f;
            float bSize = kBoardHalfSize; // 37.0f cm table cushion edge
            // Vibrant golden yellow outer boundary frame with seamless miter joints
            std::vector<MCPointFloat> outerBox = {
                { -bSize, -bSize },
                {  bSize, -bSize },
                {  bSize,  bSize },
                { -bSize,  bSize },
                { -bSize, -bSize }
            };
            drawTablePolyline(tableMVP, feather, outerBox, th, 1.0f, 0.88f, 0.0f, 1.0f);

            // High-contrast inner cushion outline with seamless miter joints
            float inSize = bSize - 0.75f;
            std::vector<MCPointFloat> innerBox = {
                { -inSize, -inSize },
                {  inSize, -inSize },
                {  inSize,  inSize },
                { -inSize,  inSize },
                { -inSize, -inSize }
            };
            drawTablePolyline(tableMVP, feather, innerBox, 0.35f, 1.0f, 0.65f, 0.0f, 0.85f);
        }

        bool shotInProgress = s_isShotInProgress.load();
        if (shotInProgress) {
            int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            if (nowMs - s_shotFiredTimeMs.load() > 8000) { // 8s safety timeout
                s_isShotInProgress.store(false);
                shotInProgress = false;
            }
        }

        double curX = 0.0, curY = s_strikerY;
        if (shotInProgress) {
            curX = s_frozenLaunchX.load();
            curY = s_frozenLaunchY.load();
        } else {
            double liveX = 0.0, liveY = 0.0;
            if (CatEngine_GetLiveStrikerPos(&liveX, &liveY)) {
                curX = liveX;
                curY = liveY;
                std::lock_guard<std::mutex> lock(s_posMutex);
                s_strikerX = curX;
                s_strikerY = curY;
                s_hasValidStriker = true;
            } else {
                std::lock_guard<std::mutex> lock(s_posMutex);
                curX = s_strikerX;
                curY = s_strikerY;
            }
        }

        float bMinX = s_baselineMinX.load();
        float bMaxX = s_baselineMaxX.load();

        curY = -25.35;
        float localY = -25.35f;
        float oppY   =  25.35f;

        // Player's active baseline (Cyan)
        drawTableLine(tableMVP, feather, bMinX, localY, bMaxX, localY, 0.35f, 0.0f, 0.8f, 1.0f, 0.90f);
        drawTableRing(tableMVP, feather, bMinX, localY, 1.55f, 0.26f, 0.0f, 0.8f, 1.0f, 0.90f);
        drawTableRing(tableMVP, feather, bMaxX, localY, 1.55f, 0.26f, 0.0f, 0.8f, 1.0f, 0.90f);

        // Opponent's baseline (Orange)
        drawTableLine(tableMVP, feather, bMinX, oppY, bMaxX, oppY, 0.35f, 1.0f, 0.4f, 0.2f, 0.90f);
        drawTableRing(tableMVP, feather, bMinX, oppY, 1.55f, 0.26f, 1.0f, 0.4f, 0.2f, 0.90f);
        drawTableRing(tableMVP, feather, bMaxX, oppY, 1.55f, 0.26f, 1.0f, 0.4f, 0.2f, 0.90f);

        float baseThickness = s_guidelineThickness.load();
        if (baseThickness < 0.10f || baseThickness > 0.40f) baseThickness = 0.20f; // Sleek, non-obstructive hairline guideline
        float stRadius      = s_strikerRadius.load();
        float pkRadius      = s_puckRadius.load();

        BodyColor stColor = getStrikerColor();
        float lineAlpha = s_lineTransparency.load();

        double shotAngle = s_aimAngle.load();
        double forceVal  = s_aimForce.load();

        forceVal = std::clamp(forceVal, 0.0, 1.0);
        if (std::isnan(shotAngle)) {
            shotAngle = (curY > 0.0) ? (-M_PI / 2.0) : (M_PI / 2.0);
        }

        bool isAiming = s_isAiming.load();
        bool autoShotEnabled = s_autoShotEnabled.load();
        bool autoShotActive = autoShotEnabled && s_isShotExecuting.load();
        bool reviewFrozen = s_isReviewFrozen.load();
        bool localTurnReady = CatEngine_IsLocalTurnReady();
        bool keepLine = s_keepLineAfterShot.load();

        bool isAutoScanning = (autoShotEnabled && localTurnReady && !isAiming && !shotInProgress && !reviewFrozen && !autoShotActive);

        bool hasSim = false;
        AimSimulationResult sim;

        static double s_lastSimX = -9999.0, s_lastSimY = -9999.0;
        static double s_lastSimAngle = -9999.0, s_lastSimForce = -9999.0;
        static AimSimulationResult s_cachedAimSim;
        static bool s_hasCachedAimSim = false;

        // 1. If keepLine is ON and shot is in progress: freeze the launched trajectory line
        if (keepLine && shotInProgress && s_hasPersistedSim) {
            std::lock_guard<std::mutex> lock(s_persistedSimMutex);
            sim = s_persistedSim;
            hasSim = true;
        }
        // 2. Active aiming touch or auto-shot execution: live aim takes highest priority!
        else if (isAiming || autoShotActive) {
            s_isReviewFrozen.store(false);
            s_waitingForHandMove.store(false);

            double effectiveForce = isAiming ? forceVal : 1.0;
            if (effectiveForce < 0.05) effectiveForce = 1.0; // Show full preview line when rotating direction

            bool inputChanged = !s_hasCachedAimSim ||
                                std::abs(curX - s_lastSimX) > 0.03 ||
                                std::abs(curY - s_lastSimY) > 0.03 ||
                                std::abs(shotAngle - s_lastSimAngle) > 0.003 ||
                                std::abs(effectiveForce - s_lastSimForce) > 0.01;

            if (inputChanged) {
                sim = simulateStrikerTrajectory(curX, curY, shotAngle, effectiveForce, 5000);
                s_cachedAimSim = sim;
                s_lastSimX = curX;
                s_lastSimY = curY;
                s_lastSimAngle = shotAngle;
                s_lastSimForce = effectiveForce;
                s_hasCachedAimSim = true;
                if (keepLine) {
                    std::lock_guard<std::mutex> lock(s_persistedSimMutex);
                    s_persistedSim = sim;
                    s_hasPersistedSim = true;
                }
            } else {
                sim = s_cachedAimSim;
            }
            hasSim = true;
        }
        // 3. Auto-Shot actively scanning candidate angles: ONLY show when Auto-Shot is ON and actively scanning!
        else if (autoShotEnabled && s_isActivelySearching.load()) {
            double scanX = s_scanLiveX.load();
            double scanY = s_scanLiveY.load();
            double scanA = s_scanLiveAngle.load();
            sim = simulateStrikerTrajectory(scanX, scanY, scanA, 1.0, 3000);
            hasSim = true;
        }
        // 4. Striker returned to baseline and "keep line after shot" is active:
        // Hold the post-shot guideline until the user manually pulls to aim or slides the striker!
        else if (keepLine && reviewFrozen && s_hasPersistedSim && !isAiming) {
            std::lock_guard<std::mutex> lock(s_persistedSimMutex);
            sim = s_persistedSim;
            hasSim = true;
        }

        // Determine target legality based on assigned color
        int targetMode = s_targetColorMode.load(); // 0=Auto, 1=White Only, 2=Black Only, 3=Any
        if (targetMode == 0) {
            targetMode = s_autoDetectedColor.load(); // Dynamic real-time auto detection
        }
        if (targetMode == 0) {
            // Infer from player's active baseline (curY > 0.0 means top baseline = Black, curY <= 0.0 means bottom baseline = White)
            targetMode = (curY > 0.0) ? 2 : 1;
        }
        bool isIllegalTarget = false;
        int targetPuckType = -1;

        if (hasSim && sim.hasFirstContact && sim.firstContactPuckId >= 0) {
            std::lock_guard<std::mutex> lock(s_puckMutex);
            for (const auto& pe : s_puckList) {
                if (pe.id == sim.firstContactPuckId) {
                    targetPuckType = pe.puckType;
                    // Mode 1: Playing White -> Black coins are illegal
                    if (targetMode == 1 && pe.puckType == 2) {
                        isIllegalTarget = true;
                    }
                    // Mode 2: Playing Black -> White coins are illegal
                    else if (targetMode == 2 && pe.puckType == 1) {
                        isIllegalTarget = true;
                    }
                    break;
                }
            }
        }

        // Determine which pocket(s) are targeted by potted pucks (or striker scratch)
        bool pocketPotted[4] = { false, false, false, false };
        bool pocketScratched[4] = { false, false, false, false };
        if (hasSim) {
            if (sim.strikerInPocket && sim.strikerPocketIndex >= 0 && sim.strikerPocketIndex < 4) {
                pocketScratched[sim.strikerPocketIndex] = true;
            }
            if (sim.hitPuck && !isIllegalTarget) {
                for (const auto& traj : sim.puckTrajectories) {
                    if (traj.inPocket && traj.pocketIndex >= 0 && traj.pocketIndex < 4) {
                        pocketPotted[traj.pocketIndex] = true;
                    }
                }
            }
        }

        float pkInset = s_pocketInset.load();
        float pkRad   = s_pocketRadius.load();
        const float pocketCenters[4][2] = {
            {  pkInset,  pkInset },
            { -pkInset,  pkInset },
            {  pkInset, -pkInset },
            { -pkInset, -pkInset }
        };

        // Render the 4 Corner Pocket Detection Guides (clean, unobtrusive wireframe)
        if (s_showPocket.load()) {
            for (int p = 0; p < 4; p++) {
                float pcx = pocketCenters[p][0];
                float pcy = pocketCenters[p][1];
                if (pocketScratched[p]) {
                    // STRIKER SCRATCH FOUL! Highlight with Crimson Warning Red (#FF2222)
                    drawTableCircle(tableMVP, feather, pcx, pcy, pkRad, 1.0f, 0.15f, 0.15f, 0.30f * lineAlpha);
                    drawTableRing(tableMVP, feather, pcx, pcy, pkRad, std::max(0.0f, pkRad - 0.22f), 1.0f, 0.20f, 0.20f, 0.95f * lineAlpha);
                } else if (pocketPotted[p]) {
                    // Legitimate Puck Pot: Subtle Emerald Green
                    drawTableCircle(tableMVP, feather, pcx, pcy, pkRad, kPotColor.r, kPotColor.g, kPotColor.b, 0.20f * lineAlpha);
                    drawTableRing(tableMVP, feather, pcx, pcy, pkRad, std::max(0.0f, pkRad - 0.22f), kPotColor.r, kPotColor.g, kPotColor.b, 0.85f * lineAlpha);
                } else {
                    // Idle pocket: Very subtle translucent cyan ring (never obstructs hole)
                    drawTableRing(tableMVP, feather, pcx, pcy, pkRad, std::max(0.0f, pkRad - 0.12f), 0.00f, 0.85f, 1.00f, 0.35f * lineAlpha);
                }
            }
        }

        // Render subtle ring ONLY on the target puck being aimed at (never obscure coins with filled discs)
        if (hasSim && sim.hasFirstContact && sim.firstContactPuckId >= 0) {
            std::lock_guard<std::mutex> lock(s_puckMutex);
            for (const auto& pe : s_puckList) {
                if (pe.id == sim.firstContactPuckId && pe.active && !isPuckPottedOrInvalid(pe.x, pe.y, pkInset, pkRad)) {
                    BodyColor col = isIllegalTarget ? BodyColor{ 1.0f, 0.15f, 0.15f, 1.0f } : getEntityColor(pe.id, pe.puckType);
                    float innerR = std::max(0.0f, pkRadius - 0.12f);
                    drawTableRing(tableMVP, feather, pe.x, pe.y, pkRadius, innerR, col.r, col.g, col.b, 0.85f * lineAlpha);
                    break;
                }
            }
        }

        // Striker live position
        float strikerCircleX = static_cast<float>(curX);
        float strikerCircleY = static_cast<float>(curY);

        if (shotInProgress) {
            // Only show frozen circle on baseline if keepLine is ON
            if (keepLine && s_hasPersistedSim) {
                float launchX = static_cast<float>(s_frozenLaunchX.load());
                drawTableCircle(tableMVP, feather, launchX, strikerCircleY, stRadius, stColor.r, stColor.g, stColor.b, 0.45f * lineAlpha);
            }
        } else {
            // Live striker circle on baseline
            drawTableCircle(tableMVP, feather, strikerCircleX, strikerCircleY, stRadius, stColor.r, stColor.g, stColor.b, 0.45f * lineAlpha);
        }

        if (hasSim && s_showPrediction.load()) {
            if (sim.strikerInPocket || isIllegalTarget) {
                // Warning Red line for scratch foul or illegal target!
                stColor = { 1.0f, 0.18f, 0.18f, 1.0f };
            }
            // Guarantee at least one forward segment so the guideline is NEVER invisible!
            if (sim.segments.empty()) {
                double endX = sim.finalStrikerX;
                double endY = sim.finalStrikerY;
                if (std::hypot(endX - curX, endY - curY) < 0.1) {
                    endX = curX + 35.0 * std::cos(shotAngle);
                    endY = curY + 35.0 * std::sin(shotAngle);
                }
                TrajectorySegment fallbackSeg;
                fallbackSeg.startX = static_cast<float>(curX);
                fallbackSeg.startY = static_cast<float>(curY);
                fallbackSeg.endX   = static_cast<float>(endX);
                fallbackSeg.endY   = static_cast<float>(endY);
                fallbackSeg.isCushionBounce = false;
                fallbackSeg.isPuckHit = false;
                sim.segments.push_back(fallbackSeg);
            }

            // 1. Draw Striker Guideline Segments as a seamless continuous mitered polyline
            if (!sim.segments.empty()) {
                std::vector<MCPointFloat> stPts;
                stPts.reserve(sim.segments.size() + 1);
                stPts.push_back({ sim.segments[0].startX, sim.segments[0].startY });
                for (const auto& seg : sim.segments) {
                    stPts.push_back({ seg.endX, seg.endY });
                }
                drawTablePolyline(tableMVP, feather, stPts,
                                  baseThickness, stColor.r, stColor.g, stColor.b, stColor.a * lineAlpha);
            }

            // 2. Ghost Striker Circle at Rest
            drawTableCircle(tableMVP, feather, sim.finalStrikerX, sim.finalStrikerY,
                            stRadius, stColor.r, stColor.g, stColor.b, stColor.a * lineAlpha);

            // 3. Struck Puck Guidelines & Landing Circles
            if (sim.hitPuck || !sim.puckTrajectories.empty()) {
                for (size_t p = 0; p < sim.puckTrajectories.size(); p++) {
                    const auto& traj = sim.puckTrajectories[p];
                    if (traj.segments.empty() && !traj.inPocket && !traj.isHit) continue;
                    // If illegal target: Warning Red; If potted: Electric Neon Emerald; Else: Entity Color
                    BodyColor pkColor;
                    if (isIllegalTarget) {
                        pkColor = { 1.0f, 0.18f, 0.18f, 1.0f };
                    } else if (traj.inPocket) {
                        pkColor = kPotColor;
                    } else {
                        pkColor = getEntityColor(traj.puckIndex, traj.puckType);
                    }

                    if (traj.segments.empty() && (traj.isHit || traj.inPocket)) {
                        drawTableLine(tableMVP, feather, traj.startX, traj.startY, traj.finalX, traj.finalY,
                                      baseThickness, pkColor.r, pkColor.g, pkColor.b, pkColor.a * lineAlpha);
                    } else if (!traj.segments.empty()) {
                        std::vector<MCPointFloat> pkPts;
                        pkPts.reserve(traj.segments.size() + 1);
                        pkPts.push_back({ traj.segments[0].startX, traj.segments[0].startY });
                        for (const auto& seg : traj.segments) {
                            pkPts.push_back({ seg.endX, seg.endY });
                        }
                        drawTablePolyline(tableMVP, feather, pkPts,
                                          baseThickness, pkColor.r, pkColor.g, pkColor.b, pkColor.a * lineAlpha);
                    }

                    if (traj.inPocket) {
                        // Pocket landing: Subtle wireframe ring, never an opaque blob covering the hole
                        drawTableRing(tableMVP, feather, traj.finalX, traj.finalY,
                                      pkRadius * 1.15f, pkRadius * 0.95f, kPotColor.r, kPotColor.g, kPotColor.b, 0.75f * lineAlpha);
                    } else if (traj.isHit || !traj.segments.empty()) {
                        drawTableRing(tableMVP, feather, traj.finalX, traj.finalY,
                                      pkRadius, std::max(0.0f, pkRadius - 0.12f), pkColor.r, pkColor.g, pkColor.b, 0.65f * lineAlpha);
                    }
                }
            }
        }

        // Restore OpenGL ES State
        glBindBuffer(GL_ARRAY_BUFFER, prevVBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prevIBO);
        glUseProgram(prevProgram);

        if (!blendWasOn)   glDisable(GL_BLEND);
        glBlendFuncSeparate(prevBlendSrcRGB, prevBlendDstRGB, prevBlendSrcAlpha, prevBlendDstAlpha);

        if (depthWasOn)    glEnable(GL_DEPTH_TEST);
        if (cullWasOn)     glEnable(GL_CULL_FACE);
        if (scissorWasOn)  glEnable(GL_SCISSOR_TEST);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    }

    if (orig_eglSwapBuffers) {
        return orig_eglSwapBuffers(dpy, surface);
    }
    return EGL_TRUE;
}

// =========================================================================================
// PUBLIC API IMPLEMENTATION (MATCHING GraphicsEngine.h)
// =========================================================================================

void initGraphicsEngine() {
    if (orig_eglSwapBuffers) return;

    void* sym = dlsym(RTLD_DEFAULT, "eglSwapBuffers");
    if (!sym) {
        void* h = dlopen("libEGL.so", RTLD_NOW);
        if (h) sym = dlsym(h, "eglSwapBuffers");
    }
    if (!sym) {
        sym = DobbySymbolResolver(nullptr, "eglSwapBuffers");
    }
    if (!sym) {
        sym = DobbySymbolResolver("libEGL.so", "eglSwapBuffers");
    }

    ALOGI("[CatEngine] [GraphicsEngine] initGraphicsEngine: symbol eglSwapBuffers at %p", sym);

    if (sym) {
        if (DobbyHook(sym, reinterpret_cast<void*>(my_eglSwapBuffers),
                      reinterpret_cast<void**>(&orig_eglSwapBuffers)) == 0) {
            ALOGI("[CatEngine] [GraphicsEngine] >>> HOOKED eglSwapBuffers SUCCESSFULLY at %p (orig=%p)! <<<", sym, orig_eglSwapBuffers);
        } else {
            ALOGE("[CatEngine] [GraphicsEngine] DobbyHook failed on eglSwapBuffers at %p!", sym);
        }
    } else {
        ALOGE("[CatEngine] [GraphicsEngine] Could not resolve eglSwapBuffers symbol!");
    }
}

void onStrikerPositionUpdated(double x, double y) {
    std::lock_guard<std::mutex> lock(s_posMutex);
    s_strikerX = x;
    s_strikerY = y;
    s_hasValidStriker = true;

    static double s_lastStrikerX = -999.0;
    static double s_lastStrikerY = -999.0;
    if (std::abs(x - s_lastStrikerX) > 0.2 || std::abs(y - s_lastStrikerY) > 0.2) {
        s_lastStrikerX = x;
        s_lastStrikerY = y;
        ALOGI("[CatEngine] Striker position on baseline: X=%.2f, Y=%.2f", x, y);
    }
}

void setGraphicsMatchActive(bool active) {
    s_matchActive.store(active);
    if (!active) {
        s_isAiming.store(false);
        s_hasValidStriker = false;
        s_isShotExecuting.store(false);
        s_isShotInProgress.store(false);
        s_hasPersistedSim = false;
        s_isReviewFrozen.store(false);
        s_waitingForHandMove.store(false);
        GraphicsEngine_ClearPucks();
    }
    ALOGI("[CatEngine] Match active state: %d", active ? 1 : 0);
}

void GraphicsEngine_UpdateAim(double x, double y, double angle, double force, bool isAiming) {
    {
        std::lock_guard<std::mutex> lock(s_posMutex);
        s_strikerX = x;
        s_strikerY = y;
        s_hasValidStriker = true;
    }
    if (!std::isnan(angle) && std::abs(angle) < 100.0) {
        s_aimAngle.store(angle);
    }
    s_aimForce.store(force);

    bool wasAiming = s_isAiming.exchange(isAiming);
    if (isAiming) {
        // User started aiming/rotating striker: exit review freeze immediately!
        s_isReviewFrozen.store(false);
        s_waitingForHandMove.store(false);
    }
    if (wasAiming && !isAiming) {
        int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        s_shotReleasedTimeMs.store(nowMs);
    }
}

bool GraphicsEngine_IsAiming() {
    return s_isAiming.load();
}

void GraphicsEngine_SetBoardBounds(float left, float right, float bottom, float top, bool showBorder) {
    s_manualL.store(left);
    s_manualR.store(right);
    s_manualB.store(bottom);
    s_manualT.store(top);
    s_hasManualBounds.store(true);
    if (showBorder) {
        s_showBorder.store(true);
    }
    ALOGI("[GraphicsEngine] Manual bounds updated: L=%.1f, R=%.1f, B=%.1f, T=%.1f, showBorder=%d",
          left, right, bottom, top, s_showBorder.load() ? 1 : 0);
}

void GraphicsEngine_SetBoardCalibration(float centerX, float centerY, float scale) {
    s_autoCenterX.store(centerX);
    s_autoCenterY.store(centerY);
    s_autoScale.store(scale);
    s_hasEngineCalibration.store(true);
}

void GraphicsEngine_SetAutoCalibration(float centerX, float centerY, float scale, bool fromEngine) {
    if (scale > 5.0f) {
        s_autoCenterX.store(centerX);
        s_autoCenterY.store(centerY);
        s_autoScale.store(scale);
        s_hasEngineCalibration.store(true);
    }
}

void GraphicsEngine_SetNativeAffineTransform(double a, double b, double c, double d, double tx, double ty) {
    s_nativeAffineA.store(a);
    s_nativeAffineB.store(b);
    s_nativeAffineC.store(c);
    s_nativeAffineD.store(d);
    s_nativeAffineTx.store(tx);
    s_nativeAffineTy.store(ty);
    s_hasNativeAffine.store(true);
}

void GraphicsEngine_ResetNativeAffine() {
    s_hasNativeAffine.store(false);
    s_hasEngineCalibration.store(false);
}

bool GraphicsEngine_HasNativeAffine() {
    return s_hasNativeAffine.load();
}

void GraphicsEngine_ResetToAuto() {
    s_hasManualBounds.store(false);
    s_userManualPhysics.store(false);
    s_hasUserPocketRadius.store(false);
}

bool GraphicsEngine_HasManualBounds() {
    return s_hasManualBounds.load();
}

void GraphicsEngine_OnTableSpacePoint(double tx, double ty, double sx, double sy) {
    // Dynamic projection refinement
}

void GraphicsEngine_SetBaselineRange(float minX, float maxX) {
    s_baselineMinX.store(minX);
    s_baselineMaxX.store(maxX);
}

void GraphicsEngine_SetObjectGeometry(float strikerRadius, float puckRadius, float pocketInset, float pocketRadius) {
    s_strikerRadius.store(strikerRadius);
    s_puckRadius.store(puckRadius);
    s_pocketInset.store(pocketInset);
    s_pocketRadius.store(pocketRadius);
    s_hasUserPocketRadius.store(true);
}

void GraphicsEngine_SetStrikerRadius(float radius) {
    s_strikerRadius.store(radius);
}

void GraphicsEngine_SetPuckRadius(float radius) {
    s_puckRadius.store(radius);
}

float GraphicsEngine_GetPuckRadius() {
    return s_puckRadius.load();
}

void GraphicsEngine_OnMatchDestroyed() {
    s_matchActive.store(false);
    s_hasValidStriker = false;
    s_isShotExecuting.store(false);
    s_isShotInProgress.store(false);
    s_hasPersistedSim = false;
    s_isReviewFrozen.store(false);
    s_waitingForHandMove.store(false);
    s_isAiming.store(false);
    s_turnScanStartTimeMs.store(0);
    GraphicsEngine_ClearPucks();
}

void GraphicsEngine_SetPocketRadius(float radius) {
    s_pocketRadius.store(radius);
    s_hasUserPocketRadius.store(true);
}

float GraphicsEngine_GetPocketRadius() {
    return s_pocketRadius.load();
}

void GraphicsEngine_SetPocketGeometry(float inset, float radius) {
    s_pocketInset.store(inset);
    if (!s_hasUserPocketRadius.load()) {
        s_pocketRadius.store(radius);
    }
}

void GraphicsEngine_SetMaxTravelDistance(float maxDistance) {
    s_maxTravelDistance.store(maxDistance);
}

void GraphicsEngine_SetStrikerPhysics(double minPower, double maxPower, double strikerBonus) {
    s_minPower.store(minPower);
    s_maxPower.store(maxPower);
    if (!s_userManualPhysics.load()) {
        s_strikerBonus.store(strikerBonus);
    }
}

void GraphicsEngine_SetPhysicalConstants(double tableFriction,
                                         double strikerFriction, double strikerMass, double strikerRestitution,
                                         double puckFriction, double puckMass, double puckRestitution,
                                         double cushionRestitution) {
    if (s_userManualPhysics.load()) {
        return;
    }
    // The raw strikerFriction from libgame.so (~0.0410) is a pure-slip mu.
    // Our simulator uses mu_roll (Coulomb rolling friction after slip phase ends).
    // Calibrated minimum is 0.1600 to match authentic stopping distances.
    // Never let the game's raw value reduce us below the calibrated minimum.
    static const double kMinStrikerFriction = 0.1600;
    static const double kMinPuckFriction    = 0.1080;
    double calibratedStriker = std::max(strikerFriction, kMinStrikerFriction);
    double calibratedPuck    = std::max(puckFriction, kMinPuckFriction);
    s_strikerFriction.store(calibratedStriker);
    s_puckFriction.store(calibratedPuck);
    s_strikerMass.store(strikerMass);
    s_puckMass.store(puckMass);
    s_restitutionBody.store(1.00); // Inter-disc collision is 100% elastic in libgame.so
    s_restitutionCushion.store(cushionRestitution);
    ALOGI("[CatEngine] Physics constants updated: StFric=%.4f (raw=%.4f), PkFric=%.4f (raw=%.4f), Mass=%.2f, CushRest=%.3f",
          calibratedStriker, strikerFriction, calibratedPuck, puckFriction, strikerMass, cushionRestitution);
}

void GraphicsEngine_SetFriction(double strikerFriction, double puckFriction) {
    if (s_userManualPhysics.load()) return;
    // Clamp to calibrated minimums (see SetPhysicalConstants)
    s_strikerFriction.store(std::max(strikerFriction, 0.1600));
    s_puckFriction.store(std::max(puckFriction, 0.1080));
}

void GraphicsEngine_SetFineTunePhysics(double strikerFriction, double puckFriction,
                                      double restitutionBody, double restitutionCushion,
                                      double strikerMass, double powerMultiplier) {
    s_userManualPhysics.store(true);
    s_strikerFriction.store(strikerFriction);
    s_puckFriction.store(puckFriction);
    s_restitutionBody.store(restitutionBody);
    s_restitutionCushion.store(restitutionCushion);
    s_strikerMass.store(strikerMass);
    s_powerMultiplier.store(powerMultiplier);
}

void GraphicsEngine_SetAdvancedPhysics(double contactFriction, double cushionTangentFriction, double strikerBonus) {
    s_userManualPhysics.store(true);
    s_contactFriction.store(contactFriction);
    s_cushionTangentFriction.store(cushionTangentFriction);
    s_strikerBonus.store(strikerBonus);
}

void GraphicsEngine_GetAdvancedPhysics(double* contactFriction, double* cushionTangentFriction, double* strikerBonus) {
    if (contactFriction)         *contactFriction         = s_contactFriction.load();
    if (cushionTangentFriction)  *cushionTangentFriction  = s_cushionTangentFriction.load();
    if (strikerBonus)            *strikerBonus            = s_strikerBonus.load();
}

void GraphicsEngine_SetPocketPhysics(double dropMaxSpeed, double gravity) {
    s_userManualPhysics.store(true);
    s_pocketDropMaxSpeed.store(dropMaxSpeed);
    s_pocketGravity.store(gravity);
}

void GraphicsEngine_GetPocketPhysics(double* dropMaxSpeed, double* gravity) {
    if (dropMaxSpeed) *dropMaxSpeed = s_pocketDropMaxSpeed.load();
    if (gravity)      *gravity      = s_pocketGravity.load();
}

bool GraphicsEngine_IsUserManualPhysics() {
    return s_userManualPhysics.load();
}

float GraphicsEngine_GetStrikerRadius() {
    return s_strikerRadius.load();
}

float GraphicsEngine_GetPocketInset() {
    return s_pocketInset.load();
}

void GraphicsEngine_GetFineTunePhysics(double* strikerFriction, double* puckFriction,
                                      double* restitutionBody, double* restitutionCushion,
                                      double* strikerMass, double* powerMultiplier) {
    if (strikerFriction)     *strikerFriction     = s_strikerFriction.load();
    if (puckFriction)        *puckFriction        = s_puckFriction.load();
    if (restitutionBody)     *restitutionBody     = s_restitutionBody.load();
    if (restitutionCushion)  *restitutionCushion  = s_restitutionCushion.load();
    if (strikerMass)         *strikerMass         = s_strikerMass.load();
    if (powerMultiplier)     *powerMultiplier     = s_powerMultiplier.load();
}

void GraphicsEngine_SaveAutoCorrectedPhysics() {}
void GraphicsEngine_LoadAutoCorrectedPhysics() {}

void GraphicsEngine_SetGuidelineThickness(float thickness) {
    s_guidelineThickness.store(thickness);
}

void GraphicsEngine_SetColorTheme(int theme) {
    s_colorTheme.store(theme);
}

void GraphicsEngine_UpdatePuck(int index, double x, double y, bool active, int puckType) {
    float pkInset = s_pocketInset.load();
    float pkRad   = s_puckRadius.load();
    bool isPotted = isPuckPottedOrInvalid(x, y, pkInset, pkRad);
    bool effectiveActive = active && !isPotted;

    std::lock_guard<std::mutex> lock(s_puckMutex);
    for (auto& pe : s_puckList) {
        if (pe.id == index) {
            pe.x = x;
            pe.y = y;
            pe.active = effectiveActive;
            pe.puckType = puckType;
            return;
        }
    }
    if (effectiveActive) {
        TablePuckEntry pe;
        pe.id = index;
        pe.x = x;
        pe.y = y;
        pe.active = true;
        pe.puckType = puckType;
        s_puckList.push_back(pe);
    }

    static int s_puckUpdateCount = 0;
    if (++s_puckUpdateCount % 20 == 0) {
        ALOGI("[CatEngine] Puck updated: id=%d, type=%d, pos=(%.2f, %.2f), active=%d (total=%zu)",
              index, puckType, x, y, effectiveActive ? 1 : 0, s_puckList.size());
    }
}

void GraphicsEngine_ClearPucks() {
    std::lock_guard<std::mutex> lock(s_puckMutex);
    s_puckList.clear();
}

void GraphicsEngine_SyncAllPucks(const GraphicsPuckInfo* pucks, int count) {
    float pkInset = s_pocketInset.load();
    float pkRad   = s_puckRadius.load();
    std::lock_guard<std::mutex> lock(s_puckMutex);
    s_puckList.clear();
    if (!pucks || count <= 0) return;
    s_puckList.reserve(count);
    for (int i = 0; i < count; i++) {
        if (!pucks[i].active) continue;
        if (isPuckPottedOrInvalid(pucks[i].x, pucks[i].y, pkInset, pkRad)) continue;
        TablePuckEntry pe;
        pe.id = pucks[i].id;
        pe.x = pucks[i].x;
        pe.y = pucks[i].y;
        pe.active = true;
        pe.puckType = pucks[i].puckType;
        s_puckList.push_back(pe);
    }
}

void GraphicsEngine_OnShotFired(double curX, double curY, double shotAngle, double shotForce) {
    s_isAiming.store(false);
    s_isReviewFrozen.store(false);
    s_waitingForHandMove.store(false);

    s_frozenLaunchX.store(curX);
    s_frozenLaunchY.store(curY);
    if (shotForce < 0.05) shotForce = 0.05;
    if (std::isnan(shotAngle)) shotAngle = M_PI / 2.0;

    int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    s_shotFiredTimeMs.store(nowMs);
    s_isShotInProgress.store(true);
    s_turnScanStartTimeMs.store(0);

    if (s_keepLineAfterShot.load()) {
        AimSimulationResult sim = simulateStrikerTrajectory(curX, curY, shotAngle, shotForce);
        std::lock_guard<std::mutex> lock(s_persistedSimMutex);
        s_persistedSim = sim;
        s_hasPersistedSim = true;
        ALOGI("[CatEngine] [GraphicsEngine] Shot fired from (%.2f, %.2f): Keep-line ON, persisting line.", curX, curY);
    } else {
        std::lock_guard<std::mutex> lock(s_persistedSimMutex);
        s_hasPersistedSim = false;
        ALOGI("[CatEngine] [GraphicsEngine] Shot fired from (%.2f, %.2f): Keep-line OFF, clearing line immediately.", curX, curY);
    }
}

void GraphicsEngine_OnStrikerReturnedToBaseline() {
    s_isShotInProgress.store(false);
    s_isAiming.store(false);
    s_turnScanStartTimeMs.store(0);
    s_hasCachedTier2.store(false);
    s_hasCachedTier3.store(false);

    if (s_keepLineAfterShot.load()) {
        s_isReviewFrozen.store(true);
        s_waitingForHandMove.store(true);
        s_freezeBaselineX.store(s_strikerX);
        ALOGI("[CatEngine] [GraphicsEngine] Striker returned to baseline: keep-line-after-shot active, waiting for hand move in X from %.2f", s_strikerX);
    } else {
        s_isReviewFrozen.store(false);
        s_waitingForHandMove.store(false);
        std::lock_guard<std::mutex> lock(s_persistedSimMutex);
        s_hasPersistedSim = false;
        ALOGI("[CatEngine] [GraphicsEngine] Striker returned to baseline: keep-line-after-shot is off, line dismissed.");
    }
}

void GraphicsEngine_SetReviewFreeze(bool freeze) {
    s_isReviewFrozen.store(freeze);
}

bool GraphicsEngine_IsReviewFrozen() {
    return s_isReviewFrozen.load();
}

static std::atomic<int> s_telemetryShotCounter(0);

ShotPredictionTelemetry GraphicsEngine_CaptureShotPrediction(double startX, double startY, double shotAngle, double forceVal) {
    ShotPredictionTelemetry tele;
    memset(&tele, 0, sizeof(tele));

    tele.shotId = ++s_telemetryShotCounter;
    tele.strikerStartX = startX;
    tele.strikerStartY = startY;
    tele.shotAngleRad  = shotAngle;
    tele.shotAngleDeg  = shotAngle * 180.0 / M_PI;
    tele.shotForceVal  = forceVal;

    double fClamped = std::clamp(forceVal, 0.0, 1.0);
    double minP = s_minPower.load();
    double maxP = s_maxPower.load();
    double bonus = s_strikerBonus.load();
    double v0 = minP + fClamped * (maxP - minP);
    if (fClamped >= 0.999 && bonus > 0.0) {
        v0 += bonus;
    }
    v0 *= s_powerMultiplier.load();
    tele.strikerInitialSpeed = v0;

    AimSimulationResult sim = simulateStrikerTrajectory(startX, startY, shotAngle, forceVal);

    tele.strikerPredFinalX = sim.finalStrikerX;
    tele.strikerPredFinalY = sim.finalStrikerY;
    tele.strikerPredPocket = sim.strikerInPocket;
    tele.strikerPocketIndex = sim.strikerPocketIndex;
    tele.strikerCushionBounces = sim.strikerCushionBounces;
    tele.hasFirstImpact    = sim.hasFirstContact;
    tele.firstContactPuckId = sim.firstContactPuckId;
    tele.firstImpactStrikerX = sim.firstImpactStrikerX;
    tele.firstImpactStrikerY = sim.firstImpactStrikerY;
    tele.firstImpactPuckX    = sim.firstImpactPuckX;
    tele.firstImpactPuckY    = sim.firstImpactPuckY;

    // Striker segments & line length
    double stTotalLen = 0.0;
    tele.strikerSegmentCount = std::min(static_cast<int>(sim.segments.size()), MAX_LINE_SEGMENTS);
    for (int i = 0; i < tele.strikerSegmentCount; i++) {
        tele.strikerSegments[i].startX = sim.segments[i].startX;
        tele.strikerSegments[i].startY = sim.segments[i].startY;
        tele.strikerSegments[i].endX   = sim.segments[i].endX;
        tele.strikerSegments[i].endY   = sim.segments[i].endY;
        tele.strikerSegments[i].isCushionBounce = sim.segments[i].isCushionBounce;
        stTotalLen += std::hypot(sim.segments[i].endX - sim.segments[i].startX,
                                 sim.segments[i].endY - sim.segments[i].startY);
    }
    if (stTotalLen < 0.1 && !sim.segments.empty()) {
        stTotalLen = std::hypot(sim.finalStrikerX - startX, sim.finalStrikerY - startY);
    }
    tele.strikerTotalLineLength = stTotalLen;

    // Populate EVERY SINGLE PUCK from sim.puckTrajectories
    tele.totalPucksOnTable = std::min(static_cast<int>(sim.puckTrajectories.size()), MAX_PREDICTED_PUCKS);
    tele.predictedPuckCount = tele.totalPucksOnTable;
    for (int i = 0; i < tele.totalPucksOnTable; i++) {
        const auto& pt = sim.puckTrajectories[i];
        tele.predictedPucks[i].puckId   = pt.puckIndex;
        tele.predictedPucks[i].puckType = pt.puckType;
        tele.predictedPucks[i].startX   = pt.startX;
        tele.predictedPucks[i].startY   = pt.startY;
        tele.predictedPucks[i].predFinalX = pt.finalX;
        tele.predictedPucks[i].predFinalY = pt.finalY;
        tele.predictedPucks[i].isHit      = pt.isHit;
        tele.predictedPucks[i].hitByEntityId = pt.hitByEntityId;
        tele.predictedPucks[i].hitSpeed   = pt.hitSpeed;
        tele.predictedPucks[i].predPocket = pt.inPocket;
        tele.predictedPucks[i].pocketIndex = pt.pocketIndex;
        tele.predictedPucks[i].predictedCushionBounces = pt.cushionBounces;

        if (pt.hitByEntityId == 0) {
            snprintf(tele.predictedPucks[i].hitByName, sizeof(tele.predictedPucks[i].hitByName), "Striker");
        } else if (pt.hitByEntityId > 0) {
            snprintf(tele.predictedPucks[i].hitByName, sizeof(tele.predictedPucks[i].hitByName), "Puck #%d", pt.hitByEntityId);
        } else {
            snprintf(tele.predictedPucks[i].hitByName, sizeof(tele.predictedPucks[i].hitByName), "Static");
        }

        if (pt.puckIndex == tele.firstContactPuckId) {
            tele.firstContactPuckType = pt.puckType;
            tele.firstImpactSpeed = pt.hitSpeed;
        }

        double pkLen = 0.0;
        int segCount = std::min(static_cast<int>(pt.segments.size()), MAX_LINE_SEGMENTS);
        tele.predictedPucks[i].segmentCount = segCount;
        for (int s = 0; s < segCount; s++) {
            tele.predictedPucks[i].segments[s].startX = pt.segments[s].startX;
            tele.predictedPucks[i].segments[s].startY = pt.segments[s].startY;
            tele.predictedPucks[i].segments[s].endX   = pt.segments[s].endX;
            tele.predictedPucks[i].segments[s].endY   = pt.segments[s].endY;
            tele.predictedPucks[i].segments[s].isCushionBounce = pt.segments[s].isCushionBounce;
            pkLen += std::hypot(pt.segments[s].endX - pt.segments[s].startX,
                                pt.segments[s].endY - pt.segments[s].startY);
        }
        if (pkLen < 0.1 && pt.isHit) {
            pkLen = std::hypot(pt.finalX - pt.startX, pt.finalY - pt.startY);
        }
        tele.predictedPucks[i].totalLineLength = pkLen;
    }

    return tele;
}

AnglePocketOutcome GraphicsEngine_PredictAnglePocket(double shotAngleRad, double forceVal) {
    AnglePocketOutcome outcome;
    outcome.willPocketPuck = false;
    outcome.pocketedPuckId = -1;
    outcome.pocketedPuckType = 0;
    outcome.strikerScratched = false;
    outcome.cushionBounces = 0;

    double curX = 0.0, curY = s_strikerY;
    {
        std::lock_guard<std::mutex> lock(s_posMutex);
        curX = s_strikerX;
        curY = s_strikerY;
    }

    AimSimulationResult sim = simulateStrikerTrajectory(curX, curY, shotAngleRad, forceVal);

    float pkInset = s_pocketInset.load();
    float pkRad   = s_pocketRadius.load();
    if (isInsidePocketHole(sim.finalStrikerX, sim.finalStrikerY, pkInset, pkRad)) {
        outcome.strikerScratched = true;
    }

    for (const auto& traj : sim.puckTrajectories) {
        if (traj.inPocket) {
            outcome.willPocketPuck = true;
            outcome.pocketedPuckId = traj.puckIndex;
            outcome.pocketedPuckType = traj.puckType;
            for (const auto& seg : traj.segments) {
                if (seg.isCushionBounce) outcome.cushionBounces++;
            }
            break;
        }
    }

    return outcome;
}

bool GraphicsEngine_WillAnglePocket(double shotAngleRad, double forceVal) {
    AnglePocketOutcome o = GraphicsEngine_PredictAnglePocket(shotAngleRad, forceVal);
    return (o.willPocketPuck && !o.strikerScratched);
}

bool checkAndProcessPocketingAngle(double forceVal) {
    for (double deg = 0.0; deg < 360.0; deg += 1.0) {
        double rad = deg * M_PI / 180.0;
        if (GraphicsEngine_WillAnglePocket(rad, forceVal)) {
            return true;
        }
    }
    return false;
}

bool GraphicsEngine_IsShotExecuting() {
    return s_isShotExecuting.load();
}

// Forward declarations from catEngine.cpp
extern "C" bool CatEngine_ExecuteInEngineShot(double angleRad, double forceVal);
extern "C" bool CatEngine_MoveStriker(double x, double y);

static const char* getPuckTypeName(int type) {
    if (type == 3) return "Red (Queen)";
    if (type == 2) return "Black (Yellow)";
    if (type == 1) return "White (Blue)";
    return "Unknown";
}

void GraphicsEngine_PullAndReleaseStriker(double angleRad, double forceVal) {
    if (s_isShotExecuting.exchange(true)) {
        ALOGW("[AUTO-SHOT EXECUTE] Shot already executing, ignoring duplicate trigger.");
        return;
    }

    std::thread([angleRad, forceVal]() {
        // Minimal 1ms pause so striker placement on baseline is registered
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        bool shotOk = CatEngine_ExecuteInEngineShot(angleRad, forceVal);
        if (!shotOk) {
            ALOGE("[AUTO-SHOT RESULT: FAILED] CatEngine_ExecuteInEngineShot returned false! Resetting s_isShotExecuting flag.");
            s_isShotExecuting.store(false);
            return;
        }

        // Minimal 1ms cooldown
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        s_isShotExecuting.store(false);
        ALOGI("[AUTO-SHOT EXECUTE] Shot execution complete.");
    }).detach();
}

void GraphicsEngine_PullAndReleaseStrikerDegrees(double angleDeg, double forceVal) {
    GraphicsEngine_PullAndReleaseStriker(angleDeg * M_PI / 180.0, forceVal);
}

static std::atomic<bool> s_continuousSearchRunning(false);

void startContinuousAnglePocketSearch() {
    if (s_continuousSearchRunning.exchange(true)) return;

    std::thread([]() {
        while (s_continuousSearchRunning.load()) {
            if (s_matchActive.load()) {
                checkAndProcessPocketingAngle(1.0);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }).detach();
}

void stopContinuousAnglePocketSearch() {
    s_continuousSearchRunning.store(false);
}

bool GraphicsEngine_IsMatchActive() {
    return s_matchActive.load();
}

void GraphicsEngine_SetAutoShotEnabled(bool enabled) {
    s_autoShotEnabled.store(enabled);
}

bool GraphicsEngine_IsAutoShotEnabled() {
    return s_autoShotEnabled.load();
}

void GraphicsEngine_SetFastFirstShotMode(bool enabled) {
    s_fastFirstShotMode.store(enabled);
    ALOGI("[GRAPHICS ENGINE] Fast First Shot Mode set to %d", enabled ? 1 : 0);
}

bool GraphicsEngine_IsFastFirstShotMode() {
    return s_fastFirstShotMode.load();
}

void GraphicsEngine_SetTargetColorMode(int mode) {
    s_targetColorMode.store(mode);
    ALOGI("[GRAPHICS ENGINE] Target Color Mode set to %d (0=Auto, 1=White, 2=Black, 3=Any)", mode);
}

int GraphicsEngine_GetTargetColorMode() {
    return s_targetColorMode.load();
}

void GraphicsEngine_SetAutoDetectedColor(int color) {
    s_autoDetectedColor.store(color);
    ALOGI("[GRAPHICS ENGINE] Auto-Detected Color updated to %d (1=White, 2=Black, 3=Any)", color);
}

int GraphicsEngine_GetAutoDetectedColor() {
    return s_autoDetectedColor.load();
}

// =========================================================================================
// AUTO-STRIKE COMBINATION SEARCH CONTROLLER (HIGH-SPEED DYNAMIC BASELINE & ALL-ANGLE SCANNER)
// =========================================================================================
struct CandidateShot {
    float candX = 0.0f;
    float candY = 0.0f;  // Will be set to live baselineY at scan time
    double angleRad = 0.0;
    double forceVal = 1.0;
    int tier = 99; // 1 = Direct Clean, 2 = 1-Cushion Bank, 3 = Direct Combo, 4 = Multi-Bank
    double score = -1e9;
    double riskScore = 999.0; // Lower is safer/cleaner (0-100)
    int targetPuckId = -1;
    int targetPuckType = 1;
    bool potted = false;
    int pocketId = -1;
    int cushionCount = 0;
    int secondaryHits = 0;
    double cutAngleDeg = 0.0;
    double puckStartX = 0.0;
    double puckStartY = 0.0;
    double predLandingX = 0.0;
    double predLandingY = 0.0;
};

static CandidateShot s_cachedFallbackTier2;
static CandidateShot s_cachedFallbackTier3;

static inline double distSqToSegment(double px, double py, double ax, double ay, double bx, double by) {
    double abx = bx - ax;
    double aby = by - ay;
    double apx = px - ax;
    double apy = py - ay;
    double abLenSq = abx * abx + aby * aby;
    if (abLenSq < 1e-6) {
        return apx * apx + apy * apy;
    }
    double t = std::clamp((apx * abx + apy * aby) / abLenSq, 0.0, 1.0);
    double closestX = ax + t * abx;
    double closestY = ay + t * aby;
    double dx = px - closestX;
    double dy = py - closestY;
    return dx * dx + dy * dy;
}

static bool evaluateCandidateShot(const AimSimulationResult& sim, float candX, float candY,
                                  double angleRad, double forceVal,
                                  CandidateShot* outCand, bool allowBankShots = false) {
    if (!outCand) return false;

    float pkInset = s_pocketInset.load();
    float pkRad   = s_pocketRadius.load();

    // Scratch foul: Striker enters pocket
    if (sim.strikerInPocket || isInsidePocketHole(sim.finalStrikerX, sim.finalStrikerY, pkInset, pkRad)) {
        return false;
    }

    if (!sim.hasFirstContact || sim.puckTrajectories.empty()) {
        return false;
    }

    // Check if striker bounced before hitting puck
    bool strikerHadBounce = false;
    for (const auto& seg : sim.segments) {
        if (seg.isCushionBounce) { strikerHadBounce = true; break; }
        if (seg.isPuckHit) break;
    }
    if (strikerHadBounce) {
        // FORBIDDEN: Striker hitting a wall before hitting a puck is strictly disallowed!
        return false;
    }

    // Count secondary intermediate puck collisions
    int secondaryPuckHits = 0;
    for (const auto& traj : sim.puckTrajectories) {
        if (traj.isHit && traj.puckIndex != sim.firstContactPuckId) {
            secondaryPuckHits++;
        }
    }

    // Check if first-contact puck had any cushion bounce before/during collision
    bool firstPuckHadCushion = false;
    for (const auto& pt : sim.puckTrajectories) {
        if (pt.puckIndex == sim.firstContactPuckId) {
            for (const auto& seg : pt.segments) {
                if (seg.isCushionBounce) {
                    firstPuckHadCushion = true;
                    break;
                }
            }
            break;
        }
    }

    for (const auto& traj : sim.puckTrajectories) {
        if (!traj.inPocket) continue;

        // Target Puck Color Filter (0=Auto, 1=White Only, 2=Black Only, 3=Any/Disc Pool/Points)
        int mode = s_targetColorMode.load();
        if (mode == 0) {
            mode = s_autoDetectedColor.load();
        }
        bool isPuckAllowed = false;
        if (traj.puckType == 3) {
            isPuckAllowed = true; // Queen is always a valid scoring target
        } else if (mode == 1) {
            isPuckAllowed = (traj.puckType == 1); // White only
        } else if (mode == 2) {
            isPuckAllowed = (traj.puckType == 2); // Black only
        } else if (mode == 3) {
            isPuckAllowed = true; // Any puck (Freestyle / Disc Pool)
        } else {
            // Auto mode: Allow White (1) or Black (2) based on active board targets
            isPuckAllowed = (traj.puckType == 1 || traj.puckType == 2);
        }

        if (!isPuckAllowed) {
            continue;
        }

        int puckCushBounces = 0;
        for (const auto& seg : traj.segments) {
            if (seg.isCushionBounce) puckCushBounces++;
        }

        int tier = 99;
        if (traj.puckIndex == sim.firstContactPuckId && puckCushBounces == 0 && secondaryPuckHits == 0) {
            // Tier 1: Direct Clean Pot (0 cushions, direct striker hit on target puck)
            tier = 1;
        } else if (traj.puckIndex == sim.firstContactPuckId && puckCushBounces == 1 && secondaryPuckHits == 0) {
            // Tier 2: 1-Cushion Bank Pot (Striker directly hits puck -> puck hits 1 cushion -> pocket)
            tier = 2;
        } else if (traj.puckIndex != sim.firstContactPuckId && puckCushBounces == 0 && !firstPuckHadCushion && secondaryPuckHits == 1) {
            // Tier 3: Direct Combination Pot (Striker -> Puck A -> Puck B -> Pocket, 0 cushions for all!)
            tier = 3;
        }

        // Only Tiers 1, 2, and 3 are allowed! Any other shot (striker bounce, puck bounce into puck, multi-bounce combo) is REJECTED!
        if (tier > 3) {
            continue;
        }

        // 1. Distance from striker to impact point
        double dStriker = std::hypot(sim.firstImpactStrikerX - candX, sim.firstImpactStrikerY - candY);
        // 2. Distance from impact point to pocket
        double dPuck = std::hypot(traj.finalX - sim.firstImpactPuckX, traj.finalY - sim.firstImpactPuckY);
        double totalPathLength = dStriker + dPuck;

        // 3. Cut Angle: straightness deviation from direct head-on pocket path
        double aimAngle = std::atan2(sim.firstImpactPuckY - candY, sim.firstImpactPuckX - candX);
        double potAngle = std::atan2(traj.finalY - sim.firstImpactPuckY, traj.finalX - sim.firstImpactPuckX);
        double cutAngle = std::abs(potAngle - aimAngle);
        while (cutAngle > M_PI) cutAngle = std::abs(cutAngle - 2.0 * M_PI);
        double cutDeg = cutAngle * 180.0 / M_PI;

        // Thin cut filter:
        if (tier == 1 && cutDeg > 75.0) continue;
        if (tier == 2 && cutDeg > 55.0) continue;
        if (tier == 3 && cutDeg > 45.0) continue;

        // Pocket Entry Quality
        double dToPocketCenter = std::hypot(traj.pocketCenterX - traj.finalX, traj.pocketCenterY - traj.finalY);

        // Risk engine
        double risk = 0.0;
        if (tier == 1) risk += 5.0;
        else if (tier == 2) risk += 30.0;
        else if (tier == 3) risk += 45.0;

        if (cutDeg > 12.0 && cutDeg <= 40.0) {
            risk += (cutDeg - 12.0) * 0.40;
        } else if (cutDeg > 40.0) {
            risk += 28.0 * 0.40 + (cutDeg - 40.0) * 0.90;
        }

        if (totalPathLength > 35.0) {
            risk += (totalPathLength - 35.0) * 0.35;
        }
        if (dToPocketCenter < 0.60) {
            risk -= 8.0;
        }
        risk = std::clamp(risk, 1.0, 99.0);

        // Scoring: Massive bonus for straight shots so they dominate over any other shots
        double baseScore = (tier == 1) ? 200000.0 : ((tier == 2) ? 60000.0 : 30000.0);
        double straightBonus = 0.0;
        if (tier == 1) {
            if (cutDeg < 12.0) {
                straightBonus = 500000.0; // Dead straight shot! Total score > 700,000!
            } else if (cutDeg < 25.0) {
                straightBonus = 300000.0; // Clean slight cut shot! Total score > 500,000!
            }
        }
        double queenBonus = (traj.puckType == 3) ? 15000.0 : 0.0;
        int targetColor = (mode == 2) ? 2 : 1;
        double myPuckBonus = (traj.puckType == targetColor) ? 5000.0 : 0.0;

        double centeringBonus = std::max(0.0, (pkRad - dToPocketCenter)) * 40000.0;
        double finalScore = baseScore + straightBonus + (100.0 - risk) * 250.0 + centeringBonus + queenBonus + myPuckBonus;

        outCand->candX = candX;
        outCand->candY = candY;
        outCand->angleRad = angleRad;
        double effectiveForce = forceVal;
        if (tier == 1 && totalPathLength < 45.0) {
            effectiveForce = std::clamp(0.60 + 0.40 * (totalPathLength / 45.0), 0.65, 1.0);
        }
        outCand->forceVal = effectiveForce;
        outCand->tier = tier;
        outCand->score = finalScore;
        outCand->riskScore = risk;
        outCand->targetPuckId = traj.puckIndex;
        outCand->targetPuckType = traj.puckType;
        outCand->potted = true;
        outCand->pocketId = traj.pocketIndex;
        outCand->cushionCount = puckCushBounces;
        outCand->secondaryHits = secondaryPuckHits;
        outCand->cutAngleDeg = cutDeg;
        outCand->puckStartX = traj.startX;
        outCand->puckStartY = traj.startY;
        outCand->predLandingX = traj.finalX;
        outCand->predLandingY = traj.finalY;
        return true;
    }

    return false;
}

bool GraphicsEngine_ScanAndExecuteBestShot() {
    if (!s_autoShotEnabled.load()) {
        return false;
    }
    if (!CatEngine_IsMatchAlive()) {
        return false;
    }
    if (s_isShotExecuting.load()) {
        ALOGW("[AUTO-SHOT SCAN] Execution already in progress, skipping scan.");
        return false;
    }

    int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    int64_t scanStart = s_turnScanStartTimeMs.load();
    if (scanStart == 0) {
        s_turnScanStartTimeMs.store(nowMs);
        scanStart = nowMs;
    }
    // Bank or combo fallback shots immediately allowed (ZERO artificial delay)
    bool allowFallback = true;

    // 1. Get live striker position and dynamic baseline
    double curStX = 0.0, curStY = -25.35;
    if (!CatEngine_GetLiveStrikerPos(&curStX, &curStY)) {
        std::lock_guard<std::mutex> lock(s_posMutex);
        curStX = s_strikerX;
        curStY = s_strikerY;
    }
    double baselineY = (curStY > 0.0) ? 25.35 : -25.35;
    s_strikerX = curStX;
    s_strikerY = baselineY;
    bool isTopBaseline = (baselineY > 0.0);

    float minX = s_baselineMinX.load();
    float maxX = s_baselineMaxX.load();
    if (minX >= maxX || minX < -35.0f || maxX > 35.0f) {
        minX = -23.85f;
        maxX =  23.85f;
    }

    float pkInset = s_pocketInset.load();
    float pkRad   = s_pocketRadius.load();
    float stRad   = s_strikerRadius.load();
    float pRad    = s_puckRadius.load();

    std::vector<TablePuckEntry> targets;
    int countQueen = 0, countWhite = 0, countBlack = 0;
    {
        std::lock_guard<std::mutex> lock(s_puckMutex);
        for (const auto& pe : s_puckList) {
            if (pe.active && !isPuckPottedOrInvalid(pe.x, pe.y, pkInset, pkRad)) {
                targets.push_back(pe);
                if (pe.puckType == 3) countQueen++;
                else if (pe.puckType == 2) countBlack++;
                else countWhite++;
            }
        }
    }

    if (targets.empty()) {
        CatEngine_UpdateTablePucks();
        std::lock_guard<std::mutex> lock(s_puckMutex);
        for (const auto& pe : s_puckList) {
            if (pe.active && !isPuckPottedOrInvalid(pe.x, pe.y, pkInset, pkRad)) {
                targets.push_back(pe);
                if (pe.puckType == 3) countQueen++;
                else if (pe.puckType == 2) countBlack++;
                else countWhite++;
            }
        }
    }

    if (targets.empty()) {
        ALOGW("[AUTO-SHOT FAILURE] No active target pucks detected on table.");
        s_isActivelySearching.store(false);
        return false;
    }

    s_isActivelySearching.store(true);

    int targetMode = s_targetColorMode.load(); // 0=Auto, 1=White Only, 2=Black Only, 3=Any/Disc Pool
    if (targetMode == 0) {
        targetMode = s_autoDetectedColor.load();
    }
    if (targetMode == 0) {
        targetMode = isTopBaseline ? 2 : 1;
    }
    auto isTargetAllowed = [&](int puckType) -> bool {
        if (puckType == 3) return true; // Queen always targetable
        if (targetMode == 1) return (puckType == 1); // White only
        if (targetMode == 2) return (puckType == 2); // Black only
        if (targetMode == 3) return true; // Any puck (Freestyle / Disc Pool)
        if (countWhite > 0 && countBlack == 0) return (puckType == 1);
        if (countBlack > 0 && countWhite == 0) return (puckType == 2);
        return true;
    };

    const double pocketCenters[4][2] = {
        {  pkInset,  pkInset },
        { -pkInset,  pkInset },
        {  pkInset, -pkInset },
        { -pkInset, -pkInset }
    };

    CandidateShot bestTier1;
    CandidateShot bestTier2;
    CandidateShot bestTier3;
    bool hasTier1 = false;
    bool hasTier2 = false;
    bool hasTier3 = false;
    CandidateShot bestShot;
    bool foundShot = false;

    auto registerCandidate = [&](const CandidateShot& cand) {
        if (cand.tier == 1) {
            if (!hasTier1 || cand.score > bestTier1.score) {
                bestTier1 = cand;
                hasTier1 = true;
            }
        } else if (cand.tier == 2) {
            if (!hasTier2 || cand.score > bestTier2.score) {
                bestTier2 = cand;
                hasTier2 = true;
            }
        } else if (cand.tier == 3) {
            if (!hasTier3 || cand.score > bestTier3.score) {
                bestTier3 = cand;
                hasTier3 = true;
            }
        }
    };

    auto isBaselineBlocked = [&](double bx) -> bool {
        for (const auto& tgt : targets) {
            double dx = tgt.x - bx;
            double dy = tgt.y - baselineY;
            if (std::hypot(dx, dy) < (stRad + pRad + 0.15)) {
                return true; // Overlaps a puck sitting on baseline
            }
        }
        return false;
    };

    // =========================================================================
    // STEP 1: ULTRA-FAST GHOST-BALL GEOMETRIC TARGETING (POCKET BACK-PROJECTION)
    // Computes ghost-ball impact coordinates (S_x, S_y) for every puck & pocket.
    // Checks path clearance and tests straight and low-cut candidate positions.
    // Only ~30-50 high-probability raycasts simulated!
    // =========================================================================
    for (const auto& tgt : targets) {
        if (!isTargetAllowed(tgt.puckType)) continue;

        for (int p = 0; p < 4; p++) {
            double Px = pocketCenters[p][0];
            double Py = pocketCenters[p][1];

            double dx = Px - tgt.x;
            double dy = Py - tgt.y;
            double dPocket = std::hypot(dx, dy);
            if (dPocket < 1.0) continue;

            double nx = dx / dPocket;
            double ny = dy / dPocket;

            double contactDist = stRad + pRad;
            double S_x = tgt.x - contactDist * nx;
            double S_y = tgt.y - contactDist * ny;

            double deltaY = S_y - baselineY;
            if (isTopBaseline && deltaY >= -1.5) continue;
            if (!isTopBaseline && deltaY <= 1.5) continue;

            // Check if path from target puck to pocket is blocked by any other puck
            bool pocketPathBlocked = false;
            for (const auto& other : targets) {
                if (other.id == tgt.id) continue;
                if (distSqToSegment(other.x, other.y, tgt.x, tgt.y, Px, Py) < (pRad * 1.85) * (pRad * 1.85)) {
                    pocketPathBlocked = true;
                    break;
                }
            }
            if (pocketPathBlocked) continue;

            // Collect targeted baseline X positions
            std::vector<double> candXList;
            candXList.reserve(6);

            // 1. Exact 0-degree straight collinear position
            double straightX = 0.0;
            bool hasStraight = false;
            if (std::abs(ny) > 0.05) {
                straightX = S_x - deltaY * (nx / ny);
                if (straightX >= minX && straightX <= maxX) {
                    candXList.push_back(straightX);
                    hasStraight = true;
                }
            }

            // 2. Current striker X position
            if (curStX >= minX && curStX <= maxX) {
                candXList.push_back(curStX);
            }

            // 3. Strategic small cut positions
            if (hasStraight) {
                if (straightX - 3.5 >= minX) candXList.push_back(straightX - 3.5);
                if (straightX + 3.5 <= maxX) candXList.push_back(straightX + 3.5);
            } else if (std::abs(ny) > 0.05) {
                double clampedEdge = std::clamp(straightX, (double)minX, (double)maxX);
                candXList.push_back(clampedEdge);
            }

            // Test candidate baseline X positions
            for (double testX : candXList) {
                if (isBaselineBlocked(testX)) continue;

                // Check if path from striker to ghost ball is blocked by any other puck
                bool strikerPathBlocked = false;
                for (const auto& other : targets) {
                    if (other.id == tgt.id) continue;
                    if (distSqToSegment(other.x, other.y, testX, baselineY, S_x, S_y) < (stRad + pRad - 0.25) * (stRad + pRad - 0.25)) {
                        strikerPathBlocked = true;
                        break;
                    }
                }
                if (strikerPathBlocked) continue;

                double aimAngle = std::atan2(S_y - baselineY, S_x - testX);
                s_scanLiveX.store(testX);
                s_scanLiveY.store(baselineY);
                s_scanLiveAngle.store(aimAngle);
                AimSimulationResult sim = simulateStrikerTrajectory(testX, baselineY, aimAngle, 1.0, 1200);
                CandidateShot cand;
                if (evaluateCandidateShot(sim, static_cast<float>(testX), static_cast<float>(baselineY), aimAngle, 1.0, &cand, true)) {
                    registerCandidate(cand);
                    if (s_fastFirstShotMode.load() && cand.potted) {
                        bestShot = cand;
                        foundShot = true;
                        goto execute_selected_shot;
                    }
                    if (cand.tier == 1 && cand.cutAngleDeg < 25.0) {
                        bestShot = cand;
                        foundShot = true;
                        goto execute_selected_shot;
                    }
                }
            }
        }
    }

    if (hasTier1 && bestTier1.cutAngleDeg < 25.0) {
        goto select_shot;
    }

    // =========================================================================
    // STEP 2: TARGETED DIRECT RAYS FROM CURRENT STRIKER POSITION
    // Only tests direct lines towards target pucks (NO striker cushion bounces!)
    // =========================================================================
    for (const auto& tgt : targets) {
        if (!isTargetAllowed(tgt.puckType)) continue;

        double baseAngle = std::atan2(tgt.y - baselineY, tgt.x - curStX);
        const double deltaAngles[3] = { -0.025, 0.0, 0.025 };
        for (double dAng : deltaAngles) {
            double rad = baseAngle + dAng;
            s_scanLiveX.store(curStX);
            s_scanLiveY.store(baselineY);
            s_scanLiveAngle.store(rad);
            AimSimulationResult sim = simulateStrikerTrajectory(curStX, baselineY, rad, 1.0, 1200);
            CandidateShot cand;
            if (evaluateCandidateShot(sim, static_cast<float>(curStX), static_cast<float>(baselineY), rad, 1.0, &cand, true)) {
                registerCandidate(cand);
                if (s_fastFirstShotMode.load() && cand.potted) {
                    bestShot = cand;
                    foundShot = true;
                    goto execute_selected_shot;
                }
            }
        }
    }

    if (hasTier1 && bestTier1.cutAngleDeg < 30.0) {
        goto select_shot;
    }

    // =========================================================================
    // STEP 3: BASELINE SLIDE COARSE SEARCH (Fallback for remaining pockets)
    // =========================================================================
    for (float candX = minX; candX <= maxX; candX += 4.0f) {
        if (isBaselineBlocked(candX)) continue;
        for (const auto& tgt : targets) {
            if (!isTargetAllowed(tgt.puckType)) continue;
            for (int p = 0; p < 4; p++) {
                double Px = pocketCenters[p][0];
                double Py = pocketCenters[p][1];
                double dx = Px - tgt.x;
                double dy = Py - tgt.y;
                double dPocket = std::hypot(dx, dy);
                if (dPocket < 1.0) continue;
                double nx = dx / dPocket;
                double ny = dy / dPocket;
                double contactDist = stRad + pRad;
                double S_x = tgt.x - contactDist * nx;
                double S_y = tgt.y - contactDist * ny;
                double deltaY = S_y - baselineY;
                if (isTopBaseline && deltaY >= -1.0) continue;
                if (!isTopBaseline && deltaY <= 1.0) continue;

                double ax = S_x - candX;
                double ay = S_y - baselineY;
                double dApp = std::hypot(ax, ay);
                if (dApp < 1.0) continue;
                double dotCut = (ax * nx + ay * ny) / dApp;
                if (dotCut < 0.50) continue; // Cut angle > 60 degrees, skip

                double aimAngle = std::atan2(ay, ax);
                AimSimulationResult sim = simulateStrikerTrajectory(candX, baselineY, aimAngle, 1.0, 1200);
                CandidateShot cand;
                if (evaluateCandidateShot(sim, candX, static_cast<float>(baselineY), aimAngle, 1.0, &cand, true)) {
                    registerCandidate(cand);
                    if (s_fastFirstShotMode.load() && cand.potted) {
                        bestShot = cand;
                        foundShot = true;
                        goto execute_selected_shot;
                    }
                }
            }
        }
        if (hasTier1 && bestTier1.score > 400000.0) {
            break;
        }
    }

select_shot:
    s_isActivelySearching.store(false);

    // Direct priority hierarchy with Lowest Risk scoring:
    // Tier 1 direct > Tier 2 bank > Tier 3 combo > Tier 4 Safety Turn Passer
    if (hasTier1) {
        bestShot = bestTier1;
        foundShot = true;
    } else if (hasTier2) {
        bestShot = bestTier2;
        foundShot = true;
    } else if (hasTier3) {
        bestShot = bestTier3;
        foundShot = true;
    } else {
        // Tier 4: Safety Placement / Turn Passer shot to prevent timeout foul!
        float remSec = CatEngine_GetTurnRemainingSeconds();
        if (remSec <= 6.0f || !targets.empty()) {
            CandidateShot bestSafety;
            bool hasSafety = false;
            double bestSafetyScore = -999999.0;

            for (double candX = minX + 1.5; candX <= maxX - 1.5; candX += 3.5) {
                for (const auto& tgt : targets) {
                    if (!isTargetAllowed(tgt.puckType)) continue;

                    double ax = tgt.x - candX;
                    double ay = tgt.y - baselineY;
                    double dApp = std::hypot(ax, ay);
                    if (dApp < 1.0) continue;

                    double aimAngle = std::atan2(ay, ax);
                    double safeForce = 0.50; // Controlled gentle shot to break cluster and pass turn safely

                    AimSimulationResult sim = simulateStrikerTrajectory(candX, baselineY, aimAngle, safeForce, 1500);

                    // Safety verification invariants:
                    // 1. Striker MUST NOT scratch into any pocket!
                    if (sim.strikerInPocket) continue;
                    // 2. Opponent puck MUST NOT be pocketed!
                    bool opponentPocketed = false;
                    for (const auto& traj : sim.puckTrajectories) {
                        if (traj.inPocket && !isTargetAllowed(traj.puckType)) {
                            opponentPocketed = true;
                            break;
                        }
                    }
                    if (opponentPocketed) continue;

                    double safetyScore = 50000.0 - (dApp * 200.0);
                    if (!hasSafety || safetyScore > bestSafetyScore) {
                        bestSafety.candX = candX;
                        bestSafety.candY = baselineY;
                        bestSafety.angleRad = aimAngle;
                        bestSafety.forceVal = safeForce;
                        bestSafety.tier = 4;
                        bestSafety.score = safetyScore;
                        bestSafety.targetPuckId = tgt.id;
                        bestSafety.targetPuckType = tgt.puckType;
                        bestSafety.puckStartX = tgt.x;
                        bestSafety.puckStartY = tgt.y;
                        bestSafety.predLandingX = tgt.x + 2.0;
                        bestSafety.predLandingY = tgt.y + 2.0;
                        bestSafety.potted = false;
                        bestSafety.pocketId = -1;
                        bestSafety.cushionCount = 0;
                        bestSafety.secondaryHits = 0;
                        bestSafety.cutAngleDeg = 0.0;
                        bestSafetyScore = safetyScore;
                        hasSafety = true;
                    }
                }
            }

            if (hasSafety) {
                bestShot = bestSafety;
                foundShot = true;
                ALOGI("[AUTO-SHOT] Executing Tier 4 Safety / Turn Passer (Timer=%.1fs, Force=%.2f) to avoid timeout foul!",
                      remSec, bestShot.forceVal);
            }
        }

        if (!foundShot) {
            return false;
        }
    }

execute_selected_shot:
    s_isActivelySearching.store(false);

    // Auto-shot requires either a confirmed pocketing shot (Tier 1-3) or a validated Safety Shot (Tier 4)
    if (!foundShot) {
        return false;
    }
    if (bestShot.tier <= 3) {
        if (!bestShot.potted || bestShot.pocketId < 0 || bestShot.pocketId >= 4) {
            return false;
        }
    }

    // 100% Precision Multi-Step Refinement pass: fine-tune angle directly into dead center of pocket
    if (!s_fastFirstShotMode.load() && bestShot.tier <= 3) {
        double refinedAngle = bestShot.angleRad;
        double minPocketDist = 999.0;
        double bestRefinedScore = bestShot.score;

        // 11-step micro-sweep around candidate angle in 0.003 rad steps
        for (double delta = -0.015; delta <= 0.0151; delta += 0.003) {
            double a = bestShot.angleRad + delta;
            AimSimulationResult sim = simulateStrikerTrajectory(bestShot.candX, baselineY, a, bestShot.forceVal, 600);
            CandidateShot cand;
            if (evaluateCandidateShot(sim, bestShot.candX, static_cast<float>(baselineY), a, bestShot.forceVal, &cand, true)) {
                if (cand.potted && cand.tier <= bestShot.tier) {
                    double dCenter = 999.0;
                    for (const auto& traj : sim.puckTrajectories) {
                        if (traj.inPocket) {
                            dCenter = std::hypot(traj.pocketCenterX - traj.finalX, traj.pocketCenterY - traj.finalY);
                            break;
                        }
                    }
                    if (dCenter < minPocketDist) {
                        minPocketDist = dCenter;
                        refinedAngle = a;
                        bestRefinedScore = cand.score;
                    }
                }
            }
        }
        bestShot.angleRad = refinedAngle;
    }

    s_turnScanStartTimeMs.store(0);
    s_isShotExecuting.store(true);

    double startX = curStX;
    double targetX = bestShot.candX;
    double targetAngle = bestShot.angleRad;
    double targetForce = bestShot.forceVal;

    // Apply real-time adaptive AI learning correction
    targetAngle = catengine::ai::AdaptiveLearningEngine::getInstance().getCorrectedAimAngle(
        targetAngle, bestShot.cutAngleDeg, 30.0);

    // Move striker horizontally along baseline to target X position
    if (std::abs(targetX - startX) > 0.08) {
        CatEngine_MoveStriker(targetX, baselineY);
        onStrikerPositionUpdated(targetX, baselineY);

        // Wait for game's internal physics to settle the striker at targetX (prevents firing mid-slide!)
        for (int w = 0; w < 12; w++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            double liveX = 0.0, liveY = 0.0;
            if (CatEngine_GetLiveStrikerPos(&liveX, &liveY)) {
                if (std::abs(liveX - targetX) < 0.20) {
                    break;
                }
            }
        }
    }

    // Record pre-shot prediction with Adaptive Learning Engine for continuous real-time model improvement
    catengine::ai::AdaptiveLearningEngine::getInstance().recordPreShotPrediction(
        targetX, baselineY, targetAngle, targetForce,
        bestShot.targetPuckId, bestShot.targetPuckType,
        bestShot.puckStartX, bestShot.puckStartY,
        bestShot.predLandingX, bestShot.predLandingY,
        bestShot.potted
    );

    // Set locked aim and update guideline overlay so trajectory lines and target pockets are displayed
    GraphicsEngine_UpdateAim(targetX, baselineY, targetAngle, targetForce, true);
    CatEngine_SetInEngineAim(targetAngle, targetForce);

    // Generate simulation and update persisted sim so eglSwapBuffers draws it on this frame
    AimSimulationResult sim = simulateStrikerTrajectory(targetX, baselineY, targetAngle, targetForce, 4000);

    {
        std::lock_guard<std::mutex> lock(s_persistedSimMutex);
        s_persistedSim = sim;
        s_hasPersistedSim = true;
    }

    // Give eglSwapBuffers 35ms to render the green trajectory line and glowing target pocket circle
    std::this_thread::sleep_for(std::chrono::milliseconds(35));

    // Release shot immediately in engine (zero artificial delay)
    bool shotOk = CatEngine_ExecuteInEngineShot(targetAngle, targetForce);
    s_hasCachedTier2.store(false);
    s_hasCachedTier3.store(false);
    s_isShotExecuting.store(false);

    if (!shotOk) {
        ALOGE("[AUTO-SHOT FAILURE] CatEngine_ExecuteInEngineShot failed on dispatch!");
        return false;
    }

    return true;
}

void GraphicsEngine_ClearFailureMemory() {}

void GraphicsEngine_SetShowPrediction(bool enable) {
    s_showPrediction.store(enable);
}

bool GraphicsEngine_GetShowPrediction() {
    return s_showPrediction.load();
}

void GraphicsEngine_SetShowPocket(bool enable) {
    s_showPocket.store(enable);
}

bool GraphicsEngine_GetShowPocket() {
    return s_showPocket.load();
}

void GraphicsEngine_SetShowBorder(bool enable) {
    s_showBorder.store(enable);
}

bool GraphicsEngine_GetShowBorder() {
    return s_showBorder.load();
}

void GraphicsEngine_SetLineTransparency(float alpha) {
    s_lineTransparency.store(std::clamp(alpha, 0.05f, 1.0f));
}

float GraphicsEngine_GetLineTransparency() {
    return s_lineTransparency.load();
}

void GraphicsEngine_SetCustomColors(uint32_t strikerColor, uint32_t myPuckColor, uint32_t opponentPuckColor) {
    s_customStrikerColor.store(strikerColor);
    s_customMyPuckColor.store(myPuckColor);
    s_customOpponentPuckColor.store(opponentPuckColor);
    s_hasCustomColors.store(true);
}

void GraphicsEngine_SetKeepLineAfterShot(bool keep) {
    s_keepLineAfterShot.store(keep);
    if (!keep) {
        s_waitingForHandMove.store(false);
        s_isReviewFrozen.store(false);
        std::lock_guard<std::mutex> lock(s_persistedSimMutex);
        s_hasPersistedSim = false;
    }
}

bool GraphicsEngine_GetKeepLineAfterShot() {
    return s_keepLineAfterShot.load();
}
