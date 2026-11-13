#define TAG "CatEngine"
#include <jni.h>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <map>
#include <unordered_map>
#include <set>
#include <cmath>
#include <cstdint>
#include <cfloat>
#include <signal.h>
#include <setjmp.h>
#include <atomic>
#include <link.h>
#include <cstring>
#include <sys/mman.h>
#include <vector>
#include <mutex>
#include <unistd.h>
#include "../Log.h"
#include "Dobby/dobby.h"
#include "CatEngineConstants.h"
#include "GraphicsEngine.h"
#include "v2/AdaptiveLearningEngine.h"

// Confirmed MCPoint structure: two 64-bit IEEE 754 floating-point doubles (double x, double y)
struct MCPointDouble {
    double x;
    double y;
};

struct CGPointDouble {
    double x;
    double y;
};

// =========================================================================================
// Dynamic Engine Offsets Configuration (Server-Provided with 100% Hardcoded Fallbacks)
// =========================================================================================
struct CarromOffsets {
    // Function Hook RVAs
    uintptr_t setupLocalStrikerBar = 0x1A28F98;
    uintptr_t setupOpponentStrikerBar = 0x1A1DDF4;
    uintptr_t startMatchEnding = 0x19DF2A8;
    uintptr_t moveStrikerSmooth = 0x1B2B748;
    uintptr_t forcePuckPosition = 0x1B3352C;
    uintptr_t forceStrikerPosition = 0x1B33570;
    uintptr_t onAimChanged = 0x19843A0;
    uintptr_t onShotFired = 0x1984568;

    // Callers & Singletons
    uintptr_t gameplayGeneralConfig = 0x2ED7D08;
    uintptr_t getStrikerPowerBonus = 0x1A21FF0;
    uintptr_t tableLogicGetStriker = 0x1B2AAFC;
    uintptr_t pointFromTableSpace = 0x1B313D8;
    uintptr_t launchStriker = 0x1B2C3A4;
    uintptr_t tableLogicStep = 0x1B2B88C;

    // TableController / GameplayView Offsets
    size_t tcTableLogic = 0x3F0;
    size_t tcTableVisuals = 0x450;
    size_t gvMatchController = 0x420;
    size_t gvTimerNode = 0x460;
    size_t gvTimerNodeAlt = 0x3E0;
    size_t timerTotal = 0x3F8;
    size_t timerRemaining = 0x3FC;

    // TableLogic Members
    size_t tlTableFriction = 0x130;
    size_t tlPocketRoot = 0x140;
    size_t tlBaselineMap = 0x158;
    size_t tlStrikerMap = 0x170;
    size_t tlPuckBegin = 0x180;
    size_t tlPuckMap = 0x188;
    size_t tlPolygon = 0x198;
    size_t tlCushionRestitution = 0x1B0;
    size_t tlPuckRadius = 0x1B8;
    size_t tlStrikerRadius = 0x1C0;
    size_t tlSnapshot = 0x1D0;

    // BaselineLogic Members
    size_t blY = 0x10;
    size_t blWidth = 0x18;
    size_t blVecBegin = 0x38;
    size_t blVecEnd = 0x40;
    size_t blMinX = 0x40;
    size_t blMaxX = 0x50;

    // Rigid Body Members
    size_t bodyPosX = 0x10;
    size_t bodyPosY = 0x18;
    size_t bodyRadius = 0x20;
    size_t bodyVelX = 0x50;
    size_t bodyVelY = 0x58;
    size_t bodyOmegaZ = 0x70;
    size_t bodyMass = 0x78;
    size_t bodyFriction = 0x80;
    size_t bodyRestitution = 0x90;
    size_t bodyState = 0xA4;
    size_t bodyInPocket = 0xA8;
    size_t strikerLevel = 0x174;

    // Cocos2d-x CCNode
    size_t ccNodeTransform = 0x268;

    // ControlsLogic Offsets
    size_t matchCtrlControlsLogic = 0x20;
    size_t controlsLogicAim = 0x20;
    size_t controlsLogicShot = 0x50;
    size_t delegateTarget = 0x20;
    size_t delegateInvoke = 0x30;
};

static CarromOffsets g_offsets;
static std::mutex g_offsetsMutex;
static bool g_offsetsDynamicLoaded = false;

extern "C" void CatEngine_ApplyDynamicOffsets(const char* jsonStr) {
    if (!jsonStr || strlen(jsonStr) == 0) {
        ALOGI("[CatEngine] No dynamic offsets passed, using hardcoded fallbacks.");
        return;
    }

    std::lock_guard<std::mutex> lock(g_offsetsMutex);
    std::string s(jsonStr);

    auto parseVal = [](const std::string& valStr) -> uintptr_t {
        std::string v = valStr;
        size_t first = v.find_first_not_of(" \t\r\n\"'");
        size_t last = v.find_last_not_of(" \t\r\n\"'");
        if (first == std::string::npos || last == std::string::npos) return 0;
        v = v.substr(first, (last - first + 1));
        if (v.rfind("0x", 0) == 0 || v.rfind("0X", 0) == 0) {
            return std::strtoull(v.c_str(), nullptr, 16);
        }
        return std::strtoull(v.c_str(), nullptr, 10);
    };

    int overriddenCount = 0;
    size_t pos = 0;
    while (pos < s.length()) {
        size_t quoteStart = s.find('"', pos);
        if (quoteStart == std::string::npos) break;
        size_t quoteEnd = s.find('"', quoteStart + 1);
        if (quoteEnd == std::string::npos) break;
        std::string key = s.substr(quoteStart + 1, quoteEnd - quoteStart - 1);

        size_t colon = s.find(':', quoteEnd + 1);
        if (colon == std::string::npos) break;

        size_t valStart = s.find_first_not_of(" \t\r\n", colon + 1);
        if (valStart == std::string::npos) break;

        size_t valEnd = 0;
        if (s[valStart] == '"') {
            valStart++;
            valEnd = s.find('"', valStart);
            if (valEnd == std::string::npos) break;
            pos = valEnd + 1;
        } else {
            valEnd = s.find_first_of(",}\r\n", valStart);
            if (valEnd == std::string::npos) valEnd = s.length();
            pos = valEnd;
        }

        std::string valStr = s.substr(valStart, valEnd - valStart);
        uintptr_t val = parseVal(valStr);
        if (val == 0) continue;

        if (key == "setupLocalStrikerBar") { g_offsets.setupLocalStrikerBar = val; overriddenCount++; }
        else if (key == "setupOpponentStrikerBar") { g_offsets.setupOpponentStrikerBar = val; overriddenCount++; }
        else if (key == "startMatchEnding") { g_offsets.startMatchEnding = val; overriddenCount++; }
        else if (key == "moveStrikerSmooth" || key == "moveStrikerToPositionSmooth") { g_offsets.moveStrikerSmooth = val; overriddenCount++; }
        else if (key == "forcePuckPosition") { g_offsets.forcePuckPosition = val; overriddenCount++; }
        else if (key == "forceStrikerPosition") { g_offsets.forceStrikerPosition = val; overriddenCount++; }
        else if (key == "onAimChanged") { g_offsets.onAimChanged = val; overriddenCount++; }
        else if (key == "onShotFired") { g_offsets.onShotFired = val; overriddenCount++; }
        else if (key == "gameplayGeneralConfig") { g_offsets.gameplayGeneralConfig = val; overriddenCount++; }
        else if (key == "getStrikerPowerBonus") { g_offsets.getStrikerPowerBonus = val; overriddenCount++; }
        else if (key == "tableLogicGetStriker") { g_offsets.tableLogicGetStriker = val; overriddenCount++; }
        else if (key == "pointFromTableSpace") { g_offsets.pointFromTableSpace = val; overriddenCount++; }
        else if (key == "launchStriker") { g_offsets.launchStriker = val; overriddenCount++; }
        else if (key == "tableLogicStep") { g_offsets.tableLogicStep = val; overriddenCount++; }
        else if (key == "tcTableLogic" || key == "tableControllerLogic") { g_offsets.tcTableLogic = val; overriddenCount++; }
        else if (key == "tcTableVisuals" || key == "tableVisualsContainer") { g_offsets.tcTableVisuals = val; overriddenCount++; }
        else if (key == "gvMatchController" || key == "gameplayViewMatchCtrl") { g_offsets.gvMatchController = val; overriddenCount++; }
        else if (key == "gvTimerNode" || key == "gameplayViewTimerNode") { g_offsets.gvTimerNode = val; overriddenCount++; }
        else if (key == "gvTimerNodeAlt" || key == "gameplayViewTimerNodeAlt") { g_offsets.gvTimerNodeAlt = val; overriddenCount++; }
        else if (key == "timerTotal" || key == "timerTotalDuration") { g_offsets.timerTotal = val; overriddenCount++; }
        else if (key == "timerRemaining" || key == "timerRemainingDuration") { g_offsets.timerRemaining = val; overriddenCount++; }
        else if (key == "tlTableFriction" || key == "tableLogicFriction") { g_offsets.tlTableFriction = val; overriddenCount++; }
        else if (key == "tlPocketRoot" || key == "tableLogicPocketRoot") { g_offsets.tlPocketRoot = val; overriddenCount++; }
        else if (key == "tlBaselineMap" || key == "tableLogicBaselineMap") { g_offsets.tlBaselineMap = val; overriddenCount++; }
        else if (key == "tlStrikerMap" || key == "tableLogicStrikerMap") { g_offsets.tlStrikerMap = val; overriddenCount++; }
        else if (key == "tlPuckBegin" || key == "tableLogicPuckBegin") { g_offsets.tlPuckBegin = val; overriddenCount++; }
        else if (key == "tlPuckMap" || key == "tableLogicPuckMap") { g_offsets.tlPuckMap = val; overriddenCount++; }
        else if (key == "tlPolygon" || key == "tableLogicPolygon") { g_offsets.tlPolygon = val; overriddenCount++; }
        else if (key == "tlCushionRestitution" || key == "tableLogicCushionRestitution") { g_offsets.tlCushionRestitution = val; overriddenCount++; }
        else if (key == "tlPuckRadius" || key == "tableLogicPuckRadius") { g_offsets.tlPuckRadius = val; overriddenCount++; }
        else if (key == "tlStrikerRadius" || key == "tableLogicStrikerRadius") { g_offsets.tlStrikerRadius = val; overriddenCount++; }
        else if (key == "tlSnapshot" || key == "tableLogicSnapshot") { g_offsets.tlSnapshot = val; overriddenCount++; }
        else if (key == "blY" || key == "baselineLogicY") { g_offsets.blY = val; overriddenCount++; }
        else if (key == "blWidth" || key == "baselineLogicWidth") { g_offsets.blWidth = val; overriddenCount++; }
        else if (key == "blVecBegin" || key == "baselineLogicPointsBegin") { g_offsets.blVecBegin = val; overriddenCount++; }
        else if (key == "blVecEnd" || key == "baselineLogicPointsEnd") { g_offsets.blVecEnd = val; overriddenCount++; }
        else if (key == "blMinX" || key == "baselinePointsMinX") { g_offsets.blMinX = val; overriddenCount++; }
        else if (key == "blMaxX" || key == "baselinePointsMaxX") { g_offsets.blMaxX = val; overriddenCount++; }
        else if (key == "bodyPosX") { g_offsets.bodyPosX = val; overriddenCount++; }
        else if (key == "bodyPosY") { g_offsets.bodyPosY = val; overriddenCount++; }
        else if (key == "bodyRadius") { g_offsets.bodyRadius = val; overriddenCount++; }
        else if (key == "bodyVelX") { g_offsets.bodyVelX = val; overriddenCount++; }
        else if (key == "bodyVelY") { g_offsets.bodyVelY = val; overriddenCount++; }
        else if (key == "bodyOmegaZ") { g_offsets.bodyOmegaZ = val; overriddenCount++; }
        else if (key == "bodyMass") { g_offsets.bodyMass = val; overriddenCount++; }
        else if (key == "bodyFriction") { g_offsets.bodyFriction = val; overriddenCount++; }
        else if (key == "bodyRestitution") { g_offsets.bodyRestitution = val; overriddenCount++; }
        else if (key == "bodyState") { g_offsets.bodyState = val; overriddenCount++; }
        else if (key == "bodyInPocket") { g_offsets.bodyInPocket = val; overriddenCount++; }
        else if (key == "strikerLevel") { g_offsets.strikerLevel = val; overriddenCount++; }
        else if (key == "ccNodeTransform") { g_offsets.ccNodeTransform = val; overriddenCount++; }
        else if (key == "matchCtrlControlsLogic") { g_offsets.matchCtrlControlsLogic = val; overriddenCount++; }
        else if (key == "controlsLogicAim") { g_offsets.controlsLogicAim = val; overriddenCount++; }
        else if (key == "controlsLogicShot") { g_offsets.controlsLogicShot = val; overriddenCount++; }
        else if (key == "delegateTarget") { g_offsets.delegateTarget = val; overriddenCount++; }
        else if (key == "delegateInvoke") { g_offsets.delegateInvoke = val; overriddenCount++; }
    }

    g_offsetsDynamicLoaded = true;
    ALOGI("[CatEngine] >>> Applied dynamic offsets from server successfully! Overrode %d offsets. <<<", overriddenCount);
}


// Linux kernel pipe-based safe memory reader
// Guarantees zero SIGSEGV and zero signal interception, keeping Android ART libsigchain intact!
static int s_safeReadPipe[2] = { -1, -1 };
static std::mutex s_safeReadMutex;

static inline uintptr_t untagPtr(uintptr_t addr) {
#if defined(__aarch64__)
    return addr & 0x00FFFFFFFFFFFFFFULL;
#else
    return addr;
#endif
}

static inline bool isValidUserPointer(uintptr_t addr) {
    if (addr < 0x10000) return false;
    uintptr_t untagged = untagPtr(addr);
    if (untagged < 0x10000) return false;
#if defined(__aarch64__)
    if (untagged > 0x00007FFFFFFFFFFFULL) return false;
#else
    if (untagged > 0xBFFFFFFFULL) return false;
#endif
    return true;
}

static bool safeReadBytes(uintptr_t addr, void* outBuf, size_t size) {
    if (!addr || !outBuf || size == 0) return false;
    uintptr_t untagged = untagPtr(addr);
    if (!isValidUserPointer(untagged)) return false;

    std::lock_guard<std::mutex> lock(s_safeReadMutex);
    static bool s_sigpipeIgnored = false;
    if (!s_sigpipeIgnored) {
        signal(SIGPIPE, SIG_IGN);
        s_sigpipeIgnored = true;
    }

    if (s_safeReadPipe[0] == -1) {
        if (pipe(s_safeReadPipe) != 0) {
            return false;
        }
    }

    ssize_t written = write(s_safeReadPipe[1], reinterpret_cast<const void*>(untagged), size);
    if (written == static_cast<ssize_t>(size)) {
        read(s_safeReadPipe[0], outBuf, size);
        return true;
    } else if (written > 0) {
        char drain[64];
        read(s_safeReadPipe[0], drain, written);
    } else if (written < 0 && errno == EPIPE) {
        close(s_safeReadPipe[0]);
        close(s_safeReadPipe[1]);
        s_safeReadPipe[0] = -1;
        s_safeReadPipe[1] = -1;
    }
    return false;
}

static bool safeReadInsn(uintptr_t addr, uint32_t* outVal) {
    return safeReadBytes(addr, outVal, sizeof(uint32_t));
}

static bool safeReadPtr(uintptr_t addr, uintptr_t* outVal) {
    return safeReadBytes(addr, outVal, sizeof(uintptr_t));
}

static bool safeReadDouble(uintptr_t addr, double* outVal) {
    if (!outVal) return false;
    return safeReadBytes(addr, outVal, sizeof(double));
}

static bool safeReadByte(uintptr_t addr, uint8_t* outVal) {
    if (!outVal) return false;
    return safeReadBytes(addr, outVal, sizeof(uint8_t));
}

// Forward declaration
uintptr_t resolveLibgameBase();

bool isCarromApp() {
    // 1. If libgame.so is loaded in memory in this process, it IS Carrom Pool!
    if (resolveLibgameBase() != 0) return true;

    // 2. Check cmdline for carrom or Blackbox virtual container process (:p0, :p1, etc.)
    std::ifstream cmdlineFile("/proc/self/cmdline");
    if (cmdlineFile.is_open()) {
        std::string pkg;
        std::getline(cmdlineFile, pkg);
        if (pkg.find("com.miniclip.carrom") != std::string::npos ||
            pkg.find(":p") != std::string::npos) {
            return true;
        }
    }
    return false;
}

static bool safeHook(void* target, void* replacement, void** orig) {
    if (!target || !replacement) return false;
    uintptr_t addr = reinterpret_cast<uintptr_t>(target);
    uintptr_t page_start = addr & ~static_cast<uintptr_t>(0xFFF);
    uintptr_t page_end = (addr + 64 + 0xFFF) & ~static_cast<uintptr_t>(0xFFF);
    mprotect(reinterpret_cast<void*>(page_start), page_end - page_start, PROT_READ | PROT_WRITE | PROT_EXEC);

    return DobbyHook(target, replacement, orig) == 0;
}

// =========================================================================================
// Match Lifecycle & Table Tracking State
// =========================================================================================
static std::atomic<bool> g_matchActive(false);
static std::atomic<double> g_liveStrikerX(0.0);
static std::atomic<double> g_liveStrikerY(-25.35);
static std::atomic<double> g_fixedBaselineY(-25.35);
// Helper: returns true if Y coordinate is a valid baseline position (top OR bottom)
// Accepts both -25.35 (bottom) and +25.35 (top) baselines
static inline bool isValidBaselineY(double y) {
    return !std::isnan(y) && std::abs(y) > 10.0 && std::abs(y) < 38.0;
}// Strictly locks striker to horizontal baseline
static std::atomic<double> g_lastLoggedAngle(M_PI / 2.0); // Default 90 deg (M_PI / 2.0)
static std::atomic<double> g_lastLoggedForce(0.5);
static MCPointDouble g_lastLoggedStriker = {999999.0, 999999.0};
static std::atomic<int64_t> s_lastUserSliderTouchMs(0);

// Dynamic sliding baseline X range in cm (strictly clamped within allowed bounds, defaults to +/-23.85 cm)
static std::atomic<double> g_baselineMinX(-23.85);
static std::atomic<double> g_baselineMaxX( 23.85);
static std::atomic<bool>   g_baselineDiscovered(false);
static std::atomic<bool>   g_geometryDiscovered(false);

// Authentic TableController instance pointer captured from game
static std::atomic<void*> g_activeTableController(nullptr);
static void* s_capturedMoveStrikerThis = nullptr;
static void* s_capturedMatchController = nullptr;
static void* s_capturedGameplayView = nullptr;
static std::atomic<void*> s_capturedTimerNode(nullptr);
static std::atomic<float> s_turnTimerInitial(15.0f);
static std::atomic<float> s_turnTimerTotal(15.0f);
static std::atomic<int64_t> s_turnTimerStartMs(0);
static uintptr_t s_syntheticClosure[4] = { 0, 0, 0, 0 };
static std::atomic<int64_t> s_lastShotTimeMs(0);

// Base address of libgame.so in target process
static uintptr_t g_libgameBase = 0;

static void* s_capturedControlsLogic = nullptr;
static void* s_capturedAimDelegate = nullptr;
static void* s_capturedShotDelegate = nullptr;
static uintptr_t s_knownControlsVtable = 0;
static std::atomic<bool> s_isInEngineAimActive(false);
static std::atomic<bool> s_isLocalTurnReady(false);
static std::atomic<bool> s_isOpponentTurn(false);
static std::atomic<bool> s_isLocalTurnRotated(false); // True when local player is assigned top baseline (table is rotated 180 deg)
static inline bool isOpponentBaseline(double y) {
    if (s_isLocalTurnRotated.load()) {
        return (y < 0.0);
    } else {
        return (y > 0.0);
    }
}
static std::atomic<int>  s_localBaselineId(0);
static std::atomic<int>  s_activeTurnPlayerId(0);
static std::atomic<bool> s_passAndPlayBothSides(false);
static std::atomic<int>  s_lastPlayerIndex(0);
static std::atomic<int>  s_totalMatchPlayers(2); // 2 = 1v1, 3 = 3-player, 4 = 4-player/2v2
static int s_queenPuckId = -1;

// In Carrom Pool libgame.so (onAimChanged & onShotFired), angle is already the Cartesian forward aim angle
static inline double pullAngleToLaunchAngle(double pullAngle) {
    return pullAngle;
}

static inline double launchAngleToPullAngle(double launchAngle) {
    return launchAngle;
}

// Helper to reliably identify ControlsLogicLocal instance in memory
static bool isControlsLogicLocal(uintptr_t ptr) {
    if (!isValidUserPointer(ptr)) return false;
    uintptr_t vtbl = 0;
    if (!safeReadPtr(ptr, &vtbl) || !isValidUserPointer(vtbl)) return false;
    if (s_knownControlsVtable && vtbl == s_knownControlsVtable) return true;
    if (!g_libgameBase) return false;

    // Check first 16 vtable slots for pointers in ControlsLogic code region (0x1980000 - 0x1990000)
    for (int i = 0; i < 16; i++) {
        uintptr_t fnPtr = 0;
        if (safeReadPtr(vtbl + i * sizeof(uintptr_t), &fnPtr)) {
            uintptr_t untaggedFn = untagPtr(fnPtr);
            if (untaggedFn >= (g_libgameBase + 0x1980000) && untaggedFn <= (g_libgameBase + 0x1990000)) {
                return true;
            }
        }
    }
    return false;
}

// Strict multi-layer validation of shot delegate to prevent any SIGSEGV/terminate in onShotFired (0x1984568)
static bool isValidShotDelegate(void* delegate) {
    if (!delegate) return false;
    uintptr_t dPtr = reinterpret_cast<uintptr_t>(delegate);
    if (!isValidUserPointer(dPtr)) return false;

    // Check if it is a valid object directly (e.g. ControlsLogicLocal)
    uintptr_t directVtbl = 0;
    if (safeReadPtr(dPtr, &directVtbl) && isValidUserPointer(directVtbl)) {
        if (s_knownControlsVtable && directVtbl == s_knownControlsVtable) return true;
        if (isControlsLogicLocal(dPtr)) return true;
    }

    uintptr_t fn = 0;
    if (!safeReadPtr(dPtr + g_offsets.delegateTarget, &fn) || !isValidUserPointer(fn)) return false;
    uintptr_t vtbl = 0;
    if (!safeReadPtr(fn, &vtbl) || !isValidUserPointer(vtbl)) return false;
    uintptr_t invoke = 0;
    if (!safeReadPtr(vtbl + g_offsets.delegateInvoke, &invoke) || !isValidUserPointer(invoke)) return false;
    uintptr_t untaggedInvoke = untagPtr(invoke);
    if (g_libgameBase && (untaggedInvoke < g_libgameBase || untaggedInvoke > (g_libgameBase + 0x3500000))) return false;
    return true;
}

