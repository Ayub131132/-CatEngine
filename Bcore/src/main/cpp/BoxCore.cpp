



#include "BoxCore.h"
#include "Log.h"
#include "IO.h"
#include <jni.h>
#include <JniHook/JniHook.h>
#include <Hook/VMClassLoaderHook.h>
#include <Hook/UnixFileSystemHook.h>
#include <Hook/FileSystemHook.h>
#include <Hook/BinderHook.h>
#include <Hook/DexFileHook.h>
#include <Hook/RuntimeHook.h>
#include "Utils/HexDump.h"
#include "hidden_api.h"
#include "catEngine/GraphicsEngine.h"

extern "C" void startCatEngineLoop();
extern "C" void CatEngine_ApplyDynamicOffsets(const char* jsonStr);

struct {
    JavaVM *vm;
    jclass NativeCoreClass;
    jmethodID getCallingUidId;
    jmethodID redirectPathString;
    jmethodID redirectPathFile;
    jmethodID loadEmptyDex;
    jmethodID loadEmptyDexL;
    int api_level;
} VMEnv;


JNIEnv *getEnv() {
    JNIEnv *env;
    VMEnv.vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
    return env;
}

JNIEnv *ensureEnvCreated() {
    JNIEnv *env = getEnv();
    if (env == NULL) {
        VMEnv.vm->AttachCurrentThread(&env, NULL);
    }
    return env;
}

int BoxCore::getCallingUid(JNIEnv *env, int orig) {
    env = ensureEnvCreated();
    return env->CallStaticIntMethod(VMEnv.NativeCoreClass, VMEnv.getCallingUidId, orig);
}

jstring BoxCore::redirectPathString(JNIEnv *env, jstring path) {
    env = ensureEnvCreated();
    return (jstring) env->CallStaticObjectMethod(VMEnv.NativeCoreClass, VMEnv.redirectPathString, path);
}

jobject BoxCore::redirectPathFile(JNIEnv *env, jobject path) {
    env = ensureEnvCreated();
    return env->CallStaticObjectMethod(VMEnv.NativeCoreClass, VMEnv.redirectPathFile, path);
}

jlongArray BoxCore::loadEmptyDex(JNIEnv *env) {
    env = ensureEnvCreated();
    return (jlongArray) env->CallStaticObjectMethod(VMEnv.NativeCoreClass, VMEnv.loadEmptyDex);
}

int BoxCore::getApiLevel() {
    return VMEnv.api_level;
}

JavaVM *BoxCore::getJavaVM() {
    return VMEnv.vm;
}

void nativeHook(JNIEnv *env) {
    BaseHook::init(env);
    UnixFileSystemHook::init(env);
    FileSystemHook::init();
    VMClassLoaderHook::init(env);

    BinderHook::init(env);
    DexFileHook::init(env);
}

void hideXposed(JNIEnv *env, jclass clazz) {
    ALOGD("set hideXposed");
    VMClassLoaderHook::hideXposed();
}

void init(JNIEnv *env, jobject clazz, jint api_level) {
    ALOGD("NativeCore init.");
    VMEnv.api_level = api_level;
    VMEnv.NativeCoreClass = (jclass) env->NewGlobalRef(env->FindClass(VMCORE_CLASS));
    VMEnv.getCallingUidId = env->GetStaticMethodID(VMEnv.NativeCoreClass, "getCallingUid", "(I)I");
    VMEnv.redirectPathString = env->GetStaticMethodID(VMEnv.NativeCoreClass, "redirectPath",
                                                      "(Ljava/lang/String;)Ljava/lang/String;");
    VMEnv.redirectPathFile = env->GetStaticMethodID(VMEnv.NativeCoreClass, "redirectPath",
                                                    "(Ljava/io/File;)Ljava/io/File;");
    VMEnv.loadEmptyDex = env->GetStaticMethodID(VMEnv.NativeCoreClass, "loadEmptyDex",
                                                "()[J");

    JniHook::InitJniHook(env, api_level);
}

void addIORule(JNIEnv *env, jclass clazz, jstring target_path,
               jstring relocate_path) {
    ALOGD("set addIORule");
    IO::addRule(env->GetStringUTFChars(target_path, JNI_FALSE),
                env->GetStringUTFChars(relocate_path, JNI_FALSE));
}

void enableIO(JNIEnv *env, jclass clazz) {
    ALOGD("set enableIO");
    IO::init(env);
    nativeHook(env);
    startCatEngineLoop();
}

