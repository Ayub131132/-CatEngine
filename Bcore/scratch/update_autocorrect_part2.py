import os

cat_cpp_path = r"C:\Users\ayubg\AndroidStudioProjects\NewBlackbox-main1\NewBlackbox-main\Bcore\src\main\cpp\catEngine\catEngine.cpp"
with open(cat_cpp_path, "r", encoding="utf-8") as f:
    cat_cpp = f.read()

target = """    } else {
        appendLog("   Striker-only shot: Striker error was %.2f cm.", stError);
    }
    appendLog("================================================================================");"""

repl = """    } else {
        appendLog("   Striker-only shot: Striker error was %.2f cm.", stError);
    }

    // 6. CLOSED-LOOP REAL-TIME AUTO-CORRECTION ENGINE
    appendLog(" ");
    appendLog("6. CLOSED-LOOP REAL-TIME AUTO-CORRECTION ENGINE:");

    double curStDamp = 0.9850, curPkDamp = 0.9888, curRestBody = 0.92, curRestCush = 0.88, curMass = 3.00, curPwrMult = 1.00;
    GraphicsEngine_GetFineTunePhysics(&curStDamp, &curPkDamp, &curRestBody, &curRestCush, &curMass, &curPwrMult);

    appendLog("   [PRE-SHOT ACTIVE MODEL PARAMETERS]:");
    appendLog("   * Striker Rolling Damping (lambda_s):  %.5f (Friction decay per 60fps tick)", curStDamp);
    appendLog("   * Puck Rolling Damping (lambda_p):     %.5f (Friction decay per 60fps tick)", curPkDamp);
    appendLog("   * Disc Elasticity (e_body):            %.2f (Impact restitution on contact)", curRestBody);
    appendLog("   * Cushion Elasticity (e_cushion):      %.2f (Rebound velocity retention)", curRestCush);
    appendLog("   * Striker Mass Ratio (Ms / Mp):        %.2f : 1.0", curMass);
    appendLog("   * Shot Power Multiplier:               %.2fx (Launch impulse scale)", curPwrMult);

    double v0_computed = tele.strikerInitialSpeed;
    double stPredDist = tele.strikerTotalLineLength;
    double stActDist = stActTravel;
    double stRatio = (stPredDist > 0.5) ? (stActDist / stPredDist) : 1.0;
    double stDelta = stActDist - stPredDist;

    appendLog(" ");
    appendLog("   [PHYSICAL TRAVEL & CONSERVATION ANALYSIS]:");
    appendLog("   * Launch Impulse: Raw Touch Pull = %.1f%% | Initial Velocity v0 = %.2f cm/s", powerPct, v0_computed);
    appendLog("   * Striker Distance: Predicted = %.2f cm | Actual Travel = %.2f cm (Delta = %+.2f cm, Ratio = %.3f)",
              stPredDist, stActDist, stDelta, stRatio);

    double newStDamp = curStDamp;
    double newPkDamp = curPkDamp;
    double newRestBody = curRestBody;
    double newRestCush = curRestCush;
    double newMass = curMass;
    double newPwrMult = curPwrMult;

    bool shotEligible = (tele.shotForceVal > 0.06 && stActDist > 2.0);

    if (shotEligible) {
        // A. Striker Damping & Launch Power Auto-Tuning
        if (std::abs(stRatio - 1.0) > 0.02) {
            double errRatio = stRatio - 1.0;
            double dampGradient = 0.35 * (1.0 - curStDamp) * (errRatio / (1.0 + std::abs(errRatio)));
            newStDamp = std::clamp(curStDamp + dampGradient, 0.9600, 0.9990);

            if (std::abs(errRatio) > 0.20) {
                double pwrGradient = 0.15 * curPwrMult * errRatio;
                newPwrMult = std::clamp(curPwrMult + pwrGradient, 0.60, 2.50);
            }
        }

        // B. Cushion Elasticity Auto-Tuning
        if (tele.strikerCushionBounces > 0) {
            double errRatio = stRatio - 1.0;
            double cushGradient = 0.10 * curRestCush * errRatio;
            newRestCush = std::clamp(curRestCush + cushGradient, 0.70, 0.96);
        }

        // C. Target Puck Damping & Collision Restitution Auto-Tuning
        if (tele.predictedPuckCount > 0) {
            const auto& pt = tele.predictedPucks[0];
            auto travIt = puckActualTravel.find(pt.puckId);
            double pkActDist = (travIt != puckActualTravel.end()) ? travIt->second : 0.0;
            double pkPredDist = pt.totalLineLength;
            bool pkPotted = false;
            for (const auto& fp : finalPucks) {
                if (fp.id == pt.puckId && (fp.isPocketed || std::abs(fp.x) > 37.5)) {
                    pkPotted = true; break;
                }
            }

            if (!pkPotted && pkPredDist > 1.0 && pkActDist > 1.0) {
                double pkRatio = pkActDist / pkPredDist;
                double pkErr = pkRatio - 1.0;
                appendLog("   * Primary Puck #%d Travel: Predicted = %.2f cm | Actual = %.2f cm (Delta = %+.2f cm, Ratio = %.3f)",
                          pt.puckId, pkPredDist, pkActDist, pkActDist - pkPredDist, pkRatio);

                if (std::abs(pkErr) > 0.03) {
                    double pkDampGrad = 0.30 * (1.0 - curPkDamp) * (pkErr / (1.0 + std::abs(pkErr)));
                    newPkDamp = std::clamp(curPkDamp + pkDampGrad, 0.9650, 0.9990);
                }

                if (stRatio > 1.05 && pkRatio < 0.95) {
                    newMass = std::clamp(curMass + 0.08, 1.50, 4.50);
                    newRestBody = std::clamp(curRestBody - 0.02, 0.70, 0.98);
                } else if (stRatio < 0.95 && pkRatio > 1.05) {
                    newMass = std::clamp(curMass - 0.08, 1.50, 4.50);
                    newRestBody = std::clamp(curRestBody + 0.02, 0.70, 0.98);
                }
            }
        }

        // Apply updated parameters to the live engine
        GraphicsEngine_SetFineTunePhysics(newStDamp, newPkDamp, newRestBody, newRestCush, newMass, newPwrMult);
        GraphicsEngine_SaveAutoCorrectedPhysics();

        double accPct = std::max(0.0, 100.0 - std::min(100.0, std::abs(stRatio - 1.0) * 100.0));

        appendLog(" ");
        appendLog("   [AUTO-CORRECTION APPLIED FOR NEXT SHOT]:");
        appendLog("   * Striker Damping:     %.5f -> %.5f (Delta = %+.5f)", curStDamp, newStDamp, newStDamp - curStDamp);
        appendLog("   * Puck Damping:        %.5f -> %.5f (Delta = %+.5f)", curPkDamp, newPkDamp, newPkDamp - curPkDamp);
        appendLog("   * Disc Elasticity:     %.2f -> %.2f (Delta = %+.2f)", curRestBody, newRestBody, newRestBody - curRestBody);
        appendLog("   * Cushion Elasticity:  %.2f -> %.2f (Delta = %+.2f)", curRestCush, newRestCush, newRestCush - curRestCush);
        appendLog("   * Striker Mass Ratio:  %.2f -> %.2f (Delta = %+.2f)", curMass, newMass, newMass - curMass);
        appendLog("   * Power Multiplier:    %.2fx -> %.2fx (Delta = %+.2fx)", curPwrMult, newPwrMult, newPwrMult - curPwrMult);
        appendLog("   * Physical Accuracy:   %.1f%%", accPct);
        if (accPct >= 96.0) {
            appendLog("   * CALIBRATION STATUS:  [LOCKED & FULLY CALIBRATED (+-4%% tolerance)]");
        } else {
            appendLog("   * CALIBRATION STATUS:  [SELF-CONVERGING - Parameters tuned closer to game engine]");
        }
    } else {
        appendLog("   * Shot force too low or striker static - skipping auto-correction adjustment for this tap.");
    }
    appendLog("================================================================================");"""

if target in cat_cpp:
    cat_cpp = cat_cpp.replace(target, repl)
elif target.replace("\n", "\r\n") in cat_cpp:
    cat_cpp = cat_cpp.replace(target.replace("\n", "\r\n"), repl.replace("\n", "\r\n"))
    print("Replaced in catEngine.cpp (CRLF)")
else:
    print("target not found in catEngine.cpp!")

with open(cat_cpp_path, "w", encoding="utf-8") as f:
    f.write(cat_cpp)
print("Updated catEngine.cpp successfully")
