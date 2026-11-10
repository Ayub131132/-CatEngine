with open(r'c:\Users\ayubg\AndroidStudioProjects\NewBlackbox-main1\NewBlackbox-main\Bcore\src\main\cpp\catEngine\catEngine.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# 1. Update config reading in updateStrikerPhysicsFromGame
old_config = '''    uintptr_t configPtr = 0;
    if (safeReadPtr(g_libgameBase + 0x2e8ef80, &configPtr) && configPtr) {
        uintptr_t configObj = 0;
        if (safeReadPtr(configPtr, &configObj) && configObj) {'''

new_config = '''    uintptr_t configObj = 0;
    if (safeReadPtr(g_libgameBase + 0x2e8ef80, &configObj) && configObj) {'''

assert old_config in content, 'old_config not found'
content = content.replace(old_config, new_config, 1)

# And remove one closing brace for the config
old_config_end = '''            if (call_getExtraMaxPower) {
                double liveBonus = call_getExtraMaxPower(reinterpret_cast<void*>(configObj), strikerLevel);
                if (!std::isnan(liveBonus) && liveBonus >= 0.0 && liveBonus < 300.0) {
                    bonus = liveBonus;
                }
            }
        }
    }'''

new_config_end = '''            if (call_getExtraMaxPower) {
                double liveBonus = call_getExtraMaxPower(reinterpret_cast<void*>(configObj), strikerLevel);
                if (!std::isnan(liveBonus) && liveBonus >= 0.0 && liveBonus < 300.0) {
                    bonus = liveBonus;
                }
            }
    }'''

assert old_config_end in content, 'old_config_end not found'
content = content.replace(old_config_end, new_config_end, 1)

# 2. Update calibrateBoardFromTableController to remove GraphicsEngine_HasManualBounds() check
old_calib = '''static void calibrateBoardFromTableController(void* tc) {
    if (!tc) return;
    updateBaselineFromTableLogic(tc);
    updateGeometryFromTableLogic(tc);
    updateStrikerPhysicsFromGame(tc);

    // If the user has manually adjusted the board, manual calibration has absolute priority!
    if (GraphicsEngine_HasManualBounds()) return;

    if (!call_tableSpaceToPoint) return;

    // Only run engine calibration once per table controller to prevent repeated re-calibrations on tap
    static void* s_lastCalibratedTC = nullptr;
    if (s_lastCalibratedTC == tc) return;
    s_lastCalibratedTC = tc;

    CGPointDouble ptCenter = call_tableSpaceToPoint(tc, nullptr, 0.0, 0.0);
    CGPointDouble ptRight10 = call_tableSpaceToPoint(tc, nullptr, 10.0, 0.0);

    float cx = static_cast<float>(ptCenter.x);
    float cy = static_cast<float>(ptCenter.y);
    float scale = static_cast<float>(std::abs(ptRight10.x - ptCenter.x) / 10.0);

    // Sanity check: must have a realistic scale (> 5.0 px/cm) and valid center
    if (scale > 5.0f && cx > 100.0f) {
        GraphicsEngine_SetAutoCalibration(cx, cy, scale, true);
        ALOGI("[CatEngine] AUTO-DETECTED BOARD FROM ENGINE! Center: (%.2f, %.2f), Scale: %.3f px/cm", cx, cy, scale);
    }
}'''

new_calib = '''static void calibrateBoardFromTableController(void* tc) {
    if (!tc) return;
    updateBaselineFromTableLogic(tc);
    updateGeometryFromTableLogic(tc);
    updateStrikerPhysicsFromGame(tc);

    if (!call_tableSpaceToPoint) return;

    CGPointDouble ptCenter = call_tableSpaceToPoint(tc, nullptr, 0.0, 0.0);
    CGPointDouble ptRight10 = call_tableSpaceToPoint(tc, nullptr, 10.0, 0.0);

    float cx = static_cast<float>(ptCenter.x);
    float cy = static_cast<float>(ptCenter.y);
    float scale = static_cast<float>(std::abs(ptRight10.x - ptCenter.x) / 10.0);

    // Sanity check: must have a realistic scale (> 5.0 px/cm) and valid center
    if (scale > 5.0f && cx > 100.0f) {
        GraphicsEngine_SetAutoCalibration(cx, cy, scale, true);
        static float s_lastLoggedScale = 0.0f;
        if (std::abs(scale - s_lastLoggedScale) > 0.1f) {
            s_lastLoggedScale = scale;
            ALOGI("[CatEngine] AUTO-DETECTED BOARD FROM ENGINE! Center: (%.2f, %.2f), Scale: %.3f px/cm", cx, cy, scale);
        }
    }
}'''

assert old_calib in content, 'old_calib not found'
content = content.replace(old_calib, new_calib, 1)

# 3. In my_setupAimControls, extract TableController from closure if not yet active
old_aim = '''static void my_setupAimControls(void* this_ptr, const int* playerIndex, const double* angle, const double* force) {
    if (this_ptr && (!playerIndex || *playerIndex == 0)) {
        s_capturedAimClosure = this_ptr;
    }'''

new_aim = '''static void my_setupAimControls(void* this_ptr, const int* playerIndex, const double* angle, const double* force) {
    if (this_ptr && (!playerIndex || *playerIndex == 0)) {
        s_capturedAimClosure = this_ptr;
        if (!g_activeTableController) {
            uintptr_t matchCtrl = 0;
            if (safeReadPtr(reinterpret_cast<uintptr_t>(this_ptr) + 8, &matchCtrl) && matchCtrl) {
                uintptr_t tc = 0;
                if (safeReadPtr(matchCtrl + 0x40, &tc) && tc) {
                    g_activeTableController = reinterpret_cast<void*>(tc);
                    calibrateBoardFromTableController(g_activeTableController);
                }
            }
        }
    }'''

assert old_aim in content, 'old_aim not found'
content = content.replace(old_aim, new_aim, 1)

# 4. In collectPucksFromTree, filter out off-table / garbage coordinates
old_collect = '''            if (!std::isnan(liveX) && !std::isnan(liveY)) {
                PuckStateSample s;
                s.id = puckId;
                s.x = liveX;
                s.y = liveY;
                s.isPocketed = (pocketPtr != 0);
                s.active = (std::abs(liveX) <= 37.2 && std::abs(liveY) <= 37.2);
                out.push_back(s);
            }'''

new_collect = '''            if (!std::isnan(liveX) && !std::isnan(liveY) && std::abs(liveX) <= 37.5 && std::abs(liveY) <= 37.5) {
                PuckStateSample s;
                s.id = puckId;
                s.x = liveX;
                s.y = liveY;
                s.isPocketed = (pocketPtr != 0);
                s.active = true;
                out.push_back(s);
            }'''

assert old_collect in content, 'old_collect not found'
content = content.replace(old_collect, new_collect, 1)

with open(r'c:\Users\ayubg\AndroidStudioProjects\NewBlackbox-main1\NewBlackbox-main\Bcore\src\main\cpp\catEngine\catEngine.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print('catEngine.cpp successfully updated!')
