bco_path = r"C:\Users\ayubg\AndroidStudioProjects\NewBlackbox-main1\NewBlackbox-main\Bcore\src\main\java\top\niunaijun\blackbox\widget\BoardCalibrationOverlay.java"
with open(bco_path, "r", encoding="utf-8") as f:
    bco = f.read()

target = """        final float[] physics = new float[]{
            sStrikerDamping,
            sPuckDamping,
            sRestitutionBody,
            sRestitutionCushion,
            sStrikerMass,
            sPowerMultiplier
        };"""

repl = """        final float[] physics = new float[]{
            sStrikerDamping,
            sPuckDamping,
            sRestitutionBody,
            sRestitutionCushion,
            sStrikerMass,
            sPowerMultiplier
        };

        // Sync with live parameters from the Auto-Correction Engine
        try {
            float[] live = NativeCore.getFineTunePhysics();
            if (live != null && live.length == 6) {
                for (int i = 0; i < 6; i++) {
                    physics[i] = live[i];
                }
            }
        } catch (Throwable ignored) {}"""

if target in bco:
    bco = bco.replace(target, repl)
elif target.replace("\n", "\r\n") in bco:
    bco = bco.replace(target.replace("\n", "\r\n"), repl.replace("\n", "\r\n"))

with open(bco_path, "w", encoding="utf-8") as f:
    f.write(bco)
print("Updated BoardCalibrationOverlay.java successfully")
