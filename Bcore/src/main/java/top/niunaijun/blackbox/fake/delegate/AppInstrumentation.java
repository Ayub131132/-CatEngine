package top.niunaijun.blackbox.fake.delegate;

import android.app.Activity;
import android.app.Application;
import android.app.Instrumentation;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.os.IBinder;
import android.os.PersistableBundle;
import android.util.Log;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.Map;

import black.android.app.BRActivity;
import black.android.app.BRActivityThread;
import black.android.app.BRActivityThreadActivityClientRecord;
import top.niunaijun.blackbox.BlackBoxCore;
import top.niunaijun.blackbox.app.BActivityThread;
import top.niunaijun.blackbox.fake.hook.HookManager;
import top.niunaijun.blackbox.fake.hook.IInjectHook;
import top.niunaijun.blackbox.fake.service.HCallbackProxy;
import top.niunaijun.blackbox.fake.service.IActivityClientProxy;
import top.niunaijun.blackbox.utils.HackAppUtils;
import top.niunaijun.blackbox.utils.compat.ActivityCompat;
import top.niunaijun.blackbox.utils.compat.ActivityManagerCompat;
import top.niunaijun.blackbox.utils.compat.ContextCompat;

public final class AppInstrumentation extends BaseInstrumentationDelegate implements IInjectHook {

    private static final String TAG = AppInstrumentation.class.getSimpleName();

    private static AppInstrumentation sAppInstrumentation;

    public static AppInstrumentation get() {
        if (sAppInstrumentation == null) {
            synchronized (AppInstrumentation.class) {
                if (sAppInstrumentation == null) {
                    sAppInstrumentation = new AppInstrumentation();
                }
            }
        }
        return sAppInstrumentation;
    }

    public AppInstrumentation() {
    }