bool disableHiddenApi(JNIEnv *env, jclass clazz) {
    ALOGD("set disableHiddenApi");
    if(!disable_hidden_api(env)){
        ALOGD("set disableHiddenApi Fail!!!");
        return false;
    }
    return true;
}

bool disableResourceLoading(JNIEnv *env, jclass clazz) {
    ALOGD("set disableResourceLoading");
    if(!disable_resource_loading()){
        ALOGD("set disableResourceLoading Fail!!!");
        return false;
    }
    return true;
}

void setBoardBounds(JNIEnv *env, jclass clazz, jfloat left, jfloat right, jfloat bottom, jfloat top, jboolean showBorder) {
    GraphicsEngine_SetBoardBounds(left, right, bottom, top, (bool) showBorder);
}

void resetBoardCalibration(JNIEnv *env, jclass clazz) {
    GraphicsEngine_ResetToAuto();
}

void setObjectGeometry(JNIEnv *env, jclass clazz, jfloat strikerRadius, jfloat puckRadius, jfloat pocketInset, jfloat pocketRadius) {
    GraphicsEngine_SetObjectGeometry(strikerRadius, puckRadius, pocketInset, pocketRadius);
}

void setPocketRadius(JNIEnv *env, jclass clazz, jfloat radius) {
    GraphicsEngine_SetPocketRadius((float) radius);
}

jfloat getPocketRadius(JNIEnv *env, jclass clazz) {
    return (jfloat) GraphicsEngine_GetPocketRadius();
}

void setPuckRadius(JNIEnv *env, jclass clazz, jfloat radius) {
    GraphicsEngine_SetPuckRadius((float) radius);
}

jfloat getPuckRadius(JNIEnv *env, jclass clazz) {
    return (jfloat) GraphicsEngine_GetPuckRadius();
}

void setMaxTravelDistance(JNIEnv *env, jclass clazz, jfloat maxDistance) {
    GraphicsEngine_SetMaxTravelDistance(maxDistance);
}

void setGuidelineThickness(JNIEnv *env, jclass clazz, jfloat thickness) {
    GraphicsEngine_SetGuidelineThickness(thickness);
}

void setColorTheme(JNIEnv *env, jclass clazz, jint theme) {
    GraphicsEngine_SetColorTheme(theme);
}

void setFineTunePhysics(JNIEnv *env, jclass clazz, jfloat strikerDamping, jfloat puckDamping,
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
}

void setAdvancedPhysics(JNIEnv *env, jclass clazz, jfloat contactFriction, jfloat cushionTangentFriction, jfloat strikerBonus) {
    GraphicsEngine_SetAdvancedPhysics((double) contactFriction, (double) cushionTangentFriction, (double) strikerBonus);
}

jfloatArray getAdvancedPhysics(JNIEnv *env, jclass clazz) {
    double contFric, cushTang, bonus;
    GraphicsEngine_GetAdvancedPhysics(&contFric, &cushTang, &bonus);
    jfloatArray result = env->NewFloatArray(3);
    if (!result) return nullptr;
    jfloat values[3] = {
        (jfloat) contFric,
        (jfloat) cushTang,
        (jfloat) bonus
    };
    env->SetFloatArrayRegion(result, 0, 3, values);
    return result;
}

void setPocketPhysics(JNIEnv *env, jclass clazz, jfloat dropMaxSpeed, jfloat gravity) {
    GraphicsEngine_SetPocketPhysics((double) dropMaxSpeed, (double) gravity);
}

jfloatArray getPocketPhysics(JNIEnv *env, jclass clazz) {
    double dropSpd, grav;
    GraphicsEngine_GetPocketPhysics(&dropSpd, &grav);
    jfloatArray result = env->NewFloatArray(2);
    if (!result) return nullptr;
    jfloat values[2] = {
        (jfloat) dropSpd,
        (jfloat) grav
    };
    env->SetFloatArrayRegion(result, 0, 2, values);
    return result;
}

jfloat getStrikerRadius(JNIEnv *env, jclass clazz) {
    return (jfloat) GraphicsEngine_GetStrikerRadius();
}

jfloat getPocketInset(JNIEnv *env, jclass clazz) {
    return (jfloat) GraphicsEngine_GetPocketInset();
}

