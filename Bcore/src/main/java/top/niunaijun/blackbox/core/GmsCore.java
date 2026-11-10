package top.niunaijun.blackbox.core;

import android.content.pm.PackageManager;

import java.util.HashSet;
import java.util.Set;

import top.niunaijun.blackbox.BlackBoxCore;
import top.niunaijun.blackbox.entity.pm.InstallResult;


public class GmsCore {
    private static final String TAG = "GmsCore";

    private static final HashSet<String> GOOGLE_APP = new HashSet<>();
    private static final HashSet<String> GOOGLE_SERVICE = new HashSet<>();
    public static final String GMS_PKG = "com.google.android.gms";
    public static final String GSF_PKG = "com.google.android.gsf";
    public static final String VENDING_PKG = "com.android.vending";

    static {
        // Essential Google Apps
        GOOGLE_APP.add(VENDING_PKG);
        GOOGLE_APP.add("com.google.android.play.games");

        // Essential Google Services for Authentication and API Broker
        GOOGLE_SERVICE.add(GMS_PKG);
        GOOGLE_SERVICE.add(GSF_PKG);
    }

    public static boolean isGoogleService(String packageName) {
        return GOOGLE_SERVICE.contains(packageName);
    }

    public static boolean isGoogleAppOrService(String str) {
        return GOOGLE_APP.contains(str) || GOOGLE_SERVICE.contains(str);
    }

    private static InstallResult installPackages(Set<String> list, int userId) {
        BlackBoxCore blackBoxCore = BlackBoxCore.get();
        InstallResult lastResult = new InstallResult();
        for (String packageName : list) {
            if (blackBoxCore.isInstalled(packageName, userId)) {
                continue;
            }
            try {
                BlackBoxCore.getContext().getPackageManager().getApplicationInfo(packageName, 0);
            } catch (PackageManager.NameNotFoundException e) {
                // Not present on device host, skip
                continue;
            }
            InstallResult installResult = blackBoxCore.installPackageAsUser(packageName, userId);
            if (!installResult.success) {
                top.niunaijun.blackbox.utils.Slog.w(TAG, "Optional GMS package failed to install: " + packageName + " (" + installResult.msg + ")");
                // Only fail if core GMS itself failed
                if (GMS_PKG.equals(packageName)) {
                    return installResult;
                }
            } else {
                lastResult = installResult;
            }
        }
        return lastResult;
    }

    private static void uninstallPackages(Set<String> list, int userId) {
        BlackBoxCore blackBoxCore = BlackBoxCore.get();
        for (String packageName : list) {
            blackBoxCore.uninstallPackageAsUser(packageName, userId);
        }
    }

    public static InstallResult installGApps(int userId) {
        // 1. Install Google Services (com.google.android.gms and com.google.android.gsf)
        InstallResult serviceResult = installPackages(GOOGLE_SERVICE, userId);
        if (!serviceResult.success && !isInstalledGoogleService(userId)) {
            top.niunaijun.blackbox.utils.Slog.e(TAG, "Failed to install core GMS: " + serviceResult.msg);
            return serviceResult;
        }

        // 2. Optionally install Google Apps (Play Store, Play Games) if available on host
        installPackages(GOOGLE_APP, userId);

        return new InstallResult();
    }

    public static void uninstallGApps(int userId) {
        uninstallPackages(GOOGLE_SERVICE, userId);
        uninstallPackages(GOOGLE_APP, userId);
    }

    public static void remove(String packageName) {
        GOOGLE_SERVICE.remove(packageName);
        GOOGLE_APP.remove(packageName);
    }


    public static boolean isSupportGms() {
        try {
            BlackBoxCore.getPackageManager().getPackageInfo(GMS_PKG, 0);
            return true;
        } catch (PackageManager.NameNotFoundException ignored) {
        }
        return false;
    }

    public static boolean isInstalledGoogleService(int userId) {
        return BlackBoxCore.get().isInstalled(GMS_PKG, userId);
    }
}