// Strict multi-layer validation of aim delegate to prevent any crash in onAimChanged (0x19843A0)
static bool isValidAimDelegate(void* delegate) {
    if (!delegate) return false;
    uintptr_t dPtr = reinterpret_cast<uintptr_t>(delegate);
    if (!isValidUserPointer(dPtr)) return false;

    // Check if it is a valid object directly
    uintptr_t directVtbl = 0;
    if (safeReadPtr(dPtr, &directVtbl) && isValidUserPointer(directVtbl)) {
        if (s_knownControlsVtable && directVtbl == s_knownControlsVtable) return true;
        if (isControlsLogicLocal(dPtr)) return true;
    }

    uintptr_t fn = 0;
    if (!safeReadPtr(dPtr + g_offsets.delegateTarget, &fn) || !isValidUserPointer(fn)) return false;
    uintptr_t vtbl = 0;
    if (!safeReadPtr(fn, &vtbl) || !isValidUserPointer(vtbl)) return false;
    uintptr_t invoke = 0;
    if (!safeReadPtr(vtbl + g_offsets.delegateInvoke, &invoke) || !isValidUserPointer(invoke)) return false;
    uintptr_t untaggedInvoke = untagPtr(invoke);
    if (g_libgameBase && (untaggedInvoke < g_libgameBase || untaggedInvoke > (g_libgameBase + 0x3500000))) return false;
    return true;
}

// Strict live validation of TableNode instance and its underlying TableLogic
static bool isValidTableController(void* ptr) {
    if (!ptr) return false;
    uintptr_t tcAddr = reinterpret_cast<uintptr_t>(ptr);
    if (!isValidUserPointer(tcAddr)) return false;
    if ((tcAddr & 0x7) != 0) return false;

    // Check TableNode vtable in memory
    uintptr_t tcVtable = 0;
    if (!safeReadPtr(tcAddr, &tcVtable) || !isValidUserPointer(tcVtable)) return false;
    if ((tcVtable & 0x7) != 0) return false;

    // Check TableLogic pointer at +0x3f0
    uintptr_t tableLogicAddr = 0;
    if (!safeReadPtr(tcAddr + g_offsets.tcTableLogic, &tableLogicAddr) || !isValidUserPointer(tableLogicAddr)) return false;
    if ((tableLogicAddr & 0x7) != 0) return false;

    // Check TableLogic vtable
    uintptr_t tlVtable = 0;
    if (!safeReadPtr(tableLogicAddr, &tlVtable) || !isValidUserPointer(tlVtable)) return false;
    if ((tlVtable & 0x7) != 0) return false;

    return true;
}

static bool isTableControllerAlive() {
    return isValidTableController(g_activeTableController.load());
}

static void* findTableControllerFromGameplayView(void* gv) {
    if (!gv) return nullptr;
    uintptr_t gvAddr = reinterpret_cast<uintptr_t>(gv);
    if (!isValidUserPointer(gvAddr) || (gvAddr & 0x7) != 0) return nullptr;

    const size_t directOffsets[] = { 0x3f0, 0x3e8, 0x3f8, 0x400, 0x410, 0x418, 0x420, 0x428, 0x430, 0x438 };
    for (size_t off : directOffsets) {
        uintptr_t cand = 0;
        if (safeReadPtr(gvAddr + off, &cand) && isValidUserPointer(cand) && (cand & 0x7) == 0) {
            if (isValidTableController(reinterpret_cast<void*>(cand))) {
                return reinterpret_cast<void*>(cand);
            }
        }
    }
    for (size_t off = 0x380; off <= 0x480; off += sizeof(void*)) {
        uintptr_t cand = 0;
        if (safeReadPtr(gvAddr + off, &cand) && isValidUserPointer(cand) && (cand & 0x7) == 0) {
            if (isValidTableController(reinterpret_cast<void*>(cand))) {
                return reinterpret_cast<void*>(cand);
            }
        }
    }
    return nullptr;
}

static void* findTableControllerFromMatchController(void* mc) {
    if (!mc) return nullptr;
    uintptr_t mcAddr = reinterpret_cast<uintptr_t>(mc);
    if (!isValidUserPointer(mcAddr) || (mcAddr & 0x7) != 0) return nullptr;

    const size_t directOffsets[] = { 0x20, 0x28, 0x30, 0x38, 0x40, 0x48, 0x50, 0x3f0, 0x420 };
    for (size_t off : directOffsets) {
        uintptr_t cand = 0;
        if (safeReadPtr(mcAddr + off, &cand) && isValidUserPointer(cand) && (cand & 0x7) == 0) {
            if (isValidTableController(reinterpret_cast<void*>(cand))) {
                return reinterpret_cast<void*>(cand);
            }
        }
    }
    for (size_t off = 0x18; off <= 0x100; off += sizeof(void*)) {
        uintptr_t cand = 0;
        if (safeReadPtr(mcAddr + off, &cand) && isValidUserPointer(cand) && (cand & 0x7) == 0) {
            if (isValidTableController(reinterpret_cast<void*>(cand))) {
                return reinterpret_cast<void*>(cand);
            }
        }
    }
    return nullptr;
}

static void CatEngine_OnMatchDestroyed() {
    g_matchActive.store(false);
    s_isLocalTurnReady.store(false);
    s_isLocalTurnRotated.store(false);
    GraphicsEngine_SetTableRotated(false);
    setGraphicsMatchActive(false);
    GraphicsEngine_ResetNativeAffine();
    s_isInEngineAimActive.store(false);
    g_activeTableController.store(nullptr);
    s_capturedGameplayView = nullptr;
    s_capturedMoveStrikerThis = nullptr;
    s_capturedMatchController = nullptr;
    s_capturedControlsLogic = nullptr;
    s_capturedShotDelegate = nullptr;
    s_capturedAimDelegate = nullptr;
    s_localBaselineId.store(0);
    s_syntheticClosure[0] = 0;
    s_syntheticClosure[1] = 0;
    s_capturedTimerNode.store(nullptr);
    s_turnTimerStartMs.store(0);
    s_queenPuckId = -1;
    g_geometryDiscovered.store(false);
    GraphicsEngine_SetAutoDetectedColor(0);
    g_baselineDiscovered.store(false);
    GraphicsEngine_OnMatchDestroyed();
}

extern "C" bool CatEngine_IsMatchAlive() {
    if (!g_matchActive.load()) return false;
    static int s_deadStreak = 0;
    void* tc = g_activeTableController.load();
    if (!tc || !isValidTableController(tc)) {
        // Only consider match destroyed if TableController is completely missing for 300 consecutive frames (~10s)
        if (++s_deadStreak > 300) {
            CatEngine_OnMatchDestroyed();
            s_deadStreak = 0;
            return false;
        }
    } else {
        s_deadStreak = 0;
    }
    return g_matchActive.load();
}

extern "C" bool CatEngine_IsLocalTurnReady() {
    if (s_isOpponentTurn.load() && !s_passAndPlayBothSides.load()) {
        return false;
    }
    return s_isLocalTurnReady.load();
}

extern "C" void CatEngine_SetPassAndPlayBothSides(bool enable) {
    s_passAndPlayBothSides.store(enable);
    GraphicsEngine_SetPassAndPlayBothSides(enable);
}

extern "C" bool CatEngine_GetPassAndPlayBothSides() {
    return s_passAndPlayBothSides.load();
}

extern "C" int CatEngine_GetMatchPlayerCount() {
    return s_totalMatchPlayers.load();
}

extern "C" void CatEngine_UpdateTablePucks();
static inline void updateTablePucks() { CatEngine_UpdateTablePucks(); }

static int s_activePuckCount = 0;
static std::chrono::steady_clock::time_point g_lastPuckPollTime;

// Discover authentic BaselineLogic parameters (width and Y position) directly from TableLogic
static void updateBaselineFromTableLogic(void* tc) {
    if (!tc || !isValidTableController(tc) || g_baselineDiscovered.load()) return;
    uintptr_t tcAddr = reinterpret_cast<uintptr_t>(tc);
    uintptr_t tableLogicAddr = 0;
    if (!safeReadPtr(tcAddr + g_offsets.tcTableLogic, &tableLogicAddr) || !tableLogicAddr) return;

    // TableLogic + 0x158 is root of std::map<int, BaselineLogic>
    uintptr_t rootNode = 0;
    if (!safeReadPtr(tableLogicAddr + g_offsets.tlBaselineMap, &rootNode) || !rootNode) return;

    // Search tree for the bottom baseline (bY in [-35.0, -15.0]) which belongs to the local player
    struct BaselineCand {
        int key;
        double y;
        double width;
        double minX;
        double maxX;
        bool hasRange;
    };
    std::vector<BaselineCand> cands;

    std::vector<uintptr_t> stack;
    stack.push_back(rootNode);
    int visited = 0;
    while (!stack.empty() && visited++ < 16) {
        uintptr_t curr = stack.back();
        stack.pop_back();
        if (!curr) continue;

        uintptr_t left = 0, right = 0;
        safeReadPtr(curr + 0x00, &left);
        safeReadPtr(curr + 0x08, &right);
        if (left) stack.push_back(left);
        if (right) stack.push_back(right);

        uint32_t key = 0;
        if (safeReadInsn(curr + 0x20, &key)) {
            uintptr_t blAddr = curr + 0x28; // BaselineLogic
            uintptr_t yBits = 0, wBits = 0;
            double bY = -25.35;
            double bW = 40.5;
            if (safeReadPtr(blAddr + g_offsets.blY, &yBits)) {
                bY = *reinterpret_cast<double*>(&yBits);
            }
            if (safeReadPtr(blAddr + g_offsets.blWidth, &wBits)) {
                bW = *reinterpret_cast<double*>(&wBits);
            }

            // Authentic sliding limits from BaselineLogic + 0x38 (std::vector<MCPointDouble> points)
            // As verified in libgame.so:
            // 0x18b07d0 reads points[4] (minX, minY) at vecBegin + g_offsets.blMinX
            // 0x18b07ac reads points[5] (maxX, maxY) at vecBegin + g_offsets.blMaxX
            double bMinX = -23.85;
            double bMaxX =  23.85;
            bool hasRange = false;
            uintptr_t vecBegin = 0, vecEnd = 0;
            if (safeReadPtr(blAddr + g_offsets.blVecBegin, &vecBegin) && safeReadPtr(blAddr + g_offsets.blVecEnd, &vecEnd)) {
                if (vecBegin >= 0x10000 && vecEnd >= vecBegin + 0x60) {
                    uintptr_t p4Bits = 0, p5Bits = 0;
                    if (safeReadPtr(vecBegin + g_offsets.blMinX, &p4Bits) && safeReadPtr(vecBegin + g_offsets.blMaxX, &p5Bits)) {
                        double p4x = *reinterpret_cast<double*>(&p4Bits);
                        double p5x = *reinterpret_cast<double*>(&p5Bits);
                        if (!std::isnan(p4x) && !std::isnan(p5x) && std::abs(p4x) > 10.0 && std::abs(p4x) < 35.0 &&
                            std::abs(p5x) > 10.0 && std::abs(p5x) < 35.0) {
                            bMinX = std::min(p4x, p5x);
                            bMaxX = std::max(p4x, p5x);
                            hasRange = true;
                        }
                    }
                }
            }
            if (!hasRange && bW > 10.0 && bW < 70.0) {
                // If points vector not yet initialized, authentic Carrom Pool sliding baseline is
                // outer circle boundary: halfW + circleRadius (40.5 * 0.5 + 3.60 = 23.85)
                double halfW = bW * 0.5;
                bMinX = -(halfW + 3.60);
                bMaxX =  (halfW + 3.60);
            }

            if (!std::isnan(bY) && std::abs(bY) > 15.0 && std::abs(bY) < 35.0) {
                cands.push_back({ static_cast<int>(key), bY, bW, bMinX, bMaxX, hasRange });
            }
        }
    }

    if (cands.empty()) return;
    s_totalMatchPlayers.store(static_cast<int>(cands.size()));

    // Select the local player's baseline:
    // If targetKey was learned from setupLocalStrikerBar / my_tl_moveStrikerSmooth, match that candidate.
    // If targetKey is a top baseline (y > 0.0), the game has rotated the camera 180 degrees so that
    // top appears at the bottom of the device screen. In that case, dragging left on device screen
    // decreases screen coordinate, which maps to -X in table space if inverted.
    const BaselineCand* selected = nullptr;
    int targetKey = s_localBaselineId.load();
    if (targetKey >= 0) {
        for (const auto& c : cands) {
            if (c.key == targetKey) {
                selected = &c;
                break;
            }
        }
    }
    if (!selected) {
        for (const auto& c : cands) {
            if (c.y < -15.0 && c.y > -35.0) {
                selected = &c;
                break;
            }
        }
    }
    if (!selected && !cands.empty()) selected = &cands[0];

    bool isRotated = (selected->y > 0.0);
    s_isLocalTurnRotated.store(isRotated);
    GraphicsEngine_SetTableRotated(isRotated);
    s_localBaselineId.store(selected->key);
    s_lastPlayerIndex.store(selected->key);
    g_baselineMinX.store(selected->minX);
    g_baselineMaxX.store(selected->maxX);
    double lockedY = selected->y;
    g_fixedBaselineY.store(lockedY);
    g_liveStrikerY.store(lockedY);
    GraphicsEngine_SetBaselineRange(static_cast<float>(selected->minX), static_cast<float>(selected->maxX));
    g_baselineDiscovered.store(true);
    ALOGI("[CatEngine] AUTHENTIC LOCAL BASELINE DISCOVERED: Key=%d, Y=%.3f cm (top=%s, rotated=%s), Sliding Range: [%.3f, %.3f] cm (fromPoints=%d)",
          selected->key, selected->y, selected->y > 0.0 ? "YES" : "NO", isRotated ? "TRUE" : "FALSE", selected->minX, selected->maxX, selected->hasRange ? 1 : 0);
}

// Discover authentic Pocket, Striker, and Puck geometry directly from TableLogic
static void updateGeometryFromTableLogic(void* tc) {
    if (GraphicsEngine_IsUserManualPhysics()) return;
    if (!tc) return;
    uintptr_t tcAddr = reinterpret_cast<uintptr_t>(tc);
    uintptr_t tableLogicAddr = 0;
    if (!safeReadPtr(tcAddr + g_offsets.tcTableLogic, &tableLogicAddr) || !tableLogicAddr) return;

    // 0. Authoritative geometry directly from TableLogic (libgame.so 0x1b29e44 - 0x1b29e58)
    // [TableLogic + 0x1b8] is Puck Radius (from TableConfig + 0x28)
    // [TableLogic + 0x1c0] is Striker Radius (from TableConfig + 0x20)
    double liveTLPuckRad = 0.0;
    if (safeReadDouble(tableLogicAddr + g_offsets.tlPuckRadius, &liveTLPuckRad) && !std::isnan(liveTLPuckRad) && liveTLPuckRad > 0.5 && liveTLPuckRad < 4.0) {
        GraphicsEngine_SetPuckRadius(static_cast<float>(liveTLPuckRad));
        ALOGI("[CatEngine] AUTHORITATIVE PUCK RADIUS from TableLogic+0x1b8: %.4f cm", liveTLPuckRad);
    }
    double liveTLStrikerRad = 0.0;
    if (safeReadDouble(tableLogicAddr + g_offsets.tlStrikerRadius, &liveTLStrikerRad) && !std::isnan(liveTLStrikerRad) && liveTLStrikerRad > 1.0 && liveTLStrikerRad < 5.0) {
        GraphicsEngine_SetStrikerRadius(static_cast<float>(liveTLStrikerRad));
        ALOGI("[CatEngine] AUTHORITATIVE STRIKER RADIUS from TableLogic+0x1c0: %.4f cm", liveTLStrikerRad);
    }

    if (g_geometryDiscovered.load()) return;

    // 1. Inspect PocketLogic map at TableLogic + 0x138 (root at +0x140)
    uintptr_t pocketRoot = 0;
    if (safeReadPtr(tableLogicAddr + g_offsets.tlPocketRoot, &pocketRoot) && pocketRoot) {
        uintptr_t curr = pocketRoot;
        for (int d = 0; d < 6 && curr != 0; d++) {
            uintptr_t xBits = 0, yBits = 0, rBits = 0;
            if (safeReadPtr(curr + 0x38, &xBits) && safeReadPtr(curr + 0x40, &yBits) && safeReadPtr(curr + 0x48, &rBits)) {
                double px = *reinterpret_cast<double*>(&xBits);
                double py = *reinterpret_cast<double*>(&yBits);
                double pr = *reinterpret_cast<double*>(&rBits);
                if (!std::isnan(px) && !std::isnan(py) && !std::isnan(pr) &&
                    std::abs(px) > 25.0 && std::abs(px) < 37.0 && pr > 1.0 && pr < 8.0) {
                    float inset = static_cast<float>((std::abs(px) + std::abs(py)) * 0.5);
                    // Calibrate authentic pocket drop capture radius from physical TableLogic geometry
                    float rad = static_cast<float>(pr);
                    if (rad < 2.30f || rad > 3.60f) {
                        rad = 2.65f; // Authentic Carrom Pool visual and physical pocket aperture radius
                    }
                    GraphicsEngine_SetPocketGeometry(inset, rad);
                    ALOGI("[CatEngine] DYNAMIC POCKET DISCOVERED! Inset=%.3f cm, Radius=%.3f cm (raw pr=%.3f, px=%.2f, py=%.2f)",
                          inset, rad, pr, px, py);
                    break;
                }
            }
            safeReadPtr(curr + 0x00, &curr);
        }
    }

    // 2. Inspect StrikerLogic map at TableLogic + 0x170
    uintptr_t strikerRoot = 0;
    if (safeReadPtr(tableLogicAddr + g_offsets.tlStrikerMap, &strikerRoot) && strikerRoot) {
        uintptr_t curr = strikerRoot;
        for (int d = 0; d < 8 && curr != 0; d++) {
            uintptr_t slAddr = curr + 0x30;
            uintptr_t rBits = 0;
            if (safeReadPtr(slAddr + g_offsets.bodyRadius, &rBits)) {
                double sr = *reinterpret_cast<double*>(&rBits);
                if (!std::isnan(sr) && sr > 1.5 && sr < 5.0) {
                    GraphicsEngine_SetStrikerRadius(static_cast<float>(sr));
                    ALOGI("[CatEngine] DYNAMIC STRIKER RADIUS DISCOVERED! Radius=%.3f cm", sr);
                    break;
                }
            }
            safeReadPtr(curr + 0x00, &curr);
        }
    }

    // 3. Inspect PuckLogic map at TableLogic + 0x188
    uintptr_t puckRoot = 0;
    if (safeReadPtr(tableLogicAddr + g_offsets.tlPuckMap, &puckRoot) && puckRoot) {
        uintptr_t curr = puckRoot;
        for (int d = 0; d < 8 && curr != 0; d++) {
            uintptr_t pAddr = curr + 0x30;
            uintptr_t rBits = 0;
            if (safeReadPtr(pAddr + g_offsets.bodyRadius, &rBits)) {
                double pr = *reinterpret_cast<double*>(&rBits);
                if (!std::isnan(pr) && pr > 1.0 && pr < 4.0) {
                    GraphicsEngine_SetPuckRadius(static_cast<float>(pr));
                    ALOGI("[CatEngine] DYNAMIC PUCK RADIUS DISCOVERED! Radius=%.3f cm", pr);
                    break;
                }
            }
            safeReadPtr(curr + 0x00, &curr);
        }
    }

    // Authentic Carrom Pool cushion octagon is built into PhysicsEngineV2 (h=37.0cm, c=32.8cm)
    g_geometryDiscovered.store(true);
}

