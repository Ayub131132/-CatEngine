package top.niunaijun.blackbox.core.env;

import android.content.ComponentName;
import android.content.Intent;
import android.os.Build;

import java.util.ArrayList;
import java.util.List;

import top.niunaijun.blackbox.BlackBoxCore;
import top.niunaijun.blackbox.utils.compat.BuildCompat;


public class AppSystemEnv {
    private static final List<String> sSystemPackages = new ArrayList<>();
    private static final List<String> sSuPackages = new ArrayList<>();

    private static final List<String> sPreInstallPackages = new ArrayList<>();

    static {
        sSystemPackages.add("android");
        sSystemPackages.add("com.google.android.webview");
        sSystemPackages.add("com.google.android.webview.dev");
        sSystemPackages.add("com.google.android.webview.beta");
        sSystemPackages.add("com.google.android.webview.canary");
        sSystemPackages.add("com.android.webview");
        sSystemPackages.add("com.android.camera");
        sSystemPackages.add("com.android.talkback");
        sSystemPackages.add("com.miui.gallery");

        // Browsers & Web Engines
        sSystemPackages.add("com.android.chrome");
        sSystemPackages.add("com.chrome.beta");
        sSystemPackages.add("com.chrome.dev");
        sSystemPackages.add("com.chrome.canary");
        sSystemPackages.add("org.chromium.chrome");
        sSystemPackages.add("com.sec.android.app.sbrowser");
        sSystemPackages.add("com.vivo.browser");
        sSystemPackages.add("com.vivo.browser.service");
        sSystemPackages.add("com.heytap.browser");
        sSystemPackages.add("com.coloros.browser");
        sSystemPackages.add("com.mi.globalbrowser");
        sSystemPackages.add("com.android.browser");
        sSystemPackages.add("org.mozilla.firefox");
        sSystemPackages.add("com.opera.browser");
        sSystemPackages.add("com.opera.mini.native");
        sSystemPackages.add("com.brave.browser");
        sSystemPackages.add("com.microsoft.emmx");
        sSystemPackages.add("com.duckduckgo.mobile.android");
        sSystemPackages.add("mark.via.gp");

        // Facebook Native SSO packages excluded so games fallback to seamless Web / Chrome Custom Tabs OAuth
        // (Prevents Facebook Native signature mismatch error: "We are experiencing issues connecting to Facebook")

        // Trichrome & WebView components
        sSystemPackages.add("com.google.android.trichromelibrary");

        // GMS fallback packages
        sSystemPackages.add("com.google.android.gms");
        sSystemPackages.add("com.google.android.gsf");
        sSystemPackages.add("com.android.vending");
        sSystemPackages.add("com.google.android.play.games");

        sSystemPackages.add("com.google.android.inputmethod.latin");
        

        
        sSystemPackages.add("com.huawei.webview");

        
        sSystemPackages.add("com.coloros.safecenter");

        
        sSuPackages.add("com.noshufou.android.su");
        sSuPackages.add("com.noshufou.android.su.elite");
        sSuPackages.add("eu.chainfire.supersu");
        sSuPackages.add("com.koushikdutta.superuser");
        sSuPackages.add("com.thirdparty.superuser");
        sSuPackages.add("com.yellowes.su");



        
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N && Build.VERSION.SDK_INT < 29){
            
        }else{

        }
    }

    public static boolean isOpenPackage(String packageName) {
        return sSystemPackages.contains(packageName);
    }

    public static boolean isOpenPackage(ComponentName componentName) {
        return componentName != null && isOpenPackage(componentName.getPackageName());
    }

    public static boolean isBlackPackage(String packageName) {
        if (BlackBoxCore.get().isHideRoot() && sSuPackages.contains(packageName)) {
            return true;
        }
        return false;
    }

    public static boolean isGooglePlayGamesService(Intent intent) {
        if (intent == null) return false;
        if ("com.google.android.play.games".equals(intent.getPackage())) return true;
        String action = intent.getAction();
        if (action != null && (
                action.contains("com.google.android.gms.games") ||
                action.contains("games.internal") ||
                action.contains("appshortcuts") ||
                action.contains("measurement") ||
                action.contains("clearcut") ||
                action.contains("phenotype") ||
                action.contains("gass") ||
                action.contains("adshield") ||
                action.contains("policy_ads") ||
                action.contains("ads") ||
                action.contains("admob") ||
                action.contains("adservices") ||
                action.contains("chimera") ||
                action.contains("telemetry")
        )) {
            return true;
        }
        ComponentName comp = intent.getComponent();
        if (comp != null) {
            String pkg = comp.getPackageName();
            String cls = comp.getClassName();
            if ("com.google.android.play.games".equals(pkg)) return true;
            if (cls != null && (
                    cls.contains("games") ||
                    cls.contains("appshortcuts") ||
                    cls.contains("measurement") ||
                    cls.contains("clearcut") ||
                    cls.contains("phenotype") ||
                    cls.contains("gass") ||
                    cls.contains("adshield") ||
                    cls.contains("policy_ads") ||
                    cls.contains("ads") ||
                    cls.contains("admob") ||
                    cls.contains("chimera")
            )) {
                return true;
            }
        }
        return false;
    }

    public static List<String> getPreInstallPackages() {
        return sPreInstallPackages;
    }
}
