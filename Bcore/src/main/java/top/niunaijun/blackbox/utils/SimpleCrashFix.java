package top.niunaijun.blackbox.utils;

import android.content.Context;
import android.content.ContextWrapper;
import android.util.Log;
import android.app.Application;

import top.niunaijun.blackbox.BlackBoxCore;


public class SimpleCrashFix {
    private static final String TAG = "SimpleCrashFix";
    private static boolean sIsInstalled = false;

    
    public static void installSimpleFix() {
        if (sIsInstalled) {
            Slog.d(TAG, "Simple crash fix already installed");
            return;
        }
        
        try {
            Slog.d(TAG, "Installing essential crash fix...");
            
            
            installGlobalExceptionHandler();
            
            
            installContextWrapperHook();
            
            sIsInstalled = true;
            Slog.d(TAG, "Essential crash fix installed successfully");
        } catch (Exception e) {
            Slog.e(TAG, "Failed to install essential crash fix: " + e.getMessage(), e);
        }
    }
    
    
    private static void installGlobalExceptionHandler() {
        try {
            
            Thread.UncaughtExceptionHandler currentHandler = Thread.getDefaultUncaughtExceptionHandler();
            
            Thread.setDefaultUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
                @Override
                public void uncaughtException(Thread thread, Throwable throwable) {
                    
                    boolean isHandled = CrashMonitor.isIgnorableBackgroundCrash(thread, throwable) ||
                            isNullContextCrash(throwable) ||
                            isGooglePlayServicesCrash(throwable) ||
                            isWebViewCrash(throwable) ||
                            isAttributionSourceCrash(throwable) ||
                            isSocialMediaAppCrash(throwable) ||
                            isAdSdkNetworkCrash(throwable);

                    if (isHandled) {
                        Slog.w(TAG, "Prevented crash: " + throwable.getMessage());
                        if (android.os.Looper.myLooper() == android.os.Looper.getMainLooper()) {
                            while (true) {
                                try {
                                    android.os.Looper.loop();
                                    break;
                                } catch (Throwable t) {
                                    Slog.w(TAG, "Resuming main looper after handled crash: " + t.getMessage());
                                }
                            }
                        }
                        return;
                    }

                    
                    Slog.e(TAG, "Fatal crash detected, attempting to report before death...");
                    try {
                         BlackBoxCore.get().sendLogs("FATAL CRASH (Uncaught): " + throwable.getMessage(), false);
                    } catch (Throwable e) {
                         Slog.e(TAG, "Failed to report fatal crash: " + e.getMessage());
                    }

                    
                    if (currentHandler != null) {
                        currentHandler.uncaughtException(thread, throwable);
                    }
                }
            });
            
