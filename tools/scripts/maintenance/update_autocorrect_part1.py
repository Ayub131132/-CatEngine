import os

# 1. Update GraphicsEngine.h
ge_h_path = r"C:\Users\ayubg\AndroidStudioProjects\NewBlackbox-main1\NewBlackbox-main\Bcore\src\main\cpp\catEngine\GraphicsEngine.h"
with open(ge_h_path, "r", encoding="utf-8") as f:
    ge_h = f.read()

target_h = "void GraphicsEngine_SetFineTunePhysics(double strikerDamping, double puckDamping,\n                                      double restitutionBody, double restitutionCushion,\n                                      double strikerMass, double powerMultiplier);"
repl_h = """void GraphicsEngine_SetFineTunePhysics(double strikerDamping, double puckDamping,
                                      double restitutionBody, double restitutionCushion,
                                      double strikerMass, double powerMultiplier);
void GraphicsEngine_GetFineTunePhysics(double* strikerDamping, double* puckDamping,
                                      double* restitutionBody, double* restitutionCushion,
                                      double* strikerMass, double* powerMultiplier);
void GraphicsEngine_SaveAutoCorrectedPhysics();
void GraphicsEngine_LoadAutoCorrectedPhysics();"""

if target_h in ge_h:
    ge_h = ge_h.replace(target_h, repl_h)
    with open(ge_h_path, "w", encoding="utf-8") as f:
        f.write(ge_h)
    print("Updated GraphicsEngine.h successfully")
else:
    print("Target in GraphicsEngine.h not found, trying with CRLF")
    target_h_crlf = target_h.replace("\n", "\r\n")
    repl_h_crlf = repl_h.replace("\n", "\r\n")
    if target_h_crlf in ge_h:
        ge_h = ge_h.replace(target_h_crlf, repl_h_crlf)
        with open(ge_h_path, "w", encoding="utf-8") as f:
            f.write(ge_h)
        print("Updated GraphicsEngine.h (CRLF) successfully")
    else:
        print("Could not find target in GraphicsEngine.h!")

# 2. Update GraphicsEngine.cpp
ge_cpp_path = r"C:\Users\ayubg\AndroidStudioProjects\NewBlackbox-main1\NewBlackbox-main\Bcore\src\main\cpp\catEngine\GraphicsEngine.cpp"
with open(ge_cpp_path, "r", encoding="utf-8") as f:
    ge_cpp = f.read()

target_cpp = """    ALOGI("[GraphicsEngine] FineTune Physics: StDamp=%.5f, PkDamp=%.5f, RestBody=%.2f, RestCush=%.2f, Mass=%.2f, PwrMult=%.2f",
          s_strikerDamping.load(), s_puckDamping.load(), s_restitutionBody.load(),
          s_restitutionCushion.load(), s_strikerMass.load(), s_powerMultiplier.load());
}"""

