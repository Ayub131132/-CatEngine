#ifndef GRAPHICS_ENGINE_H
#define GRAPHICS_ENGINE_H

#include <cstdint>

#ifdef __cplusplus
#include <vector>

struct TrajectorySegment {
    float startX, startY;
    float endX, endY;
    bool isCushionBounce;
    bool isPuckHit;
};

struct PuckTrajectoryInfo {
    int puckIndex = -1;
    int puckType = 1;
    float startX = 0.0f, startY = 0.0f;
    float finalX = 0.0f, finalY = 0.0f;
    bool inPocket = false;
    int pocketIndex = -1;
    float pocketCenterX = 0.0f, pocketCenterY = 0.0f;
    std::vector<TrajectorySegment> segments;
    bool hasDeflection = false;
    TrajectorySegment deflectSegment;
    bool isHit = false;
    int hitByEntityId = -1;
    float hitSpeed = 0.0f;
    int cushionBounces = 0;
};

struct AimSimulationResult {
    std::vector<TrajectorySegment> segments;
    float finalStrikerX = 0.0f;
    float finalStrikerY = 0.0f;
    bool hasFirstContact = false;
    float firstImpactStrikerX = 0.0f;
    float firstImpactStrikerY = 0.0f;
    float firstImpactPuckX = 0.0f;
    float firstImpactPuckY = 0.0f;
    int firstContactPuckId = -1;
    bool hitPuck = false;
    bool strikerInPocket = false;
    int strikerPocketIndex = -1;
    int strikerCushionBounces = 0;
    std::vector<PuckTrajectoryInfo> puckTrajectories;
};

AimSimulationResult CatEngine_SimulateInEngineNative(double startX, double startY, double shotAngle, double forceVal, int maxSimSteps);
bool CatEngine_CanSimulateNative();
#endif

