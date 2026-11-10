package top.niunaijun.blackbox.core;

import android.os.Looper;
import android.util.Log;
import top.niunaijun.blackbox.BlackBoxCore;
import top.niunaijun.blackbox.widget.CatEngineLogManager;

public class CrashHandler implements Thread.UncaughtExceptionHandler {
    private static final String TAG = "CrashHandler";
    private Thread.UncaughtExceptionHandler mDefaultHandler;

    public static void create() {
        new CrashHandler();
    }

    public CrashHandler() {
        mDefaultHandler = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler(this);
    }

    private boolean isIgnorableCrash(Thread t, Throwable e) {
        if (e == null) return false;

        String threadName = (t != null && t.getName() != null) ? t.getName() : "";
        String msg = e.getMessage() != null ? e.getMessage() : "";
        String exClassName = e.getClass().getName();

        // 1. GassClient / GassDGClient background Google Ads crashes inside virtual container
        if (threadName.contains("Gass") || threadName.contains("AdWorker") || threadName.contains("GoogleApi") ||
            threadName.contains("OkHttp") || threadName.contains("pool") || threadName.contains("Firebase")) {
            return true;
        }

        // 2. Unknown calling package name SecurityException
        if (msg.contains("Unknown calling package name") || msg.contains("SecurityException")) {
            return true;
        }

        // 3. Google Play Services / AdMob / Firebase measurement crashes
        if (msg.contains("com.google.android.gms") ||
            msg.contains("GooglePlayServices") ||
            msg.contains("GoogleApiAvailability") ||
            msg.contains("measurement") ||
            msg.contains("policy_ads") ||
            exClassName.contains("GassClient")) {
            if (!isMainThread()) return true;
        }

        // 4. DeadSystemException / RemoteException on background thread
        if (exClassName.contains("DeadSystemException") || exClassName.contains("TransactionTooLargeException")) {
            if (!isMainThread()) return true;
        }

        return false;
    }

    private boolean isMainThread() {
        return Looper.myLooper() != null && Looper.myLooper() == Looper.getMainLooper();
    }

    @Override
    public void uncaughtException(Thread t, Throwable e) {
        if (isIgnorableCrash(t, e) || top.niunaijun.blackbox.utils.CrashMonitor.isIgnorableBackgroundCrash(t, e)) {
            Log.w(TAG, "Suppressed background non-fatal crash in thread [" + (t != null ? t.getName() : "unknown") + "]: " + (e != null ? e.getMessage() : "null"));
            try {
                CatEngineLogManager.INSTANCE.log("CrashHandler", "Prevented background crash in [" + (t != null ? t.getName() : "unknown") + "]: " + (e != null ? e.getMessage() : "null"));
            } catch (Throwable ignored) {}
            return;
        }

        CatEngineLogManager.INSTANCE.log("FATAL_CRASH", "Uncaught exception in [" + (t != null ? t.getName() : "unknown") + "]: " + Log.getStackTraceString(e));

        if (isMainThread()) {
            // Keep main looper alive for ignorable main-thread UI exceptions
            if (e != null && (e.getMessage() != null && (e.getMessage().contains("Unknown calling package name") || e.getMessage().contains("NullPointerException")))) {
                Log.w(TAG, "Recovering main looper from handled UI exception: " + e.getMessage());
                while (true) {
                    try {
                        Looper.loop();
                        break;
                    } catch (Throwable inner) {
                        Log.w(TAG, "Loop exception handled: " + inner.getMessage());
                    }
                }
                return;
            }
        }

        if (BlackBoxCore.get().getExceptionHandler() != null) {
            BlackBoxCore.get().getExceptionHandler().uncaughtException(t, e);
        }
        if (mDefaultHandler != null && mDefaultHandler != this) {
            mDefaultHandler.uncaughtException(t, e);
        }
    }
}