    @Override
    public void injectHook() {
        try {
            Instrumentation mInstrumentation = getCurrInstrumentation();
            if (mInstrumentation == this || checkInstrumentation(mInstrumentation))
                return;
            mBaseInstrumentation = (Instrumentation) mInstrumentation;
            BRActivityThread.get(BlackBoxCore.mainThread())._set_mInstrumentation(this);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private Instrumentation getCurrInstrumentation() {
        Object currentActivityThread = BlackBoxCore.mainThread();
        return BRActivityThread.get(currentActivityThread).mInstrumentation();
    }

    @Override
    public boolean isBadEnv() {
        return !checkInstrumentation(getCurrInstrumentation());
    }

    private boolean checkInstrumentation(Instrumentation instrumentation) {
        if (instrumentation instanceof AppInstrumentation) {
            return true;
        }
        Class<?> clazz = instrumentation.getClass();
        if (Instrumentation.class.equals(clazz)) {
            return false;
        }
        do {
            assert clazz != null;
            Field[] fields = clazz.getDeclaredFields();
            for (Field field : fields) {
                if (Instrumentation.class.isAssignableFrom(field.getType())) {
                    field.setAccessible(true);
                    try {
                        Object obj = field.get(instrumentation);
                        if ((obj instanceof AppInstrumentation)) {
                            return true;
                        }
                    } catch (Exception e) {
                        return false;
                    }
                }
            }
            clazz = clazz.getSuperclass();
        } while (!Instrumentation.class.equals(clazz));
        return false;
    }

    private void checkHCallback() {
        HookManager.get().checkEnv(HCallbackProxy.class);
    }

    private void checkActivity(Activity activity) {
        Log.d(TAG, "callActivityOnCreate: " + activity.getClass().getName());
        HackAppUtils.enableQQLogOutput(activity.getPackageName(), activity.getClassLoader());
        checkHCallback();
        HookManager.get().checkEnv(IActivityClientProxy.class);
        ActivityInfo info = BRActivity.get(activity).mActivityInfo();
        ContextCompat.fix(activity);
        ActivityCompat.fix(activity);
        if (info.theme != 0) {
            activity.getTheme().applyStyle(info.theme, true);
        }
        ActivityManagerCompat.setActivityOrientation(activity, info.screenOrientation);

        if ("com.facebook.CustomTabActivity".equals(activity.getClass().getName())) {
            Intent incoming = activity.getIntent();
            String redirectUrl = incoming != null ? incoming.getDataString() : null;
            Log.d(TAG, "[CustomTabActivity] Handling Facebook OAuth redirect: " + redirectUrl);
            if (redirectUrl != null) {
                try {
                    // 1. Send LocalBroadcast (action_customTabRedirect)
                    Intent broadcast = new Intent("CustomTabActivity.action_customTabRedirect");
                    broadcast.putExtra("CustomTabMainActivity.extra_url", redirectUrl);
                    try {
                        Class<?> lbmClass = activity.getClassLoader().loadClass("androidx.localbroadcastmanager.content.LocalBroadcastManager");
                        Method getInstance = lbmClass.getMethod("getInstance", Context.class);
                        Object lbm = getInstance.invoke(null, activity);
                        Method sendBroadcast = lbmClass.getMethod("sendBroadcast", Intent.class);
                        sendBroadcast.invoke(lbm, broadcast);
                        Log.d(TAG, "[CustomTabActivity] Sent LocalBroadcast for redirect");
                    } catch (Throwable t) {
                        Log.w(TAG, "[CustomTabActivity] LocalBroadcastManager reflection failed: " + t.getMessage());
                    }

                    // 2. Find target CustomTabMainActivity in this process
                    Activity targetMainAct = null;
                    try {
                        Map<IBinder, Object> mActivities = BRActivityThread.get(BlackBoxCore.mainThread()).mActivities();
                        for (Map.Entry<IBinder, Object> entry : mActivities.entrySet()) {
                            Object clientRecord = entry.getValue();
                            Activity act = BRActivityThreadActivityClientRecord.get(clientRecord).activity();
                            if (act != null && "com.facebook.CustomTabMainActivity".equals(act.getClass().getName())) {
                                targetMainAct = act;
                                break;
                            }
                        }
                    } catch (Throwable t) {
                        Log.w(TAG, "[CustomTabActivity] Error scanning mActivities: " + t.getMessage());
                    }

                    if (targetMainAct != null) {
                        Log.d(TAG, "[CustomTabActivity] Found live CustomTabMainActivity: " + targetMainAct);
                        // Prevent CustomTabMainActivity.onResume from calling sendResult(0, null)
                        try {
                            Field field = targetMainAct.getClass().getDeclaredField("shouldCloseCustomTab");
                            field.setAccessible(true);
                            field.setBoolean(targetMainAct, false);
                            Log.d(TAG, "[CustomTabActivity] Set shouldCloseCustomTab=false on targetMainAct");
                        } catch (Throwable t) {
                            Log.w(TAG, "[CustomTabActivity] Failed to clear shouldCloseCustomTab: " + t.getMessage());
                        }

                        // Call sendResult(-1, resultIntent) directly on CustomTabMainActivity
                        try {
                            Intent refreshIntent = new Intent(activity, targetMainAct.getClass());
                            refreshIntent.setAction("CustomTabMainActivity.action_refresh");
                            refreshIntent.putExtra("CustomTabMainActivity.extra_url", redirectUrl);

                            Method sendResultMethod = null;
                            for (Method m : targetMainAct.getClass().getDeclaredMethods()) {
                                if ("sendResult".equals(m.getName()) && m.getParameterTypes().length == 2) {
                                    sendResultMethod = m;
                                    break;
                                }
                            }
                            if (sendResultMethod != null) {
                                sendResultMethod.setAccessible(true);
                                sendResultMethod.invoke(targetMainAct, -1, refreshIntent);
                                Log.d(TAG, "[CustomTabActivity] Directly invoked sendResult(-1, refreshIntent) on CustomTabMainActivity!");
                            } else {
                                Method onNewIntent = Activity.class.getDeclaredMethod("onNewIntent", Intent.class);
                                onNewIntent.setAccessible(true);
                                onNewIntent.invoke(targetMainAct, refreshIntent);
                                Log.d(TAG, "[CustomTabActivity] Directly invoked onNewIntent on CustomTabMainActivity");
                            }
                        } catch (Throwable t) {
                            Log.w(TAG, "[CustomTabActivity] Direct invocation failed: " + t.getMessage(), t);
                        }
                    } else {
                        Log.w(TAG, "[CustomTabActivity] CustomTabMainActivity not found in mActivities, fallback to startActivity");
                        Class<?> mainActCls = activity.getClassLoader().loadClass("com.facebook.CustomTabMainActivity");
                        Intent refreshIntent = new Intent(activity, mainActCls);
                        refreshIntent.setAction("CustomTabMainActivity.action_refresh");
                        refreshIntent.putExtra("CustomTabMainActivity.extra_url", redirectUrl);
                        refreshIntent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
                        activity.startActivity(refreshIntent);
                    }
                } catch (Throwable t) {
                    Log.w(TAG, "[CustomTabActivity] Error handling redirect: " + t.getMessage(), t);
                }
            }
            activity.finish();
        }
    }

    @Override
    public Application newApplication(ClassLoader cl, String className, Context context) throws InstantiationException, IllegalAccessException, ClassNotFoundException {
        ContextCompat.fix(context);

        return super.newApplication(cl, className, context);
    }

    @Override
    public void callActivityOnCreate(Activity activity, Bundle icicle, PersistableBundle persistentState) {
        checkActivity(activity);
        super.callActivityOnCreate(activity, icicle, persistentState);
    }

    @Override
    public void callActivityOnCreate(Activity activity, Bundle icicle) {
        checkActivity(activity);
        super.callActivityOnCreate(activity, icicle);
    }

    @Override
    public void callApplicationOnCreate(Application app) {
        checkHCallback();
        super.callApplicationOnCreate(app);
    }

    public Activity newActivity(ClassLoader cl, String className, Intent intent) throws InstantiationException, IllegalAccessException, ClassNotFoundException {
        try {
            return super.newActivity(cl, className, intent);
        } catch (ClassNotFoundException e) {
            return mBaseInstrumentation.newActivity(cl, className, intent);
        }
    }
}
