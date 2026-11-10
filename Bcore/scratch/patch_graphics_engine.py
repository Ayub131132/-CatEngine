path = r"c:\Users\ayubg\AndroidStudioProjects\NewBlackbox-main1\NewBlackbox-main\Bcore\src\main\cpp\catEngine\GraphicsEngine.cpp"
with open(path, "r", encoding="utf-8") as f:
    text = f.read()

# 1. Fix screen striker coordinate calculation in GraphicsEngine_PullAndReleaseStriker
t1 = """        float screenStrikerX = 0.0f;
        float screenStrikerY = 0.0f;

        // CRITICAL FIX: If manual bounds are calibrated, ALWAYS use them for screen touch injection!
        // This guarantees touch coordinates land directly on the striker, never at (0.0, 354.9).
        if (s_hasManualBounds.load()) {
            float rawL = s_boundLeft.load();
            float rawR = s_boundRight.load();
            float rawB = s_boundBottom.load();
            float rawT = s_boundTop.load();
            float left   = std::min(rawL, rawR);
            float right  = std::max(rawL, rawR);
            float bottom = std::min(rawB, rawT);
            float top    = std::max(rawB, rawT);
            float cx = (left + right) * 0.5f;
            float cy = (bottom + top) * 0.5f;
            float scale = std::min(right - left, top - bottom) / (2.0f * kBoardHalfSize);

            screenStrikerX = cx + static_cast<float>(curStX) * scale;
            screenStrikerY = cy - static_cast<float>(curStY) * scale;
        } else {
            bool hasScreenPos = CatEngine_GetStrikerScreenPos(&screenStrikerX, &screenStrikerY);
            if (!hasScreenPos || screenStrikerX < 50.0f || screenStrikerY < 50.0f) {
                float cx = s_autoCenterX.load();
                float cy = s_autoCenterY.load();
                float scale = s_autoScale.load();
                if (scale <= 0.1f) {
                    cx = 532.5f;
                    cy = 1140.0f;
                    scale = 13.88f;
                }
                screenStrikerX = cx + static_cast<float>(curStX) * scale;
                screenStrikerY = cy - static_cast<float>(curStY) * scale;
            }
        }"""

r1 = """        float screenStrikerX = 0.0f;
        float screenStrikerY = 0.0f;

        float cx = 540.0f;
        float cy = 1140.0f;
        float scale = 13.88f;

        if (s_hasManualBounds.load()) {
            float rawL = s_boundLeft.load();
            float rawR = s_boundRight.load();
            float rawB = s_boundBottom.load();
            float rawT = s_boundTop.load();
            float left   = std::min(rawL, rawR);
            float right  = std::max(rawL, rawR);
            float bottom = std::min(rawB, rawT);
            float top    = std::max(rawB, rawT);
            cx = (left + right) * 0.5f;
            cy = (bottom + top) * 0.5f;
            scale = std::min(right - left, top - bottom) / (2.0f * kBoardHalfSize);
        } else {
            float aCx = s_autoCenterX.load();
            float aCy = s_autoCenterY.load();
            float aSc = s_autoScale.load();
            if (aSc > 1.0f) {
                cx = aCx;
                cy = aCy;
                scale = aSc;
            }
        }

        // In Android touch screen space: +X is right, +Y is DOWNWARD (cy - curStY*scale lands at bottom baseline)
        screenStrikerX = cx + static_cast<float>(curStX) * scale;
        screenStrikerY = cy - static_cast<float>(curStY) * scale;"""

assert t1 in text, "t1 not found"
text = text.replace(t1, r1, 1)

# 2. Fix target puck evaluation in evaluateCandidateShot: support Black pucks when userWhiteCount == 0
t2 = """    // 3. Identify target pocketed puck (Prioritize White=1 and Queen=3 over Black=2)
    const PuckTrajectoryInfo* bestPuck = nullptr;
    for (const auto& pt : sim.puckTrajectories) {
        if (pt.inPocket) {
            if (!bestPuck) {
                bestPuck = &pt;
            } else if (pt.puckType != 2 && bestPuck->puckType == 2) {
                bestPuck = &pt; // Prefer user puck over opponent puck
            } else if (pt.puckType == 3 && bestPuck->puckType != 3) {
                bestPuck = &pt; // Prefer Queen
            }
        }
    }

    if (!bestPuck) return false;"""