            Slog.d(TAG, "Global exception handler installed successfully");
        } catch (Exception e) {
            Slog.e(TAG, "Failed to install global exception handler: " + e.getMessage(), e);
        }
    }
    
    
    private static void installContextWrapperHook() {
        try {
            
            ContextWrapperHook.installHook();
            Slog.d(TAG, "Context wrapper hook installed");
        } catch (Exception e) {
            Slog.e(TAG, "Failed to install context wrapper hook: " + e.getMessage(), e);
        }
    }
    
    
    private static boolean isNullContextCrash(Throwable throwable) {
        if (throwable == null) {
            return false;
        }
        
        String message = throwable.getMessage();
        if (message != null) {
            return message.contains("Context") || 
                   message.contains("context") ||
                   message.contains("getResources") ||
                   message.contains("getPackageManager") ||
                   message.contains("getClassLoader");
        }
        
        
        StackTraceElement[] stackTrace = throwable.getStackTrace();
        if (stackTrace != null) {
            for (StackTraceElement element : stackTrace) {
                String className = element.getClassName();
                String methodName = element.getMethodName();
                
                if (className.contains("Context") || 
                    className.contains("ContextWrapper") ||
                    methodName.contains("getResources") ||
                    methodName.contains("getPackageManager") ||
                    methodName.contains("getClassLoader")) {
                    return true;
                }
            }
        }
        
        return false;
    }
    
    
    private static boolean isGooglePlayServicesCrash(Throwable throwable) {
        if (throwable == null) {
            return false;
        }
        
        String message = throwable.getMessage();
        if (message != null) {
            return message.contains("Google Play Services") ||
                   message.contains("GooglePlayServicesUtil") ||
                   message.contains("GoogleApiAvailability") ||
                   message.contains("com.google.android.gms") ||
                   message.contains("Unknown calling package name") ||
                   message.contains("measurement");
        }
        
        
        StackTraceElement[] stackTrace = throwable.getStackTrace();
        if (stackTrace != null) {
            for (StackTraceElement element : stackTrace) {
                String className = element.getClassName();
                if (className.contains("com.google.android.gms") ||
                    className.contains("GooglePlayServicesUtil") ||
                    className.contains("GoogleApiAvailability") ||
                    className.contains("measurement") ||
                    className.contains("dynamite_measurement")) {
                    return true;
                }
            }
        }
        
        return false;
    }

    
    private static boolean isWebViewCrash(Throwable throwable) {
        if (throwable == null) {
            return false;
        }
        
        String message = throwable.getMessage();
        if (message != null) {
            return message.contains("WebView") ||
                   message.contains("webview") ||
                   message.contains("WebViewDatabase") ||
                   message.contains("WebSettings") ||
                   message.contains("data directory");
        }
        
        
        StackTraceElement[] stackTrace = throwable.getStackTrace();
        if (stackTrace != null) {
            for (StackTraceElement element : stackTrace) {
                String className = element.getClassName();
                String methodName = element.getMethodName();
                if (className.contains("WebView") ||
                    className.contains("WebViewDatabase") ||
                    className.contains("WebSettings") ||
                    methodName.contains("webView") ||
                    methodName.contains("WebView")) {
                    return true;
                }
            }
        }
        
        return false;
    }

    
    private static boolean isAttributionSourceCrash(Throwable throwable) {
        if (throwable == null) {
            return false;
        }
        
        String message = throwable.getMessage();
        if (message != null) {
            return message.contains("AttributionSource") ||
                   message.contains("attribution") ||
                   message.contains("Calling uid") ||
                   message.contains("source uid") ||
                   message.contains("UID mismatch");
        }
        
        
        StackTraceElement[] stackTrace = throwable.getStackTrace();
        if (stackTrace != null) {
            for (StackTraceElement element : stackTrace) {
                String className = element.getClassName();
                String methodName = element.getMethodName();
                if (className.contains("AttributionSource") ||
                    className.contains("ContentProvider") ||
                    methodName.contains("enforceCallingUid") ||
                    methodName.contains("enforceCallingUidAndPid")) {
                    return true;
                }
            }
        }
        
        return false;
    }

    
    private static boolean isSocialMediaAppCrash(Throwable throwable) {
        if (throwable == null) {
            return false;
        }
        
        String message = throwable.getMessage();
        if (message != null) {
            return message.contains("Facebook") ||
                   message.contains("Instagram") ||
                   message.contains("WhatsApp") ||
                   message.contains("Telegram") ||
                   message.contains("Twitter") ||
                   message.contains("TikTok") ||
                   message.contains("Snapchat") ||
                   message.contains("YouTube") ||
                   message.contains("LinkedIn");
        }
        
        
        StackTraceElement[] stackTrace = throwable.getStackTrace();
        if (stackTrace != null) {
            for (StackTraceElement element : stackTrace) {
                String className = element.getClassName();
                if (className.contains("com.facebook") ||
                    className.contains("com.instagram") ||
                    className.contains("com.whatsapp") ||
                    className.contains("org.telegram") ||
                    className.contains("com.twitter") ||
                    className.contains("com.zhiliaoapp.musically") ||
                    className.contains("com.snapchat") ||
                    className.contains("com.google.android.youtube") ||
                    className.contains("com.linkedin")) {
                    return true;
                }
            }
        }
        
        return false;
    }

    private static boolean isAdSdkNetworkCrash(Throwable throwable) {
        if (throwable == null) {
            return false;
        }

        Throwable cause = throwable;
        while (cause != null) {
            String message = cause.getMessage();
            if (message != null) {
                if (message.contains("applovin") ||
                    message.contains("AppLovin") ||
                    message.contains("ConnectException") ||
                    message.contains("Failed to connect to") ||
                    message.contains("admob") ||
                    message.contains("unity3d") ||
                    message.contains("ironsource")) {
                    return true;
                }
            }

            StackTraceElement[] stackTrace = cause.getStackTrace();
            if (stackTrace != null) {
                for (StackTraceElement element : stackTrace) {
                    String className = element.getClassName();
                    if (className != null) {
                        if (className.contains("com.applovin") ||
                            className.contains("com.google.android.gms.ads") ||
                            className.contains("com.unity3d") ||
                            className.contains("com.ironsource") ||
                            className.contains("com.vungle") ||
                            className.contains("com.mbridge") ||
                            className.contains("com.mopub") ||
                            className.contains("com.chartboost") ||
                            className.contains("com.adcolony") ||
                            className.contains("com.liftoff") ||
                            className.contains("b4$b") ||
                            className.contains("RealConnection") ||
                            className.contains("HttpEngine")) {
                            return true;
                        }
                    }
                }
            }
            cause = cause.getCause();
        }

        return false;
    }
}
