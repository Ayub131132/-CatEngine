package top.niunaijun.blackbox.utils.compat;

import android.app.ActivityManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;

import java.util.Locale;

import top.niunaijun.blackbox.BlackBoxCore;
import top.niunaijun.blackbox.app.BActivityThread;
import top.niunaijun.blackbox.utils.DrawableUtils;

public class TaskDescriptionCompat {
    public static ActivityManager.TaskDescription fix(ActivityManager.TaskDescription td) {
        CharSequence rawLabel = td.getLabel();
        if (rawLabel == null || rawLabel.length() == 0) {
            rawLabel = getApplicationLabel();
        }
        String label = getTaskDescriptionLabel(BlackBoxCore.getUserId(), rawLabel);

        Bitmap icon = td.getIcon();
        if (icon == null) {
            Drawable drawable = getApplicationIcon();
            if (drawable != null && BlackBoxCore.getContext() != null) {
                ActivityManager am = (ActivityManager) BlackBoxCore.getContext().getSystemService(Context.ACTIVITY_SERVICE);
                if (am != null) {
                    int iconSize = am.getLauncherLargeIconSize();
                    icon = DrawableUtils.drawableToBitmap(drawable, iconSize, iconSize);
                }
            }
        }
        return new ActivityManager.TaskDescription(label, icon, td.getPrimaryColor());
    }

    public static String getTaskDescriptionLabel(int userId, CharSequence label) {
        if (label == null || label.length() == 0) {
            label = getApplicationLabel();
        }
        if (label == null || label.length() == 0) {
            return "[Cat]";
        }
        // Remove existing [B0], [b0], [B%d], or [Cat] prefixes if any
        String cleanLabel = label.toString().replaceAll("^\\[[bB]\\d+\\]\\s*", "").trim();
        if (cleanLabel.endsWith("[Cat]")) {
            return cleanLabel;
        }
        if (userId > 0) {
            return String.format(Locale.ENGLISH, "%s (%d) [Cat]", cleanLabel, userId);
        }
        return String.format(Locale.ENGLISH, "%s [Cat]", cleanLabel);
    }

    private static CharSequence getApplicationLabel() {
        try {
            PackageManager pm = BlackBoxCore.getPackageManager();
            return pm.getApplicationLabel(pm.getApplicationInfo(BlackBoxCore.getAppPackageName(), 0));
        } catch (PackageManager.NameNotFoundException e) {
            return null;
        }
    }

    private static Drawable getApplicationIcon() {
        try {
            PackageManager pm = BlackBoxCore.getPackageManager();
            return pm.getApplicationIcon(BlackBoxCore.getAppPackageName());
        } catch (Exception ignore) {
            return null;
        }
    }
}