jboolean isMatchActive(JNIEnv *env, jclass clazz) {
    return (jboolean) GraphicsEngine_IsMatchActive();
}

void setAutoShotEnabled(JNIEnv *env, jclass clazz, jboolean enabled) {
    GraphicsEngine_SetAutoShotEnabled((bool) enabled);
}

jboolean isAutoShotEnabled(JNIEnv *env, jclass clazz) {
    return (jboolean) GraphicsEngine_IsAutoShotEnabled();
}

void setFastFirstShotMode(JNIEnv *env, jclass clazz, jboolean enabled) {
    GraphicsEngine_SetFastFirstShotMode((bool) enabled);
}

jboolean isFastFirstShotMode(JNIEnv *env, jclass clazz) {
    return (jboolean) GraphicsEngine_IsFastFirstShotMode();
}

void updateAim(JNIEnv *env, jclass clazz, jfloat x, jfloat y, jfloat angle, jfloat force, jboolean isAiming) {
    GraphicsEngine_UpdateAim((double) x, (double) y, (double) angle, (double) force, (bool) isAiming);
}
void setShowPrediction(JNIEnv *env, jclass clazz, jboolean enable) {
    GraphicsEngine_SetShowPrediction((bool) enable);
}

jboolean getShowPrediction(JNIEnv *env, jclass clazz) {
    return (jboolean) GraphicsEngine_GetShowPrediction();
}

void setShowPocket(JNIEnv *env, jclass clazz, jboolean enable) {
    GraphicsEngine_SetShowPocket((bool) enable);
}

jboolean getShowPocket(JNIEnv *env, jclass clazz) {
    return (jboolean) GraphicsEngine_GetShowPocket();
}

void setShowBorder(JNIEnv *env, jclass clazz, jboolean enable) {
    GraphicsEngine_SetShowBorder((bool) enable);
}

jboolean getShowBorder(JNIEnv *env, jclass clazz) {
    return (jboolean) GraphicsEngine_GetShowBorder();
}

void setLineTransparency(JNIEnv *env, jclass clazz, jfloat alpha) {
    GraphicsEngine_SetLineTransparency((float) alpha);
}

jfloat getLineTransparency(JNIEnv *env, jclass clazz) {
    return (jfloat) GraphicsEngine_GetLineTransparency();
}

void setCustomColors(JNIEnv *env, jclass clazz, jint strikerColor, jint myPuckColor, jint opponentPuckColor) {
    GraphicsEngine_SetCustomColors((uint32_t) strikerColor, (uint32_t) myPuckColor, (uint32_t) opponentPuckColor);
}

void setKeepLineAfterShot(JNIEnv *env, jclass clazz, jboolean keep) {
    GraphicsEngine_SetKeepLineAfterShot((bool) keep);
}

jboolean getKeepLineAfterShot(JNIEnv *env, jclass clazz) {
    return (jboolean) GraphicsEngine_GetKeepLineAfterShot();
}

void setPhysicsEngineMode(JNIEnv *env, jclass clazz, jint mode) {
    GraphicsEngine_SetPhysicsEngineMode((int) mode);
}

jint getPhysicsEngineMode(JNIEnv *env, jclass clazz) {
    return (jint) GraphicsEngine_GetPhysicsEngineMode();
}

void setTargetColorMode(JNIEnv *env, jclass clazz, jint mode) {
    GraphicsEngine_SetTargetColorMode((int) mode);
}

jint getTargetColorMode(JNIEnv *env, jclass clazz) {
    return (jint) GraphicsEngine_GetTargetColorMode();
}

void applyEngineOffsets(JNIEnv *env, jclass clazz, jstring jsonOffsets) {
    if (!jsonOffsets) return;
    const char *nativeStr = env->GetStringUTFChars(jsonOffsets, JNI_FALSE);
    if (nativeStr) {
        CatEngine_ApplyDynamicOffsets(nativeStr);
        env->ReleaseStringUTFChars(jsonOffsets, nativeStr);
    }
}