#ifdef __cplusplus
extern "C" {
#endif

void CatEngine_UpdateTablePucks();
bool CatEngine_IsLocalTurnReady();

// Initialize the graphics engine and hook swap buffers
void initGraphicsEngine();

// Called from catEngine when the striker position changes (game board coordinates)
void onStrikerPositionUpdated(double x, double y);

// Called when match starts/ends to enable/disable rendering
void setGraphicsMatchActive(bool active);

// Update aiming line vector state (game board coordinates, angle in radians, force 0.0 - 1.0)
void GraphicsEngine_UpdateAim(double x, double y, double angle, double force, bool isAiming);
bool GraphicsEngine_IsAiming();

// Freeze and persist shot trajectory throughout the shot until striker returns to baseline
void GraphicsEngine_OnShotFired(double curX, double curY, double shotAngle, double shotForce);

// Retain previous shot trajectory lines for post-shot review until user pulls striker
void GraphicsEngine_OnStrikerReturnedToBaseline();
void GraphicsEngine_SetReviewFreeze(bool freeze);
bool GraphicsEngine_IsReviewFrozen();

// Set manual 4-side bounding box in screen pixels (Left, Right, Bottom, Top)
// When showBorder is true, renders a thick visual calibration frame around the board
void GraphicsEngine_SetBoardBounds(float left, float right, float bottom, float top, bool showBorder);

// Optional calibration override for center and scale
void GraphicsEngine_SetBoardCalibration(float centerX, float centerY, float scale);

// Set auto-detected board calibration from game engine projection
void GraphicsEngine_SetAutoCalibration(float centerX, float centerY, float scale, bool fromEngine);

// Native Affine Transform (a, b, c, d, tx, ty) extracted directly from TableNode / _tableContainer
void GraphicsEngine_SetNativeAffineTransform(double a, double b, double c, double d, double tx, double ty);
void GraphicsEngine_ResetNativeAffine();
bool GraphicsEngine_HasNativeAffine();

// Query live native affine transform from active TableController/TableNode
bool CatEngine_GetTableTransform(double* outAffine6);

// Viewport framebuffer dimensions synchronization
void CatEngine_SetViewportDimensions(int w, int h);

// Reset manual overrides to pure auto-detection mode
void GraphicsEngine_ResetToAuto();

// Check if user manual bounds are active
bool GraphicsEngine_HasManualBounds();

// Inform graphics engine of a table space to screen point sample from the game
void GraphicsEngine_OnTableSpacePoint(double tx, double ty, double sx, double sy);

// Dynamic baseline sliding range in cm
void GraphicsEngine_SetBaselineRange(float minX, float maxX);

// Object geometry configuration (cm)
void GraphicsEngine_SetObjectGeometry(float strikerRadius, float puckRadius, float pocketInset, float pocketRadius);
void GraphicsEngine_SetStrikerRadius(float radius);
float GraphicsEngine_GetStrikerRadius();
void GraphicsEngine_SetPuckRadius(float radius);
float GraphicsEngine_GetPuckRadius();
void GraphicsEngine_SetPocketGeometry(float inset, float radius);
float GraphicsEngine_GetPocketInset();
void GraphicsEngine_SetPocketRadius(float radius);
float GraphicsEngine_GetPocketRadius();
void GraphicsEngine_SetMaxTravelDistance(float maxDistance);

// Authentic Cushion Polygon (from libgame.so TableLogic + 0x198)
void GraphicsEngine_SetCushionPolygon(const double* coords, int pointCount);

// Striker physics configuration (cm/s and bonus cm/s from active striker level)
void GraphicsEngine_SetStrikerPhysics(double minPower, double maxPower, double strikerBonus);

// Authentic Coulomb friction and collision constants extracted live from TableLogic & StrikerLogic
void GraphicsEngine_SetPhysicalConstants(double tableFriction,
                                         double strikerFriction, double strikerMass, double strikerRestitution,
                                         double puckFriction, double puckMass, double puckRestitution,
                                         double cushionRestitution);

// Distinct friction configuration (Coulomb friction coefficient mu ~0.061)
void GraphicsEngine_SetFriction(double strikerFriction, double puckFriction);

// Live fine-tuning physics configuration from UI sliders or telemetry
void GraphicsEngine_SetFineTunePhysics(double strikerFriction, double puckFriction,
                                      double restitutionBody, double restitutionCushion,
                                      double strikerMass, double powerMultiplier);
void GraphicsEngine_GetFineTunePhysics(double* strikerFriction, double* puckFriction,
                                      double* restitutionBody, double* restitutionCushion,
                                      double* strikerMass, double* powerMultiplier);
void GraphicsEngine_SetAdvancedPhysics(double contactFriction, double cushionTangentFriction, double strikerBonus);
void GraphicsEngine_GetAdvancedPhysics(double* contactFriction, double* cushionTangentFriction, double* strikerBonus);
void GraphicsEngine_SetPocketPhysics(double dropMaxSpeed, double gravity);
void GraphicsEngine_GetPocketPhysics(double* dropMaxSpeed, double* gravity);
bool GraphicsEngine_IsUserManualPhysics();
void GraphicsEngine_SaveAutoCorrectedPhysics();
void GraphicsEngine_LoadAutoCorrectedPhysics();

// Guideline visual styling (thickness in cm, color theme 0-4)
void GraphicsEngine_SetGuidelineThickness(float thickness);
void GraphicsEngine_SetColorTheme(int theme);

// Dynamic Cat Engine UI controls
void GraphicsEngine_SetShowPrediction(bool enable);
bool GraphicsEngine_GetShowPrediction();
void GraphicsEngine_SetShowPocket(bool enable);
bool GraphicsEngine_GetShowPocket();
void GraphicsEngine_SetShowBorder(bool enable);
bool GraphicsEngine_GetShowBorder();
void GraphicsEngine_SetLineTransparency(float alpha);
float GraphicsEngine_GetLineTransparency();
void GraphicsEngine_SetCustomColors(uint32_t strikerColor, uint32_t myPuckColor, uint32_t opponentPuckColor);
void GraphicsEngine_SetKeepLineAfterShot(bool keep);
bool GraphicsEngine_GetKeepLineAfterShot();

// Runtime Physics Engine Mode: 0 = Snake Engine CCD (Fast & Calibrated), 1 = In-Engine Native Stepper (libgame.so exact)
void GraphicsEngine_SetPhysicsEngineMode(int mode);
int GraphicsEngine_GetPhysicsEngineMode();

// Update or clear puck positions on the board (game board coordinates, puckType: 1=White, 2=Black, 3=Red)
void GraphicsEngine_UpdatePuck(int index, double x, double y, bool active, int puckType);
void GraphicsEngine_ClearPucks();

typedef struct GraphicsPuckInfo {
    int id;
    double x;
    double y;
    bool active;
    int puckType;
} GraphicsPuckInfo;

void GraphicsEngine_SyncAllPucks(const GraphicsPuckInfo* pucks, int count);

#define MAX_PREDICTED_PUCKS 36
#define MAX_LINE_SEGMENTS 8

typedef struct TelemetryLineSegment {
    double startX;
    double startY;
    double endX;
    double endY;
    bool isCushionBounce;
} TelemetryLineSegment;

typedef struct PredictedPuckTelemetry {
    int puckId;
    int puckType; // 1 = White (User), 2 = Black (Opponent), 3 = Red (Queen)
    double startX;
    double startY;
    double predFinalX;
    double predFinalY;
    bool isHit;
    int hitByEntityId; // -1 = None, 0 = Striker, >0 = Puck ID
    char hitByName[32]; // "Striker", "Puck #X", "Static"
    double hitSpeed;
    bool predPocket;
    int pocketIndex;
    int predictedCushionBounces;
    int segmentCount;
    TelemetryLineSegment segments[MAX_LINE_SEGMENTS]; // The exact line segments drawn on screen
    double totalLineLength;                          // Total drawn line length in cm
} PredictedPuckTelemetry;

// Comprehensive telemetry prediction snapshot capturing striker line and every puck on table
typedef struct ShotPredictionTelemetry {
    // Shot release parameters
    int shotId;
    int playerIndex;
    double strikerStartX;
    double strikerStartY;
    double shotAngleRad;
    double shotAngleDeg;
    double shotForceVal;
    double strikerInitialSpeed; // v0 in cm/s

    // Striker line drawn on screen
    int strikerSegmentCount;
    TelemetryLineSegment strikerSegments[MAX_LINE_SEGMENTS]; // Striker line segments drawn on screen
    double strikerTotalLineLength;                          // Total striker line length in cm
    double strikerPredFinalX;
    double strikerPredFinalY;
    bool strikerPredPocket;
    int strikerPocketIndex;
    int strikerCushionBounces;

    // First impact event (Striker <-> Target Puck)
    bool hasFirstImpact;
    int firstContactPuckId;
    int firstContactPuckType;
    double firstImpactStrikerX;
    double firstImpactStrikerY;
    double firstImpactPuckX;
    double firstImpactPuckY;
    double firstImpactSpeed;

    // Every puck on table
    int totalPucksOnTable;
    int predictedPuckCount;
    PredictedPuckTelemetry predictedPucks[MAX_PREDICTED_PUCKS];
} ShotPredictionTelemetry;

// Capture the simulation trajectory prediction at the instant of shot release
ShotPredictionTelemetry GraphicsEngine_CaptureShotPrediction(double startX, double startY, double shotAngle, double forceVal);

typedef struct AnglePocketOutcome {
    bool willPocketPuck;     // True if at least one puck will enter a pocket
    int pocketedPuckId;      // ID of the pocketed puck (-1 if none)
    int pocketedPuckType;    // 1 = White (User), 2 = Black (Opponent), 3 = Red (Queen)
    bool strikerScratched;   // True if striker also enters a pocket (scratch foul)
    int cushionBounces;      // Cushion bounces before entering pocket
} AnglePocketOutcome;

// Evaluate if a given angle (in radians) will pocket any puck under current board physics
AnglePocketOutcome GraphicsEngine_PredictAnglePocket(double shotAngleRad, double forceVal);

// Returns true if any puck is predicted to be pocketed at the given angle, false otherwise.
// Fully integrated with the active line simulation engine (uses live striker position and table pucks).
bool GraphicsEngine_WillAnglePocket(double shotAngleRad, double forceVal);

// Scans angles and passes each angle to GraphicsEngine_WillAnglePocket.
// If it will pocket, returns true (with a placeholder to call your custom function later).
bool checkAndProcessPocketingAngle(double forceVal);

// Check if a striker shot is currently executing (shot cooldown/debounce)
bool GraphicsEngine_IsShotExecuting();
bool GraphicsEngine_IsShotInProgress();
void GraphicsEngine_ResetTurnScanTimer();
int64_t GraphicsEngine_GetTurnScanStartTimeMs();

#ifdef __cplusplus
// Pulls the striker in the slingshot angle and releases it.
// Default angle is 90 degrees (M_PI / 2.0 radians = 1.5707963267948966).
// Alternative to Android Accessibility Services (uses native in-process JNI engine input dispatch).
void GraphicsEngine_PullAndReleaseStriker(double angleRad = 1.5707963267948966, double forceVal = 1.0);

// Degree-based helper (default 90.0 degrees)
void GraphicsEngine_PullAndReleaseStrikerDegrees(double angleDeg = 90.0, double forceVal = 1.0);
#else
void GraphicsEngine_PullAndReleaseStriker(double angleRad, double forceVal);
void GraphicsEngine_PullAndReleaseStrikerDegrees(double angleDeg, double forceVal);
#endif

// Starts running checkAndProcessPocketingAngle continuously in a background thread.
void startContinuousAnglePocketSearch();

// Stops the continuous background search thread.
void stopContinuousAnglePocketSearch();

// Match active state query
bool GraphicsEngine_IsMatchActive();
bool CatEngine_IsMatchAlive();
float CatEngine_GetTurnRemainingSeconds();
void GraphicsEngine_OnMatchDestroyed();
void CatEngine_SetInEngineAim(double angleRad, double forceVal);

// Auto-strike combination search controller
void GraphicsEngine_SetAutoShotEnabled(bool enabled);
bool GraphicsEngine_IsAutoShotEnabled();
void GraphicsEngine_SetFastFirstShotMode(bool enabled);
bool GraphicsEngine_IsFastFirstShotMode();
bool GraphicsEngine_ScanAndExecuteBestShot();

#ifdef __cplusplus
}
#endif

#endif // GRAPHICS_ENGINE_H