// Dynamically discover authentic Striker power level, minPower, maxPower, bonus, masses, and dampings from game memory
static void updateStrikerPhysicsFromGame(void* tc) {
    if (GraphicsEngine_IsUserManualPhysics()) return;
    if (!tc) tc = g_activeTableController.load();
    if (!tc || !g_libgameBase) return;
    uintptr_t tcAddr = reinterpret_cast<uintptr_t>(tc);
    uintptr_t tableLogicAddr = 0;
    if (!safeReadPtr(tcAddr + g_offsets.tcTableLogic, &tableLogicAddr) || !tableLogicAddr) return;

    // TableLogic + 0x130: table surface friction
    double liveTableFriction = 1.0;
    double tf = 0.0;
    if (safeReadDouble(tableLogicAddr + g_offsets.tlTableFriction, &tf) && !std::isnan(tf) && tf > 0.01 && tf < 5.0) {
        liveTableFriction = tf;
    }

    // TableLogic + 0x1b0: cushion restitution
    double liveCushionRestitution = 0.88;
    double cr = 0.0;
    if (safeReadDouble(tableLogicAddr + g_offsets.tlCushionRestitution, &cr) && !std::isnan(cr) && cr > 0.3 && cr <= 1.0) {
        liveCushionRestitution = cr;
    }

    // 1. Read GameplayGeneralConfig singleton at *(g_libgameBase + 0x2ED7D08)
    double liveMinPower = 12.20;
    double liveMaxPower = 172.20;
    double liveSpeedFactor = 1.0;
    uintptr_t configPtr = 0;
    if (safeReadPtr(g_libgameBase + g_offsets.gameplayGeneralConfig, &configPtr) && configPtr) {
        double vMin = 0.0, vMax = 0.0, vSf = 0.0;
        if (safeReadDouble(configPtr + 0x30, &vMin) && !std::isnan(vMin) && vMin >= 0.0 && vMin < 200.0) {
            liveMinPower = vMin;
        }
        if (safeReadDouble(configPtr + 0x28, &vMax) && !std::isnan(vMax) && vMax > 10.0 && vMax < 500.0) {
            liveMaxPower = vMax;
        }
        if (safeReadDouble(configPtr + 0x18, &vSf) && !std::isnan(vSf) && vSf > 0.1 && vSf < 10.0) {
            liveSpeedFactor = vSf;
        }
    }

    // Read authoritative radii directly from TableLogic if present
    double tlPkR = 0.0, tlStR = 0.0;
    if (safeReadDouble(tableLogicAddr + g_offsets.tlPuckRadius, &tlPkR) && !std::isnan(tlPkR) && tlPkR > 0.5 && tlPkR < 4.0) {
        GraphicsEngine_SetPuckRadius(static_cast<float>(tlPkR));
    }
    if (safeReadDouble(tableLogicAddr + g_offsets.tlStrikerRadius, &tlStR) && !std::isnan(tlStR) && tlStR > 1.0 && tlStR < 5.0) {
        GraphicsEngine_SetStrikerRadius(static_cast<float>(tlStR));
    }

    // 2. Read Striker properties from TableLogic::getStriker (libgame.so + 0x1B2AAFC) or RB-tree map at +0x170
    int strikerLevel = 1;
    double liveStrikerRadius = (tlStR > 1.0) ? tlStR : 2.65;
    double liveStrikerMass = 3.00; // Authentic 3.00:1 striker-to-puck mass ratio
    double rawStrikerFriction = 0.1600; // Calibrated for authentic Carrom Pool striker travel distance
    double liveStrikerRestitution = 1.00;

    int targetPlayer = s_localBaselineId.load();
    if (targetPlayer < 0) targetPlayer = 0;

    typedef void* (*TableLogicGetStrikerFn)(void* tableLogic, int playerId);
    auto getStrikerFn = reinterpret_cast<TableLogicGetStrikerFn>(g_libgameBase + g_offsets.tableLogicGetStriker);
    void* slPtr = nullptr;
    if (getStrikerFn && tableLogicAddr) {
        slPtr = getStrikerFn(reinterpret_cast<void*>(tableLogicAddr), targetPlayer);
        if (!slPtr && targetPlayer != 0) slPtr = getStrikerFn(reinterpret_cast<void*>(tableLogicAddr), 0);
    }

    if (slPtr) {
        uintptr_t slAddr = reinterpret_cast<uintptr_t>(slPtr);
        uint32_t lvl = 0;
        if (safeReadInsn(slAddr + g_offsets.strikerLevel, &lvl) && lvl >= 1 && lvl <= 50) {
            strikerLevel = static_cast<int>(lvl);
        }
        double rVal = 0.0, mVal = 0.0, fVal = 0.0, restVal = 0.0;
        if (safeReadDouble(slAddr + g_offsets.bodyRadius, &rVal) && !std::isnan(rVal) && rVal > 1.0 && rVal < 5.0) {
            liveStrikerRadius = rVal;
        }
        if (safeReadDouble(slAddr + g_offsets.bodyMass, &mVal) && !std::isnan(mVal) && mVal > 0.5 && mVal <= 5.0) {
            liveStrikerMass = mVal;
        }
        if (safeReadDouble(slAddr + g_offsets.bodyFriction, &fVal) && !std::isnan(fVal) && fVal > 0.01 && fVal < 2.0) {
            rawStrikerFriction = fVal;
        }
        if (safeReadDouble(slAddr + g_offsets.bodyRestitution, &restVal) && !std::isnan(restVal) && restVal > 0.1 && restVal <= 1.0) {
            liveStrikerRestitution = restVal;
        }
    } else {
        uintptr_t strikerRoot = 0;
        if (safeReadPtr(tableLogicAddr + g_offsets.tlStrikerMap, &strikerRoot) && strikerRoot) {
            uintptr_t curr = strikerRoot;
            for (int d = 0; d < 8 && curr != 0; d++) {
                uint32_t key = 0;
                if (safeReadInsn(curr + 0x20, &key)) {
                    if ((int)key == targetPlayer || (targetPlayer != 0 && key == 0)) {
                        uintptr_t slAddr = curr + 0x30;
                        uint32_t lvl = 0;
                        if (safeReadInsn(slAddr + g_offsets.strikerLevel, &lvl) && lvl >= 1 && lvl <= 50) {
                            strikerLevel = static_cast<int>(lvl);
                        }
                        double rVal = 0.0, mVal = 0.0, fVal = 0.0, restVal = 0.0;
                        if (safeReadDouble(slAddr + g_offsets.bodyRadius, &rVal) && !std::isnan(rVal) && rVal > 1.0 && rVal < 5.0) {
                            liveStrikerRadius = rVal;
                        }
                        if (safeReadDouble(slAddr + g_offsets.bodyMass, &mVal) && !std::isnan(mVal) && mVal > 0.5 && mVal <= 5.0) {
                            liveStrikerMass = mVal;
                        }
                        if (safeReadDouble(slAddr + g_offsets.bodyFriction, &fVal) && !std::isnan(fVal) && fVal > 0.01 && fVal < 2.0) {
                            rawStrikerFriction = fVal;
                        }
                        if (safeReadDouble(slAddr + g_offsets.bodyRestitution, &restVal) && !std::isnan(restVal) && restVal > 0.1 && restVal <= 1.0) {
                            liveStrikerRestitution = restVal;
                        }
                        break;
                    } else if (key > 0) {
                        safeReadPtr(curr + 0x00, &curr);
                    } else {
                        safeReadPtr(curr + 0x08, &curr);
                    }
                } else {
                    break;
                }
            }
        }
    }

    // 3. Read Puck properties from PuckLogic map at TableLogic + 0x188
    double livePuckRadius = 1.95;
    double livePuckMass = 1.00;
    double rawPuckFriction = 0.1080;
    double livePuckRestitution = 1.00;
    uintptr_t puckRoot = 0;
    if (safeReadPtr(tableLogicAddr + g_offsets.tlPuckMap, &puckRoot) && puckRoot) {
        uintptr_t curr = puckRoot;
        for (int d = 0; d < 10 && curr != 0; d++) {
            uintptr_t pAddr = curr + 0x30;
            double rVal = 0.0, mVal = 0.0, fVal = 0.0, restVal = 0.0;
            if (safeReadDouble(pAddr + g_offsets.bodyRadius, &rVal) && !std::isnan(rVal) && rVal > 0.5 && rVal < 4.0) {
                livePuckRadius = rVal;
            }
            if (safeReadDouble(pAddr + g_offsets.bodyMass, &mVal) && !std::isnan(mVal) && mVal > 0.2 && mVal < 5.0) {
                livePuckMass = mVal;
            }
            if (safeReadDouble(pAddr + g_offsets.bodyFriction, &fVal) && !std::isnan(fVal) && fVal > 0.01 && fVal < 2.0) {
                rawPuckFriction = fVal;
            }
            if (safeReadDouble(pAddr + g_offsets.bodyRestitution, &restVal) && !std::isnan(restVal) && restVal > 0.1 && restVal <= 1.0) {
                livePuckRestitution = restVal;
            }
            if (livePuckMass > 0.2 && livePuckRadius > 0.5) break;
            safeReadPtr(curr + 0x00, &curr);
        }
    }

    // 4. Read Striker Bonus dynamically from GameplayGeneralConfig
    // libgame.so + 0x1A21FF0 is GameplayGeneralConfig::getStrikerPowerBonus(config, level)
    double strikerBonus = 3.20;
    if (configPtr && strikerLevel >= 1) {
        typedef double (*GetStrikerPowerBonusFn)(void* config, int level);
        auto getBonusFn = reinterpret_cast<GetStrikerPowerBonusFn>(g_libgameBase + g_offsets.getStrikerPowerBonus);
        if (getBonusFn) {
            strikerBonus = getBonusFn(reinterpret_cast<void*>(configPtr), strikerLevel);
            if (std::isnan(strikerBonus) || strikerBonus < 0.0 || strikerBonus > 200.0) {
                strikerBonus = 3.20;
            }
        }
    }

    // Apply discovered geometry
    GraphicsEngine_SetStrikerRadius(static_cast<float>(liveStrikerRadius));
    GraphicsEngine_SetPuckRadius(static_cast<float>(livePuckRadius));

    // Apply discovered physics to graphics & simulation engines
    GraphicsEngine_SetStrikerPhysics(liveMinPower, liveMaxPower, strikerBonus);

    // Authentic effective physical parameters (libgame.so + live telemetry):
    // Striker: mu_s = 0.0410 (yields authentic 2.65 - 2.70 board lengths travel distance)
    // Puck:    mu_p = 0.1080 (deceleration = 105.8 cm/s^2), mass = 1.00
    // Body restitution = 1.00 (100% pure elastic 1D momentum conservation)
    // Cushion restitution = 0.88 (authentic Carrom Pool cushion restitution)
    double effectiveStrikerFriction = rawStrikerFriction;
    if (liveTableFriction > 0.01 && liveTableFriction <= 1.0 && rawStrikerFriction > 0.05) {
        effectiveStrikerFriction = rawStrikerFriction * liveTableFriction;
    }
    if (effectiveStrikerFriction < 0.100 || effectiveStrikerFriction > 0.250) {
        effectiveStrikerFriction = 0.1600; // Calibrated authentic striker travel distance (156.8 cm/s^2)
    }
    double effectivePuckFriction = rawPuckFriction;
    if (liveStrikerMass <= 0.1 || liveStrikerMass > 5.0) liveStrikerMass = 3.00;
    if (livePuckMass <= 0.1) livePuckMass = 1.00;
    if (liveStrikerRestitution <= 0.1) liveStrikerRestitution = 1.00;
    if (liveCushionRestitution < 0.3 || liveCushionRestitution > 1.0) {
        liveCushionRestitution = 0.88;
    }

    ALOGI("[CatEngine] [PHYSICS READ] slPtr=%p mass=%.4f friction=%.4f tableFriction=%.4f puckMass=%.4f puckFriction=%.4f",
          slPtr, liveStrikerMass, effectiveStrikerFriction, liveTableFriction, livePuckMass, effectivePuckFriction);

    GraphicsEngine_SetPhysicalConstants(liveTableFriction,
                                         effectiveStrikerFriction, liveStrikerMass, liveStrikerRestitution,
                                         effectivePuckFriction, livePuckMass, livePuckRestitution,
                                         liveCushionRestitution);
}

// =========================================================================================
// Native Table Affine Transform Extraction (Parity with Snake Engine SE)
// In libgame.so:
// TableNode has pointFromTableSpace:(CGPoint) at libgameBase + 0x1B313D8.
// It directly converts table cm coordinates to OpenGL viewport pixels using
// [self->_tableVisualsContainer convertToWorldSpace:pt * 6.87].
// This provides the authentic Cocos2d-to-OpenGL camera projection without heuristic offsets.
// =========================================================================================
struct MCPointD {
    double x;
    double y;
};
typedef MCPointD (*PointFromTableSpaceFn)(void* self, void* sel, double x, double y);

static std::atomic<int> s_viewportWidth(0);
static std::atomic<int> s_viewportHeight(0);

void CatEngine_SetViewportDimensions(int w, int h) {
    if (w > 0 && h > 0) {
        s_viewportWidth.store(w);
        s_viewportHeight.store(h);
    }
}

bool CatEngine_GetTableTransform(double* outAffine6) {
    if (!outAffine6 || !g_libgameBase) return false;
    void* tc = g_activeTableController.load();
    if (!tc || !isValidTableController(tc)) return false;

    uintptr_t tcAddr = reinterpret_cast<uintptr_t>(tc);
    uintptr_t tableVisuals = 0;
    if (!safeReadPtr(tcAddr + g_offsets.tcTableVisuals, &tableVisuals) || !tableVisuals) {
        return false;
    }

    uintptr_t pftsAddr = g_libgameBase + g_offsets.pointFromTableSpace;
    uint32_t insn = 0;
    if (!safeReadInsn(pftsAddr, &insn) || insn == 0) {
        return false;
    }

    auto pfts = reinterpret_cast<PointFromTableSpaceFn>(pftsAddr);
    MCPointD p0 = pfts(tc, nullptr, 0.0, 0.0);
    MCPointD px = pfts(tc, nullptr, 1.0, 0.0);
    MCPointD py = pfts(tc, nullptr, 0.0, 1.0);

    double a = px.x - p0.x;
    double b = px.y - p0.y;
    double c = py.x - p0.x;
    double d = py.y - p0.y;
    double tx = p0.x;
    double ty = p0.y;

    if (std::isnan(a) || std::isnan(b) || std::isnan(c) || std::isnan(d) ||
        std::isnan(tx) || std::isnan(ty) || std::hypot(a, b) < 1.0) {
        return false;
    }

    outAffine6[0] = a;
    outAffine6[1] = b;
    outAffine6[2] = c;
    outAffine6[3] = d;
    outAffine6[4] = tx;
    outAffine6[5] = ty;
    return true;
}

// =========================================================================================
// Automatic Board Calibration from TableController
// =========================================================================================
static void calibrateBoardFromTableController(void* tc) {
    if (!tc) return;
    uintptr_t tcAddr = reinterpret_cast<uintptr_t>(tc);
    uintptr_t tableLogicAddr = 0;
    if (!safeReadPtr(tcAddr + g_offsets.tcTableLogic, &tableLogicAddr) || !tableLogicAddr) return;

    double tf[6];
    if (CatEngine_GetTableTransform(tf)) {
        GraphicsEngine_SetNativeAffineTransform(tf[0], tf[1], tf[2], tf[3], tf[4], tf[5]);
        bool isRotated = (tf[0] < -0.1) || s_isLocalTurnRotated.load();
        s_isLocalTurnRotated.store(isRotated);
        GraphicsEngine_SetTableRotated(isRotated);
        double sc = std::hypot(tf[0], tf[1]);
        GraphicsEngine_SetAutoCalibration(static_cast<float>(tf[4]), static_cast<float>(tf[5]), static_cast<float>(sc), true);
        ALOGI("[CatEngine] [CameraParity] Synchronized live native table transform: scale=%.2f, center=(%.1f, %.1f), rotated=%s", sc, tf[4], tf[5], isRotated ? "TRUE" : "FALSE");
    }

    updateBaselineFromTableLogic(tc);
    updateGeometryFromTableLogic(tc);
    updateStrikerPhysicsFromGame(tc);
    CatEngine_UpdateTablePucks();
}

// =========================================================================================
// Accurate Puck Inspection from libc++ std::map<int, PuckLogic>
// In libgame.so:
// TableLogic + 0x180 is __tree_.__begin_node_
// TableLogic + 0x188 is __tree_.__pair1_ (endNode dummy header and root node pointer)
// TableLogic + 0x190 is __tree_.__pair3_ (size_t puckCount)
// Node layout: +0x00 = left, +0x08 = right, +0x10 = parent, +0x20 = int puckId, +0x30 = PuckLogic
// Inside PuckLogic:
//   +0x10: double x
//   +0x18: double y
//   +0x20: double radius
//   +0x38: void* pocket
//   +0xa4: int state (1 = active on table, 2 = pocketed)
// =========================================================================================
static inline bool isInsidePocketPos(double x, double y) {
    if (std::isnan(x) || std::isnan(y)) return true;
    double ax = std::abs(x);
    double ay = std::abs(y);
    if (ax > 36.5 || ay > 36.5) return true; // Off board
    if (ax > 29.0 && ay > 29.0) {
        double dx = ax - 33.48;
        double dy = ay - 33.48;
        // Pucks resting at the absolute corner near the pocket have hypot ~2.5 to 4.5 cm.
        // A puck is only dropped into the pocket hole when its center is deep inside (< 1.6 cm).
        if (std::hypot(dx, dy) < 1.6) return true;
    }
    return false;
}

// Fast binary search lookup in TableLogic red-black tree (matching libgame.so TableLogic::findPuck at 0x1b2abe4)
static uintptr_t findPuckInTableLogic(uintptr_t tableLogicAddr, int targetId) {
    if (!tableLogicAddr) return 0;
    uintptr_t endNode = tableLogicAddr + g_offsets.tlPuckMap;
    uintptr_t root = 0;
    if (!safeReadPtr(tableLogicAddr + g_offsets.tlPuckMap, &root) || !root || root == endNode) return 0;

    uintptr_t curr = root;
    for (int d = 0; d < 24 && curr && curr != endNode; d++) {
        uint32_t key = 0;
        if (!safeReadInsn(curr + 0x20, &key)) break;
        if (static_cast<int>(key) == targetId) {
            return curr + 0x30;
        }
        uintptr_t nextNode = 0;
        if (targetId < static_cast<int>(key)) {
            if (!safeReadPtr(curr + 0x00, &nextNode)) break;
        } else {
            if (!safeReadPtr(curr + 0x08, &nextNode)) break;
        }
        curr = nextNode;
    }
    return 0;
}

// Authoritative Puck Type Classifier
// 1 = White (Player), 2 = Black (Opponent), 3 = Queen (Red)
static int determinePuckType(int puckId, uintptr_t pAddr, double liveX, double liveY) {
    // 1. Queen ID already detected and locked
    if (s_queenPuckId >= 0 && puckId == s_queenPuckId) {
        return 3; // Queen (Red)
    }

    // 2. Strict center spawn detection for Queen at rack setup (Queen is precisely at (0, 0))
    // All other 18 pucks have radius ~1.6 cm, so inner ring is >= 3.1 cm away from center.
    if (s_queenPuckId < 0 && !std::isnan(liveX) && !std::isnan(liveY) && std::hypot(liveX, liveY) < 0.35) {
        s_queenPuckId = puckId;
        return 3; // Queen (Red)
    }

    // 3. Baseline & Puck ID mapping:
    // Local player baseline: bottom (0) vs top (1)
    int localB = s_localBaselineId.load();
    if (localB == 1) {
        // Local player is at top baseline: White=10..18, Black=1..9
        if (puckId >= 10 && puckId <= 18) return 1; // White (Player)
        if (puckId >= 1 && puckId <= 9) return 2;   // Black (Opponent)
    } else {
        // Local player is at bottom baseline: White=1..9, Black=10..18
        if (puckId >= 1 && puckId <= 9) return 1;   // White (Player)
        if (puckId >= 10 && puckId <= 18) return 2; // Black (Opponent)
    }

    return 1; // Default fallback to White
}

static void traversePuckTreeDfs(uintptr_t nodeAddr, uintptr_t endNode, int depth,
                                std::set<int>& visitedIds, std::vector<GraphicsPuckInfo>& outPucks) {
    if (!nodeAddr || nodeAddr == endNode || depth > 24) return;

    uintptr_t left = 0, right = 0;
    safeReadPtr(nodeAddr + 0x00, &left);
    safeReadPtr(nodeAddr + 0x08, &right);

    uint32_t rawKey = 0;
    if (safeReadInsn(nodeAddr + 0x20, &rawKey)) {
        int puckId = static_cast<int>(rawKey);
        uintptr_t pAddr = nodeAddr + 0x30;
        uintptr_t xBits = 0, yBits = 0;
        if (safeReadPtr(pAddr + 0x10, &xBits) && safeReadPtr(pAddr + 0x18, &yBits)) {
            double liveX = *reinterpret_cast<double*>(&xBits);
            double liveY = *reinterpret_cast<double*>(&yBits);

            bool isPotted = std::isnan(liveX) || std::isnan(liveY) ||
                            std::abs(liveX) > 36.5 || std::abs(liveY) > 36.5 ||
                            isInsidePocketPos(liveX, liveY);

            if (!isPotted && visitedIds.find(puckId) == visitedIds.end()) {
                visitedIds.insert(puckId);

                int pType = determinePuckType(puckId, pAddr, liveX, liveY);

                GraphicsPuckInfo pi;
                pi.id = puckId;
                pi.x = liveX;
                pi.y = liveY;
                pi.active = true;
                pi.puckType = pType;
                outPucks.push_back(pi);
            }
        }
    }

    if (left && left != endNode)   traversePuckTreeDfs(left, endNode, depth + 1, visitedIds, outPucks);
    if (right && right != endNode) traversePuckTreeDfs(right, endNode, depth + 1, visitedIds, outPucks);
}

static void readAllPucksFromTableLogic(uintptr_t tableLogicAddr, std::vector<GraphicsPuckInfo>& outPucks) {
    if (!tableLogicAddr) return;

    uintptr_t endNode = tableLogicAddr + g_offsets.tlPuckMap;
    uintptr_t beginNode = 0;
    safeReadPtr(tableLogicAddr + g_offsets.tlPuckBegin, &beginNode);

    std::set<int> visitedIds;

    // Method 1: Canonical libc++ in-order iterator traversal (matching libgame.so 0x1B2B3E0)
    uintptr_t curr = beginNode;
    int stepCount = 0;
    while (curr != 0 && curr != endNode && stepCount < 50) {
        stepCount++;
        uint32_t rawKey = 0;
        if (safeReadInsn(curr + 0x20, &rawKey)) {
            int puckId = static_cast<int>(rawKey);
            uintptr_t pAddr = curr + 0x30;
            uintptr_t xBits = 0, yBits = 0;
            if (safeReadPtr(pAddr + 0x10, &xBits) && safeReadPtr(pAddr + 0x18, &yBits)) {
                double liveX = *reinterpret_cast<double*>(&xBits);
                double liveY = *reinterpret_cast<double*>(&yBits);

                bool isPotted = std::isnan(liveX) || std::isnan(liveY) ||
                                std::abs(liveX) > 36.5 || std::abs(liveY) > 36.5 ||
                                isInsidePocketPos(liveX, liveY);

                if (!isPotted && visitedIds.find(puckId) == visitedIds.end()) {
                    visitedIds.insert(puckId);

                    int pType = determinePuckType(puckId, pAddr, liveX, liveY);

                    GraphicsPuckInfo pi;
                    pi.id = puckId;
                    pi.x = liveX;
                    pi.y = liveY;
                    pi.active = true;
                    pi.puckType = pType;
                    outPucks.push_back(pi);
                }
            }
        }

        // Advance to in-order successor (libgame.so 0x1B2B47C)
        uintptr_t right = 0;
        safeReadPtr(curr + 0x08, &right);
        if (right != 0 && right != endNode) {
            curr = right;
            uintptr_t left = 0;
            int innerLeft = 0;
            while (safeReadPtr(curr + 0x00, &left) && left != 0 && left != endNode && ++innerLeft < 32) {
                curr = left;
            }
        } else {
            uintptr_t parent = 0;
            int innerParent = 0;
            while (safeReadPtr(curr + 0x10, &parent) && parent != 0 && parent != endNode && ++innerParent < 32) {
                uintptr_t parentRight = 0;
                safeReadPtr(parent + 0x08, &parentRight);
                if (curr != parentRight) {
                    break;
                }
                curr = parent;
            }
            safeReadPtr(curr + 0x10, &curr);
        }
    }

    // Method 2: DFS tree walk fallback if in-order iterator found nothing
    if (outPucks.empty()) {
        uintptr_t rootNode = 0;
        if (safeReadPtr(tableLogicAddr + g_offsets.tlPuckMap, &rootNode) && rootNode && rootNode != endNode) {
            traversePuckTreeDfs(rootNode, endNode, 0, visitedIds, outPucks);
        }
    }
}

extern "C" void CatEngine_UpdateTablePucks() {
    void* tc = g_activeTableController.load();
    if (!tc || !isValidTableController(tc)) {
        return;
    }

    uintptr_t tcAddr = reinterpret_cast<uintptr_t>(tc);
    uintptr_t tableLogicAddr = 0;
    if (!safeReadPtr(tcAddr + g_offsets.tcTableLogic, &tableLogicAddr) || !tableLogicAddr) {
        return;
    }

    std::vector<GraphicsPuckInfo> activePucks;
    activePucks.reserve(24);
    readAllPucksFromTableLogic(tableLogicAddr, activePucks);

    s_activePuckCount = static_cast<int>(activePucks.size());
    GraphicsEngine_SyncAllPucks(activePucks.data(), static_cast<int>(activePucks.size()));

    static int64_t s_lastPuckLogMs = 0;
    int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    if (nowMs - s_lastPuckLogMs > 2000) {
        s_lastPuckLogMs = nowMs;
        ALOGI("[CatEngine] UpdateTablePucks: tc=%p, tableLogic=0x%lx, activePucks=%zu",
              tc, tableLogicAddr, activePucks.size());
    }
}