static JNINativeMethod gMethods[] = {
        {"disableHiddenApi", "()Z",                               (void *) disableHiddenApi},
        {"disableResourceLoading", "()Z",                         (void *) disableResourceLoading},
        {"hideXposed", "()V",                                     (void *) hideXposed},
        {"addIORule",  "(Ljava/lang/String;Ljava/lang/String;)V", (void *) addIORule},
        {"enableIO",   "()V",                                     (void *) enableIO},
        {"init",       "(I)V",                                    (void *) init},
        {"setBoardBounds", "(FFFFZ)V",                            (void *) setBoardBounds},
        {"resetBoardCalibration", "()V",                          (void *) resetBoardCalibration},
        {"setObjectGeometry", "(FFFF)V",                          (void *) setObjectGeometry},
        {"setPocketRadius",   "(F)V",                             (void *) setPocketRadius},
        {"getPocketRadius",   "()F",                              (void *) getPocketRadius},
        {"setPuckRadius",     "(F)V",                             (void *) setPuckRadius},
        {"getPuckRadius",     "()F",                              (void *) getPuckRadius},
        {"setMaxTravelDistance", "(F)V",                          (void *) setMaxTravelDistance},
        {"setGuidelineThickness", "(F)V",                         (void *) setGuidelineThickness},
        {"setColorTheme", "(I)V",                                 (void *) setColorTheme},
        {"setFineTunePhysics", "(FFFFFF)V",                        (void *) setFineTunePhysics},
        {"getFineTunePhysics", "()[F",                             (void *) getFineTunePhysics},
        {"setAdvancedPhysics", "(FFF)V",                           (void *) setAdvancedPhysics},
        {"getAdvancedPhysics", "()[F",                             (void *) getAdvancedPhysics},
        {"setPocketPhysics",   "(FF)V",                            (void *) setPocketPhysics},
        {"getPocketPhysics",   "()[F",                             (void *) getPocketPhysics},
        {"getStrikerRadius",   "()F",                              (void *) getStrikerRadius},
        {"getPocketInset",     "()F",                              (void *) getPocketInset},
        {"isMatchActive",      "()Z",                              (void *) isMatchActive},
        {"setAutoShotEnabled", "(Z)V",                             (void *) setAutoShotEnabled},
        {"isAutoShotEnabled",  "()Z",                              (void *) isAutoShotEnabled},
        {"setFastFirstShotMode", "(Z)V",                           (void *) setFastFirstShotMode},
        {"isFastFirstShotMode",  "()Z",                            (void *) isFastFirstShotMode},
        {"updateAim",          "(FFFFZ)V",                         (void *) updateAim},
        {"setShowPrediction",  "(Z)V",                             (void *) setShowPrediction},
        {"getShowPrediction",  "()Z",                              (void *) getShowPrediction},
        {"setShowPocket",      "(Z)V",                             (void *) setShowPocket},
        {"getShowPocket",      "()Z",                              (void *) getShowPocket},
        {"setShowBorder",      "(Z)V",                             (void *) setShowBorder},
        {"getShowBorder",      "()Z",                              (void *) getShowBorder},
        {"setLineTransparency","(F)V",                             (void *) setLineTransparency},
        {"getLineTransparency","()F",                              (void *) getLineTransparency},
        {"setCustomColors",    "(III)V",                           (void *) setCustomColors},
        {"setKeepLineAfterShot","(Z)V",                            (void *) setKeepLineAfterShot},
        {"getKeepLineAfterShot","()Z",                             (void *) getKeepLineAfterShot},
        {"setPhysicsEngineMode", "(I)V",                           (void *) setPhysicsEngineMode},
        {"getPhysicsEngineMode", "()I",                            (void *) getPhysicsEngineMode},
        {"setTargetColorMode",   "(I)V",                           (void *) setTargetColorMode},
        {"getTargetColorMode",   "()I",                            (void *) getTargetColorMode},
        {"applyEngineOffsets", "(Ljava/lang/String;)V",           (void *) applyEngineOffsets},
};

int registerNativeMethods(JNIEnv *env, const char *className,
                          JNINativeMethod *gMethods, int numMethods) {
    jclass clazz;
    clazz = env->FindClass(className);
    if (clazz == nullptr) {
        return JNI_FALSE;
    }
    if (env->RegisterNatives(clazz, gMethods, numMethods) < 0) {
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

int registerNatives(JNIEnv *env) {
    if (!registerNativeMethods(env, VMCORE_CLASS, gMethods,
                               sizeof(gMethods) / sizeof(gMethods[0])))
        return JNI_FALSE;
    return JNI_TRUE;
}

void registerMethod(JNIEnv *jenv) {
    registerNatives(jenv);
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    VMEnv.vm = vm;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_EVERSION;
    }
    registerMethod(env);
    return JNI_VERSION_1_6;
}