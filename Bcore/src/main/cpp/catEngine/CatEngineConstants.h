#ifndef CAT_ENGINE_CONSTANTS_H
#define CAT_ENGINE_CONSTANTS_H

namespace catengine {
namespace constants {

// Table Geometry (Centimeters)
constexpr float kBoardHalfSize = 37.0f;
constexpr float kTableOutOfBounds = 36.5f;
constexpr float kPocketInset = 33.48f;
constexpr float kPocketRadius = 2.65f;
constexpr float kCushionJawCutoff = 32.80f;

// Baseline Geometry (Centimeters)
constexpr float kBaselineY = 25.35f;
constexpr float kBaselineMinX = -23.85f;
constexpr float kBaselineMaxX = 23.85f;

// Disc Dimensions (Centimeters)
constexpr float kStrikerRadius = 2.65f;
constexpr float kPuckRadius = 1.95f;

// Physical Properties
constexpr float kStrikerMass = 3.00f;
constexpr float kPuckMass = 1.00f;
constexpr float kRestitutionBody = 1.00f;
constexpr float kRestitutionCushion = 0.88f;
constexpr float kFrictionStriker = 0.160f;
constexpr float kFrictionPuck = 0.108f;
constexpr float kFrictionTable = 1.00f;

// Cocos2d-x Conversion Factor (cm to internal Cocos points)
constexpr double kCocos2dDesignScale = 6.87;

} // namespace constants
} // namespace catengine

#endif // CAT_ENGINE_CONSTANTS_H