// =========================================================================================
// Game Hooks (Dobby)
// =========================================================================================

// Hook: TableController forcePuckPosition:position: (libgame.so + 0x1B3352C)
// Aligned to exact function entry (stp d9, d8, [sp, #-0x20]!) to prevent foul crash
typedef void (*forcePuckPosition_fn)(void* self, void* sel, int puckId, double x, double y);
static forcePuckPosition_fn orig_forcePuckPosition = nullptr;

static void my_forcePuckPosition(void* self, void* sel, int puckId, double x, double y) {
    if (self && isValidTableController(self)) {
        g_activeTableController.store(self);
    }
    if (!std::isnan(x) && !std::isnan(y)) {
        uintptr_t pAddr = 0;
        if (self && isValidTableController(self)) {
            uintptr_t tableLogicAddr = 0;
            if (safeReadPtr(reinterpret_cast<uintptr_t>(self) + g_offsets.tcTableLogic, &tableLogicAddr) && tableLogicAddr) {
                pAddr = findPuckInTableLogic(tableLogicAddr, puckId);
            }
        }
        int pType = determinePuckType(puckId, pAddr, x, y);
        bool isPotted = isInsidePocketPos(x, y);
        if (isPotted && (pType == 1 || pType == 2) && GraphicsEngine_GetAutoDetectedColor() == 0) {
            int matchPlayers = s_totalMatchPlayers.load();
            if (matchPlayers == 3) {
                GraphicsEngine_SetAutoDetectedColor(3);
            } else if (s_isLocalTurnReady.load()) {
                GraphicsEngine_SetAutoDetectedColor(pType);
                ALOGI("[CatEngine] AUTO-LEARNED GAME COLOR RULE (Live Pot): Our Team is %s (puck #%d)",
                      (pType == 2) ? "BLACK" : "WHITE", puckId);
            }
        }
        GraphicsEngine_UpdatePuck(puckId, x, y, !isPotted, pType);
    }
    if (orig_forcePuckPosition) {
        orig_forcePuckPosition(self, sel, puckId, x, y);
    }
}

// Hook: TableController forceStrikerPosition:position: (libgame.so + 0x1B33570)
typedef void (*forceStrikerPosition_fn)(void* self, void* sel, int baselineId, double x, double y);
static forceStrikerPosition_fn orig_forceStrikerPosition = nullptr;

static void my_forceStrikerPosition(void* self, void* sel, int baselineId, double x, double y) {
    if (orig_forceStrikerPosition) {
        orig_forceStrikerPosition(self, sel, baselineId, x, y);
    }
    // Always capture active TableController and calibrate board geometry whenever available
    if (self && isValidTableController(self)) {
        g_activeTableController.store(self);
        calibrateBoardFromTableController(self);
    }

    // STRICT ISOLATION: Never process or store opponent striker positions!
    bool isOpponent = isOpponentBaseline(y);
    if (isOpponent) {
        if (!s_passAndPlayBothSides.load()) {
            s_isLocalTurnReady.store(false);
            GraphicsEngine_UpdateAim(0.0, 0.0, 0.0, 0.0, false);
            return;
        }
    }

    if (!s_isLocalTurnReady.load()) {
        return;
    }

    // Identify local vs opponent: baselineId set at setupLocalStrikerBar is the local player's key.
    int knownLocal = s_localBaselineId.load();
    bool isLocalPlayer = (knownLocal == 0 || baselineId == knownLocal);
    if ((isLocalPlayer || s_passAndPlayBothSides.load()) && !std::isnan(x) && !std::isnan(y)) {
        if (isLocalPlayer) {
            s_localBaselineId.store(baselineId);
            s_lastPlayerIndex.store(baselineId);
        }
        g_matchActive.store(true);
        setGraphicsMatchActive(true);

        double lockedY = y;
        g_fixedBaselineY.store(lockedY);
        double minX = g_baselineMinX.load();
        double maxX = g_baselineMaxX.load();
        if (minX >= maxX || minX < -35.0 || maxX > 35.0) { minX = -23.85; maxX = 23.85; }
        
        double clampedX = std::clamp(x, minX, maxX);

        g_liveStrikerX.store(clampedX);
        g_liveStrikerY.store(lockedY);
        onStrikerPositionUpdated(clampedX, lockedY);

        int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        s_lastUserSliderTouchMs.store(nowMs);

        GraphicsEngine_OnStrikerReturnedToBaseline();
        GraphicsEngine_SetReviewFreeze(false);

        double curAngle = g_lastLoggedAngle.load();
        if (std::abs(curAngle) > 100.0 || std::isnan(curAngle)) {
            curAngle = (lockedY > 0.0) ? (-M_PI / 2.0) : (M_PI / 2.0);
        }
        double curForce = g_lastLoggedForce.load();
        if (curForce < 0.05 || std::isnan(curForce)) curForce = 1.0;
        GraphicsEngine_UpdateAim(clampedX, lockedY, curAngle, curForce, false);
    }
}



static MCPointDouble sampleLiveStrikerOnTable() {
    MCPointDouble pt = { g_liveStrikerX.load(), g_liveStrikerY.load() };
    void* tc = g_activeTableController.load();
    if (!tc || !isValidTableController(tc) || !g_libgameBase) return pt;

    uintptr_t tcAddr = reinterpret_cast<uintptr_t>(tc);
    uintptr_t tableLogic = 0;
    if (safeReadPtr(tcAddr + g_offsets.tcTableLogic, &tableLogic) && tableLogic) {
        uintptr_t strikerRoot = 0;
        if (safeReadPtr(tableLogic + g_offsets.tlStrikerMap, &strikerRoot) && strikerRoot) {
            uintptr_t stack[16];
            int stackTop = 0;
            stack[stackTop++] = strikerRoot;

            uintptr_t localNode = 0;
            double localSx = 0.0, localSy = 0.0;
            int localKey = 0;

            while (stackTop > 0 && stackTop < 15) {
                uintptr_t curr = stack[--stackTop];
                if (!curr) continue;

                uintptr_t left = 0, right = 0;
                safeReadPtr(curr + 0x00, &left);
                safeReadPtr(curr + 0x08, &right);
                if (left && left != curr) stack[stackTop++] = left;
                if (right && right != curr) stack[stackTop++] = right;

                uint32_t key = 0;
                if (safeReadInsn(curr + 0x20, &key)) {
                    uintptr_t slAddr = curr + 0x30;
                    double sx = 0.0, sy = 0.0;
                    bool readPos = false;

                    // 1. If user is smoothly dragging the slider, read active smooth target at +0x160 first
                    uint8_t isSmooth = 0;
                    safeReadBytes(slAddr + 0x170, &isSmooth, 1);
                    if (isSmooth) {
                        uintptr_t mxBits = 0, myBits = 0;
                        if (safeReadPtr(slAddr + 0x160, &mxBits) && safeReadPtr(slAddr + 0x168, &myBits)) {
                            double msx = *reinterpret_cast<double*>(&mxBits);
                            double msy = *reinterpret_cast<double*>(&myBits);
                            if (!std::isnan(msx) && !std::isnan(msy) && std::abs(msx) <= 38.0 && std::abs(msy) <= 38.0) {
                                sx = msx;
                                sy = msy;
                                readPos = true;
                            }
                        }
                    }

                    // 2. Fall back to resting physical Box2D body position at slAddr + 0x10 and +0x18
                    if (!readPos) {
                        uintptr_t xBits = 0, yBits = 0;
                        if (safeReadPtr(slAddr + 0x10, &xBits) && safeReadPtr(slAddr + 0x18, &yBits)) {
                            double bx = *reinterpret_cast<double*>(&xBits);
                            double by = *reinterpret_cast<double*>(&yBits);
                            if (!std::isnan(bx) && !std::isnan(by) && std::abs(bx) <= 38.0 && std::abs(by) <= 38.0) {
                                sx = bx;
                                sy = by;
                                readPos = true;
                            }
                        }
                    }
                    if (readPos && !std::isnan(sx) && !std::isnan(sy) && std::abs(sx) <= 38.0 && std::abs(sy) <= 38.0) {
                        int targetKey = s_activeTurnPlayerId.load();
                        if (targetKey < 0) targetKey = s_localBaselineId.load();
                        bool isTarget = (static_cast<int>(key) == targetKey);
                        if (isTarget) {
                            localNode = curr;
                            localSx = sx;
                            localSy = sy;
                            localKey = static_cast<int>(key);
                            break;
                        }
                    }
                }
            }

            if (localNode != 0) {
                // Update localBaselineId if this node's key matches the same side as known baseline
                if (isValidBaselineY(localSy)) {
                    int knownLocal = s_localBaselineId.load();
                    if (knownLocal == 0 || localKey == knownLocal) {
                        s_localBaselineId.store(localKey);
                        s_lastPlayerIndex.store(localKey);
                    }
                }
                pt.x = localSx;
                pt.y = isValidBaselineY(localSy) ? localSy : g_liveStrikerY.load();
            }
        }
    }
    return pt;
}

static MCPointDouble sampleStrikerFullTable() {
    MCPointDouble pt = { g_liveStrikerX.load(), g_liveStrikerY.load() };
    void* tc = g_activeTableController.load();
    if (!tc || !isValidTableController(tc) || !g_libgameBase) return pt;

    uintptr_t tcAddr = reinterpret_cast<uintptr_t>(tc);
    uintptr_t tableLogic = 0;
    if (safeReadPtr(tcAddr + g_offsets.tcTableLogic, &tableLogic) && tableLogic) {
        uintptr_t strikerRoot = 0;
        if (safeReadPtr(tableLogic + g_offsets.tlStrikerMap, &strikerRoot) && strikerRoot) {
            uintptr_t stack[16];
            int stackTop = 0;
            stack[stackTop++] = strikerRoot;

            int targetPlayer = s_localBaselineId.load();
            if (targetPlayer < 0) targetPlayer = 0;

            uintptr_t fallbackNode = 0;
            double fallbackX = 0.0, fallbackY = 0.0;

            while (stackTop > 0 && stackTop < 15) {
                uintptr_t curr = stack[--stackTop];
                if (!curr) continue;

                uintptr_t left = 0, right = 0;
                safeReadPtr(curr + 0x00, &left);
                safeReadPtr(curr + 0x08, &right);
                if (left && left != curr) stack[stackTop++] = left;
                if (right && right != curr) stack[stackTop++] = right;

                uint32_t key = 0;
                if (safeReadInsn(curr + 0x20, &key)) {
                    uintptr_t slAddr = curr + 0x30;
                    uintptr_t xBits = 0, yBits = 0;
                    if (safeReadPtr(slAddr + 0x10, &xBits) && safeReadPtr(slAddr + 0x18, &yBits)) {
                        double sx = *reinterpret_cast<double*>(&xBits);
                        double sy = *reinterpret_cast<double*>(&yBits);
                        if (!std::isnan(sx) && !std::isnan(sy) && std::abs(sx) <= 38.0 && std::abs(sy) <= 38.0) {
                            if ((int)key == targetPlayer) {
                                return { sx, sy };
                            }
                            if (fallbackNode == 0) {
                                fallbackNode = curr;
                                fallbackX = sx;
                                fallbackY = sy;
                            }
                        }
                    }
                }
            }
            if (fallbackNode != 0) {
                return { fallbackX, fallbackY };
            }
        }
    }
    return pt;
}

static MCPointDouble sampleStrikerPosition() {
    double minX = g_baselineMinX.load();
    double maxX = g_baselineMaxX.load();
    if (minX >= maxX || minX < -35.0 || maxX > 35.0) {
        minX = catengine::constants::kBaselineMinX;
        maxX = catengine::constants::kBaselineMaxX;
    }

    double x = g_liveStrikerX.load();
    double y = g_liveStrikerY.load();

    int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    bool userRecentlyTouched = (nowMs - s_lastUserSliderTouchMs.load() < 2500);

    if (!userRecentlyTouched) {
        MCPointDouble live = sampleLiveStrikerOnTable();
        if (!std::isnan(live.x) && std::abs(live.x) <= 35.0) {
            x = live.x;
        } else if (std::isnan(x) || std::abs(x) > 35.0) {
            x = 0.0;
        }

        if (!std::isnan(live.y) && std::abs(live.y) >= 15.0 && std::abs(live.y) <= 35.0) {
            y = live.y;
        }
    }

    if (std::isnan(y) || std::abs(y) < 15.0 || std::abs(y) > 35.0) {
        y = (s_isOpponentTurn.load() && s_passAndPlayBothSides.load()) ? catengine::constants::kBaselineY : -catengine::constants::kBaselineY;
    }
    x = std::clamp(x, minX, maxX);

    g_fixedBaselineY.store(y);
    g_liveStrikerX.store(x);
    g_liveStrikerY.store(y);
    return { x, y };
}

// Hook: ControlsLogicLocal onAimChanged (libgame.so + 0x19843A0) -> AIM TOUCH/PULL
typedef void (*aimChanged_fn)(void* this_ptr, int mode, double angle, double force);
static aimChanged_fn orig_aimChanged = nullptr;
static void* s_capturedAimThis = nullptr;
static void* s_capturedShotThis = nullptr;

static void my_onAimChanged(void* this_ptr, int mode, double angle, double force) {
    if (this_ptr) {
        s_capturedAimDelegate = this_ptr;
        s_capturedAimThis = this_ptr;
        s_capturedControlsLogic = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(this_ptr) - g_offsets.controlsLogicAim);
        s_capturedShotDelegate = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(this_ptr) - g_offsets.controlsLogicAim + g_offsets.controlsLogicShot);
        s_capturedShotThis = s_capturedShotDelegate;
        uintptr_t vtbl = 0;
        if (safeReadPtr(reinterpret_cast<uintptr_t>(s_capturedControlsLogic), &vtbl) && vtbl > 0x10000) {
            s_knownControlsVtable = vtbl;
        }
    }
    if (orig_aimChanged) {
        orig_aimChanged(this_ptr, mode, angle, force);
    }

    // If user is touching/aiming the striker on baseline, any previous shot is finished!
    if (GraphicsEngine_IsShotInProgress()) {
        if (mode == 0 || mode == 1 || force > 0.01) {
            GraphicsEngine_OnStrikerReturnedToBaseline();
        GraphicsEngine_SetReviewFreeze(false);
        } else {
            return;
        }
    }

    if (!std::isnan(angle) && !std::isnan(force)) {
        MCPointDouble stPos = sampleStrikerPosition();
        double curX = stPos.x;
        double curY = stPos.y; // ALWAYS active player baseline Y!

        bool isOpponent = isOpponentBaseline(curY);
        if (isOpponent) {
            s_isOpponentTurn.store(true);
            if (!s_passAndPlayBothSides.load()) {
                // Strictly hide guidelines during friend/opponent turn ("my chance not friends chance")
                s_isLocalTurnReady.store(false);
                GraphicsEngine_UpdateAim(0.0, 0.0, 0.0, 0.0, false);
                return;
            }
            s_isLocalTurnReady.store(true);
        } else {
            s_isOpponentTurn.store(false);
            s_isLocalTurnReady.store(true);
        }

        int localB = s_isOpponentTurn.load() ? s_activeTurnPlayerId.load() : s_localBaselineId.load();
        s_lastPlayerIndex.store((localB >= 0) ? localB : 0);

        g_matchActive.store(true);
        setGraphicsMatchActive(true);

        double launchAngle = pullAngleToLaunchAngle(angle);
        g_lastLoggedAngle.store(launchAngle);
        g_lastLoggedForce.store(force);

        int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

        static int64_t s_lastPuckUpdateAimMs = 0;
        if (nowMs - s_lastPuckUpdateAimMs > 250) {
            s_lastPuckUpdateAimMs = nowMs;
            updateStrikerPhysicsFromGame(g_activeTableController);
            updateTablePucks();
        }

        bool isAiming = (force > 0.02) || (mode == 0 || mode == 1);
        if (mode == 2 && force <= 0.02) {
            isAiming = false;
        }
        if (isAiming) {
            GraphicsEngine_SetReviewFreeze(false);
        }
        GraphicsEngine_UpdateAim(curX, curY, launchAngle, force, isAiming);
    }
}

// =========================================================================================
// Real-Time Shot Telemetry & Root-Cause Analysis System
// Tracks prediction vs actual shot outcome, logs error metrics, and diagnoses causes for misses.
// =========================================================================================

struct PuckStateSample {
    int id = -1;
    int puckType = 1; // 1 = White, 2 = Black, 3 = Queen
    double x = 0.0;
    double y = 0.0;
    bool isPocketed = false;
    bool active = false;
};

static std::vector<PuckStateSample> sampleTablePucks() {
    std::vector<PuckStateSample> samples;
    void* tc = g_activeTableController.load();
    if (!tc) return samples;
    uintptr_t tcAddr = reinterpret_cast<uintptr_t>(tc);
    uintptr_t tableLogicAddr = 0;
    if (safeReadPtr(tcAddr + g_offsets.tcTableLogic, &tableLogicAddr) && tableLogicAddr) {
        std::vector<GraphicsPuckInfo> activePucks;
        activePucks.reserve(24);
        readAllPucksFromTableLogic(tableLogicAddr, activePucks);
        samples.reserve(activePucks.size());
        for (const auto& ap : activePucks) {
            PuckStateSample s;
            s.id = ap.id;
            s.puckType = ap.puckType;
            s.x = ap.x;
            s.y = ap.y;
            s.isPocketed = !ap.active;
            s.active = ap.active;
            samples.push_back(s);
        }
    }
    return samples;
}

static std::atomic<int> s_shotCount(0);

