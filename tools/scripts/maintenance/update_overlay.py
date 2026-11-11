import os

file_path = r"C:\Users\ayubg\AndroidStudioProjects\NewBlackbox-main1\NewBlackbox-main\Bcore\src\main\java\top\niunaijun\blackbox\widget\BoardCalibrationOverlay.java"

content = """package top.niunaijun.blackbox.widget;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Application;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.DisplayMetrics;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

import java.lang.ref.WeakReference;

import top.niunaijun.blackbox.core.NativeCore;

public class BoardCalibrationOverlay {
    private static final String TAG = "BoardCalibration";
    private static final String PREF_NAME = "blackbox_carrom_calib";
    private static final String KEY_LEFT = "calib_left";
    private static final String KEY_RIGHT = "calib_right";
    private static final String KEY_BOTTOM = "calib_bottom";
    private static final String KEY_TOP = "calib_top";
    private static final String KEY_SHOW_BORDER = "calib_show_border";
    private static final String KEY_THICKNESS = "calib_thickness";
    private static final String KEY_SAVED = "calib_saved";

    // Live physics fine-tuning keys
    private static final String KEY_STRIKER_DAMPING = "calib_striker_damping";
    private static final String KEY_PUCK_DAMPING = "calib_puck_damping";
    private static final String KEY_RESTITUTION_BODY = "calib_rest_body";
    private static final String KEY_RESTITUTION_CUSHION = "calib_rest_cushion";
    private static final String KEY_STRIKER_MASS = "calib_striker_mass";
    private static final String KEY_POWER_MULTIPLIER = "calib_power_mult";

    private static boolean sInstalled = false;
    private static WeakReference<Activity> sCurrentActivity = null;
    private static FloatingButtonView sFloatingButton = null;
    private static AlertDialog sActiveDialog = null;

    // User-verified default board bounds: Left=20px, Right=714px, Bottom=452px, Top=1146px
    private static float sLeft = 20.0f;
    private static float sRight = 714.0f;
    private static float sBottom = 452.0f;
    private static float sTop = 1146.0f;

    // Fixed calibrated geometry for Carrom Pool (in Centimeters)
    private static final float STRIKER_RADIUS_CM = 2.65f;
    private static final float PUCK_RADIUS_CM = 1.95f;
    private static final float POCKET_INSET_CM = 32.80f;
    private static final float POCKET_RADIUS_CM = 4.00f;

    // Guideline visual styling defaults
    private static boolean sShowBorder = false;
    private static float sGuidelineThickness = 0.70f;
    private static final int DEFAULT_COLOR_THEME = 0; // Neon Green / Classic Laser

    // Live physics fine-tuning defaults
    private static float sStrikerDamping = 0.9850f;
    private static float sPuckDamping = 0.9888f;
    private static float sRestitutionBody = 0.92f;
    private static float sRestitutionCushion = 0.88f;
    private static float sStrikerMass = 3.00f;
    private static float sPowerMultiplier = 1.00f;

    private static boolean sHasSavedBounds = true;

    private static final Handler sMainHandler = new Handler(Looper.getMainLooper());

    public static synchronized void install(Application app) {
        if (sInstalled || app == null) return;
        sInstalled = true;
        Log.i(TAG, "Installing BoardCalibrationOverlay lifecycle callback");

        app.registerActivityLifecycleCallbacks(new Application.ActivityLifecycleCallbacks() {
            @Override
            public void onActivityCreated(Activity activity, Bundle savedInstanceState) {}

            @Override
            public void onActivityStarted(Activity activity) {}

            @Override
            public void onActivityResumed(Activity activity) {
                BoardCalibrationOverlay.onActivityResumed(activity);
            }

            @Override
            public void onActivityPaused(Activity activity) {
                BoardCalibrationOverlay.onActivityPaused(activity);
            }

            @Override
            public void onActivityStopped(Activity activity) {}

            @Override
            public void onActivitySaveInstanceState(Activity activity, Bundle outState) {}

            @Override
            public void onActivityDestroyed(Activity activity) {
                BoardCalibrationOverlay.onActivityDestroyed(activity);
            }
        });
    }

    public static void onActivityResumed(Activity activity) {
        if (activity == null || activity.isFinishing()) return;
        sCurrentActivity = new WeakReference<>(activity);

        // Load calibration from SharedPreferences
        loadCalibration(activity);

        // Always lock and apply verified bounds, object geometry, thickness, and physics parameters to native engine
        NativeCore.setBoardBounds(sLeft, sRight, sBottom, sTop, sShowBorder);
        NativeCore.setObjectGeometry(STRIKER_RADIUS_CM, PUCK_RADIUS_CM, POCKET_INSET_CM, POCKET_RADIUS_CM);
        NativeCore.setGuidelineThickness(sGuidelineThickness);
        NativeCore.setColorTheme(DEFAULT_COLOR_THEME);
        NativeCore.setFineTunePhysics(sStrikerDamping, sPuckDamping, sRestitutionBody, sRestitutionCushion, sStrikerMass, sPowerMultiplier);

        sMainHandler.post(() -> attachFloatingButton(activity));
    }

    public static void onActivityPaused(Activity activity) {
        detachFloatingButton(activity);
        if (sActiveDialog != null && sActiveDialog.isShowing()) {
            sActiveDialog.dismiss();
            sActiveDialog = null;
        }
    }

    public static void onActivityDestroyed(Activity activity) {
        detachFloatingButton(activity);
        if (sCurrentActivity != null && sCurrentActivity.get() == activity) {
            sCurrentActivity = null;
        }
    }

    private static void loadCalibration(Context context) {
        SharedPreferences sp = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
        sHasSavedBounds = sp.getBoolean(KEY_SAVED, true);

        DisplayMetrics dm = context.getResources().getDisplayMetrics();
        int screenW = dm.widthPixels;
        int screenH = dm.heightPixels;

        float defaultLeft = 20.0f;
        float defaultBottom = 452.0f;
        float defaultTop = 1146.0f;
        float boardH = defaultTop - defaultBottom; // 694.0f
        float defaultRight = (screenW > 714) ? (screenW - 20.0f) : (defaultLeft + boardH);

        sLeft = sp.getFloat(KEY_LEFT, defaultLeft);
        sRight = sp.getFloat(KEY_RIGHT, defaultRight);
        sBottom = sp.getFloat(KEY_BOTTOM, defaultBottom);
        sTop = sp.getFloat(KEY_TOP, defaultTop);

        sShowBorder = sp.getBoolean(KEY_SHOW_BORDER, false);
        sGuidelineThickness = sp.getFloat(KEY_THICKNESS, 0.70f);

        sStrikerDamping = sp.getFloat(KEY_STRIKER_DAMPING, 0.9850f);
        sPuckDamping = sp.getFloat(KEY_PUCK_DAMPING, 0.9888f);
        sRestitutionBody = sp.getFloat(KEY_RESTITUTION_BODY, 0.92f);
        sRestitutionCushion = sp.getFloat(KEY_RESTITUTION_CUSHION, 0.88f);
        sStrikerMass = sp.getFloat(KEY_STRIKER_MASS, 3.00f);
        sPowerMultiplier = sp.getFloat(KEY_POWER_MULTIPLIER, 1.00f);
    }

    private static void saveCalibration(Context context, float left, float right, float bottom, float top,
                                       float thickness, boolean showBorder,
                                       float strikerDamping, float puckDamping,
                                       float restBody, float restCushion,
                                       float strikerMass, float powerMultiplier) {
        sLeft = left;
        sRight = right;
        sBottom = bottom;
        sTop = top;
        sGuidelineThickness = thickness;
        sShowBorder = showBorder;
        sStrikerDamping = strikerDamping;
        sPuckDamping = puckDamping;
        sRestitutionBody = restBody;
        sRestitutionCushion = restCushion;
        sStrikerMass = strikerMass;
        sPowerMultiplier = powerMultiplier;
        sHasSavedBounds = true;

        SharedPreferences sp = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
        sp.edit()
                .putFloat(KEY_LEFT, left)
                .putFloat(KEY_RIGHT, right)
                .putFloat(KEY_BOTTOM, bottom)
                .putFloat(KEY_TOP, top)
                .putFloat(KEY_THICKNESS, thickness)
                .putBoolean(KEY_SHOW_BORDER, showBorder)
                .putFloat(KEY_STRIKER_DAMPING, strikerDamping)
                .putFloat(KEY_PUCK_DAMPING, puckDamping)
                .putFloat(KEY_RESTITUTION_BODY, restBody)
                .putFloat(KEY_RESTITUTION_CUSHION, restCushion)
                .putFloat(KEY_STRIKER_MASS, strikerMass)
                .putFloat(KEY_POWER_MULTIPLIER, powerMultiplier)
                .putBoolean(KEY_SAVED, true)
                .apply();
        Log.i(TAG, String.format(java.util.Locale.US,
                "Calibration saved: L=%.1f R=%.1f B=%.1f T=%.1f Thick=%.2f ShowBorder=%b " +
                "DampSt=%.4f DampPk=%.4f RestBody=%.2f RestCush=%.2f Mass=%.2f Pow=%.2f",
                left, right, bottom, top, thickness, showBorder,
                strikerDamping, puckDamping, restBody, restCushion, strikerMass, powerMultiplier));
    }

    private static void attachFloatingButton(Activity activity) {
        if (activity == null || activity.isFinishing()) return;
        try {
            ViewGroup decor = (ViewGroup) activity.getWindow().getDecorView();
            if (sFloatingButton != null && sFloatingButton.getParent() != null) {
                ((ViewGroup) sFloatingButton.getParent()).removeView(sFloatingButton);
            }

            int btnSize = dpToPx(activity, 52);
            FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(btnSize, btnSize);
            lp.gravity = Gravity.TOP | Gravity.START;
            lp.leftMargin = dpToPx(activity, 16);
            lp.topMargin = dpToPx(activity, 120);

            if (sFloatingButton == null) {
                sFloatingButton = new FloatingButtonView(activity);
            }

            sFloatingButton.setLayoutParams(lp);
            sFloatingButton.setOnClickListener(v -> showCalibrationDialog(activity));

            decor.addView(sFloatingButton);
            Log.d(TAG, "Floating button attached to activity: " + activity.getClass().getSimpleName());
        } catch (Exception e) {
            Log.e(TAG, "Failed to attach floating button", e);
        }
    }

    private static void detachFloatingButton(Activity activity) {
        try {
            if (sFloatingButton != null && sFloatingButton.getParent() != null) {
                ((ViewGroup) sFloatingButton.getParent()).removeView(sFloatingButton);
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to detach floating button", e);
        }
    }

    public static void showCalibrationDialog(Activity activity) {
        if (activity == null || activity.isFinishing()) return;

        // Dismiss any existing active dialog first to prevent orphaned/stacked dialogs
        if (sActiveDialog != null) {
            try {
                if (sActiveDialog.isShowing()) {
                    sActiveDialog.dismiss();
                }
            } catch (Exception ignored) {}
            sActiveDialog = null;
        }

        DisplayMetrics dm = activity.getResources().getDisplayMetrics();
        int screenW = dm.widthPixels;
        int screenH = dm.heightPixels;

        // Current working bounds & parameters
        final float[] bounds = new float[]{sLeft, sRight, sBottom, sTop};
        final float[] lineThickness = new float[]{sGuidelineThickness};
        final boolean[] showLines = new boolean[]{sShowBorder};
        final float[] physics = new float[]{
            sStrikerDamping,
            sPuckDamping,
            sRestitutionBody,
            sRestitutionCushion,
            sStrikerMass,
            sPowerMultiplier
        };

        // Immediately apply in OpenGL
        NativeCore.setBoardBounds(bounds[0], bounds[1], bounds[2], bounds[3], showLines[0]);
        NativeCore.setObjectGeometry(STRIKER_RADIUS_CM, PUCK_RADIUS_CM, POCKET_INSET_CM, POCKET_RADIUS_CM);
        NativeCore.setGuidelineThickness(lineThickness[0]);
        NativeCore.setColorTheme(DEFAULT_COLOR_THEME);
        NativeCore.setFineTunePhysics(physics[0], physics[1], physics[2], physics[3], physics[4], physics[5]);

        // Build UI programmatically
        ScrollView scrollView = new ScrollView(activity);
        scrollView.setFillViewport(true);

        LinearLayout contentLayout = new LinearLayout(activity);
        contentLayout.setOrientation(LinearLayout.VERTICAL);
        int pad = dpToPx(activity, 16);
        contentLayout.setPadding(pad, pad, pad, pad);

        // 90% transparent background (10% opacity: #1A) so the carrom board is clearly visible through the dialog
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(Color.parseColor("#1A12121A"));
        bg.setCornerRadius(dpToPx(activity, 16));
        bg.setStroke(dpToPx(activity, 1), Color.parseColor("#80FFD700"));
        contentLayout.setBackground(bg);

        // Header Title (Opaque Gold with shadow for crisp readability)
        TextView tvTitle = new TextView(activity);
        tvTitle.setText("🎯 BlackBox Board Calibration");
        tvTitle.setTextSize(TypedValue.COMPLEX_UNIT_SP, 17);
        tvTitle.setTextColor(Color.parseColor("#FFD700"));
        tvTitle.setTypeface(tvTitle.getTypeface(), android.graphics.Typeface.BOLD);
        tvTitle.setShadowLayer(4f, 0f, 2f, Color.BLACK);
        tvTitle.setGravity(Gravity.CENTER);
        contentLayout.addView(tvTitle);

        TextView tvSubtitle = new TextView(activity);
        tvSubtitle.setText("Adjust cushion bounds and live physics until visual guidelines match real game shots.");
        tvSubtitle.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        tvSubtitle.setTextColor(Color.parseColor("#FFFFFF"));
        tvSubtitle.setShadowLayer(3f, 0f, 1f, Color.BLACK);
        tvSubtitle.setGravity(Gravity.CENTER);
        tvSubtitle.setPadding(0, dpToPx(activity, 4), 0, dpToPx(activity, 10));
        contentLayout.addView(tvSubtitle);

        // --- CUSHION SLIDERS ---
        SliderControl scLeft = new SliderControl(activity, "Left Cushion (X)", 0, screenW, (int) bounds[0], val -> {
            bounds[0] = val;
            NativeCore.setBoardBounds(bounds[0], bounds[1], bounds[2], bounds[3], showLines[0]);
        });
        contentLayout.addView(scLeft.view);

        SliderControl scRight = new SliderControl(activity, "Right Cushion (X)", 0, screenW, (int) bounds[1], val -> {
            bounds[1] = val;
            NativeCore.setBoardBounds(bounds[0], bounds[1], bounds[2], bounds[3], showLines[0]);
        });
        contentLayout.addView(scRight.view);

        SliderControl scBottom = new SliderControl(activity, "Bottom Cushion (Y)", 0, screenH, (int) bounds[2], val -> {
            bounds[2] = val;
            NativeCore.setBoardBounds(bounds[0], bounds[1], bounds[2], bounds[3], showLines[0]);
        });
        contentLayout.addView(scBottom.view);

        SliderControl scTop = new SliderControl(activity, "Top Cushion (Y)", 0, screenH, (int) bounds[3], val -> {
            bounds[3] = val;
            NativeCore.setBoardBounds(bounds[0], bounds[1], bounds[2], bounds[3], showLines[0]);
        });
        contentLayout.addView(scTop.view);

        // --- LIVE PHYSICS FINE-TUNING SECTION ---
        TextView tvPhysicsHeader = new TextView(activity);
        tvPhysicsHeader.setText("⚡ Live Physics Fine-Tuning");
        tvPhysicsHeader.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
        tvPhysicsHeader.setTextColor(Color.parseColor("#00E5FF"));
        tvPhysicsHeader.setTypeface(tvPhysicsHeader.getTypeface(), android.graphics.Typeface.BOLD);
        tvPhysicsHeader.setShadowLayer(4f, 0f, 2f, Color.BLACK);
        tvPhysicsHeader.setPadding(0, dpToPx(activity, 10), 0, dpToPx(activity, 4));
        contentLayout.addView(tvPhysicsHeader);

        TextView tvPhysicsDesc = new TextView(activity);
        tvPhysicsDesc.setText("Adjust rolling drag, bounce elasticities, mass ratio, and power multiplier to match exact physical shot travel.");
        tvPhysicsDesc.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        tvPhysicsDesc.setTextColor(Color.parseColor("#A0E0FF"));
        tvPhysicsDesc.setShadowLayer(2f, 0f, 1f, Color.BLACK);
        tvPhysicsDesc.setPadding(0, 0, 0, dpToPx(activity, 6));
        contentLayout.addView(tvPhysicsDesc);

        // 1. Striker Rolling Damping (0.9500 to 0.9990, default 0.9850, step 0.0001)
        FloatSliderControl scStrikerDamping = new FloatSliderControl(
                activity, "Striker Rolling Damping (Higher = Longer Line)",
                0.9500f, 0.9990f, physics[0], "%.4f", 490, "#FF55AA", val -> {
            physics[0] = val;
            NativeCore.setFineTunePhysics(physics[0], physics[1], physics[2], physics[3], physics[4], physics[5]);
        });
        contentLayout.addView(scStrikerDamping.view);

        // 2. Puck Rolling Damping (0.9500 to 0.9990, default 0.9888, step 0.0001)
        FloatSliderControl scPuckDamping = new FloatSliderControl(
                activity, "Puck Rolling Damping (Target Travel Distance)",
                0.9500f, 0.9990f, physics[1], "%.4f", 490, "#FF55AA", val -> {
            physics[1] = val;
            NativeCore.setFineTunePhysics(physics[0], physics[1], physics[2], physics[3], physics[4], physics[5]);
        });
        contentLayout.addView(scPuckDamping.view);

        // 3. Disc Collision Restitution e_body (0.50 to 1.00, default 0.92, step 0.01)
        FloatSliderControl scRestBody = new FloatSliderControl(
                activity, "Disc Elasticity (Collision Bounce Energy)",
                0.50f, 1.00f, physics[2], "%.2f", 50, "#00FF88", val -> {
            physics[2] = val;
            NativeCore.setFineTunePhysics(physics[0], physics[1], physics[2], physics[3], physics[4], physics[5]);
        });
        contentLayout.addView(scRestBody.view);

        // 4. Cushion Bounce Restitution e_cushion (0.50 to 1.00, default 0.88, step 0.01)
        FloatSliderControl scRestCushion = new FloatSliderControl(
                activity, "Cushion Elasticity (Wall Bounce Energy)",
                0.50f, 1.00f, physics[3], "%.2f", 50, "#00FF88", val -> {
            physics[3] = val;
            NativeCore.setFineTunePhysics(physics[0], physics[1], physics[2], physics[3], physics[4], physics[5]);
        });
        contentLayout.addView(scRestCushion.view);

        // 5. Striker Mass Ratio (1.00 to 5.00, default 3.00, step 0.05)
        FloatSliderControl scStrikerMass = new FloatSliderControl(
                activity, "Striker Mass Ratio (Striker : Puck)",
                1.00f, 5.00f, physics[4], "%.2f : 1", 80, "#FFD700", val -> {
            physics[4] = val;
            NativeCore.setFineTunePhysics(physics[0], physics[1], physics[2], physics[3], physics[4], physics[5]);
        });
        contentLayout.addView(scStrikerMass.view);

        // 6. Shot Power Multiplier (0.50x to 2.50x, default 1.00x, step 0.02)
        FloatSliderControl scPowerMult = new FloatSliderControl(
                activity, "Shot Power Multiplier (Global Launch Velocity)",
                0.50f, 2.50f, physics[5], "%.2fx", 100, "#00E5FF", val -> {
            physics[5] = val;
            NativeCore.setFineTunePhysics(physics[0], physics[1], physics[2], physics[3], physics[4], physics[5]);
        });
        contentLayout.addView(scPowerMult.view);

        // --- GUIDELINE STYLING SECTION ---
        TextView tvStyleHeader = new TextView(activity);
        tvStyleHeader.setText("✨ Guideline Styling (Thickness)");
        tvStyleHeader.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
        tvStyleHeader.setTextColor(Color.parseColor("#FFD700"));
        tvStyleHeader.setTypeface(tvStyleHeader.getTypeface(), android.graphics.Typeface.BOLD);
        tvStyleHeader.setShadowLayer(4f, 0f, 2f, Color.BLACK);
        tvStyleHeader.setPadding(0, dpToPx(activity, 10), 0, dpToPx(activity, 4));
        contentLayout.addView(tvStyleHeader);

        FloatSliderControl scThickness = new FloatSliderControl(
                activity, "Guideline Thickness",
                0.20f, 1.50f, lineThickness[0], "%.2f", 130, "#FFD700", val -> {
            lineThickness[0] = val;
            NativeCore.setGuidelineThickness(val);
        });
        contentLayout.addView(scThickness.view);

        // Border Line Toggle CheckBox
        CheckBox cbShowBorder = new CheckBox(activity);
        cbShowBorder.setText("Show Thick Yellow Border Lines");
        cbShowBorder.setTextColor(Color.WHITE);
        cbShowBorder.setTypeface(cbShowBorder.getTypeface(), android.graphics.Typeface.BOLD);
        cbShowBorder.setShadowLayer(3f, 0f, 1f, Color.BLACK);
        cbShowBorder.setButtonTintList(ColorStateList.valueOf(Color.parseColor("#FFD700")));
        cbShowBorder.setChecked(showLines[0]);
        cbShowBorder.setPadding(dpToPx(activity, 8), dpToPx(activity, 8), dpToPx(activity, 8), dpToPx(activity, 8));
        cbShowBorder.setOnCheckedChangeListener((btn, isChecked) -> {
            showLines[0] = isChecked;
            NativeCore.setBoardBounds(bounds[0], bounds[1], bounds[2], bounds[3], isChecked);
        });
        contentLayout.addView(cbShowBorder);

        // Auto-Detect Button (Direct Engine Auto-Calibration)
        Button btnAutoDetect = new Button(activity);
        btnAutoDetect.setText("⚡ Auto-Detect (Lock to Game Engine)");
        btnAutoDetect.setTextColor(Color.parseColor("#00E5FF"));
        btnAutoDetect.setTypeface(btnAutoDetect.getTypeface(), android.graphics.Typeface.BOLD);
        btnAutoDetect.setBackground(createButtonBg("#1A2B3C", "#00E5FF", dpToPx(activity, 8)));
        LinearLayout.LayoutParams lpAuto = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lpAuto.setMargins(0, dpToPx(activity, 8), 0, dpToPx(activity, 4));
        btnAutoDetect.setLayoutParams(lpAuto);
        btnAutoDetect.setOnClickListener(v -> {
            SharedPreferences sp = activity.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
            sp.edit().clear().apply();
            sHasSavedBounds = false;
            NativeCore.resetBoardCalibration();

            float defL = 20.0f;
            float defB = 452.0f;
            float defT = 1146.0f;
            float defR = (screenW > 714) ? (screenW - 20.0f) : (defL + (defT - defB));

            bounds[0] = defL;
            bounds[1] = defR;
            bounds[2] = defB;
            bounds[3] = defT;

            scLeft.setValue((int) defL);
            scRight.setValue((int) defR);
            scBottom.setValue((int) defB);
            scTop.setValue((int) defT);

            physics[0] = 0.9850f;
            physics[1] = 0.9888f;
            physics[2] = 0.92f;
            physics[3] = 0.88f;
            physics[4] = 3.00f;
            physics[5] = 1.00f;

            scStrikerDamping.setValue(physics[0]);
            scPuckDamping.setValue(physics[1]);
            scRestBody.setValue(physics[2]);
            scRestCushion.setValue(physics[3]);
            scStrikerMass.setValue(physics[4]);
            scPowerMult.setValue(physics[5]);

            NativeCore.setBoardBounds(bounds[0], bounds[1], bounds[2], bounds[3], showLines[0]);
            NativeCore.setFineTunePhysics(physics[0], physics[1], physics[2], physics[3], physics[4], physics[5]);
            if (sActiveDialog != null) {
                try {
                    sActiveDialog.dismiss();
                } catch (Exception ignored) {}
                sActiveDialog = null;
            }
            Toast.makeText(activity, "⚡ Auto-Detection Active! Locked to Carrom Pool projection.", Toast.LENGTH_SHORT).show();
        });
        contentLayout.addView(btnAutoDetect);

        // Quick Vertical Shift Buttons Row (Nudge Up / Down)
        LinearLayout nudgeRow = new LinearLayout(activity);
        nudgeRow.setOrientation(LinearLayout.HORIZONTAL);
        nudgeRow.setPadding(0, dpToPx(activity, 8), 0, 0);

        Button btnNudgeUp = new Button(activity);
        btnNudgeUp.setText("▲ Shift Board UP (+20px)");
        btnNudgeUp.setTextColor(Color.WHITE);
        btnNudgeUp.setBackground(createButtonBg("#252535", "#8888AA", dpToPx(activity, 8)));
        LinearLayout.LayoutParams lpNudgeUp = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
        lpNudgeUp.rightMargin = dpToPx(activity, 6);
        btnNudgeUp.setLayoutParams(lpNudgeUp);
        btnNudgeUp.setOnClickListener(v -> {
            bounds[2] += 20;
            bounds[3] += 20;
            scBottom.setValue((int) bounds[2]);
            scTop.setValue((int) bounds[3]);
            NativeCore.setBoardBounds(bounds[0], bounds[1], bounds[2], bounds[3], showLines[0]);
        });
        nudgeRow.addView(btnNudgeUp);

        Button btnNudgeDown = new Button(activity);
        btnNudgeDown.setText("▼ Shift Board DOWN (-20px)");
        btnNudgeDown.setTextColor(Color.WHITE);
        btnNudgeDown.setBackground(createButtonBg("#252535", "#8888AA", dpToPx(activity, 8)));
        LinearLayout.LayoutParams lpNudgeDown = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
        btnNudgeDown.setLayoutParams(lpNudgeDown);
        btnNudgeDown.setOnClickListener(v -> {
            bounds[2] -= 20;
            bounds[3] -= 20;
            scBottom.setValue((int) bounds[2]);
            scTop.setValue((int) bounds[3]);
            NativeCore.setBoardBounds(bounds[0], bounds[1], bounds[2], bounds[3], showLines[0]);
        });
        nudgeRow.addView(btnNudgeDown);

        contentLayout.addView(nudgeRow);

        // Buttons Row (Reset & Save)
        LinearLayout btnRow = new LinearLayout(activity);
        btnRow.setOrientation(LinearLayout.HORIZONTAL);
        btnRow.setPadding(0, dpToPx(activity, 12), 0, 0);

        Button btnReset = new Button(activity);
        btnReset.setText("Reset");
        btnReset.setTextColor(Color.parseColor("#FF5555"));
        btnReset.setBackground(createButtonBg("#2A202A", "#FF5555", dpToPx(activity, 8)));
        LinearLayout.LayoutParams lpReset = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
        lpReset.rightMargin = dpToPx(activity, 8);
        btnReset.setLayoutParams(lpReset);
        btnReset.setOnClickListener(v -> {
            float defL = 20.0f;
            float defB = 452.0f;
            float defT = 1146.0f;
            float defR = (screenW > 714) ? (screenW - 20.0f) : (defL + (defT - defB)); // 714.0f

            bounds[0] = defL;
            bounds[1] = defR;
            bounds[2] = defB;
            bounds[3] = defT;

            scLeft.setValue((int) defL);
            scRight.setValue((int) defR);
            scBottom.setValue((int) defB);
            scTop.setValue((int) defT);

            physics[0] = 0.9850f;
            physics[1] = 0.9888f;
            physics[2] = 0.92f;
            physics[3] = 0.88f;
            physics[4] = 3.00f;
            physics[5] = 1.00f;

            scStrikerDamping.setValue(physics[0]);
            scPuckDamping.setValue(physics[1]);
            scRestBody.setValue(physics[2]);
            scRestCushion.setValue(physics[3]);
            scStrikerMass.setValue(physics[4]);
            scPowerMult.setValue(physics[5]);

            lineThickness[0] = 0.70f;
            showLines[0] = false;

            scThickness.setValue(lineThickness[0]);
            cbShowBorder.setChecked(false);

            NativeCore.setBoardBounds(bounds[0], bounds[1], bounds[2], bounds[3], false);
            NativeCore.setFineTunePhysics(physics[0], physics[1], physics[2], physics[3], physics[4], physics[5]);
            NativeCore.setGuidelineThickness(lineThickness[0]);
        });
        btnRow.addView(btnReset);

        Button btnSave = new Button(activity);
        btnSave.setText("Save & Close");
        btnSave.setTextColor(Color.BLACK);
        btnSave.setTypeface(btnSave.getTypeface(), android.graphics.Typeface.BOLD);
        btnSave.setBackground(createButtonBg("#FFD700", "#FFFFFF", dpToPx(activity, 8)));
        LinearLayout.LayoutParams lpSave = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.5f);
        btnSave.setLayoutParams(lpSave);
        btnSave.setOnClickListener(v -> {
            saveCalibration(activity, bounds[0], bounds[1], bounds[2], bounds[3],
                            lineThickness[0], showLines[0],
                            physics[0], physics[1], physics[2], physics[3], physics[4], physics[5]);
            NativeCore.setBoardBounds(bounds[0], bounds[1], bounds[2], bounds[3], sShowBorder);
            NativeCore.setGuidelineThickness(sGuidelineThickness);
            NativeCore.setFineTunePhysics(sStrikerDamping, sPuckDamping, sRestitutionBody, sRestitutionCushion, sStrikerMass, sPowerMultiplier);
            if (sActiveDialog != null) {
                try {
                    sActiveDialog.dismiss();
                } catch (Exception ignored) {}
                sActiveDialog = null;
            }
            Toast.makeText(activity, "Board calibration & physics saved perfectly!", Toast.LENGTH_SHORT).show();
        });
        btnRow.addView(btnSave);

        contentLayout.addView(btnRow);
        scrollView.addView(contentLayout);

        AlertDialog.Builder builder = new AlertDialog.Builder(activity);
        builder.setView(scrollView);
        builder.setCancelable(true);
        builder.setOnCancelListener(dialog -> {
            // Restore saved bounds, thickness, and physics if cancelled
            NativeCore.setBoardBounds(sLeft, sRight, sBottom, sTop, sShowBorder);
            NativeCore.setGuidelineThickness(sGuidelineThickness);
            NativeCore.setFineTunePhysics(sStrikerDamping, sPuckDamping, sRestitutionBody, sRestitutionCushion, sStrikerMass, sPowerMultiplier);
            sActiveDialog = null;
        });

        sActiveDialog = builder.create();
        if (sActiveDialog.getWindow() != null) {
            Window window = sActiveDialog.getWindow();
            // Keep background completely undimmed and transparent so game board and yellow lines are clearly seen
            window.clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
            window.setDimAmount(0.0f);
            window.setBackgroundDrawableResource(android.R.color.transparent);
            WindowManager.LayoutParams wlp = window.getAttributes();
            wlp.dimAmount = 0.0f;
            wlp.flags &= ~WindowManager.LayoutParams.FLAG_DIM_BEHIND;
            wlp.gravity = Gravity.BOTTOM;
            wlp.width = (int) (screenW * 0.95f);
            window.setAttributes(wlp);
        }
        sActiveDialog.show();
        if (sActiveDialog.getWindow() != null) {
            sActiveDialog.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
            sActiveDialog.getWindow().setDimAmount(0.0f);
        }
    }

    private static GradientDrawable createButtonBg(String colorHex, String strokeHex, int cornerRadiusPx) {
        GradientDrawable gd = new GradientDrawable();
        gd.setColor(Color.parseColor(colorHex));
        gd.setCornerRadius(cornerRadiusPx);
        gd.setStroke(2, Color.parseColor(strokeHex));
        return gd;
    }

    private static int dpToPx(Context context, int dp) {
        return (int) TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, dp, context.getResources().getDisplayMetrics());
    }

    // ---------------------------------------------------------------------------------------------
    // Slider Component (Label, Value, [-], SeekBar, [+])
    // ---------------------------------------------------------------------------------------------
    private static class SliderControl {
        final View view;
        final SeekBar seekBar;
        final TextView tvVal;

        interface OnChangeListener {
            void onChange(int value);
        }

        SliderControl(Context context, String label, int min, int max, int initialValue, OnChangeListener listener) {
            LinearLayout root = new LinearLayout(context);
            root.setOrientation(LinearLayout.VERTICAL);
            root.setPadding(0, dpToPx(context, 4), 0, dpToPx(context, 6));

            // Header Row (Label + Value in px)
            LinearLayout header = new LinearLayout(context);
            header.setOrientation(LinearLayout.HORIZONTAL);

            TextView tvLabel = new TextView(context);
            tvLabel.setText(label);
            tvLabel.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
            tvLabel.setTextColor(Color.WHITE);
            tvLabel.setTypeface(tvLabel.getTypeface(), android.graphics.Typeface.BOLD);
            tvLabel.setShadowLayer(3f, 0f, 1f, Color.BLACK);
            LinearLayout.LayoutParams lpLabel = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
            header.addView(tvLabel, lpLabel);

            tvVal = new TextView(context);
            tvVal.setText(initialValue + " px");
            tvVal.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
            tvVal.setTextColor(Color.parseColor("#FFD700"));
            tvVal.setTypeface(tvVal.getTypeface(), android.graphics.Typeface.BOLD);
            tvVal.setShadowLayer(3f, 0f, 1f, Color.BLACK);
            header.addView(tvVal);
            root.addView(header);

            // Controls Row ([-] Button, SeekBar, [+] Button)
            LinearLayout ctrlRow = new LinearLayout(context);
            ctrlRow.setOrientation(LinearLayout.HORIZONTAL);
            ctrlRow.setGravity(Gravity.CENTER_VERTICAL);

            Button btnMinus = new Button(context);
            btnMinus.setText("-");
            btnMinus.setTextColor(Color.WHITE);
            btnMinus.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
            btnMinus.setBackground(createButtonBg("#2E2E3A", "#555566", dpToPx(context, 6)));
            int btnSize = dpToPx(context, 34);
            LinearLayout.LayoutParams lpBtnMinus = new LinearLayout.LayoutParams(btnSize, btnSize);
            ctrlRow.addView(btnMinus, lpBtnMinus);

            seekBar = new SeekBar(context);
            seekBar.setMax(max);
            seekBar.setProgress(initialValue);
            // Ensure slider track and thumb are 100% opaque and brightly visible on transparent overlay
            seekBar.setProgressTintList(ColorStateList.valueOf(Color.parseColor("#FFD700")));
            seekBar.setThumbTintList(ColorStateList.valueOf(Color.parseColor("#FFD700")));
            seekBar.setProgressBackgroundTintList(ColorStateList.valueOf(Color.parseColor("#777788")));
            LinearLayout.LayoutParams lpSeek = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
            lpSeek.leftMargin = dpToPx(context, 6);
            lpSeek.rightMargin = dpToPx(context, 6);
            ctrlRow.addView(seekBar, lpSeek);

            Button btnPlus = new Button(context);
            btnPlus.setText("+");
            btnPlus.setTextColor(Color.WHITE);
            btnPlus.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
            btnPlus.setBackground(createButtonBg("#2E2E3A", "#555566", dpToPx(context, 6)));
            LinearLayout.LayoutParams lpBtnPlus = new LinearLayout.LayoutParams(btnSize, btnSize);
            ctrlRow.addView(btnPlus, lpBtnPlus);

            root.addView(ctrlRow);

            seekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                @Override
                public void onProgressChanged(SeekBar sb, int progress, boolean fromUser) {
                    tvVal.setText(progress + " px");
                    if (listener != null) listener.onChange(progress);
                }

                @Override
                public void onStartTrackingTouch(SeekBar sb) {}

                @Override
                public void onStopTrackingTouch(SeekBar sb) {}
            });

            btnMinus.setOnClickListener(v -> {
                int cur = seekBar.getProgress();
                if (cur > 0) {
                    seekBar.setProgress(cur - 1);
                }
            });

            btnPlus.setOnClickListener(v -> {
                int cur = seekBar.getProgress();
                if (cur < max) {
                    seekBar.setProgress(cur + 1);
                }
            });

            this.view = root;
        }

        void setValue(int val) {
            seekBar.setProgress(val);
            tvVal.setText(val + " px");
        }
    }

    // ---------------------------------------------------------------------------------------------
    // Float Slider Component for Precision Physics Parameters
    // ---------------------------------------------------------------------------------------------
    private static class FloatSliderControl {
        final View view;
        final SeekBar seekBar;
        final TextView tvVal;
        final String formatPattern;
        final float min;
        final float max;
        final int steps;

        interface OnChangeListener {
            void onChange(float value);
        }

        FloatSliderControl(Context context, String label, float min, float max, float initialValue,
                           String formatPattern, int steps, String colorHex, OnChangeListener listener) {
            this.min = min;
            this.max = max;
            this.steps = steps;
            this.formatPattern = formatPattern;

            LinearLayout root = new LinearLayout(context);
            root.setOrientation(LinearLayout.VERTICAL);
            root.setPadding(0, dpToPx(context, 4), 0, dpToPx(context, 6));

            LinearLayout header = new LinearLayout(context);
            header.setOrientation(LinearLayout.HORIZONTAL);

            TextView tvLabel = new TextView(context);
            tvLabel.setText(label);
            tvLabel.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
            tvLabel.setTextColor(Color.WHITE);
            tvLabel.setTypeface(tvLabel.getTypeface(), android.graphics.Typeface.BOLD);
            tvLabel.setShadowLayer(3f, 0f, 1f, Color.BLACK);
            LinearLayout.LayoutParams lpLabel = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
            header.addView(tvLabel, lpLabel);

            tvVal = new TextView(context);
            tvVal.setText(String.format(java.util.Locale.US, formatPattern, initialValue));
            tvVal.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
            tvVal.setTextColor(Color.parseColor(colorHex));
            tvVal.setTypeface(tvVal.getTypeface(), android.graphics.Typeface.BOLD);
            tvVal.setShadowLayer(3f, 0f, 1f, Color.BLACK);
            header.addView(tvVal);
            root.addView(header);

            LinearLayout ctrlRow = new LinearLayout(context);
            ctrlRow.setOrientation(LinearLayout.HORIZONTAL);
            ctrlRow.setGravity(Gravity.CENTER_VERTICAL);

            Button btnMinus = new Button(context);
            btnMinus.setText("-");
            btnMinus.setTextColor(Color.WHITE);
            btnMinus.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
            btnMinus.setBackground(createButtonBg("#2E2E3A", "#555566", dpToPx(context, 6)));
            int btnSize = dpToPx(context, 34);
            LinearLayout.LayoutParams lpBtnMinus = new LinearLayout.LayoutParams(btnSize, btnSize);
            ctrlRow.addView(btnMinus, lpBtnMinus);

            int curStep = Math.round(((initialValue - min) / (max - min)) * steps);

            seekBar = new SeekBar(context);
            seekBar.setMax(steps);
            seekBar.setProgress(curStep);
            seekBar.setProgressTintList(ColorStateList.valueOf(Color.parseColor(colorHex)));
            seekBar.setThumbTintList(ColorStateList.valueOf(Color.parseColor(colorHex)));
            seekBar.setProgressBackgroundTintList(ColorStateList.valueOf(Color.parseColor("#777788")));
            LinearLayout.LayoutParams lpSeek = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
            lpSeek.leftMargin = dpToPx(context, 6);
            lpSeek.rightMargin = dpToPx(context, 6);
            ctrlRow.addView(seekBar, lpSeek);

            Button btnPlus = new Button(context);
            btnPlus.setText("+");
            btnPlus.setTextColor(Color.WHITE);
            btnPlus.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
            btnPlus.setBackground(createButtonBg("#2E2E3A", "#555566", dpToPx(context, 6)));
            LinearLayout.LayoutParams lpBtnPlus = new LinearLayout.LayoutParams(btnSize, btnSize);
            ctrlRow.addView(btnPlus, lpBtnPlus);

            root.addView(ctrlRow);

            seekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                @Override
                public void onProgressChanged(SeekBar sb, int progress, boolean fromUser) {
                    float val = min + ((float) progress / (float) steps) * (max - min);
                    tvVal.setText(String.format(java.util.Locale.US, formatPattern, val));
                    if (listener != null) listener.onChange(val);
                }

                @Override
                public void onStartTrackingTouch(SeekBar sb) {}

                @Override
                public void onStopTrackingTouch(SeekBar sb) {}
            });

            btnMinus.setOnClickListener(v -> {
                int cur = seekBar.getProgress();
                if (cur > 0) seekBar.setProgress(cur - 1);
            });

            btnPlus.setOnClickListener(v -> {
                int cur = seekBar.getProgress();
                if (cur < steps) seekBar.setProgress(cur + 1);
            });

            this.view = root;
        }

        void setValue(float val) {
            int step = Math.round(((val - min) / (max - min)) * steps);
            seekBar.setProgress(step);
            tvVal.setText(String.format(java.util.Locale.US, formatPattern, val));
        }
    }

    // ---------------------------------------------------------------------------------------------
    // Draggable Floating Button View
    // ---------------------------------------------------------------------------------------------
    private static class FloatingButtonView extends View {
        private final Paint mPaintCircle = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint mPaintBorder = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint mPaintText = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final RectF mBounds = new RectF();

        private float mDownRawX, mDownRawY;
        private float mTouchDownX, mTouchDownY;
        private boolean mIsDragging = false;
        private OnClickListener mClickListener;

        public FloatingButtonView(Context context) {
            super(context);
            init();
        }

        private void init() {
            setClickable(true);
            setFocusable(true);

            mPaintCircle.setColor(Color.parseColor("#EE1A1A24"));
            mPaintCircle.setStyle(Paint.Style.FILL);

            mPaintBorder.setColor(Color.parseColor("#FFD700"));
            mPaintBorder.setStyle(Paint.Style.STROKE);
            mPaintBorder.setStrokeWidth(dpToPx(getContext(), 2.5f));

            mPaintText.setColor(Color.parseColor("#FFD700"));
            mPaintText.setTextSize(dpToPx(getContext(), 15));
            mPaintText.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);
            mPaintText.setTextAlign(Paint.Align.CENTER);

            setElevation(dpToPx(getContext(), 8));
        }

        private float dpToPx(Context context, float dp) {
            return TypedValue.applyDimension(
                    TypedValue.COMPLEX_UNIT_DIP, dp, context.getResources().getDisplayMetrics());
        }

        @Override
        public void setOnClickListener(OnClickListener l) {
            this.mClickListener = l;
        }

        @Override
        protected void onDraw(Canvas canvas) {
            super.onDraw(canvas);
            float w = getWidth();
            float h = getHeight();
            float cx = w * 0.5f;
            float cy = h * 0.5f;
            float radius = (Math.min(w, h) * 0.5f) - dpToPx(getContext(), 3);

            // Draw circular background & gold border
            canvas.drawCircle(cx, cy, radius, mPaintCircle);
            canvas.drawCircle(cx, cy, radius, mPaintBorder);

            // Draw "BB" in center
            Paint.FontMetrics fm = mPaintText.getFontMetrics();
            float textY = cy - (fm.ascent + fm.descent) * 0.5f;
            canvas.drawText("BB", cx, textY, mPaintText);
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            switch (event.getAction()) {
                case MotionEvent.ACTION_DOWN:
                    mDownRawX = event.getRawX();
                    mDownRawY = event.getRawY();
                    mTouchDownX = event.getRawX() - getX();
                    mTouchDownY = event.getRawY() - getY();
                    mIsDragging = false;
                    return true;

                case MotionEvent.ACTION_MOVE:
                    float newX = event.getRawX() - mTouchDownX;
                    float newY = event.getRawY() - mTouchDownY;

                    if (Math.abs(event.getRawX() - mDownRawX) > dpToPx(getContext(), 6) ||
                        Math.abs(event.getRawY() - mDownRawY) > dpToPx(getContext(), 6)) {
                        mIsDragging = true;
                    }

                    // Constrain within screen boundaries
                    ViewGroup parent = (ViewGroup) getParent();
                    if (parent != null) {
                        int parentW = parent.getWidth();
                        int parentH = parent.getHeight();
                        newX = Math.max(0, Math.min(newX, parentW - getWidth()));
                        newY = Math.max(0, Math.min(newY, parentH - getHeight()));
                    }

                    setX(newX);
                    setY(newY);
                    return true;

                case MotionEvent.ACTION_UP:
                    if (!mIsDragging) {
                        if (mClickListener != null) {
                            mClickListener.onClick(this);
                        } else {
                            performClick();
                        }
                    }
                    return true;
            }
            return super.onTouchEvent(event);
        }

        @Override
        public boolean performClick() {
            super.performClick();
            if (mClickListener != null) {
                mClickListener.onClick(this);
            }
            return true;
        }
    }
}
"""

with open(file_path, "w", encoding="utf-8") as f:
    f.write(content)
print(f"Successfully wrote {len(content)} characters to {file_path}")