repl_cpp = """    ALOGI("[GraphicsEngine] FineTune Physics: StDamp=%.5f, PkDamp=%.5f, RestBody=%.2f, RestCush=%.2f, Mass=%.2f, PwrMult=%.2f",
          s_strikerDamping.load(), s_puckDamping.load(), s_restitutionBody.load(),
          s_restitutionCushion.load(), s_strikerMass.load(), s_powerMultiplier.load());
}

void GraphicsEngine_GetFineTunePhysics(double* strikerDamping, double* puckDamping,
                                      double* restitutionBody, double* restitutionCushion,
                                      double* strikerMass, double* powerMultiplier) {
    if (strikerDamping) *strikerDamping = s_strikerDamping.load();
    if (puckDamping) *puckDamping = s_puckDamping.load();
    if (restitutionBody) *restitutionBody = s_restitutionBody.load();
    if (restitutionCushion) *restitutionCushion = s_restitutionCushion.load();
    if (strikerMass) *strikerMass = s_strikerMass.load();
    if (powerMultiplier) *powerMultiplier = s_powerMultiplier.load();
}

void GraphicsEngine_SaveAutoCorrectedPhysics() {
    double stDamp = s_strikerDamping.load();
    double pkDamp = s_puckDamping.load();
    double restBody = s_restitutionBody.load();
    double restCush = s_restitutionCushion.load();
    double mass = s_strikerMass.load();
    double pwr = s_powerMultiplier.load();

    const char* paths[] = {
        "/data/data/com.miniclip.carrom/files/blackbox_physics_autocorrect.cfg",
        "/sdcard/Download/blackbox_physics_autocorrect.cfg"
    };
    for (const char* p : paths) {
        FILE* f = fopen(p, "w");
        if (f) {
            fprintf(f, "striker_damping=%.8f\\n", stDamp);
            fprintf(f, "puck_damping=%.8f\\n", pkDamp);
            fprintf(f, "restitution_body=%.4f\\n", restBody);
            fprintf(f, "restitution_cushion=%.4f\\n", restCush);
            fprintf(f, "striker_mass=%.4f\\n", mass);
            fprintf(f, "power_multiplier=%.4f\\n", pwr);
            fclose(f);
        }
    }
}

void GraphicsEngine_LoadAutoCorrectedPhysics() {
    const char* paths[] = {
        "/data/data/com.miniclip.carrom/files/blackbox_physics_autocorrect.cfg",
        "/sdcard/Download/blackbox_physics_autocorrect.cfg"
    };
    for (const char* p : paths) {
        FILE* f = fopen(p, "r");
        if (f) {
            double stDamp = 0, pkDamp = 0, restBody = 0, restCush = 0, mass = 0, pwr = 0;
            if (fscanf(f, "striker_damping=%lf\\npuck_damping=%lf\\nrestitution_body=%lf\\nrestitution_cushion=%lf\\nstriker_mass=%lf\\npower_multiplier=%lf\\n",
                       &stDamp, &pkDamp, &restBody, &restCush, &mass, &pwr) == 6) {
                GraphicsEngine_SetFineTunePhysics(stDamp, pkDamp, restBody, restCush, mass, pwr);
                ALOGI("[GraphicsEngine] Loaded persisted auto-corrected physics: stDamp=%.5f, pkDamp=%.5f, pwr=%.2f", stDamp, pkDamp, pwr);
                fclose(f);
                return;
            }
            fclose(f);
        }
    }
}"""

if target_cpp in ge_cpp:
    ge_cpp = ge_cpp.replace(target_cpp, repl_cpp)
elif target_cpp.replace("\n", "\r\n") in ge_cpp:
    ge_cpp = ge_cpp.replace(target_cpp.replace("\n", "\r\n"), repl_cpp.replace("\n", "\r\n"))
    print("Replaced in GraphicsEngine.cpp (CRLF)")
else:
    print("Could not find target_cpp in GraphicsEngine.cpp!")

# In initGraphicsEngine, call GraphicsEngine_LoadAutoCorrectedPhysics()
target_init = "void initGraphicsEngine() {"
repl_init = """void initGraphicsEngine() {
    GraphicsEngine_LoadAutoCorrectedPhysics();"""
if target_init in ge_cpp:
    ge_cpp = ge_cpp.replace(target_init, repl_init, 1)
    with open(ge_cpp_path, "w", encoding="utf-8") as f:
        f.write(ge_cpp)
    print("Updated initGraphicsEngine in GraphicsEngine.cpp")
else:
    print("target_init not found")

# 3. Update BoxCore.cpp
bc_path = r"C:\Users\ayubg\AndroidStudioProjects\NewBlackbox-main1\NewBlackbox-main\Bcore\src\main\cpp\BoxCore.cpp"
with open(bc_path, "r", encoding="utf-8") as f:
    bc = f.read()