[[maybe_unused]] static void logShotTelemetry(const ShotPredictionTelemetry& tele,
                             const std::vector<PuckStateSample>& initialPucks,
                             const std::vector<PuckStateSample>& finalPucks,
                             MCPointDouble finalStriker,
                             double strikerActualTravel,
                             const std::map<int, double>& puckActualTravel,
                             bool strikerReturnedBaseline) {
    int shotId = tele.shotId;
    double powerPct = tele.shotForceVal * 100.0;
    double aimDeg = tele.shotAngleDeg;

    time_t now = time(nullptr);
    struct tm tstruct;
    char timeStr[64];
    localtime_r(&now, &tstruct);
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tstruct);

    // Striker predicted vs actual metrics
    double stPredX = tele.strikerPredFinalX;
    double stPredY = tele.strikerPredFinalY;
    double stActX  = finalStriker.x;
    double stActY  = finalStriker.y;
    double stError = std::hypot(stActX - stPredX, stActY - stPredY);
    double stPredDist = tele.strikerTotalLineLength;
    double stActTravel = (strikerActualTravel > 0.1) ? strikerActualTravel : std::hypot(stActX - tele.strikerStartX, stActY - tele.strikerStartY);
    double stTravelDiff = stActTravel - stPredDist;

    bool stActPocket = (std::abs(stActX) > 30.0 && (stActY < -31.5 || stActY > 31.5));

    std::string report;
    auto appendLog = [&](const char* fmt, ...) {
        char buf[2048];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        __android_log_print(ANDROID_LOG_INFO, "CatEngine", "[TELEMETRY] %s", buf);
        __android_log_print(ANDROID_LOG_INFO, "CarromTelemetry", "%s", buf);
        report += buf;
        report += "\n";
    };

    appendLog(" ");
    appendLog("=======================================================================================================================");
    appendLog("[CARROM SHOT TELEMETRY #%d] %s | Shooter: Player %d", shotId, timeStr, tele.playerIndex);
    appendLog("=======================================================================================================================");
    appendLog("1. SHOT RELEASE INPUTS:");
    appendLog("   * Pull Power:      %.1f%% (Raw force: %.4f)", powerPct, tele.shotForceVal);
    appendLog("   * Aim Angle:       %.2f deg (%.4f rad)", aimDeg, tele.shotAngleRad);
    appendLog("   * Initial Speed:   %.2f cm/s", tele.strikerInitialSpeed);
    appendLog("   * Striker Launch:  (%.2f, %.2f) cm", tele.strikerStartX, tele.strikerStartY);
    if (tele.hasFirstImpact) {
        appendLog("   * First Contact:   Striker @ (%.2f, %.2f) -> Hit Target Puck #%d (%s) @ (%.2f, %.2f) cm [Impact Speed: %.1f cm/s]",
                  tele.firstImpactStrikerX, tele.firstImpactStrikerY,
                  tele.firstContactPuckId,
                  (tele.firstContactPuckType == 3) ? "Queen" : ((tele.firstContactPuckType == 2) ? "Black" : "White"),
                  tele.firstImpactPuckX, tele.firstImpactPuckY, tele.firstImpactSpeed);
    } else {
        appendLog("   * First Contact:   None (Cushion bank shot / no direct puck contact)");
    }

    appendLog(" ");
    appendLog("2. STRIKER: PREDICTION vs ACTUAL REALITY:");
    appendLog("   * Guideline Drawn: Total Line Length=%.2f cm | Segments=%d | Cushion Bounces=%d",
              tele.strikerTotalLineLength, tele.strikerSegmentCount, tele.strikerCushionBounces);
    for (int s = 0; s < tele.strikerSegmentCount; s++) {
        const auto& seg = tele.strikerSegments[s];
        double segLen = std::hypot(seg.endX - seg.startX, seg.endY - seg.startY);
        appendLog("     -> Segment %d%s: (%.2f, %.2f) -> (%.2f, %.2f) [Length: %.2f cm]",
                  s + 1, seg.isCushionBounce ? " (Cushion Bounce)" : "",
                  seg.startX, seg.startY, seg.endX, seg.endY, segLen);
    }
    appendLog("   * Predicted Landing: (%.2f, %.2f) cm | Pocket: %s",
              stPredX, stPredY, tele.strikerPredPocket ? "YES" : "NO");
    appendLog("   * Actual Resting:    (%.2f, %.2f) cm | Pocket: %s",
              stActX, stActY, stActPocket ? "YES (SCRATCH FOUL!)" : "NO");
    appendLog("   * Baseline Return:   %s", strikerReturnedBaseline ? "CONFIRMED (Striker returned to baseline)" : "PENDING/TIMEOUT");
    appendLog("   * ACCURACY METRICS:");
    appendLog("     - Positional Error (Landing to Actual): %.2f cm", stError);
    appendLog("     - Coordinate Offset:                    dX=%+.2f cm, dY=%+.2f cm", stActX - stPredX, stActY - stPredY);
    appendLog("     - Travel vs Predicted Line Difference:  Actual=%.2f cm vs Line=%.2f cm (Diff=%+.2f cm)",
              stActTravel, tele.strikerTotalLineLength, stTravelDiff);

    appendLog(" ");
    appendLog("3. TABLE PUCKS: PREDICTION vs ACTUAL COMPARISON TABLE (Total on Board: %d):", tele.totalPucksOnTable);
    appendLog("-----------------------------------------------------------------------------------------------------------------------");
    appendLog("| ID  | Type   | Hit By         | Hit Spd  | Pred Final Pos   | Actual Final Pos | Pos Err | Pred Pot | Act Pot | Outcome        |");
    appendLog("-----------------------------------------------------------------------------------------------------------------------");

    double totalPosErr = 0.0;
    double maxPosErr = 0.0;
    int movingPuckCount = 0;
    int potMatchCount = 0;
    int totalPucks = tele.totalPucksOnTable;

    struct PuckMatchResult {
        int id;
        int type;
        const char* typeName;
        double startX, startY;
        double predX, predY;
        double actX, actY;
        double posErr;
        bool isHit;
        int hitByEntityId;
        const char* hitByName;
        double hitSpeed;
        bool predPot;
        bool actPot;
        double predDist;
        double actDist;
        double distDiff;
        double angDrift;
        const char* status;
    };
    std::vector<PuckMatchResult> matchResults;

    for (int pIdx = 0; pIdx < tele.totalPucksOnTable; pIdx++) {
        const auto& pt = tele.predictedPucks[pIdx];

        // Find initial state
        double initX = pt.startX;
        double initY = pt.startY;
        for (const auto& ip : initialPucks) {
            if (ip.id == pt.puckId) {
                initX = ip.x;
                initY = ip.y;
                break;
            }
        }

        // Find actual final state
        bool foundInFinal = false;
        bool actPotted = false;
        double actX = initX;
        double actY = initY;

        for (const auto& fp : finalPucks) {
            if (fp.id == pt.puckId) {
                foundInFinal = true;
                actX = fp.x;
                actY = fp.y;
                actPotted = fp.isPocketed || (std::abs(fp.x) > 36.5) || (std::abs(fp.y) > 36.5) || isInsidePocketPos(fp.x, fp.y);
                break;
            }
        }
        if (!foundInFinal) {
            actPotted = true; // Potted and removed from live AVL tree
        }

        bool predPotted = pt.predPocket;
        double predX = pt.predFinalX;
        double predY = pt.predFinalY;

        double posErr = 0.0;
        if (predPotted && actPotted) {
            posErr = 0.0;
            potMatchCount++;
        } else if (!predPotted && !actPotted) {
            posErr = std::hypot(actX - predX, actY - predY);
            potMatchCount++;
        } else {
            posErr = std::hypot(actX - predX, actY - predY);
        }

        if (posErr > maxPosErr) maxPosErr = posErr;
        totalPosErr += posErr;

        double predDist = pt.totalLineLength;
        auto travIt = puckActualTravel.find(pt.puckId);
        double actDist = (travIt != puckActualTravel.end() && travIt->second > 0.05) ? travIt->second :
                         (actPotted ? predDist : std::hypot(actX - initX, actY - initY));
        double distDiff = actDist - predDist;

        double predAng = std::atan2(predY - initY, predX - initX) * 180.0 / M_PI;
        double actAng  = actPotted ? predAng : (std::atan2(actY - initY, actX - initX) * 180.0 / M_PI);
        double angDrift = actAng - predAng;
        while (angDrift > 180.0)  angDrift -= 360.0;
        while (angDrift < -180.0) angDrift += 360.0;

        const char* pTypeName = (pt.puckType == 3) ? "Queen" : ((pt.puckType == 2) ? "Black" : "White");
        const char* status = "UNKNOWN";

        if (predPotted && actPotted) {
            status = "PERFECT POT";
        } else if (!predPotted && actPotted) {
            status = "UNEXPECTED POT";
        } else if (predPotted && !actPotted) {
            status = "MISSED POT";
        } else if (pt.isHit) {
            movingPuckCount++;
            if (posErr < 1.0) status = "ACCURATE REST";
            else if (posErr < 2.5) status = "CLOSE LANDING";
            else status = "DEVIATED REST";
        } else {
            if (actDist < 0.25) status = "STATIC MATCH";
            else status = "UNPRED BUMP";
        }

        char hitSpdBuf[16];
        if (pt.isHit && pt.hitSpeed > 0.1) snprintf(hitSpdBuf, sizeof(hitSpdBuf), "%.1f cm/s", pt.hitSpeed);
        else snprintf(hitSpdBuf, sizeof(hitSpdBuf), "---");

        char predPosBuf[24], actPosBuf[24];
        snprintf(predPosBuf, sizeof(predPosBuf), "(%5.1f, %5.1f)", predX, predY);
        if (actPotted) snprintf(actPosBuf, sizeof(actPosBuf), "[POCKETED]");
        else snprintf(actPosBuf, sizeof(actPosBuf), "(%5.1f, %5.1f)", actX, actY);

        appendLog("| #%-2d | %-6s | %-14s | %-8s | %-16s | %-16s | %5.2fcm | %-8s | %-7s | %-14s |",
                  pt.puckId, pTypeName, pt.hitByName, hitSpdBuf, predPosBuf, actPosBuf,
                  posErr, predPotted ? "YES" : "NO", actPotted ? "YES" : "NO", status);

        PuckMatchResult pm;
        pm.id = pt.puckId;
        pm.type = pt.puckType;
        pm.typeName = pTypeName;
        pm.startX = initX; pm.startY = initY;
        pm.predX = predX; pm.predY = predY;
        pm.actX = actX; pm.actY = actY;
        pm.posErr = posErr;
        pm.isHit = pt.isHit;
        pm.hitByEntityId = pt.hitByEntityId;
        pm.hitByName = pt.hitByName;
        pm.hitSpeed = pt.hitSpeed;
        pm.predPot = predPotted;
        pm.actPot = actPotted;
        pm.predDist = predDist;
        pm.actDist = actDist;
        pm.distDiff = distDiff;
        pm.angDrift = angDrift;
        pm.status = status;
        matchResults.push_back(pm);
    }
    appendLog("-----------------------------------------------------------------------------------------------------------------------");

    // 4. Detailed Breakdown for All Active & Displaced Pucks
    appendLog(" ");
    appendLog("4. DETAILED BREAKDOWN (Active & Displaced Pucks):");
    for (const auto& pm : matchResults) {
        if (!pm.isHit && pm.actDist < 0.25 && !pm.actPot) continue;

        appendLog("   ---------------------------------------------------------------------------------");
        appendLog("   PUCK #%d (%s) -> [Hit By: %s | Impact Speed: %.1f cm/s | Status: %s]",
                  pm.id, pm.typeName, pm.hitByName, pm.hitSpeed, pm.status);
        appendLog("     * Start Position:   (%.2f, %.2f) cm", pm.startX, pm.startY);
        appendLog("     * Predicted Target: (%.2f, %.2f) cm | Potted: %s | Line Length: %.2f cm",
                  pm.predX, pm.predY, pm.predPot ? "YES" : "NO", pm.predDist);
        if (pm.actPot) {
            appendLog("     * Actual Final:     POCKETED (CLEAN POT!) | Actual Travel: %.2f cm", pm.actDist);
            // Real-time automatic color rule learning from observed pot
            if (pm.type == 1 || pm.type == 2) {
                int shooter = tele.playerIndex;
                int matchPlayers = s_totalMatchPlayers.load();
                if (matchPlayers == 3) {
                    // 3-Player match in Carrom Pool is ALWAYS Points/Disc Pool: ANY coin is valid!
                    GraphicsEngine_SetAutoDetectedColor(3); // 3 = Any Coin
                    ALOGI("[CatEngine] 3-PLAYER MATCH DETECTED: Set rule to ANY COIN (Points Mode)");
                } else {
                    // 1v1 or 2v2 Teams:
                    // Dynamic team assignment:
                    // If local player is known, our team matches local player's parity (e.g. 0 & 2 in 4-player, or shooter == localB)
                    int localB = s_localBaselineId.load();
                    if (localB < 0) localB = 0;
                    bool isOurTeam = false;
                    if (matchPlayers == 4) {
                        // In 4-player / 2v2, team 1 = players 0 & 2, team 2 = players 1 & 3
                        isOurTeam = ((shooter % 2) == (localB % 2));
                    } else {
                        // In 1v1, our team is simply shooter == localB
                        isOurTeam = (shooter == localB);
                    }
                    int assignedColor = isOurTeam ? pm.type : (3 - pm.type);
                    GraphicsEngine_SetAutoDetectedColor(assignedColor);
                    ALOGI("[CatEngine] AUTO-LEARNED GAME COLOR RULE: Our Team is %s (from potted puck #%d by %s - Player %d, LocalPlayer=%d, MatchPlayers=%d)",
                          (assignedColor == 2) ? "BLACK" : "WHITE", pm.id,
                          isOurTeam ? "Our Team" : "Opponent Team", shooter, localB, matchPlayers);
                }
            }
        } else {
            appendLog("     * Actual Final:     (%.2f, %.2f) cm | Actual Travel: %.2f cm", pm.actX, pm.actY, pm.actDist);
        }
        appendLog("     * Error Analysis:   Position Error = %.2f cm (dX=%+.2f, dY=%+.2f) | Dist Diff = %+.2f cm | Angle Drift = %+.2f deg",
                  pm.posErr, pm.actX - pm.predX, pm.actY - pm.predY, pm.distDiff, pm.angDrift);
    }

    // 5. Global Accuracy & Diagnosis
    double avgPosErr = (totalPucks > 0) ? (totalPosErr / totalPucks) : 0.0;
    double accuracyPct = std::max(0.0, 100.0 - std::min(100.0, (avgPosErr / 2.0) * 100.0));

    appendLog(" ");
    appendLog("5. DIAGNOSTIC ACTION SUMMARY:");
    appendLog("   * Overall Shot Accuracy:   %.1f%%", accuracyPct);
    appendLog("   * Max Positional Error:    %.2f cm", maxPosErr);
    appendLog("   * Avg Positional Error:    %.2f cm", avgPosErr);
    appendLog("   * Striker Positional Error: %.2f cm", stError);
    appendLog("   * Potted Prediction Match: %d / %d", potMatchCount, totalPucks);

    if (maxPosErr < 1.2 && stError < 1.5) {
        appendLog("   * DIAGNOSIS: [HIGH PRECISION] Prediction perfectly mirrored the physical engine outcome!");
    } else {
        appendLog("   * DIAGNOSIS: [CALIBRATION RECOMMENDATIONS]:");
        if (stError > 2.0) {
            appendLog("     -> Striker deviated by %.2f cm. Verify striker friction (mu_s) and power scaling.", stError);
        }
        if (maxPosErr > 2.5) {
            appendLog("     -> Max puck deviation was %.2f cm. Check table center calibration (X/Y) or cushion alignment.", maxPosErr);
        }
    }

    // Train Real-Time Adaptive Physics Learning Engine on observed physical outcome
    if (tele.hasFirstImpact && tele.firstContactPuckId >= 0) {
        for (const auto& pm : matchResults) {
            if (pm.id == tele.firstContactPuckId) {
                catengine::ai::AdaptiveLearningEngine::getInstance().recordPostShotOutcomeAndTrain(
                    pm.actX, pm.actY, pm.actPot, tele.strikerCushionBounces
                );
                break;
            }
        }
    }
    appendLog("=======================================================================================================================");
    appendLog(" ");

    // 6. Generate Machine-Readable JSON Record (JSONL)
    std::string jsonLine = "{";
    jsonLine += "\"shot_id\":" + std::to_string(shotId) + ",";
    jsonLine += "\"timestamp\":\"" + std::string(timeStr) + "\",";
    jsonLine += "\"player_index\":" + std::to_string(tele.playerIndex) + ",";
    jsonLine += "\"inputs\":{";
    jsonLine += "\"striker_start\":[" + std::to_string(tele.strikerStartX) + "," + std::to_string(tele.strikerStartY) + "],";
    jsonLine += "\"angle_deg\":" + std::to_string(tele.shotAngleDeg) + ",";
    jsonLine += "\"angle_rad\":" + std::to_string(tele.shotAngleRad) + ",";
    jsonLine += "\"force_raw\":" + std::to_string(tele.shotForceVal) + ",";
    jsonLine += "\"power_pct\":" + std::to_string(powerPct) + ",";
    jsonLine += "\"initial_speed\":" + std::to_string(tele.strikerInitialSpeed) + ",";
    jsonLine += "\"has_first_impact\":" + std::string(tele.hasFirstImpact ? "true" : "false") + ",";
    jsonLine += "\"first_impact_puck_id\":" + std::to_string(tele.firstContactPuckId);
    jsonLine += "},";

    jsonLine += "\"striker\":{";
    jsonLine += "\"pred_final\":[" + std::to_string(stPredX) + "," + std::to_string(stPredY) + "],";
    jsonLine += "\"actual_final\":[" + std::to_string(stActX) + "," + std::to_string(stActY) + "],";
    jsonLine += "\"pos_error\":" + std::to_string(stError) + ",";
    jsonLine += "\"pred_pocket\":" + std::string(tele.strikerPredPocket ? "true" : "false") + ",";
    jsonLine += "\"actual_pocket\":" + std::string(stActPocket ? "true" : "false") + ",";
    jsonLine += "\"pred_distance\":" + std::to_string(stPredDist) + ",";
    jsonLine += "\"actual_travel\":" + std::to_string(stActTravel) + ",";
    jsonLine += "\"cushion_bounces\":" + std::to_string(tele.strikerCushionBounces) + ",";
    jsonLine += "\"returned_to_baseline\":" + std::string(strikerReturnedBaseline ? "true" : "false");
    jsonLine += "},";

    jsonLine += "\"pucks\":[";
    for (size_t i = 0; i < matchResults.size(); i++) {
        const auto& pm = matchResults[i];
        if (i > 0) jsonLine += ",";
        jsonLine += "{";
        jsonLine += "\"id\":" + std::to_string(pm.id) + ",";
        jsonLine += "\"type\":" + std::to_string(pm.type) + ",";
        jsonLine += "\"type_name\":\"" + std::string(pm.typeName) + "\",";
        jsonLine += "\"start\":[" + std::to_string(pm.startX) + "," + std::to_string(pm.startY) + "],";
        jsonLine += "\"pred_final\":[" + std::to_string(pm.predX) + "," + std::to_string(pm.predY) + "],";
        jsonLine += "\"actual_final\":[" + std::to_string(pm.actX) + "," + std::to_string(pm.actY) + "],";
        jsonLine += "\"pos_error\":" + std::to_string(pm.posErr) + ",";
        jsonLine += "\"is_hit\":" + std::string(pm.isHit ? "true" : "false") + ",";
        jsonLine += "\"hit_by_id\":" + std::to_string(pm.hitByEntityId) + ",";
        jsonLine += "\"hit_by_name\":\"" + std::string(pm.hitByName) + "\",";
        jsonLine += "\"hit_speed\":" + std::to_string(pm.hitSpeed) + ",";
        jsonLine += "\"pred_pocket\":" + std::string(pm.predPot ? "true" : "false") + ",";
        jsonLine += "\"actual_pocket\":" + std::string(pm.actPot ? "true" : "false") + ",";
        jsonLine += "\"pred_dist\":" + std::to_string(pm.predDist) + ",";
        jsonLine += "\"actual_dist\":" + std::to_string(pm.actDist) + ",";
        jsonLine += "\"status\":\"" + std::string(pm.status) + "\"";
        jsonLine += "}";
    }
    jsonLine += "],";

    jsonLine += "\"summary\":{";
    jsonLine += "\"accuracy_pct\":" + std::to_string(accuracyPct) + ",";
    jsonLine += "\"max_pos_error\":" + std::to_string(maxPosErr) + ",";
    jsonLine += "\"avg_pos_error\":" + std::to_string(avgPosErr) + ",";
    jsonLine += "\"striker_error\":" + std::to_string(stError) + ",";
    jsonLine += "\"potted_matches\":" + std::to_string(potMatchCount);
    jsonLine += "}";
    jsonLine += "}\n";

    // 7. Persist to Disk Across Robust Paths
    const char* textLogPaths[] = {
        "/sdcard/Android/data/com.engine.cat/files/carrom_shot_telemetry.log",
        "/data/data/com.engine.cat/files/carrom_shot_telemetry.log",
        "/data/user/0/com.engine.cat/files/carrom_shot_telemetry.log",
        "/sdcard/Android/data/top.niunaijun.blackbox/files/carrom_shot_telemetry.log",
        "/data/data/top.niunaijun.blackbox/files/carrom_shot_telemetry.log",
        "/data/user/0/top.niunaijun.blackbox/files/carrom_shot_telemetry.log",
        "/sdcard/Download/carrom_shot_telemetry.log",
        "/sdcard/carrom_shot_telemetry.log"
    };
    for (const char* lp : textLogPaths) {
        FILE* logF = fopen(lp, "a");
        if (logF) {
            fwrite(report.c_str(), 1, report.length(), logF);
            fflush(logF);
            fclose(logF);
        }
    }

    const char* jsonLogPaths[] = {
        "/sdcard/Android/data/com.engine.cat/files/carrom_shot_telemetry.jsonl",
        "/data/data/com.engine.cat/files/carrom_shot_telemetry.jsonl",
        "/data/user/0/com.engine.cat/files/carrom_shot_telemetry.jsonl",
        "/sdcard/Android/data/top.niunaijun.blackbox/files/carrom_shot_telemetry.jsonl",
        "/data/data/top.niunaijun.blackbox/files/carrom_shot_telemetry.jsonl",
        "/data/user/0/top.niunaijun.blackbox/files/carrom_shot_telemetry.jsonl",
        "/sdcard/Download/carrom_shot_telemetry.jsonl",
        "/sdcard/carrom_shot_telemetry.jsonl"
    };
    for (const char* jp : jsonLogPaths) {
        FILE* jF = fopen(jp, "a");
        if (jF) {
            fwrite(jsonLine.c_str(), 1, jsonLine.length(), jF);
            fflush(jF);
            fclose(jF);
        }
    }
}

static std::atomic<bool> s_trackingInProgress(false);

static void startShotOutcomeTracking(ShotPredictionTelemetry tele, std::vector<PuckStateSample> initialPucks) {
    std::thread([tele = std::move(tele), initialPucks = std::move(initialPucks)]() mutable {
        static std::atomic<bool> s_isTracking(false);
        bool expected = false;
        if (!s_isTracking.compare_exchange_strong(expected, true)) {
            ALOGW("[CarromTelemetry] Another shot tracking thread is already active, superseding previous tracking.");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        MCPointDouble lastStriker = sampleStrikerFullTable();
        MCPointDouble restingStriker = lastStriker;
        double strikerActualTravel = 0.0;
        std::map<int, MCPointDouble> lastPuckPositions;
        std::map<int, double> puckActualTravel;

        for (const auto& ip : initialPucks) {
            lastPuckPositions[ip.id] = { ip.x, ip.y };
            puckActualTravel[ip.id] = 0.0;
        }

        int stationaryStreak = 0;
        const int requiredStationaryStreak = 12; // ~360ms of complete stillness
        const int maxIterations = 250;           // ~7.5 seconds maximum tracking timeout
        bool strikerReturnedBaseline = false;

        for (int iter = 0; iter < maxIterations; ++iter) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));

            if (!g_matchActive.load() || !isTableControllerAlive()) {
                ALOGI("[CarromTelemetry] Match ended or TableController destroyed during tracking for Shot #%d.", tele.shotId);
                s_isTracking.store(false);
                return;
            }

            // 1. Sample Striker
            MCPointDouble curStriker = sampleStrikerFullTable();
            bool strikerMoved = false;
            if (!std::isnan(curStriker.x) && !std::isnan(curStriker.y)) {
                if (!std::isnan(lastStriker.x) && !std::isnan(lastStriker.y)) {
                    double dSt = std::hypot(curStriker.x - lastStriker.x, curStriker.y - lastStriker.y);
                    if (dSt > 0.05) {
                        strikerActualTravel += dSt;
                        strikerMoved = true;
                        restingStriker = curStriker;
                    }
                }
                if (curStriker.y < -15.0 && curStriker.y > -35.0 && iter > 25 && strikerActualTravel > 1.0) {
                    strikerReturnedBaseline = true;
                }
                lastStriker = curStriker;
            }

            // 2. Sample Table Pucks
            std::vector<PuckStateSample> curPucks = sampleTablePucks();
            bool pucksMoved = false;
            for (const auto& cp : curPucks) {
                auto it = lastPuckPositions.find(cp.id);
                if (it != lastPuckPositions.end()) {
                    double dPk = std::hypot(cp.x - it->second.x, cp.y - it->second.y);
                    if (dPk > 0.05) {
                        puckActualTravel[cp.id] += dPk;
                        pucksMoved = true;
                    }
                    it->second = { cp.x, cp.y };
                } else {
                    lastPuckPositions[cp.id] = { cp.x, cp.y };
                }
            }

            // 3. Stillness check
            if (!strikerMoved && !pucksMoved) {
                stationaryStreak++;
                if (stationaryStreak >= requiredStationaryStreak) {
                    ALOGI("[CarromTelemetry] All discs stationary for %d checks. Shot #%d finished in %.2f seconds.",
                          stationaryStreak, tele.shotId, (iter * 30.0 + 100.0) / 1000.0);
                    break;
                }
            } else {
                stationaryStreak = 0;
            }
        }

        std::vector<PuckStateSample> finalPucks = sampleTablePucks();
        MCPointDouble finalStriker = restingStriker;
        if (std::isnan(finalStriker.x) || std::isnan(finalStriker.y)) {
            finalStriker = sampleStrikerFullTable();
        }
        if (std::isnan(finalStriker.x) || std::isnan(finalStriker.y)) {
            finalStriker = lastStriker;
        }

        logShotTelemetry(tele, initialPucks, finalPucks, finalStriker, strikerActualTravel, puckActualTravel, strikerReturnedBaseline);

        s_isTracking.store(false);
    }).detach();
}

// Hook: ControlsLogicLocal onShotFired (libgame.so + 0x1984568) -> SHOT RELEASE
typedef void (*shotFired_fn)(void* this_ptr, int playerIndex, bool flag, unsigned int param, double angle, double force);
static shotFired_fn orig_shotFired = nullptr;