r2 = """    // 3. Identify target pocketed puck (Prioritize User / Queen pucks, fallback to any valid pocketed puck)
    int userWhiteCount = 0;
    int opponentBlackCount = 0;
    {
        std::lock_guard<std::mutex> lock(s_puckMutex);
        for (int i = 0; i < 36; i++) {
            if (s_pucks[i].active) {
                if (s_pucks[i].puckType == 1) userWhiteCount++;
                else if (s_pucks[i].puckType == 2) opponentBlackCount++;
            }
        }
    }

    const PuckTrajectoryInfo* bestPuck = nullptr;
    for (const auto& pt : sim.puckTrajectories) {
        if (pt.inPocket) {
            if (!bestPuck) {
                bestPuck = &pt;
            } else if (pt.puckType == 3 && bestPuck->puckType != 3) {
                bestPuck = &pt; // Prefer Queen
            } else if (userWhiteCount > 0 && pt.puckType == 1 && bestPuck->puckType != 1 && bestPuck->puckType != 3) {
                bestPuck = &pt; // Prefer User White puck if user has white pucks left
            } else if (userWhiteCount == 0 && pt.puckType == 2 && bestPuck->puckType != 3) {
                bestPuck = &pt; // Target Black puck if no white pucks left or user is Black!
            }
        }
    }

    if (!bestPuck) return false;"""

assert t2 in text, "t2 not found"
text = text.replace(t2, r2, 1)

# 3. Update tier calculation in evaluateCandidateShot
t3 = """    if (pType == 2) {
        // Opponent black puck: lowest priority
        tier = 6;
        baseScore = -500000.0;
    } else if (strikerBounces == 0 && puckBounces == 0 && chain == 0) {"""

r3 = """    if (userWhiteCount > 0 && pType == 2) {
        // Disallowed opponent black puck while user white pucks still exist
        tier = 6;
        baseScore = -500000.0;
    } else if (strikerBounces == 0 && puckBounces == 0 && chain == 0) {"""

assert t3 in text, "t3 not found"
text = text.replace(t3, r3, 1)

# 4. Update isBoardStatic motion threshold
t4 = """    bool moving = false;
    if (std::hypot(stX - s_prevStrikerX, stY - s_prevStrikerY) > 0.08) {
        moving = true;
    }

    for (int i = 0; i < 36; i++) {
        if (!current[i].active) continue;
        if (std::hypot(current[i].x - s_prevPucks[i].x, current[i].y - s_prevPucks[i].y) > 0.06) {
            moving = true;
            break;
        }
    }"""

r4 = """    bool moving = false;
    if (std::hypot(stX - s_prevStrikerX, stY - s_prevStrikerY) > 0.15) {
        moving = true;
    }

    for (int i = 0; i < 36; i++) {
        if (!current[i].active) continue;
        if (std::hypot(current[i].x - s_prevPucks[i].x, current[i].y - s_prevPucks[i].y) > 0.10) {
            moving = true;
            break;
        }
    }"""

assert t4 in text, "t4 not found"
text = text.replace(t4, r4, 1)

# 5. Add diagnostics to GraphicsEngine_ScanAndExecuteBestShot
t5 = """    if (activeCount == 0) {
        return false;
    }"""

r5 = """    if (activeCount == 0) {
        ALOGI("[GraphicsEngine] Scan: No active pucks on board");
        return false;
    }"""

assert t5 in text, "t5 not found"
text = text.replace(t5, r5, 1)

t6 = """    return false;
}

void GraphicsEngine_SetAutoShotEnabled(bool enabled) {"""

r6 = """    ALOGI("[GraphicsEngine] Scan: No winning shot found among %d active pucks", activeCount);
    return false;
}

void GraphicsEngine_SetAutoShotEnabled(bool enabled) {"""

assert t6 in text, "t6 not found"
text = text.replace(t6, r6, 1)

with open(path, "w", encoding="utf-8") as f:
    f.write(text)

print("GraphicsEngine.cpp successfully updated!")