target_bc = """void setFineTunePhysics(JNIEnv *env, jclass clazz, jfloat strikerDamping, jfloat puckDamping,
                        jfloat restitutionBody, jfloat restitutionCushion, jfloat strikerMass,
                        jfloat powerMultiplier) {
    GraphicsEngine_SetFineTunePhysics((double) strikerDamping, (double) puckDamping,
                                      (double) restitutionBody, (double) restitutionCushion,
                                      (double) strikerMass, (double) powerMultiplier);
}"""

repl_bc = """void setFineTunePhysics(JNIEnv *env, jclass clazz, jfloat strikerDamping, jfloat puckDamping,
                        jfloat restitutionBody, jfloat restitutionCushion, jfloat strikerMass,
                        jfloat powerMultiplier) {
    GraphicsEngine_SetFineTunePhysics((double) strikerDamping, (double) puckDamping,
                                      (double) restitutionBody, (double) restitutionCushion,
                                      (double) strikerMass, (double) powerMultiplier);
}

jfloatArray getFineTunePhysics(JNIEnv *env, jclass clazz) {
    double stDamp, pkDamp, restBody, restCush, mass, pwr;
    GraphicsEngine_GetFineTunePhysics(&stDamp, &pkDamp, &restBody, &restCush, &mass, &pwr);
    jfloatArray result = env->NewFloatArray(6);
    if (!result) return nullptr;
    jfloat values[6] = {
        (jfloat) stDamp,
        (jfloat) pkDamp,
        (jfloat) restBody,
        (jfloat) restCush,
        (jfloat) mass,
        (jfloat) pwr
    };
    env->SetFloatArrayRegion(result, 0, 6, values);
    return result;
}"""

if target_bc in bc:
    bc = bc.replace(target_bc, repl_bc)
elif target_bc.replace("\n", "\r\n") in bc:
    bc = bc.replace(target_bc.replace("\n", "\r\n"), repl_bc.replace("\n", "\r\n"))
    print("Replaced setFineTunePhysics in BoxCore.cpp (CRLF)")

target_methods = '{"setFineTunePhysics", "(FFFFFF)V",                        (void *) setFineTunePhysics},'
repl_methods = """{"setFineTunePhysics", "(FFFFFF)V",                        (void *) setFineTunePhysics},
        {"getFineTunePhysics", "()[F",                             (void *) getFineTunePhysics},"""
if target_methods in bc:
    bc = bc.replace(target_methods, repl_methods)
elif target_methods.replace("\n", "\r\n") in bc:
    bc = bc.replace(target_methods.replace("\n", "\r\n"), repl_methods.replace("\n", "\r\n"))

with open(bc_path, "w", encoding="utf-8") as f:
    f.write(bc)
print("Updated BoxCore.cpp successfully")

# 4. Update NativeCore.java
nc_path = r"C:\Users\ayubg\AndroidStudioProjects\NewBlackbox-main1\NewBlackbox-main\Bcore\src\main\java\top\niunaijun\blackbox\core\NativeCore.java"
with open(nc_path, "r", encoding="utf-8") as f:
    nc = f.read()

target_nc = """    public static native void setFineTunePhysics(float strikerDamping, float puckDamping,
                                                 float restitutionBody, float restitutionCushion,
                                                 float strikerMass, float powerMultiplier);"""

repl_nc = """    public static native void setFineTunePhysics(float strikerDamping, float puckDamping,
                                                 float restitutionBody, float restitutionCushion,
                                                 float strikerMass, float powerMultiplier);

    public static native float[] getFineTunePhysics();"""

if target_nc in nc:
    nc = nc.replace(target_nc, repl_nc)
elif target_nc.replace("\n", "\r\n") in nc:
    nc = nc.replace(target_nc.replace("\n", "\r\n"), repl_nc.replace("\n", "\r\n"))

with open(nc_path, "w", encoding="utf-8") as f:
    f.write(nc)
print("Updated NativeCore.java successfully")