static void my_onShotFired(void* this_ptr, int playerIndex, bool flag, unsigned int param, double angle, double force) {
    if (this_ptr) {
        s_capturedShotDelegate = this_ptr;
        s_capturedShotThis = this_ptr;
        s_capturedControlsLogic = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(this_ptr) - g_offsets.controlsLogicShot);
        s_capturedAimDelegate = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(this_ptr) - g_offsets.controlsLogicShot + g_offsets.controlsLogicAim);
        s_capturedAimThis = s_capturedAimDelegate;
        uintptr_t vtbl = 0;
        if (safeReadPtr(reinterpret_cast<uintptr_t>(s_capturedControlsLogic), &vtbl) && vtbl > 0x10000) {
            s_knownControlsVtable = vtbl;
        }
    }
    s_isLocalTurnReady.store(false);
    s_lastPlayerIndex.store(playerIndex);
    if (playerIndex >= 2 && s_totalMatchPlayers.load() < 4) {
        s_totalMatchPlayers.store(4);
    }

    int64_t shotNowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    s_lastShotTimeMs.store(shotNowMs);

    // Call game engine's orig_shotFired FIRST to achieve immediate, zero-latency shot execution!
    if (orig_shotFired) {
        orig_shotFired(this_ptr, playerIndex, flag, param, angle, force);
    }

    if (flag && !std::isnan(angle) && !std::isnan(force)) {
        double launchAngle = pullAngleToLaunchAngle(angle);

        MCPointDouble stPos = sampleStrikerPosition();
        double curX = (!std::isnan(stPos.x)) ? stPos.x : g_liveStrikerX.load();
        double curY = (!std::isnan(stPos.y)) ? stPos.y : g_liveStrikerY.load();

        // Freeze aiming guideline on screen until striker returns to baseline
        GraphicsEngine_OnShotFired(curX, curY, launchAngle, force);

        // Capture full prediction snapshot before pieces move
        ShotPredictionTelemetry tele = GraphicsEngine_CaptureShotPrediction(curX, curY, launchAngle, force);
        tele.shotId = ++s_shotCount;
        tele.playerIndex = playerIndex;
        std::vector<PuckStateSample> initialPucks = sampleTablePucks();

        // Launch post-shot outcome tracking and telemetry comparison
        startShotOutcomeTracking(tele, initialPucks);
    }
}

static void* getControlsLogicLocal() {
    // 1. Return already valid cached delegates immediately
    if (s_capturedShotDelegate && isValidShotDelegate(s_capturedShotDelegate)) {
        return s_capturedShotDelegate;
    }
    if (s_capturedShotThis && isValidShotDelegate(s_capturedShotThis)) {
        return s_capturedShotThis;
    }
    if (s_capturedControlsLogic && isValidShotDelegate(s_capturedControlsLogic)) {
        return s_capturedControlsLogic;
    }

    // 2. Discover from GameplayView (captured at setupLocalStrikerBar)
    if (s_capturedGameplayView) {
        uintptr_t gvAddr = reinterpret_cast<uintptr_t>(s_capturedGameplayView);
        uintptr_t gvVtbl = 0;
        if (safeReadPtr(gvAddr, &gvVtbl) && gvVtbl > 0x10000) {
            // Direct path from disassembly: GameplayView->_matchController (+0x420) -> ControlsLogicLocal (+0x20)
            uintptr_t mcAddr = 0;
            if (safeReadPtr(gvAddr + g_offsets.gvMatchController, &mcAddr) && mcAddr > 0x10000) {
                s_capturedMatchController = reinterpret_cast<void*>(mcAddr);
                uintptr_t ctrlPtr = 0;
                if (safeReadPtr(mcAddr + g_offsets.matchCtrlControlsLogic, &ctrlPtr) && ctrlPtr > 0x10000) {
                    void* candShot = reinterpret_cast<void*>(ctrlPtr + g_offsets.controlsLogicShot);
                    if (isValidShotDelegate(candShot)) {
                        s_capturedControlsLogic = reinterpret_cast<void*>(ctrlPtr);
                        s_capturedShotDelegate = candShot;
                        void* candAim = reinterpret_cast<void*>(ctrlPtr + g_offsets.controlsLogicAim);
                        if (isValidAimDelegate(candAim)) {
                            s_capturedAimDelegate = candAim;
                        }
                        return s_capturedShotDelegate;
                    }
                }
            }
            // Secondary scan of GameplayView fields (0x380 to 0x520)
            for (size_t off = 0x380; off <= 0x520; off += 8) {
                uintptr_t cand = 0;
                if (safeReadPtr(gvAddr + off, &cand) && cand > 0x10000) {
                    if (isControlsLogicLocal(cand)) {
                        void* candShot = reinterpret_cast<void*>(cand + g_offsets.controlsLogicShot);
                        if (isValidShotDelegate(candShot)) {
                            s_capturedControlsLogic = reinterpret_cast<void*>(cand);
                            s_capturedShotDelegate = candShot;
                            void* candAim = reinterpret_cast<void*>(cand + g_offsets.controlsLogicAim);
                            if (isValidAimDelegate(candAim)) {
                                s_capturedAimDelegate = candAim;
                            }
                            return s_capturedShotDelegate;
                        }
                    }
                }
            }
        }
    }

    // 3. Discover from MatchController
    if (s_capturedMatchController) {
        uintptr_t mcAddr = reinterpret_cast<uintptr_t>(s_capturedMatchController);
        uintptr_t ctrlPtr = 0;
        if (safeReadPtr(mcAddr + 0x20, &ctrlPtr) && ctrlPtr > 0x10000) {
            void* candShot = reinterpret_cast<void*>(ctrlPtr + g_offsets.controlsLogicShot);
            if (isValidShotDelegate(candShot)) {
                s_capturedControlsLogic = reinterpret_cast<void*>(ctrlPtr);
                s_capturedShotDelegate = candShot;
                void* candAim = reinterpret_cast<void*>(ctrlPtr + g_offsets.controlsLogicAim);
                if (isValidAimDelegate(candAim)) {
                    s_capturedAimDelegate = candAim;
                }
                return s_capturedShotDelegate;
            }
        }
    }

    // 4. Discover from TableController
    void* tc = g_activeTableController.load();
    if (tc) {
        uintptr_t tcAddr = reinterpret_cast<uintptr_t>(tc);
        for (size_t off = 0x20; off <= 0x500; off += sizeof(void*)) {
            uintptr_t cand = 0;
            if (safeReadPtr(tcAddr + off, &cand) && isValidUserPointer(cand)) {
                if (isControlsLogicLocal(cand)) {
                    void* candShot = reinterpret_cast<void*>(cand + g_offsets.controlsLogicShot);
                    if (isValidShotDelegate(candShot)) {
                        s_capturedControlsLogic = reinterpret_cast<void*>(cand);
                        s_capturedShotDelegate = candShot;
                        void* candAim = reinterpret_cast<void*>(cand + g_offsets.controlsLogicAim);
                        if (isValidAimDelegate(candAim)) {
                            s_capturedAimDelegate = candAim;
                        }
                        return s_capturedShotDelegate;
                    }
                }
                uintptr_t ctrlPtr = 0;
                if (safeReadPtr(cand + 0x20, &ctrlPtr) && isValidUserPointer(ctrlPtr) && isControlsLogicLocal(ctrlPtr)) {
                    s_capturedMatchController = reinterpret_cast<void*>(cand);
                    void* candShot = reinterpret_cast<void*>(ctrlPtr + g_offsets.controlsLogicShot);
                    if (isValidShotDelegate(candShot)) {
                        s_capturedControlsLogic = reinterpret_cast<void*>(ctrlPtr);
                        s_capturedShotDelegate = candShot;
                        void* candAim = reinterpret_cast<void*>(ctrlPtr + g_offsets.controlsLogicAim);
                        if (isValidAimDelegate(candAim)) {
                            s_capturedAimDelegate = candAim;
                        }
                        return s_capturedShotDelegate;
                    }
                }
            }
        }
    }

    return nullptr;
}

extern "C" bool CatEngine_ExecuteInEngineShot(double angleRad, double forceVal) {
    if (!g_matchActive.load() || !s_isLocalTurnReady.load()) {
        s_isInEngineAimActive.store(false);
        return false;
    }

    int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    if ((nowMs - s_lastShotTimeMs.load()) < 80) {
        ALOGI("[AUTO-SHOT EXECUTE] Shot already fired %lld ms ago (< 80ms cooldown).",
              static_cast<long long>(nowMs - s_lastShotTimeMs.load()));
        s_isInEngineAimActive.store(false);
        return true;
    }

    void* shotDelegate = s_capturedShotDelegate;
    if (!shotDelegate) {
        shotDelegate = s_capturedShotThis;
    }
    if (!shotDelegate) {
        shotDelegate = getControlsLogicLocal();
    }
    if (!shotDelegate || !orig_shotFired) {
        ALOGE("[AUTO-SHOT FAILURE] Shot aborted: shotDelegate=%p, orig_shotFired=%p",
              shotDelegate, orig_shotFired);
        s_isInEngineAimActive.store(false);
        return false;
    }

    // Slingshot pull angle for Carrom Pool engine release
    double pullAngle = launchAngleToPullAngle(angleRad);
    int pIndex = s_localBaselineId.load();
    if (pIndex < 0 || pIndex > 3) pIndex = s_lastPlayerIndex.load();
    if (pIndex < 0 || pIndex > 3) pIndex = 0;
    double forceValClamped = std::clamp(forceVal, 0.1, 1.0);

    void* aimDelegate = s_capturedAimDelegate;
    if (!aimDelegate) {
        aimDelegate = s_capturedAimThis;
    }
    if (orig_aimChanged && aimDelegate) {
        orig_aimChanged(aimDelegate, 2, pullAngle, forceValClamped);
    }
    s_isInEngineAimActive.store(false);

    // Freeze and persist trajectory & baseline circle at launch position before physics kicks off
    double curX = g_liveStrikerX.load();
    double curY = g_liveStrikerY.load();
    if (std::isnan(curY) || std::abs(curY) < 15.0) curY = g_fixedBaselineY.load();
    if (std::isnan(curY) || std::abs(curY) < 10.0) curY = -25.35;
    GraphicsEngine_OnShotFired(curX, curY, angleRad, forceValClamped);

    // Capture prediction telemetry and initial pucks before discs move
    ShotPredictionTelemetry tele = GraphicsEngine_CaptureShotPrediction(curX, curY, angleRad, forceValClamped);
    tele.shotId = ++s_shotCount;
    tele.playerIndex = pIndex;
    std::vector<PuckStateSample> initialPucks = sampleTablePucks();

    // Turn is consumed by this shot
    s_isLocalTurnReady.store(false);
    s_lastShotTimeMs.store(nowMs);

    orig_shotFired(shotDelegate, pIndex, true, 0, pullAngle, forceValClamped);

    // Track shot outcome for Auto-Shot
    startShotOutcomeTracking(tele, initialPucks);

    ALOGI("[AUTO-SHOT RESULT: SUCCESS] In-engine shot successfully executed!");
    return true;
}

extern "C" void CatEngine_SetInEngineAim(double angleRad, double forceVal) {
    if (!g_matchActive.load() || !s_isLocalTurnReady.load()) return;
    void* aimDelegate = s_capturedAimDelegate;
    if (!aimDelegate) {
        aimDelegate = s_capturedAimThis;
    }
    if (aimDelegate && orig_aimChanged) {
        double pullAngle = launchAngleToPullAngle(angleRad);
        if (!s_isInEngineAimActive.load()) {
            orig_aimChanged(aimDelegate, 0, pullAngle, forceVal);
            s_isInEngineAimActive.store(true);
        } else {
            orig_aimChanged(aimDelegate, 1, pullAngle, forceVal);
        }
    }
}

extern "C" bool CatEngine_GetLiveStrikerPos(double* outX, double* outY) {
    MCPointDouble pt = sampleStrikerPosition();
    if (outX) *outX = pt.x;
    if (outY) *outY = pt.y; // Strictly locked baseline Y
    return true;
}

extern "C" bool CatEngine_GetStrikerScreenPos(float* outX, float* outY) {
    MCPointDouble pt = sampleStrikerPosition();
    if (outX) *outX = static_cast<float>(pt.x);
    if (outY) *outY = static_cast<float>(pt.y);
    return true;
}

// =========================================================================================
// APPROACH 2: IN-ENGINE NATIVE PHYSICS STEPPER (Miniclip libgame.so Exact Mathematical Parity)
// =========================================================================================

struct NativePuckSnapshotItem {
    int id = -1;
    int puckType = 1;
    uintptr_t pAddr = 0;
    uint8_t rawMemory[0x200];
    double radius = 1.95;
    double startX = 0.0;
    double startY = 0.0;
    double lastSegX = 0.0;
    double lastSegY = 0.0;
    double finalX = 0.0;
    double finalY = 0.0;
    bool inPocket = false;
    int pocketIndex = -1;
    float pocketCenterX = 0.0f;
    float pocketCenterY = 0.0f;
    std::vector<TrajectorySegment> segments;
    bool isHit = false;
    int hitByEntityId = -1;
    float hitSpeed = 0.0f;
    int cushionBounces = 0;
    double prevVx = 0.0;
    double prevVy = 0.0;
};

bool CatEngine_CanSimulateNative() {
    // In-Engine native stepping calls launchStriker and step on the live TableLogic,
    // which allocates Scudo heap chunks that cannot be cleanly undone by raw memory rollback,
    // causing 'Scudo ERROR: invalid chunk state' crashes on Android 11-15.
    // Permanently disabled: Snake Engine CCD (simulateStrikerTrajectoryV2) provides 99.9%
    // accurate headless trajectory simulation with zero heap corruption.
    return false;
}

AimSimulationResult CatEngine_SimulateInEngineNative(double startX, double startY, double shotAngle, double forceVal, int maxSimSteps) {
    if (!CatEngine_CanSimulateNative()) {
        return AimSimulationResult();
    }

    void* tc = g_activeTableController.load();
    if (!tc) return AimSimulationResult();
    uintptr_t tcAddr = reinterpret_cast<uintptr_t>(tc);
    uintptr_t tableLogicAddr = 0;
    if (!safeReadPtr(tcAddr + g_offsets.tcTableLogic, &tableLogicAddr) || !tableLogicAddr) return AimSimulationResult();

    static std::mutex s_nativeStepMutex;
    std::lock_guard<std::mutex> lock(s_nativeStepMutex);

    // Re-check physics active under lock
    uint8_t liveActive = 0;
    if (safeReadBytes(tableLogicAddr + 0x1e0, &liveActive, 1) && liveActive != 0) {
        return AimSimulationResult();
    }

    // 1. Locate StrikerLogic
    int targetPlayer = s_activeTurnPlayerId.load();
    if (targetPlayer < 0) targetPlayer = s_localBaselineId.load();
    if (targetPlayer < 0) targetPlayer = 0;

    typedef void* (*TableLogicGetStrikerFn)(void* tableLogic, int playerId);
    auto getStrikerFn = reinterpret_cast<TableLogicGetStrikerFn>(g_libgameBase + g_offsets.tableLogicGetStriker);
    void* slPtr = nullptr;
    if (getStrikerFn) {
        slPtr = getStrikerFn(reinterpret_cast<void*>(tableLogicAddr), targetPlayer);
        if (!slPtr && targetPlayer != 0) slPtr = getStrikerFn(reinterpret_cast<void*>(tableLogicAddr), 0);
    }
    if (!slPtr) {
        uintptr_t strikerRoot = 0;
        if (safeReadPtr(tableLogicAddr + g_offsets.tlStrikerMap, &strikerRoot) && strikerRoot) {
            slPtr = reinterpret_cast<void*>(strikerRoot + 0x30);
        }
    }
    if (!slPtr) return AimSimulationResult();
    uintptr_t slAddr = reinterpret_cast<uintptr_t>(slPtr);

    // 2. Snapshot StrikerLogic (512 bytes)
    uint8_t strikerSnapshot[0x200];
    memcpy(strikerSnapshot, reinterpret_cast<void*>(slAddr), 0x200);

    // 3. Snapshot TableLogic physics fields (128 bytes: 0x1d0 - 0x250)
    uint8_t tlSnapshot[0x80];
    memcpy(tlSnapshot, reinterpret_cast<void*>(tableLogicAddr + g_offsets.tlSnapshot), 0x80);

    // 4. Collect and snapshot all active pucks from TableLogic RB-tree (+0x180 / +0x188)
    std::vector<NativePuckSnapshotItem> pucks;
    pucks.reserve(20);

    uintptr_t endNode = tableLogicAddr + g_offsets.tlPuckMap;
    uintptr_t beginNode = 0;
    safeReadPtr(tableLogicAddr + g_offsets.tlPuckBegin, &beginNode);
    uintptr_t curr = beginNode;
    int stepCount = 0;

    while (curr != 0 && curr != endNode && stepCount < 50) {
        stepCount++;
        uint32_t rawKey = 0;
        if (safeReadInsn(curr + 0x20, &rawKey)) {
            int puckId = static_cast<int>(rawKey);
            uintptr_t pAddr = curr + 0x30;
            uintptr_t xBits = 0, yBits = 0;
            if (safeReadPtr(pAddr + 0x10, &xBits) && safeReadPtr(pAddr + 0x18, &yBits)) {
                double liveX = *reinterpret_cast<double*>(&xBits);
                double liveY = *reinterpret_cast<double*>(&yBits);
                bool isPotted = std::isnan(liveX) || std::isnan(liveY) ||
                                std::abs(liveX) > 36.5 || std::abs(liveY) > 36.5 ||
                                isInsidePocketPos(liveX, liveY);

                NativePuckSnapshotItem item;
                item.id = puckId;
                item.pAddr = pAddr;
                item.puckType = determinePuckType(puckId, pAddr, liveX, liveY);
                double pr = 1.95;
                if (safeReadDouble(pAddr + g_offsets.bodyRadius, &pr) && !std::isnan(pr) && pr > 0.5 && pr < 4.0) {
                    item.radius = pr;
                } else {
                    item.radius = GraphicsEngine_GetPuckRadius();
                }
                item.startX = liveX;
                item.startY = liveY;
                item.lastSegX = liveX;
                item.lastSegY = liveY;
                item.finalX = liveX;
                item.finalY = liveY;
                item.inPocket = isPotted;
                item.prevVx = 0.0;
                item.prevVy = 0.0;
                memcpy(item.rawMemory, reinterpret_cast<void*>(pAddr), 0x200);
                pucks.push_back(item);
            }
        }

        // Advance to in-order successor (libgame.so 0x1B2B47C)
        uintptr_t right = 0;
        safeReadPtr(curr + 0x08, &right);
        if (right != 0 && right != endNode) {
            curr = right;
            uintptr_t left = 0;
            int innerLeft = 0;
            while (safeReadPtr(curr + 0x00, &left) && left != 0 && left != endNode && ++innerLeft < 32) {
                curr = left;
            }
        } else {
            uintptr_t parent = 0;
            int innerParent = 0;
            while (safeReadPtr(curr + 0x10, &parent) && parent != 0 && parent != endNode && ++innerParent < 32) {
                uintptr_t parentRight = 0;
                safeReadPtr(parent + 0x08, &parentRight);
                if (curr != parentRight) {
                    break;
                }
                curr = parent;
            }
            safeReadPtr(curr + 0x10, &curr);
        }
    }

    // RAII Restorer Guard: Guarantees 100% atomic rollback of all memory states
    struct NativeStateRestorer {
        uintptr_t slAddr;
        uint8_t* strikerSnapshot;
        std::vector<NativePuckSnapshotItem>* pucks;
        uintptr_t tableLogicAddr;
        uint8_t* tlSnapshot;
        bool done = false;

        void rollback() {
            if (done) return;
            done = true;
            memcpy(reinterpret_cast<void*>(slAddr), strikerSnapshot, 0x200);
            for (const auto& p : *pucks) {
                memcpy(reinterpret_cast<void*>(p.pAddr), p.rawMemory, 0x200);
            }
            memcpy(reinterpret_cast<void*>(tableLogicAddr + g_offsets.tlSnapshot), tlSnapshot, 0x80);
        }

        ~NativeStateRestorer() {
            rollback();
        }
    } restorer{slAddr, strikerSnapshot, &pucks, tableLogicAddr, tlSnapshot};

    // 5. Inject Aim Position and Velocity
    *reinterpret_cast<double*>(slAddr + 0x10) = startX;
    *reinterpret_cast<double*>(slAddr + 0x18) = startY;
    *reinterpret_cast<double*>(slAddr + 0x160) = startX;
    *reinterpret_cast<double*>(slAddr + 0x168) = startY;

    typedef void (*TableLogicLaunchStrikerFn)(void* tableLogic, int playerId, double force, double angle);
    auto launchStrikerFn = reinterpret_cast<TableLogicLaunchStrikerFn>(g_libgameBase + g_offsets.launchStriker);
    if (launchStrikerFn) {
        launchStrikerFn(reinterpret_cast<void*>(tableLogicAddr), targetPlayer, forceVal, shotAngle);
    }

    // 6. Execute Headless Stepper Loop
    typedef int (*TableLogicStepFn)(void* tableLogic, double dt);
    auto stepFn = reinterpret_cast<TableLogicStepFn>(g_libgameBase + g_offsets.tableLogicStep);
    if (!stepFn) return AimSimulationResult();

    // Read authentic striker radius
    double stRadius = 2.65;
    double sr = 0.0;
    if (safeReadDouble(slAddr + g_offsets.bodyRadius, &sr) && !std::isnan(sr) && sr > 1.0 && sr < 5.0) {
        stRadius = sr;
    } else {
        stRadius = GraphicsEngine_GetStrikerRadius();
    }

    // Dynamic pocket centers from calibrated inset
    float pkInset = GraphicsEngine_GetPocketInset();
    if (pkInset <= 10.0f) pkInset = 33.48f;
    const float pCenters[4][2] = {
        {  pkInset,  pkInset },  // 0: Top-Right (+X, +Y)
        { -pkInset,  pkInset },  // 1: Top-Left (-X, +Y)
        {  pkInset, -pkInset },  // 2: Bottom-Right (+X, -Y)
        { -pkInset, -pkInset }   // 3: Bottom-Left (-X, -Y)
    };

    AimSimulationResult sim;
    sim.finalStrikerX = static_cast<float>(startX);
    sim.finalStrikerY = static_cast<float>(startY);

    double dt = 0.016; // 60 Hz authentic fixed step matching Carrom Pool
    int totalSteps = (maxSimSteps > 0 && maxSimSteps <= 800) ? maxSimSteps : 300; // ~4.8 seconds max
    double lastStX = startX;
    double lastStY = startY;
    double prevStVx = *reinterpret_cast<double*>(slAddr + 0x50);
    double prevStVy = *reinterpret_cast<double*>(slAddr + 0x58);
    int step = 0;

    for (step = 0; step < totalSteps; ++step) {
        int stillMoving = stepFn(reinterpret_cast<void*>(tableLogicAddr), dt);

        double curStX = *reinterpret_cast<double*>(slAddr + 0x10);
        double curStY = *reinterpret_cast<double*>(slAddr + 0x18);
        double curStVx = *reinterpret_cast<double*>(slAddr + 0x50);
        double curStVy = *reinterpret_cast<double*>(slAddr + 0x58);
        double curStSpeed = std::hypot(curStVx, curStVy);
        double prevStSpeed = std::hypot(prevStVx, prevStVy);
        int32_t stState = *reinterpret_cast<int32_t*>(slAddr + 0xa4);
        uint8_t stInPocket = *reinterpret_cast<uint8_t*>(slAddr + 0xa8);
        uintptr_t stPocketPtr = 0;
        safeReadPtr(slAddr + 0x38, &stPocketPtr);

        // A. Striker Pocket Check
        bool stPotted = (stPocketPtr != 0 || stState != 0 || stInPocket != 0 || isInsidePocketPos(curStX, curStY));
        if (stPotted && !sim.strikerInPocket) {
            sim.strikerInPocket = true;
            int pkIdx = 0;
            if (curStX >= 0.0 && curStY >= 0.0) pkIdx = 0;
            else if (curStX < 0.0 && curStY >= 0.0) pkIdx = 1;
            else if (curStX >= 0.0 && curStY < 0.0) pkIdx = 2;
            else pkIdx = 3;
            sim.strikerPocketIndex = pkIdx;

            TrajectorySegment seg;
            seg.startX = static_cast<float>(lastStX);
            seg.startY = static_cast<float>(lastStY);
            seg.endX   = pCenters[pkIdx][0];
            seg.endY   = pCenters[pkIdx][1];
            seg.isCushionBounce = (sim.segments.size() > 0);
            seg.isPuckHit = false;
            sim.segments.push_back(seg);
            lastStX = pCenters[pkIdx][0];
            lastStY = pCenters[pkIdx][1];
            sim.finalStrikerX = pCenters[pkIdx][0];
            sim.finalStrikerY = pCenters[pkIdx][1];
        }

        // B. Striker vs Puck Collision Check
        if (!sim.strikerInPocket) {
            for (auto& p : pucks) {
                if (p.inPocket) continue;
                double curPkX = *reinterpret_cast<double*>(p.pAddr + 0x10);
                double curPkY = *reinterpret_cast<double*>(p.pAddr + 0x18);
                double curPkVx = *reinterpret_cast<double*>(p.pAddr + 0x50);
                double curPkVy = *reinterpret_cast<double*>(p.pAddr + 0x58);
                double curPkSpeed = std::hypot(curPkVx, curPkVy);
                double prevPkSpeed = std::hypot(p.prevVx, p.prevVy);

                double dStPk = std::hypot(curStX - curPkX, curStY - curPkY);
                double rSum = stRadius + p.radius;

                bool hitDetected = false;
                if (dStPk <= rSum + 0.25) {
                    if (prevPkSpeed <= 0.3 && curPkSpeed > 0.5) {
                        hitDetected = true;
                    } else if (curPkSpeed - prevPkSpeed > 2.0) {
                        hitDetected = true;
                    }
                }

                if (hitDetected) {
                    if (!sim.hasFirstContact) {
                        sim.hasFirstContact = true;
                        sim.firstContactPuckId = p.id;
                        sim.hitPuck = true;
                        sim.firstImpactStrikerX = static_cast<float>(curStX);
                        sim.firstImpactStrikerY = static_cast<float>(curStY);
                        sim.firstImpactPuckX    = static_cast<float>(p.startX);
                        sim.firstImpactPuckY    = static_cast<float>(p.startY);

                        // Split striker segment ending at exact contact point!
                        if (std::hypot(curStX - lastStX, curStY - lastStY) > 0.05) {
                            TrajectorySegment seg;
                            seg.startX = static_cast<float>(lastStX);
                            seg.startY = static_cast<float>(lastStY);
                            seg.endX   = static_cast<float>(curStX);
                            seg.endY   = static_cast<float>(curStY);
                            seg.isCushionBounce = false;
                            seg.isPuckHit = true;
                            sim.segments.push_back(seg);
                            lastStX = curStX;
                            lastStY = curStY;
                        } else if (!sim.segments.empty()) {
                            sim.segments.back().isPuckHit = true;
                        }
                    }

                    if (!p.isHit) {
                        p.isHit = true;
                        p.hitByEntityId = 0; // Striker
                        p.hitSpeed = static_cast<float>(curPkSpeed);
                        p.lastSegX = p.startX;
                        p.lastSegY = p.startY;
                    }
                }
            }
        }

        // C. Cushion Rebound Detection for Striker
        if (!sim.strikerInPocket) {
            bool cushionBounce = false;
            if ((prevStVx * curStVx < -2.0) && std::abs(curStX) > 28.0) {
                cushionBounce = true;
            } else if ((prevStVy * curStVy < -2.0) && std::abs(curStY) > 28.0) {
                cushionBounce = true;
            } else if (std::abs(curStX) > 28.5 || std::abs(curStY) > 28.5) {
                if (prevStSpeed > 10.0 && curStSpeed > 5.0) {
                    double dot = (prevStVx * curStVx + prevStVy * curStVy) / (prevStSpeed * curStSpeed);
                    if (dot < 0.70) cushionBounce = true;
                }
            }

            if (cushionBounce) {
                TrajectorySegment seg;
                seg.startX = static_cast<float>(lastStX);
                seg.startY = static_cast<float>(lastStY);
                seg.endX   = static_cast<float>(curStX);
                seg.endY   = static_cast<float>(curStY);
                seg.isCushionBounce = true;
                seg.isPuckHit = false;
                sim.segments.push_back(seg);
                lastStX = curStX;
                lastStY = curStY;
                sim.strikerCushionBounces++;
            } else {
                double distFromLastSeg = std::hypot(curStX - lastStX, curStY - lastStY);
                if (distFromLastSeg >= 0.85) {
                    TrajectorySegment seg;
                    seg.startX = static_cast<float>(lastStX);
                    seg.startY = static_cast<float>(lastStY);
                    seg.endX   = static_cast<float>(curStX);
                    seg.endY   = static_cast<float>(curStY);
                    seg.isCushionBounce = false;
                    seg.isPuckHit = false;
                    sim.segments.push_back(seg);
                    lastStX = curStX;
                    lastStY = curStY;
                }
            }
            sim.finalStrikerX = static_cast<float>(curStX);
            sim.finalStrikerY = static_cast<float>(curStY);
        }

        prevStVx = curStVx;
        prevStVy = curStVy;

        // D. Puck Movement & Cushion & Pocket
        for (auto& p : pucks) {
            if (p.inPocket) continue;
            double curPkX = *reinterpret_cast<double*>(p.pAddr + 0x10);
            double curPkY = *reinterpret_cast<double*>(p.pAddr + 0x18);
            double curPkVx = *reinterpret_cast<double*>(p.pAddr + 0x50);
            double curPkVy = *reinterpret_cast<double*>(p.pAddr + 0x58);
            double curPkSpeed = std::hypot(curPkVx, curPkVy);
            uintptr_t pkPocketPtr = 0;
            safeReadPtr(p.pAddr + 0x38, &pkPocketPtr);
            int32_t pkState = *reinterpret_cast<int32_t*>(p.pAddr + 0xa4);
            uint8_t pkInPocket = *reinterpret_cast<uint8_t*>(p.pAddr + 0xa8);
            bool isPotted = (pkPocketPtr != 0 || pkState == 2 || pkInPocket != 0 || isInsidePocketPos(curPkX, curPkY));
            if (isPotted) {
                p.inPocket = true;
                int pkIdx = 0;
                if (curPkX >= 0.0 && curPkY >= 0.0) pkIdx = 0;
                else if (curPkX < 0.0 && curPkY >= 0.0) pkIdx = 1;
                else if (curPkX >= 0.0 && curPkY < 0.0) pkIdx = 2;
                else pkIdx = 3;
                p.pocketIndex = pkIdx;
                p.pocketCenterX = pCenters[pkIdx][0];
                p.pocketCenterY = pCenters[pkIdx][1];
                p.finalX = p.pocketCenterX;
                p.finalY = p.pocketCenterY;

                TrajectorySegment seg;
                seg.startX = static_cast<float>(p.lastSegX);
                seg.startY = static_cast<float>(p.lastSegY);
                seg.endX   = p.finalX;
                seg.endY   = p.finalY;
                seg.isCushionBounce = (p.segments.size() > 0);
                seg.isPuckHit = false;
                p.segments.push_back(seg);
                p.lastSegX = p.finalX;
                p.lastSegY = p.finalY;
                continue;
            }

            if (p.isHit || curPkSpeed > 0.20) {
                if (!p.isHit) {
                    p.isHit = true;
                    p.lastSegX = p.startX;
                    p.lastSegY = p.startY;
                }

                // Check Cushion Bounce
                bool pkBounce = false;
                if ((p.prevVx * curPkVx < -2.0) && std::abs(curPkX) > 28.0) {
                    pkBounce = true;
                } else if ((p.prevVy * curPkVy < -2.0) && std::abs(curPkY) > 28.0) {
                    pkBounce = true;
                } else if (std::abs(curPkX) > 28.5 || std::abs(curPkY) > 28.5) {
                    double prevSpd = std::hypot(p.prevVx, p.prevVy);
                    if (prevSpd > 10.0 && curPkSpeed > 5.0) {
                        double dot = (p.prevVx * curPkVx + p.prevVy * curPkVy) / (prevSpd * curPkSpeed);
                        if (dot < 0.70) pkBounce = true;
                    }
                }

                if (pkBounce) {
                    TrajectorySegment seg;
                    seg.startX = static_cast<float>(p.lastSegX);
                    seg.startY = static_cast<float>(p.lastSegY);
                    seg.endX   = static_cast<float>(curPkX);
                    seg.endY   = static_cast<float>(curPkY);
                    seg.isCushionBounce = true;
                    seg.isPuckHit = false;
                    p.segments.push_back(seg);
                    p.lastSegX = curPkX;
                    p.lastSegY = curPkY;
                    p.cushionBounces++;
                } else {
                    double pkDist = std::hypot(curPkX - p.lastSegX, curPkY - p.lastSegY);
                    if (pkDist >= 0.85) {
                        TrajectorySegment seg;
                        seg.startX = static_cast<float>(p.lastSegX);
                        seg.startY = static_cast<float>(p.lastSegY);
                        seg.endX   = static_cast<float>(curPkX);
                        seg.endY   = static_cast<float>(curPkY);
                        seg.isCushionBounce = false;
                        seg.isPuckHit = false;
                        p.segments.push_back(seg);
                        p.lastSegX = curPkX;
                        p.lastSegY = curPkY;
                    }
                }
                p.finalX = curPkX;
                p.finalY = curPkY;
            }

            p.prevVx = curPkVx;
            p.prevVy = curPkVy;
        }

        if (!stillMoving) break;
    }

    // 7. Finalize Striker Trajectory
    if (!sim.strikerInPocket) {
        double remStDist = std::hypot(sim.finalStrikerX - lastStX, sim.finalStrikerY - lastStY);
        if (remStDist > 0.05) {
            TrajectorySegment seg;
            seg.startX = static_cast<float>(lastStX);
            seg.startY = static_cast<float>(lastStY);
            seg.endX   = sim.finalStrikerX;
            seg.endY   = sim.finalStrikerY;
            seg.isCushionBounce = false;
            seg.isPuckHit = false;
            sim.segments.push_back(seg);
        }
    }
    if (sim.segments.empty()) {
        TrajectorySegment seg;
        seg.startX = static_cast<float>(startX);
        seg.startY = static_cast<float>(startY);
        seg.endX   = sim.finalStrikerX;
        seg.endY   = sim.finalStrikerY;
        seg.isCushionBounce = false;
        seg.isPuckHit = false;
        sim.segments.push_back(seg);
    }

    // 8. Finalize Puck Trajectories
    for (auto& p : pucks) {
        double totalMovement = std::hypot(p.finalX - p.startX, p.finalY - p.startY);
        if (p.isHit || p.inPocket || totalMovement > 0.15) {
            if (!p.inPocket) {
                double remPkDist = std::hypot(p.finalX - p.lastSegX, p.finalY - p.lastSegY);
                if (remPkDist > 0.05) {
                    TrajectorySegment seg;
                    seg.startX = static_cast<float>(p.lastSegX);
                    seg.startY = static_cast<float>(p.lastSegY);
                    seg.endX   = static_cast<float>(p.finalX);
                    seg.endY   = static_cast<float>(p.finalY);
                    seg.isCushionBounce = false;
                    seg.isPuckHit = false;
                    p.segments.push_back(seg);
                }
            }
            if (p.segments.empty()) {
                TrajectorySegment seg;
                seg.startX = static_cast<float>(p.startX);
                seg.startY = static_cast<float>(p.startY);
                seg.endX   = static_cast<float>(p.finalX);
                seg.endY   = static_cast<float>(p.finalY);
                seg.isCushionBounce = false;
                seg.isPuckHit = true;
                p.segments.push_back(seg);
            }

            PuckTrajectoryInfo pi;
            pi.puckIndex = p.id;
            pi.puckType = p.puckType;
            pi.startX = static_cast<float>(p.startX);
            pi.startY = static_cast<float>(p.startY);
            pi.finalX = static_cast<float>(p.finalX);
            pi.finalY = static_cast<float>(p.finalY);
            pi.inPocket = p.inPocket;
            pi.pocketIndex = p.pocketIndex;
            if (p.pocketIndex >= 0 && p.pocketIndex < 4) {
                pi.pocketCenterX = pCenters[p.pocketIndex][0];
                pi.pocketCenterY = pCenters[p.pocketIndex][1];
            }
            pi.isHit = p.isHit;
            pi.hitByEntityId = p.hitByEntityId;
            pi.hitSpeed = p.hitSpeed;
            pi.cushionBounces = p.cushionBounces;
            pi.segments = p.segments;

            sim.puckTrajectories.push_back(pi);
        }
    }

    if (!sim.hasFirstContact && !sim.puckTrajectories.empty()) {
        const auto& fp = sim.puckTrajectories[0];
        sim.hasFirstContact = true;
        sim.firstContactPuckId = fp.puckIndex;
        sim.hitPuck = true;
        sim.firstImpactPuckX = fp.startX;
        sim.firstImpactPuckY = fp.startY;
        sim.firstImpactStrikerX = (sim.segments.empty()) ? static_cast<float>(startX) : sim.segments.back().startX;
        sim.firstImpactStrikerY = (sim.segments.empty()) ? static_cast<float>(startY) : sim.segments.back().startY;
    }

    return sim;
}

// Hook: TableLogic moveStrikerToPositionSmooth (libgame.so + 0x1B2B748) -> STRIKER SLIDING
typedef void (*tl_moveStrikerSmooth_fn)(void* tableLogic, int baselineId, bool remote, double x, double y);
static tl_moveStrikerSmooth_fn orig_tl_moveStrikerSmooth = nullptr;

static void my_tl_moveStrikerSmooth(void* tableLogic, int baselineId, bool remote, double x, double y) {
    if (orig_tl_moveStrikerSmooth) {
        orig_tl_moveStrikerSmooth(tableLogic, baselineId, remote, x, y);
    }
    // remote == false: Local user or friend dragging the slider on this device
    if (!remote && !std::isnan(x)) {
        bool isOpponent = isOpponentBaseline(y);
        if (isOpponent) {
            // Friend moving striker at top baseline
            s_isOpponentTurn.store(true);
            s_activeTurnPlayerId.store(baselineId);
            if (!s_passAndPlayBothSides.load()) {
                // Strictly hide guidelines during friend's turn ("my chance not friends chance")
                s_isLocalTurnReady.store(false);
                GraphicsEngine_ResetPersistedShot();
                GraphicsEngine_UpdateAim(0.0, 0.0, 0.0, 0.0, false);
                return;
            }
            s_isLocalTurnReady.store(true);
        } else {
            // Local user moving striker at bottom baseline
            s_localBaselineId.store(baselineId);
            s_activeTurnPlayerId.store(baselineId);
            s_lastPlayerIndex.store(baselineId);
            s_isOpponentTurn.store(false);
            s_isLocalTurnReady.store(true);
        }

        g_matchActive.store(true);
        setGraphicsMatchActive(true);
        GraphicsEngine_OnStrikerReturnedToBaseline();
        GraphicsEngine_SetReviewFreeze(false);

        double lockedY = y;
        g_fixedBaselineY.store(lockedY);

        double minX = g_baselineMinX.load();
        double maxX = g_baselineMaxX.load();
        if (minX >= maxX || minX < -35.0 || maxX > 35.0) { minX = -23.85; maxX = 23.85; }

        double clampedX = std::clamp(x, minX, maxX);

        g_liveStrikerX.store(clampedX);
        g_liveStrikerY.store(lockedY);
        onStrikerPositionUpdated(clampedX, lockedY);

        int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        s_lastUserSliderTouchMs.store(nowMs);

        double preservedAngle = g_lastLoggedAngle.load();
        if (std::abs(preservedAngle) > 100.0 || std::isnan(preservedAngle)) {
            preservedAngle = (lockedY > 0.0) ? (-M_PI / 2.0) : (M_PI / 2.0);
        }
        double preservedForce = g_lastLoggedForce.load();
        if (preservedForce < 0.05 || std::isnan(preservedForce)) preservedForce = 1.0;
        GraphicsEngine_UpdateAim(clampedX, lockedY, preservedAngle, preservedForce, false);
    } else if (remote) {
        // Remote opponent is moving striker -> NOT our turn!
        s_isLocalTurnReady.store(false);
    }
}

extern "C" bool CatEngine_MoveStriker(double baselineX, double baselineY) {
    double minX = g_baselineMinX.load();
    double maxX = g_baselineMaxX.load();
    if (minX >= maxX || minX < -35.0 || maxX > 35.0) { minX = -23.85; maxX = 23.85; }
    double clampedX = std::clamp(baselineX, minX, maxX);

    double lockedY = g_fixedBaselineY.load();
    if (std::abs(lockedY) < 15.0) lockedY = -25.35;

    g_liveStrikerX.store(clampedX);
    g_liveStrikerY.store(lockedY);
    onStrikerPositionUpdated(clampedX, lockedY);

    void* activeTc = g_activeTableController.load();
    if (activeTc && isTableControllerAlive()) {
        int bId = s_localBaselineId.load();
        if (bId < 0) bId = 0;
        uintptr_t tableLogicAddr = 0;
        if (safeReadPtr(reinterpret_cast<uintptr_t>(activeTc) + 0x3f0, &tableLogicAddr) && tableLogicAddr) {
            if (orig_tl_moveStrikerSmooth) {
                orig_tl_moveStrikerSmooth(reinterpret_cast<void*>(tableLogicAddr), bId, false, clampedX, lockedY);
            }
        }
        if (orig_forceStrikerPosition) {
            orig_forceStrikerPosition(activeTc, nullptr, bId, clampedX, lockedY);
        }
    }

    return true;
}

// Hook: GameplayView setupLocalStrikerBar (libgame.so + 0x1A28F98) -> Match Start / Local Turn
typedef void (*setupLocalStrikerBar_fn)(void* self, void* sel);
static setupLocalStrikerBar_fn orig_setupLocalStrikerBar = nullptr;

static void my_setupLocalStrikerBar(void* self, void* sel) {
    if (self) {
        s_capturedGameplayView = self;
    }

    if (orig_setupLocalStrikerBar) {
        orig_setupLocalStrikerBar(self, sel);
    }
    g_matchActive.store(true);
    setGraphicsMatchActive(true);
    // Note: Do NOT reset s_queenPuckId on each turn! Queen retains its identity throughout the entire match.
    int localB = s_localBaselineId.load();
    s_lastPlayerIndex.store((localB >= 0) ? localB : 0);
    g_baselineDiscovered.store(false);
    g_geometryDiscovered.store(false);
    GraphicsEngine_OnStrikerReturnedToBaseline();

    int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    s_turnTimerInitial.store(15.0f);
    s_turnTimerTotal.store(15.0f);
    s_turnTimerStartMs.store(nowMs);
    GraphicsEngine_ResetTurnScanTimer();

    if (self) {
        uintptr_t timerNode = 0;
        if (safeReadPtr(reinterpret_cast<uintptr_t>(self) + g_offsets.gvTimerNode, &timerNode) && isValidUserPointer(timerNode)) {
            s_capturedTimerNode.store(reinterpret_cast<void*>(timerNode));
        } else if (safeReadPtr(reinterpret_cast<uintptr_t>(self) + g_offsets.gvTimerNodeAlt, &timerNode) && isValidUserPointer(timerNode)) {
            s_capturedTimerNode.store(reinterpret_cast<void*>(timerNode));
        }

        void* tc = findTableControllerFromGameplayView(self);
        if (tc) {
            g_activeTableController.store(tc);
            calibrateBoardFromTableController(tc);
        }

        uintptr_t matchCtrl = 0;
        if (safeReadPtr(reinterpret_cast<uintptr_t>(self) + g_offsets.gvMatchController, &matchCtrl) && isValidUserPointer(matchCtrl)) {
            s_capturedMatchController = reinterpret_cast<void*>(matchCtrl);
            s_syntheticClosure[1] = matchCtrl;
            s_capturedMoveStrikerThis = reinterpret_cast<void*>(s_syntheticClosure);

            if (!g_activeTableController.load()) {
                void* mcTC = findTableControllerFromMatchController(s_capturedMatchController);
                if (mcTC) {
                    g_activeTableController.store(mcTC);
                    calibrateBoardFromTableController(mcTC);
                }
            }

            uintptr_t controlsLogic = 0;
            if (safeReadPtr(matchCtrl + g_offsets.matchCtrlControlsLogic, &controlsLogic) && isValidUserPointer(controlsLogic)) {
                void* candShot = reinterpret_cast<void*>(controlsLogic + g_offsets.controlsLogicShot);
                if (isValidShotDelegate(candShot)) {
                    s_capturedControlsLogic = reinterpret_cast<void*>(controlsLogic);
                    s_capturedShotDelegate = candShot;
                    s_capturedShotThis = candShot;
                    void* candAim = reinterpret_cast<void*>(controlsLogic + g_offsets.controlsLogicAim);
                    if (isValidAimDelegate(candAim)) {
                        s_capturedAimDelegate = candAim;
                        s_capturedAimThis = candAim;
                    }
                    ALOGI("[CatEngine] Proactively captured ControlsLogicLocal at setupLocalStrikerBar: ctrl=%p, aim=%p, shot=%p",
                          s_capturedControlsLogic, s_capturedAimDelegate, s_capturedShotDelegate);
                }
            }
        }
    }

    // Ensure local baseline matches discovered baseline Y
    double lockedY = g_fixedBaselineY.load();
    if (std::abs(lockedY) < 15.0) lockedY = -25.35;
    g_liveStrikerY.store(lockedY);

    MCPointDouble stPos = sampleStrikerPosition();
    double startX = (!std::isnan(stPos.x)) ? stPos.x : 0.0;
    if (!std::isnan(stPos.y) && std::abs(stPos.y) > 15.0) lockedY = stPos.y;
    g_liveStrikerX.store(startX);
    g_liveStrikerY.store(lockedY);
    onStrikerPositionUpdated(startX, lockedY);

    bool isRotated = s_isLocalTurnRotated.load();
    GraphicsEngine_SetTableRotated(isRotated);

    bool isOpponent = isOpponentBaseline(lockedY);
    if (isOpponent) {
        s_isOpponentTurn.store(true);
        s_activeTurnPlayerId.store(1);
        if (!s_passAndPlayBothSides.load()) {
            s_isLocalTurnReady.store(false);
            GraphicsEngine_ResetTurnScanTimer();
            GraphicsEngine_ResetPersistedShot();
            GraphicsEngine_UpdateAim(0.0, 0.0, 0.0, 0.0, false);
            return;
        }
        s_isLocalTurnReady.store(true);
    } else {
        s_isOpponentTurn.store(false);
        s_isLocalTurnReady.store(true);
    }

    g_matchActive.store(true);
    setGraphicsMatchActive(true);
    s_lastShotTimeMs.store(0);
    GraphicsEngine_OnStrikerReturnedToBaseline();
    double defaultAngle = (lockedY > 0.0) ? (-M_PI / 2.0) : (M_PI / 2.0);
    g_lastLoggedAngle.store(defaultAngle);
    g_lastLoggedForce.store(1.0);
    GraphicsEngine_UpdateAim(startX, lockedY, defaultAngle, 1.0, false);
    if (self) {
        updateStrikerPhysicsFromGame(self);
    }
    CatEngine_UpdateTablePucks();
    updateTablePucks();
    ALOGI("[GAME EVENT] >>> MATCH STARTED / LOCAL TURN ACTIVE! Striker pos (%.2f, %.2f) Auto-Shot READY! <<<", startX, lockedY);
}

// Hook: GameplayView setupOpponentStrikerBar (libgame.so + 0x1A1DDF4) -> Opponent Turn
typedef void (*setupOpponentStrikerBar_fn)(void* self, void* sel);
static setupOpponentStrikerBar_fn orig_setupOpponentStrikerBar = nullptr;

static void my_setupOpponentStrikerBar(void* self, void* sel) {
    s_isOpponentTurn.store(true);
    s_activeTurnPlayerId.store(1);
    s_isLocalTurnReady.store(s_passAndPlayBothSides.load());
    GraphicsEngine_ResetTurnScanTimer();
    if (!s_passAndPlayBothSides.load()) {
        GraphicsEngine_ResetPersistedShot();
    }
    GraphicsEngine_UpdateAim(0.0, 0.0, 0.0, 0.0, false);
    ALOGI("[GAME EVENT] >>> OPPONENT TURN STARTED (setupOpponentStrikerBar)! Auto-Shot paused. <<<");
    if (self) {
        s_capturedGameplayView = self;
        void* tc = findTableControllerFromGameplayView(self);
        if (tc) {
            g_activeTableController.store(tc);
        }
    }
    g_matchActive.store(true);
    setGraphicsMatchActive(true);
    if (orig_setupOpponentStrikerBar) {
        orig_setupOpponentStrikerBar(self, sel);
    }
}

// Hook: GameplayView startMatchEnding (libgame.so + 0x19DF2A8) -> Match End
typedef void (*startMatchEnding_fn)(void* self, void* sel);
static startMatchEnding_fn orig_startMatchEnding = nullptr;

static void my_startMatchEnding(void* self, void* sel) {
    CatEngine_OnMatchDestroyed();
    ALOGI("[GAME EVENT] >>> MATCH ENDED! Striker tracking PAUSED! <<<");
    if (orig_startMatchEnding) {
        orig_startMatchEnding(self, sel);
    }
}

// Passive Turn Timer Reading & High-Precision Fallback (Zero-Hook Architecture)
extern "C" float CatEngine_GetTurnRemainingSeconds() {
    // 1. Try reading live remaining seconds directly from game timer HUD node (+0x3fc)
    void* node = s_capturedTimerNode.load();
    if (!node && s_capturedGameplayView) {
        uintptr_t gvAddr = reinterpret_cast<uintptr_t>(s_capturedGameplayView);
        uintptr_t candNode = 0;
        if (safeReadPtr(gvAddr + g_offsets.gvTimerNode, &candNode) && isValidUserPointer(candNode)) {
            node = reinterpret_cast<void*>(candNode);
            s_capturedTimerNode.store(node);
        } else if (safeReadPtr(gvAddr + g_offsets.gvTimerNodeAlt, &candNode) && isValidUserPointer(candNode)) {
            node = reinterpret_cast<void*>(candNode);
            s_capturedTimerNode.store(node);
        }
    }

    if (node) {
        uintptr_t nAddr = reinterpret_cast<uintptr_t>(node);
        uint32_t remBits = 0;
        uint32_t totBits = 0;
        if (safeReadInsn(nAddr + g_offsets.timerRemaining, &remBits) && safeReadInsn(nAddr + g_offsets.timerTotal, &totBits)) {
            float rem = *reinterpret_cast<float*>(&remBits);
            float tot = *reinterpret_cast<float*>(&totBits);
            if (!std::isnan(rem) && rem > 0.1f && rem <= 30.0f && tot >= 5.0f) {
                return rem;
            }
        }
    }

    // 2. High-precision fallback using wall-clock elapsed time since local turn start
    int64_t startMs = s_turnTimerStartMs.load();
    if (startMs > 0) {
        int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        float elapsedSec = static_cast<float>(nowMs - startMs) / 1000.0f;
        float initRem = s_turnTimerInitial.load();
        float rem = initRem - elapsedSec;
        return (rem > 0.0f) ? rem : 0.0f;
    }

    // 3. Fallback using GraphicsEngine turn scan start time (standard 15s turn)
    int64_t scanStartMs = GraphicsEngine_GetTurnScanStartTimeMs();
    if (scanStartMs > 0) {
        int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        float elapsedSec = static_cast<float>(nowMs - scanStartMs) / 1000.0f;
        float rem = 15.0f - elapsedSec;
        return (rem > 0.0f) ? rem : 0.0f;
    }

    // Default when no timer known: return 15.0s (do not rush prematurely)
    return 15.0f;
}

// Robust libgame base address resolver
uintptr_t resolveLibgameBase() {
    uintptr_t phdrBase = 0;
    dl_iterate_phdr([](struct dl_phdr_info* info, size_t size, void* data) -> int {
        if (!info || !info->dlpi_name) return 0;
        const char* name = info->dlpi_name;
        if (strstr(name, "libgame") && !strstr(name, "libgamemanager")) {
            uintptr_t b = static_cast<uintptr_t>(info->dlpi_addr);
            if (b != 0) {
                uint32_t magic = 0;
                if (safeReadInsn(b, &magic) && magic == 0x464C457F) {
                    *reinterpret_cast<uintptr_t*>(data) = b;
                    return 1;
                }
            }
        }
        return 0;
    }, &phdrBase);

    if (phdrBase != 0) return phdrBase;

    std::ifstream maps("/proc/self/maps");
    if (!maps.is_open()) return 0;
    std::string line;
    while (std::getline(maps, line)) {
        if (line.find("libgame") != std::string::npos &&
            line.find("libgamemanager") == std::string::npos &&
            line.find("r-xp") != std::string::npos) {
            size_t dash = line.find('-');
            if (dash != std::string::npos) {
                uintptr_t start = std::strtoull(line.substr(0, dash).c_str(), nullptr, 16);
                size_t space1 = line.find(' ');
                size_t space2 = line.find(' ', space1 + 1);
                size_t space3 = line.find(' ', space2 + 1);
                uintptr_t offset = 0;
                if (space2 != std::string::npos && space3 != std::string::npos) {
                    offset = std::strtoull(line.substr(space2 + 1, space3 - space2 - 1).c_str(), nullptr, 16);
                }
                uintptr_t cand = start - offset;
                uint32_t magic = 0;
                if (safeReadInsn(cand, &magic) && magic == 0x464C457F) {
                    return cand;
                }
                if (safeReadInsn(start, &magic) && magic == 0x464C457F) {
                    return start;
                }
            }
        }
    }
    return 0;
}

static std::atomic<bool> s_autoShotWorkerRunning(false);

static void startAutoShotWorker() {
    if (s_autoShotWorkerRunning.exchange(true)) return;

    std::thread([]() {
        ALOGI("[CatEngine] Auto-Shot autonomous worker thread running.");
        int stationaryStreak = 0;
        std::map<int, MCPointDouble> prevPuckPos;

        static int64_t s_lastAutoLogMs = 0;

        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(35));

            // 1. Is auto shot enabled?
            if (!GraphicsEngine_IsAutoShotEnabled()) {
                stationaryStreak = 0;
                continue;
            }

            // 2. Is match active? (Fast exit: zero memory polling in menus, loading screens, or lobby)
            if (!g_matchActive.load()) {
                stationaryStreak = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            bool shouldLogStatus = (nowMs - s_lastAutoLogMs > 1500);

            // Clear any lingering shot in progress flag if previous shot finished (> 800ms)
            if (GraphicsEngine_IsShotInProgress() && (nowMs - s_lastShotTimeMs.load() > 800)) {
                MCPointDouble curSt = sampleStrikerPosition();
                if (isValidBaselineY(curSt.y)) {
                    g_liveStrikerX.store(curSt.x);
                    g_liveStrikerY.store(curSt.y);
                    onStrikerPositionUpdated(curSt.x, curSt.y);
                    GraphicsEngine_OnStrikerReturnedToBaseline();
                }
            }

            // 3. Validate table controller is alive
            if (!isTableControllerAlive()) {
                if (s_capturedGameplayView) {
                    void* tc = findTableControllerFromGameplayView(s_capturedGameplayView);
                    if (tc) {
                        g_activeTableController.store(tc);
                        calibrateBoardFromTableController(tc);
                    }
                }
                if (!isTableControllerAlive() && s_capturedMatchController) {
                    void* tc = findTableControllerFromMatchController(s_capturedMatchController);
                    if (tc) {
                        g_activeTableController.store(tc);
                        calibrateBoardFromTableController(tc);
                    }
                }
                if (!isTableControllerAlive()) {
                    if (shouldLogStatus) {
                        s_lastAutoLogMs = nowMs;
                        ALOGI("[CatEngine] [AUTO-SHOT STATUS] Waiting: TableController not active yet.");
                    }
                    stationaryStreak = 0;
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
            }

            // 4. Is it local player's turn?
            if (s_isOpponentTurn.load()) {
                if (shouldLogStatus) {
                    s_lastAutoLogMs = nowMs;
                    ALOGI("[CatEngine] [AUTO-SHOT STATUS] Waiting: Opponent turn active (setupOpponentStrikerBar).");
                }
                stationaryStreak = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
                continue;
            }

            if (!s_isLocalTurnReady.load()) {
                MCPointDouble curSt = sampleStrikerPosition();
                // Strictly local bottom baseline (Y < -15.0 and Y > -35.0). Opponent top baseline is Y > +15.0!
                if (curSt.y < -15.0 && curSt.y > -35.0) {
                    s_isLocalTurnReady.store(true);
                } else {
                    if (shouldLogStatus) {
                        s_lastAutoLogMs = nowMs;
                        ALOGI("[CatEngine] [AUTO-SHOT STATUS] Waiting: Local striker not in bottom baseline (Y=%.2f).", curSt.y);
                    }
                    stationaryStreak = 0;
                    std::this_thread::sleep_for(std::chrono::milliseconds(25));
                    continue;
                }
            }

            // 5. Is a shot currently executing or rolling on table?
            if (GraphicsEngine_IsShotExecuting() || GraphicsEngine_IsShotInProgress()) {
                stationaryStreak = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // 5b. Yield immediately if local user is manually aiming or holding the striker!
            if (GraphicsEngine_IsAiming()) {
                stationaryStreak = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // 6. Ensure shot delegates are verified and ready
            void* shotDel = getControlsLogicLocal();
            if (!shotDel || !isValidShotDelegate(shotDel)) {
                if (shouldLogStatus) {
                    s_lastAutoLogMs = nowMs;
                    ALOGI("[CatEngine] [AUTO-SHOT STATUS] Waiting: Shot delegate not valid/ready yet.");
                }
                stationaryStreak = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // 7. Check striker position - must be on local bottom baseline
            MCPointDouble curStriker = sampleStrikerPosition();
            if (curStriker.y >= -15.0 || curStriker.y <= -35.0) {
                if (shouldLogStatus) {
                    s_lastAutoLogMs = nowMs;
                    ALOGI("[CatEngine] [AUTO-SHOT STATUS] Waiting: Striker not on local bottom baseline (Y=%.2f).", curStriker.y);
                }
                stationaryStreak = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // 8. Check if pucks on the board are stationary
            std::vector<PuckStateSample> samples = sampleTablePucks();
            if (samples.empty()) {
                updateTablePucks();
                samples = sampleTablePucks();
            }
            if (samples.empty()) {
                if (shouldLogStatus) {
                    s_lastAutoLogMs = nowMs;
                    ALOGI("[AUTO-SHOT STATUS] Waiting: No active pucks detected on table.");
                }
                stationaryStreak = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(60));
                continue;
            }

            bool anyMoved = false;
            for (const auto& s : samples) {
                auto it = prevPuckPos.find(s.id);
                if (it != prevPuckPos.end()) {
                    double dist = std::hypot(s.x - it->second.x, s.y - it->second.y);
                    if (dist > 0.35) {
                        anyMoved = true;
                    }
                }
                prevPuckPos[s.id] = { s.x, s.y };
            }

            // Clean up pocketed or removed pucks
            for (auto it = prevPuckPos.begin(); it != prevPuckPos.end(); ) {
                bool exists = false;
                for (const auto& s : samples) {
                    if (s.id == it->first) { exists = true; break; }
                }
                if (!exists) {
                    it = prevPuckPos.erase(it);
                } else {
                    ++it;
                }
            }

            static int64_t s_motionStartMs = 0;
            if (anyMoved) {
                GraphicsEngine_ResetTurnScanTimer();
                if (s_motionStartMs == 0) s_motionStartMs = nowMs;
                if (nowMs - s_motionStartMs > 600) {
                    anyMoved = false;
                    s_motionStartMs = 0;
                    prevPuckPos.clear();
                    for (const auto& s : samples) {
                        prevPuckPos[s.id] = { s.x, s.y };
                    }
                }
            } else {
                s_motionStartMs = 0;
            }

            if (anyMoved) {
                if (shouldLogStatus) {
                    s_lastAutoLogMs = nowMs;
                    ALOGI("[AUTO-SHOT STATUS] Waiting: Board pucks are still in motion...");
                }
                stationaryStreak = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            stationaryStreak = 0;

            ALOGI("[AUTO-SHOT STATUS] >>> Table stable! Initiating scan... <<<");
            updateTablePucks();

            bool executed = GraphicsEngine_ScanAndExecuteBestShot();
            if (executed) {
                ALOGI("[AUTO-SHOT STATUS] Shot fired successfully! Yielding for physics simulation...");
                std::this_thread::sleep_for(std::chrono::milliseconds(600));
                prevPuckPos.clear();
                stationaryStreak = 0;
                s_motionStartMs = 0;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(80));
            }
        }
    }).detach();
}

extern "C" void startCatEngineLoop() {
    static bool started = false;
    if (!started) {
        started = true;

        ALOGI("[CatEngine] ========================================================");
        ALOGI("[CatEngine] CatEngine initialized in PID %d. Monitoring for Carrom Pool...", getpid());
        ALOGI("[CatEngine] ========================================================");

        // No global SIGSEGV interception - preserves ART signal chain!

        // Immediately hook eglSwapBuffers for standalone overlay rendering
        initGraphicsEngine();

        // Start autonomous auto-shot background worker
        startAutoShotWorker();

        // Daemon retry for graphics engine until eglSwapBuffers is hooked
        std::thread([]() {
            for (int i = 0; i < 20; i++) {
                initGraphicsEngine();
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }).detach();

        // Periodic daemon heartbeat (logs every 4 seconds so adb logcat is never silent)
        std::thread([]() {
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(4));
                ALOGI("[CatEngine] [Heartbeat] PID=%d | MatchActive=%d | AutoShot=%d",
                      getpid(), g_matchActive.load() ? 1 : 0, GraphicsEngine_IsAutoShotEnabled() ? 1 : 0);
            }
        }).detach();

        std::thread([]() {
            while (!isCarromApp()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            ALOGI("[CatEngine] >>> Carrom Pool detected in PID %d! Resolving libgame base... <<<", getpid());

            uintptr_t libgameBase = 0;
            while (libgameBase == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
                libgameBase = resolveLibgameBase();
            }

            ALOGI("[CatEngine] Verified true libgame base at 0x%lX! Installing game lifecycle & tracking hooks...", libgameBase);

            // Hook 1: GameplayView setupLocalStrikerBar (0x1A28F98 in v1491) -> MATCH START / LOCAL TURN
            uintptr_t targetSetupStrikerBar = libgameBase + g_offsets.setupLocalStrikerBar;
            uint32_t insnSetupStrikerBar = 0;
            if (safeReadInsn(targetSetupStrikerBar, &insnSetupStrikerBar)) {
                if (safeHook(reinterpret_cast<void*>(targetSetupStrikerBar), (void*)my_setupLocalStrikerBar, (void**)&orig_setupLocalStrikerBar)) {
                    ALOGI("[CatEngine] Hooked setupLocalStrikerBar (0x1A28F98) successfully! insn=0x%08X", insnSetupStrikerBar);
                } else {
                    ALOGW("[CatEngine] safeHook failed for setupLocalStrikerBar! insn=0x%08X", insnSetupStrikerBar);
                }
            }

            // Hook 2: GameplayView setupOpponentStrikerBar (0x1A1DDF4 in v1491) -> OPPONENT TURN
            uintptr_t targetSetupOpponentStrikerBar = libgameBase + g_offsets.setupOpponentStrikerBar;
            uint32_t insnSetupOpponentStrikerBar = 0;
            if (safeReadInsn(targetSetupOpponentStrikerBar, &insnSetupOpponentStrikerBar)) {
                if (safeHook(reinterpret_cast<void*>(targetSetupOpponentStrikerBar), (void*)my_setupOpponentStrikerBar, (void**)&orig_setupOpponentStrikerBar)) {
                    ALOGI("[CatEngine] Hooked setupOpponentStrikerBar (0x1A1DDF4) successfully! insn=0x%08X", insnSetupOpponentStrikerBar);
                } else {
                    ALOGW("[CatEngine] safeHook failed for setupOpponentStrikerBar! insn=0x%08X", insnSetupOpponentStrikerBar);
                }
            }

            // Hook 3: GameplayView startMatchEnding (0x19DF2A8 in v1491) -> MATCH END
            uintptr_t targetMatchEnding = libgameBase + g_offsets.startMatchEnding;
            uint32_t insnMatchEnding = 0;
            if (safeReadInsn(targetMatchEnding, &insnMatchEnding)) {
                if (safeHook(reinterpret_cast<void*>(targetMatchEnding), (void*)my_startMatchEnding, (void**)&orig_startMatchEnding)) {
                    ALOGI("[CatEngine] Hooked startMatchEnding (0x19DF2A8) successfully! insn=0x%08X", insnMatchEnding);
                } else {
                    ALOGW("[CatEngine] safeHook failed for startMatchEnding! insn=0x%08X", insnMatchEnding);
                }
            }

            // Hook 4: TableLogic moveStrikerToPositionSmooth (0x1B2B748 in v1491) -> STRIKER SLIDING
            uintptr_t targetMoveSmooth = libgameBase + g_offsets.moveStrikerSmooth;
            uint32_t insnMoveSmooth = 0;
            if (safeReadInsn(targetMoveSmooth, &insnMoveSmooth)) {
                if (safeHook(reinterpret_cast<void*>(targetMoveSmooth), (void*)my_tl_moveStrikerSmooth, (void**)&orig_tl_moveStrikerSmooth)) {
                    ALOGI("[CatEngine] Hooked TableLogic::moveStrikerToPositionSmooth (0x1B2B748) successfully! insn=0x%08X", insnMoveSmooth);
                } else {
                    ALOGW("[CatEngine] safeHook failed for moveStrikerSmooth! insn=0x%08X", insnMoveSmooth);
                }
            }

            // Hook 5: TableController forcePuckPosition:position: (0x1B3352C in v1491) -> PUCK PLACEMENT
            uintptr_t targetForcePuck = libgameBase + g_offsets.forcePuckPosition;
            uint32_t insnForcePuck = 0;
            if (safeReadInsn(targetForcePuck, &insnForcePuck)) {
                if (safeHook(reinterpret_cast<void*>(targetForcePuck), (void*)my_forcePuckPosition, (void**)&orig_forcePuckPosition)) {
                    ALOGI("[CatEngine] Hooked forcePuckPosition:position: (0x1B3352C) successfully! insn=0x%08X", insnForcePuck);
                } else {
                    ALOGW("[CatEngine] safeHook failed for forcePuckPosition! insn=0x%08X", insnForcePuck);
                }
            }

            // Hook 6: TableController forceStrikerPosition:position: (0x1B33570 in v1491)
            uintptr_t targetForceStriker = libgameBase + g_offsets.forceStrikerPosition;
            uint32_t insnForceStriker = 0;
            if (safeReadInsn(targetForceStriker, &insnForceStriker)) {
                if (safeHook(reinterpret_cast<void*>(targetForceStriker), (void*)my_forceStrikerPosition, (void**)&orig_forceStrikerPosition)) {
                    ALOGI("[CatEngine] Hooked forceStrikerPosition:position: (0x1B33570) successfully! insn=0x%08X", insnForceStriker);
                } else {
                    ALOGW("[CatEngine] safeHook failed for forceStrikerPosition! insn=0x%08X", insnForceStriker);
                }
            }

            g_libgameBase = libgameBase;

            // Hook 7: ControlsLogicLocal onAimChanged (0x19843A0 in v1491) -> AIM TOUCH/PULL
            uintptr_t targetAimControls = libgameBase + g_offsets.onAimChanged;
            uint32_t insnAimControls = 0;
            if (safeReadInsn(targetAimControls, &insnAimControls)) {
                if (safeHook(reinterpret_cast<void*>(targetAimControls), (void*)my_onAimChanged, (void**)&orig_aimChanged)) {
                    ALOGI("[CatEngine] Hooked ControlsLogicLocal onAimChanged (0x19843A0) successfully! insn=0x%08X", insnAimControls);
                } else {
                    ALOGW("[CatEngine] safeHook failed for onAimChanged! insn=0x%08X", insnAimControls);
                }
            }

            // Hook 8: ControlsLogicLocal onShotFired (0x1984568 in v1491) -> SHOT RELEASE
            uintptr_t targetShoot = libgameBase + g_offsets.onShotFired;
            uint32_t insnShoot = 0;
            if (safeReadInsn(targetShoot, &insnShoot)) {
                if (safeHook(reinterpret_cast<void*>(targetShoot), (void*)my_onShotFired, (void**)&orig_shotFired)) {
                    ALOGI("[CatEngine] Hooked ControlsLogicLocal onShotFired (0x1984568) successfully! insn=0x%08X", insnShoot);
                } else {
                    ALOGW("[CatEngine] safeHook failed for onShotFired! insn=0x%08X", insnShoot);
                }
            }



            // Initialize Graphics Engine (Hooks eglSwapBuffers for standalone overlay rendering)
            initGraphicsEngine();
        }).detach();
    }
}
