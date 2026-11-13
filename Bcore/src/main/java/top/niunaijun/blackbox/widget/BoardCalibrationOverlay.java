package top.niunaijun.blackbox.widget;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Application;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Path;
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
import android.view.accessibility.AccessibilityEvent;
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
    private static final String KEY_POCKET_RADIUS = "calib_pocket_radius";
    private static final String KEY_PUCK_RADIUS = "calib_puck_radius";
    private static final String KEY_SAVED = "calib_saved";

    // Live physics fine-tuning keys
    private static final String KEY_STRIKER_DAMPING = "calib_striker_damping";
    private static final String KEY_PUCK_DAMPING = "calib_puck_damping";
    private static final String KEY_RESTITUTION_BODY = "calib_rest_body";
    private static final String KEY_RESTITUTION_CUSHION = "calib_rest_cushion";
    private static final String KEY_STRIKER_MASS = "calib_striker_mass";
    private static final String KEY_POWER_MULTIPLIER = "calib_power_mult";

    // Advanced physics & geometry keys
    private static final String KEY_STRIKER_RADIUS = "calib_striker_radius";
    private static final String KEY_POCKET_INSET = "calib_pocket_inset";
    private static final String KEY_CONTACT_FRICTION = "calib_contact_friction";
    private static final String KEY_CUSHION_TANGENT = "calib_cushion_tangent";
    private static final String KEY_STRIKER_BONUS = "calib_striker_bonus";
    private static final String KEY_POCKET_DROP_SPEED = "calib_pocket_drop_speed";
    private static final String KEY_POCKET_GRAVITY = "calib_pocket_gravity";

    private static boolean sInstalled = false;
    private static WeakReference<Activity> sCurrentActivity = null;
    private static FloatingButtonView sFloatingButton = null;
    private static AutoShotButtonView sAutoButton = null;
    private static boolean sAutoShotEnabled = false;
    private static AlertDialog sActiveDialog = null;

    // User-verified default board bounds: Left=20px, Right=714px, Bottom=452px, Top=1146px
    private static float sLeft = 0.0f;
    private static float sRight = 0.0f;
    private static float sBottom = 0.0f;
    private static float sTop = 0.0f;

    // Calibrated geometry for Carrom Pool (in Centimeters)
    private static float sStrikerRadius = 2.65f;
    private static float sPuckRadius = 1.95f;
    private static float sPocketInset = 33.48f;
    private static float sPocketRadius = 2.15f;

    // Guideline visual styling defaults
    private static boolean sShowBorder = false;
    private static float sGuidelineThickness = 0.70f;
    private static final int DEFAULT_COLOR_THEME = 0; // Neon Green / Classic Laser

    // Live physics fine-tuning defaults (calibrated for 2.65 - 2.70 board lengths travel distance)
    private static float sStrikerDamping = 0.0410f;
    private static float sPuckDamping = 0.1080f;
    private static float sRestitutionBody = 1.00f;
    private static float sRestitutionCushion = 0.88f;
    private static float sStrikerMass = 3.00f;
    private static float sPowerMultiplier = 1.00f;

    // Advanced physics fine-tuning defaults
    private static float sContactFriction = 0.050f;
    private static float sCushionTangentFriction = 0.90f;
    private static float sStrikerBonus = 0.0f;
    private static float sPocketDropSpeed = 250.0f;
    private static float sPocketGravity = 980.0f;

    private static boolean sHasSavedBounds = false;

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
        if (sHasSavedBounds) {
            boolean showBorderToSet = sShowBorder || NativeCore.getShowBorder();
            NativeCore.setBoardBounds(sLeft, sRight, sBottom, sTop, showBorderToSet);
        } else {
            NativeCore.resetBoardCalibration();
        }
        NativeCore.setObjectGeometry(sStrikerRadius, sPuckRadius, sPocketInset, sPocketRadius);
        NativeCore.setPocketRadius(sPocketRadius);
        NativeCore.setPuckRadius(sPuckRadius);
        NativeCore.setGuidelineThickness(sGuidelineThickness);
        NativeCore.setColorTheme(DEFAULT_COLOR_THEME);
        NativeCore.setFineTunePhysics(sStrikerDamping, sPuckDamping, sRestitutionBody, sRestitutionCushion, sStrikerMass, sPowerMultiplier);
        NativeCore.setAdvancedPhysics(sContactFriction, sCushionTangentFriction, sStrikerBonus);
        NativeCore.setPocketPhysics(sPocketDropSpeed, sPocketGravity);
        CatEngineConfig.INSTANCE.applyAll(activity);

        CatEngineSettingsDialog.prewarm(activity);
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
        sHasSavedBounds = sp.getBoolean(KEY_SAVED, false);

        DisplayMetrics dm = context.getResources().getDisplayMetrics();
        int screenW = dm.widthPixels;
        int screenH = dm.heightPixels;

        float boardSize = screenW * 0.94144f;
        float defaultLeft = (screenW - boardSize) * 0.5f;
        float defaultRight = (screenW + boardSize) * 0.5f;
        // Board is centered on the screen
        float boardCenterY = screenH * 0.5f;
        float defaultTop = boardCenterY - boardSize * 0.5f;
        float defaultBottom = boardCenterY + boardSize * 0.5f;

        sLeft = sp.getFloat(KEY_LEFT, defaultLeft);
        sRight = sp.getFloat(KEY_RIGHT, defaultRight);
        sBottom = sp.getFloat(KEY_BOTTOM, defaultBottom);
        sTop = sp.getFloat(KEY_TOP, defaultTop);

        if (!sHasSavedBounds) {
            sLeft = defaultLeft;
            sRight = defaultRight;
            sBottom = defaultBottom;
            sTop = defaultTop;
        }

        sShowBorder = sp.getBoolean(KEY_SHOW_BORDER, sp.getBoolean("cat_engine_show_table", false));
        sGuidelineThickness = sp.getFloat(KEY_THICKNESS, 0.70f);
        sPocketRadius = sp.getFloat(KEY_POCKET_RADIUS, 2.15f);
        sPuckRadius = sp.getFloat(KEY_PUCK_RADIUS, 1.95f);

        sStrikerRadius = sp.getFloat(KEY_STRIKER_RADIUS, 2.65f);
        sPocketInset = sp.getFloat(KEY_POCKET_INSET, 33.48f);
        final int CURRENT_PREF_VERSION = 13;
        final String KEY_PREF_VERSION = "calib_pref_version_v13";
        int savedVersion = sp.getInt(KEY_PREF_VERSION, 0);

        if (savedVersion < CURRENT_PREF_VERSION) {
            // Automatically upgrade & migrate physics constants while PRESERVING user manual calibration bounds
            if (!sHasSavedBounds) {
                sLeft = defaultLeft;
                sRight = defaultRight;
                sBottom = defaultBottom;
                sTop = defaultTop;
            }
            sStrikerDamping = 0.0410f;
            sPuckDamping = 0.1080f;
            sRestitutionBody = 1.00f;
            sRestitutionCushion = 0.88f;
            sStrikerMass = 3.00f;
            sPowerMultiplier = 1.00f;
            sContactFriction = 0.050f;
            sCushionTangentFriction = 0.90f;
            sStrikerBonus = 0.0f;
            sPocketDropSpeed = 250.0f;
            sPocketGravity = 980.0f;

            sp.edit()
                .putInt(KEY_PREF_VERSION, CURRENT_PREF_VERSION)
                .putFloat(KEY_STRIKER_DAMPING, sStrikerDamping)
                .putFloat(KEY_PUCK_DAMPING, sPuckDamping)
                .putFloat(KEY_RESTITUTION_BODY, sRestitutionBody)
                .putFloat(KEY_RESTITUTION_CUSHION, sRestitutionCushion)
                .putFloat(KEY_STRIKER_MASS, sStrikerMass)
                .putFloat(KEY_POWER_MULTIPLIER, sPowerMultiplier)
                .putFloat(KEY_CONTACT_FRICTION, sContactFriction)
                .putFloat(KEY_CUSHION_TANGENT, sCushionTangentFriction)
                .putFloat(KEY_STRIKER_BONUS, sStrikerBonus)
                .putFloat(KEY_POCKET_DROP_SPEED, sPocketDropSpeed)
                .putFloat(KEY_POCKET_GRAVITY, sPocketGravity)
                .apply();
        } else {
            sStrikerBonus = sp.getFloat(KEY_STRIKER_BONUS, 0.0f);
            sStrikerDamping = sp.getFloat(KEY_STRIKER_DAMPING, 0.0410f);
            sPuckDamping = sp.getFloat(KEY_PUCK_DAMPING, 0.1080f);
            sRestitutionBody = sp.getFloat(KEY_RESTITUTION_BODY, 1.00f);
            sRestitutionCushion = sp.getFloat(KEY_RESTITUTION_CUSHION, 0.88f);
            sStrikerMass = sp.getFloat(KEY_STRIKER_MASS, 3.00f);
            sPowerMultiplier = sp.getFloat(KEY_POWER_MULTIPLIER, 1.00f);
            sPocketDropSpeed = sp.getFloat(KEY_POCKET_DROP_SPEED, 250.0f);
            sPocketGravity = sp.getFloat(KEY_POCKET_GRAVITY, 980.0f);
            sContactFriction = sp.getFloat(KEY_CONTACT_FRICTION, 0.050f);
            sCushionTangentFriction = sp.getFloat(KEY_CUSHION_TANGENT, 0.90f);
        }

        boolean passAndPlay = sp.getBoolean("calib_pass_and_play", false);
        NativeCore.setPassAndPlayBothSides(passAndPlay);
    }

    private static void persistCurrentState(Context context,
                                            float[] bounds, float[] lineThickness, boolean[] showLines,
                                            float[] pocketRadius, float[] puckRadius,
                                            float[] strikerRadius, float[] pocketInset,
                                            float[] physics, float[] advPhysics, float[] pocketPhys) {
        sLeft = bounds[0];
        sRight = bounds[1];
        sBottom = bounds[2];
        sTop = bounds[3];
        sGuidelineThickness = lineThickness[0];
        sShowBorder = showLines[0];
        sPocketRadius = pocketRadius[0];
        sPuckRadius = puckRadius[0];
        sStrikerRadius = strikerRadius[0];
        sPocketInset = pocketInset[0];
        sStrikerDamping = physics[0];
        sPuckDamping = physics[1];
        sRestitutionBody = physics[2];
        sRestitutionCushion = physics[3];
        sStrikerMass = physics[4];
        sPowerMultiplier = physics[5];
        sContactFriction = advPhysics[0];
        sCushionTangentFriction = advPhysics[1];
        sStrikerBonus = advPhysics[2];
        if (pocketPhys != null && pocketPhys.length >= 2) {
            sPocketDropSpeed = pocketPhys[0];
            sPocketGravity = pocketPhys[1];
        }
        sHasSavedBounds = true;

        if (context != null) {
            context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE).edit()
                    .putFloat(KEY_LEFT, sLeft)
                    .putFloat(KEY_RIGHT, sRight)
                    .putFloat(KEY_BOTTOM, sBottom)
                    .putFloat(KEY_TOP, sTop)
                    .putFloat(KEY_THICKNESS, sGuidelineThickness)
                    .putBoolean(KEY_SHOW_BORDER, sShowBorder)
                    .putFloat(KEY_POCKET_RADIUS, sPocketRadius)
                    .putFloat(KEY_PUCK_RADIUS, sPuckRadius)
                    .putFloat(KEY_STRIKER_RADIUS, sStrikerRadius)
                    .putFloat(KEY_POCKET_INSET, sPocketInset)
                    .putFloat(KEY_STRIKER_DAMPING, sStrikerDamping)
                    .putFloat(KEY_PUCK_DAMPING, sPuckDamping)
                    .putFloat(KEY_RESTITUTION_BODY, sRestitutionBody)
                    .putFloat(KEY_RESTITUTION_CUSHION, sRestitutionCushion)
                    .putFloat(KEY_STRIKER_MASS, sStrikerMass)
                    .putFloat(KEY_POWER_MULTIPLIER, sPowerMultiplier)
                    .putFloat(KEY_CONTACT_FRICTION, sContactFriction)
                    .putFloat(KEY_CUSHION_TANGENT, sCushionTangentFriction)
                    .putFloat(KEY_STRIKER_BONUS, sStrikerBonus)
                    .putFloat(KEY_POCKET_DROP_SPEED, sPocketDropSpeed)
                    .putFloat(KEY_POCKET_GRAVITY, sPocketGravity)
                    .putBoolean(KEY_SAVED, true)
                    .apply();
        }
    }

    private static void saveCalibration(Context context, float left, float right, float bottom, float top,
                                       float thickness, boolean showBorder, float pocketRadius, float puckRadius,
                                       float strikerRadius, float pocketInset,
                                       float strikerDamping, float puckDamping,
                                       float restBody, float restCushion,
                                       float strikerMass, float powerMultiplier,
                                       float contactFriction, float cushionTangentFriction, float strikerBonus,
                                       float pocketDropSpeed, float pocketGravity) {
        sLeft = left;
        sRight = right;
        sBottom = bottom;
        sTop = top;
        sGuidelineThickness = thickness;
        sShowBorder = showBorder;
        sPocketRadius = pocketRadius;
        sPuckRadius = puckRadius;
        sStrikerRadius = strikerRadius;
        sPocketInset = pocketInset;
        sStrikerDamping = strikerDamping;
        sPuckDamping = puckDamping;
        sRestitutionBody = restBody;
        sRestitutionCushion = restCushion;
        sStrikerMass = strikerMass;
        sPowerMultiplier = powerMultiplier;
        sContactFriction = contactFriction;
        sCushionTangentFriction = cushionTangentFriction;
        sStrikerBonus = strikerBonus;
        sPocketDropSpeed = pocketDropSpeed;
        sPocketGravity = pocketGravity;
        sHasSavedBounds = true;

        SharedPreferences sp = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
        sp.edit()
                .putFloat(KEY_LEFT, left)
                .putFloat(KEY_RIGHT, right)
                .putFloat(KEY_BOTTOM, bottom)
                .putFloat(KEY_TOP, top)
                .putFloat(KEY_THICKNESS, thickness)
                .putBoolean(KEY_SHOW_BORDER, showBorder)
                .putFloat(KEY_POCKET_RADIUS, pocketRadius)
                .putFloat(KEY_PUCK_RADIUS, puckRadius)
                .putFloat(KEY_STRIKER_RADIUS, strikerRadius)
                .putFloat(KEY_POCKET_INSET, pocketInset)
                .putFloat(KEY_STRIKER_DAMPING, strikerDamping)
                .putFloat(KEY_PUCK_DAMPING, puckDamping)
                .putFloat(KEY_RESTITUTION_BODY, restBody)
                .putFloat(KEY_RESTITUTION_CUSHION, restCushion)
                .putFloat(KEY_STRIKER_MASS, strikerMass)
                .putFloat(KEY_POWER_MULTIPLIER, powerMultiplier)
                .putFloat(KEY_CONTACT_FRICTION, contactFriction)
                .putFloat(KEY_CUSHION_TANGENT, cushionTangentFriction)
                .putFloat(KEY_STRIKER_BONUS, strikerBonus)
                .putFloat(KEY_POCKET_DROP_SPEED, pocketDropSpeed)
                .putFloat(KEY_POCKET_GRAVITY, pocketGravity)
                .putBoolean(KEY_SAVED, true)
                .apply();
        Log.i(TAG, String.format(java.util.Locale.US,
                "Calibration saved: L=%.1f R=%.1f B=%.1f T=%.1f Thick=%.2f ShowBorder=%b PocketRad=%.2f PuckRad=%.2f " +
                "DampSt=%.4f DampPk=%.4f RestBody=%.2f RestCush=%.2f Mass=%.2f Pow=%.2f",
                left, right, bottom, top, thickness, showBorder, pocketRadius, puckRadius,
                strikerDamping, puckDamping, restBody, restCushion, strikerMass, powerMultiplier));
    }

    public static void toggleAutoShot(Activity activity) {
        sAutoShotEnabled = !sAutoShotEnabled;
        NativeCore.setAutoShotEnabled(sAutoShotEnabled);

        if (sAutoButton != null) {
            sAutoButton.setActive(sAutoShotEnabled);
        }

        if (sAutoShotEnabled) {
            Toast.makeText(activity, "Auto-Strike ON - Autonomous targeting armed", Toast.LENGTH_SHORT).show();
        } else {
            Toast.makeText(activity, "Auto-Strike OFF", Toast.LENGTH_SHORT).show();
        }
    }

    private static void attachFloatingButton(Activity activity) {
        if (activity == null || activity.isFinishing()) return;
        try {
            ViewGroup decor = (ViewGroup) activity.getWindow().getDecorView();

            if (sAutoButton != null && sAutoButton.getParent() != null) {
                ((ViewGroup) sAutoButton.getParent()).removeView(sAutoButton);
            }
            if (sFloatingButton != null && sFloatingButton.getParent() != null) {
                ((ViewGroup) sFloatingButton.getParent()).removeView(sFloatingButton);
            }

            int btnSize = dpToPx(activity, 52);
            int autoBtnSize = dpToPx(activity, 46);
            int leftMargin = dpToPx(activity, 16);
            int topMargin = dpToPx(activity, 120);

            // 1. Auto Button View (Located directly above BB button)
            if (sAutoButton == null) {
                sAutoButton = new AutoShotButtonView(activity);
            }
            FrameLayout.LayoutParams lpAuto = new FrameLayout.LayoutParams(autoBtnSize, autoBtnSize);
            lpAuto.gravity = Gravity.TOP | Gravity.START;
            lpAuto.leftMargin = leftMargin + (btnSize - autoBtnSize) / 2;
            lpAuto.topMargin = topMargin - autoBtnSize - dpToPx(activity, 8);
            sAutoButton.setLayoutParams(lpAuto);
            sAutoButton.setOnClickListener(v -> toggleAutoShot(activity));
            decor.addView(sAutoButton);

            // 2. BB Calibration Floating Button
            FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(btnSize, btnSize);
            lp.gravity = Gravity.TOP | Gravity.START;
            lp.leftMargin = leftMargin;
            lp.topMargin = topMargin;

            if (sFloatingButton == null) {
                sFloatingButton = new FloatingButtonView(activity);
            }
            sFloatingButton.setLayoutParams(lp);
            sFloatingButton.setOnClickListener(v -> CatEngineSettingsDialog.show(activity));
            decor.addView(sFloatingButton);

            Log.d(TAG, "Floating buttons attached to activity: " + activity.getClass().getSimpleName());
        } catch (Exception e) {
            Log.e(TAG, "Failed to attach floating buttons", e);
        }
    }

    private static void detachFloatingButton(Activity activity) {
        try {
            if (sFloatingButton != null && sFloatingButton.getParent() != null) {
                ((ViewGroup) sFloatingButton.getParent()).removeView(sFloatingButton);
            }
            if (sAutoButton != null && sAutoButton.getParent() != null) {
                ((ViewGroup) sAutoButton.getParent()).removeView(sAutoButton);
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to detach floating buttons", e);
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
        final float[] pocketRadius = new float[]{sPocketRadius};
        final float[] puckRadius = new float[]{sPuckRadius};
        final float[] strikerRadius = new float[]{sStrikerRadius};
        final float[] pocketInset = new float[]{sPocketInset};
        final float[] physics = new float[]{
            sStrikerDamping,
            sPuckDamping,
            sRestitutionBody,
            sRestitutionCushion,
            sStrikerMass,
            sPowerMultiplier
        };
        loadCalibration(activity);
        final float[] advPhysics = new float[]{
            sContactFriction,
            sCushionTangentFriction,
            sStrikerBonus
        };
        final float[] pocketPhys = new float[]{
            sPocketDropSpeed,
            sPocketGravity
        };

        // User-saved values from SharedPreferences are the definitive source of truth.

        // Immediately apply in OpenGL
        NativeCore.setBoardBounds(bounds[0], bounds[1], bounds[2], bounds[3], showLines[0]);
        NativeCore.setObjectGeometry(strikerRadius[0], puckRadius[0], pocketInset[0], pocketRadius[0]);
        NativeCore.setPocketRadius(pocketRadius[0]);
        NativeCore.setPuckRadius(puckRadius[0]);
        NativeCore.setGuidelineThickness(lineThickness[0]);
        NativeCore.setColorTheme(DEFAULT_COLOR_THEME);
        NativeCore.setFineTunePhysics(physics[0], physics[1], physics[2], physics[3], physics[4], physics[5]);
        NativeCore.setAdvancedPhysics(advPhysics[0], advPhysics[1], advPhysics[2]);
        NativeCore.setPocketPhysics(pocketPhys[0], pocketPhys[1]);

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

        final SliderControl[] scLeftRef = new SliderControl[1];
        final SliderControl[] scRightRef = new SliderControl[1];
        final SliderControl[] scBottomRef = new SliderControl[1];
        final SliderControl[] scTopRef = new SliderControl[1];

        // --- HORIZONTAL (X) & VERTICAL (Y) WHOLE-BOARD PAN/SHIFT CONTROLS ---
        LinearLayout panContainer = new LinearLayout(activity);
        panContainer.setOrientation(LinearLayout.VERTICAL);
        panContainer.setBackground(createButtonBg("#141E28", "#00E5FF", dpToPx(activity, 8)));
        panContainer.setPadding(dpToPx(activity, 10), dpToPx(activity, 8), dpToPx(activity, 10), dpToPx(activity, 8));
        LinearLayout.LayoutParams lpPan = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lpPan.setMargins(0, 0, 0, dpToPx(activity, 8));
        panContainer.setLayoutParams(lpPan);

        int btnMargin = dpToPx(activity, 2);
        int btnPadH = dpToPx(activity, 4);
        int btnPadV = dpToPx(activity, 6);

        // 1. Horizontal (X-Axis) Shift
        TextView tvPanHeader = new TextView(activity);
        tvPanHeader.setText("↔ Move Board (X-Axis Shift)");
        tvPanHeader.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        tvPanHeader.setTextColor(Color.parseColor("#00E5FF"));
        tvPanHeader.setTypeface(null, android.graphics.Typeface.BOLD);
        tvPanHeader.setShadowLayer(3f, 0f, 1f, Color.BLACK);
        panContainer.addView(tvPanHeader);

        LinearLayout xButtonsRow = new LinearLayout(activity);
        xButtonsRow.setOrientation(LinearLayout.HORIZONTAL);
        xButtonsRow.setGravity(Gravity.CENTER_VERTICAL);
        xButtonsRow.setPadding(0, dpToPx(activity, 4), 0, dpToPx(activity, 6));

        int[] xDeltas = {-5, -1, 1, 5};
        String[] xLabels = {"◀◀ -5px", "◀ -1px", "+1px ▶", "+5px ▶▶"};

        for (int i = 0; i < xDeltas.length; i++) {
            final int d = xDeltas[i];
            Button b = new Button(activity);
            b.setText(xLabels[i]);
            b.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
            b.setTextColor(Color.WHITE);
            b.setBackground(createButtonBg("#1F2E3D", "#00E5FF", dpToPx(activity, 6)));
            b.setPadding(btnPadH, btnPadV, btnPadH, btnPadV);
            LinearLayout.LayoutParams lpBtn = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
            lpBtn.setMargins(btnMargin, 0, btnMargin, 0);
            b.setLayoutParams(lpBtn);
            b.setOnClickListener(v -> {
                bounds[0] += d;
                bounds[1] += d;
                NativeCore.setBoardBounds(bounds[0], bounds[1], bounds[2], bounds[3], showLines[0]);
                persistCurrentState(activity, bounds, lineThickness, showLines, pocketRadius, puckRadius, strikerRadius, pocketInset, physics, advPhysics, pocketPhys);
                if (scLeftRef[0] != null) scLeftRef[0].setValue((int) bounds[0]);
                if (scRightRef[0] != null) scRightRef[0].setValue((int) bounds[1]);
            });
            xButtonsRow.addView(b);
        }
        panContainer.addView(xButtonsRow);

        // 2. Vertical (Y-Axis) Shift
        TextView tvYPanHeader = new TextView(activity);
        tvYPanHeader.setText("↕ Move Board (Y-Axis Shift)");
        tvYPanHeader.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        tvYPanHeader.setTextColor(Color.parseColor("#00E5FF"));
        tvYPanHeader.setTypeface(null, android.graphics.Typeface.BOLD);
        tvYPanHeader.setShadowLayer(3f, 0f, 1f, Color.BLACK);
        tvYPanHeader.setPadding(0, dpToPx(activity, 4), 0, 0);
        panContainer.addView(tvYPanHeader);

        LinearLayout yButtonsRow = new LinearLayout(activity);
        yButtonsRow.setOrientation(LinearLayout.HORIZONTAL);
        yButtonsRow.setGravity(Gravity.CENTER_VERTICAL);
        yButtonsRow.setPadding(0, dpToPx(activity, 4), 0, dpToPx(activity, 4));

        int[] yDeltas = {-5, -1, 1, 5};
        String[] yLabels = {"▲▲ -5px (Up)", "▲ -1px (Up)", "+1px ▼ (Down)", "+5px ▼▼ (Down)"};

        for (int i = 0; i < yDeltas.length; i++) {
            final int d = yDeltas[i];
            Button b = new Button(activity);
            b.setText(yLabels[i]);
            b.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
            b.setTextColor(Color.WHITE);
            b.setBackground(createButtonBg("#1F2E3D", "#00E5FF", dpToPx(activity, 6)));
            b.setPadding(btnPadH, btnPadV, btnPadH, btnPadV);
            LinearLayout.LayoutParams lpBtn = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
            lpBtn.setMargins(btnMargin, 0, btnMargin, 0);
            b.setLayoutParams(lpBtn);
            b.setOnClickListener(v -> {
                bounds[2] += d; // bottom
                bounds[3] += d; // top
                NativeCore.setBoardBounds(bounds[0], bounds[1], bounds[2], bounds[3], showLines[0]);
                persistCurrentState(activity, bounds, lineThickness, showLines, pocketRadius, puckRadius, strikerRadius, pocketInset, physics, advPhysics, pocketPhys);
                if (scBottomRef[0] != null) scBottomRef[0].setValue((int) bounds[2]);
                if (scTopRef[0] != null) scTopRef[0].setValue((int) bounds[3]);
            });
            yButtonsRow.addView(b);
        }
        panContainer.addView(yButtonsRow);

        contentLayout.addView(panContainer);

        // --- CUSHION SLIDERS ---
        SliderControl scLeft = new SliderControl(activity, "Left Cushion (X)", 0, screenW, (int) bounds[0], val -> {
            bounds[0] = val;
            NativeCore.setBoardBounds(bounds[0], bounds[1], bounds[2], bounds[3], showLines[0]);
            persistCurrentState(activity, bounds, lineThickness, showLines, pocketRadius, puckRadius, strikerRadius, pocketInset, physics, advPhysics, pocketPhys);
        });
        scLeftRef[0] = scLeft;
        contentLayout.addView(scLeft.view);

        SliderControl scRight = new SliderControl(activity, "Right Cushion (X)", 0, screenW, (int) bounds[1], val -> {
            bounds[1] = val;
            NativeCore.setBoardBounds(bounds[0], bounds[1], bounds[2], bounds[3], showLines[0]);
            persistCurrentState(activity, bounds, lineThickness, showLines, pocketRadius, puckRadius, strikerRadius, pocketInset, physics, advPhysics, pocketPhys);
        });
        scRightRef[0] = scRight;
        contentLayout.addView(scRight.view);

        SliderControl scBottom = new SliderControl(activity, "Bottom Cushion (Y)", 0, screenH, (int) bounds[2], val -> {
            bounds[2] = val;
            NativeCore.setBoardBounds(bounds[0], bounds[1], bounds[2], bounds[3], showLines[0]);
            persistCurrentState(activity, bounds, lineThickness, showLines, pocketRadius, puckRadius, strikerRadius, pocketInset, physics, advPhysics, pocketPhys);
        });
        scBottomRef[0] = scBottom;
        contentLayout.addView(scBottom.view);

        SliderControl scTop = new SliderControl(activity, "Top Cushion (Y)", 0, screenH, (int) bounds[3], val -> {
            bounds[3] = val;
            NativeCore.setBoardBounds(bounds[0], bounds[1], bounds[2], bounds[3], showLines[0]);
            persistCurrentState(activity, bounds, lineThickness, showLines, pocketRadius, puckRadius, strikerRadius, pocketInset, physics, advPhysics, pocketPhys);
        });
        scTopRef[0] = scTop;
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

        // 1. Table & Striker Friction (0.0050 to 0.4000, default 0.1100, fine step 0.0005)
        FloatSliderControl scStrikerDamping = new FloatSliderControl(
                activity, "Striker Coulomb Friction mu (Lower = Slippery, Higher = Grippy)",
                0.0050f, 0.4000f, physics[0], "%.4f", 790, "#FF55AA", val -> {
            physics[0] = val;
            NativeCore.setFineTunePhysics(physics[0], physics[1], physics[2], physics[3], physics[4], physics[5]);
            persistCurrentState(activity, bounds, lineThickness, showLines, pocketRadius, puckRadius, strikerRadius, pocketInset, physics, advPhysics, pocketPhys);
        });
        contentLayout.addView(scStrikerDamping.view);

        // 2. Puck Coulomb Friction (0.0050 to 0.4000, default 0.1080, fine step 0.0005)
        FloatSliderControl scPuckDamping = new FloatSliderControl(
                activity, "Puck Coulomb Friction mu (Target Puck Travel Distance)",
                0.0050f, 0.4000f, physics[1], "%.4f", 790, "#FF55AA", val -> {
            physics[1] = val;
            NativeCore.setFineTunePhysics(physics[0], physics[1], physics[2], physics[3], physics[4], physics[5]);
            persistCurrentState(activity, bounds, lineThickness, showLines, pocketRadius, puckRadius, strikerRadius, pocketInset, physics, advPhysics, pocketPhys);
        });
        contentLayout.addView(scPuckDamping.view);

        // 3. Disc Collision Restitution e_body (0.10 to 1.00, default 0.90, step 0.01)
        FloatSliderControl scRestBody = new FloatSliderControl(
                activity, "Disc Elasticity (Collision Bounce Energy)",
                0.10f, 1.00f, physics[2], "%.2f", 90, "#00FF88", val -> {
            physics[2] = val;
            NativeCore.setFineTunePhysics(physics[0], physics[1], physics[2], physics[3], physics[4], physics[5]);
            persistCurrentState(activity, bounds, lineThickness, showLines, pocketRadius, puckRadius, strikerRadius, pocketInset, physics, advPhysics, pocketPhys);
        });
        contentLayout.addView(scRestBody.view);

        // 4. Cushion Bounce Restitution e_cushion (0.10 to 1.00, default 0.78, step 0.01)
        FloatSliderControl scRestCushion = new FloatSliderControl(
                activity, "Cushion Elasticity (Wall Bounce Energy)",
                0.10f, 1.00f, physics[3], "%.2f", 90, "#00FF88", val -> {
            physics[3] = val;
            NativeCore.setFineTunePhysics(physics[0], physics[1], physics[2], physics[3], physics[4], physics[5]);
            persistCurrentState(activity, bounds, lineThickness, showLines, pocketRadius, puckRadius, strikerRadius, pocketInset, physics, advPhysics, pocketPhys);
        });
        contentLayout.addView(scRestCushion.view);

        // 5. Striker Mass Ratio (0.50 to 10.00, default 2.40, step 0.05)
        FloatSliderControl scStrikerMass = new FloatSliderControl(
                activity, "Striker Mass Ratio (Striker : Puck)",
                0.50f, 10.00f, physics[4], "%.2f : 1", 190, "#FFD700", val -> {
            physics[4] = val;
            NativeCore.setFineTunePhysics(physics[0], physics[1], physics[2], physics[3], physics[4], physics[5]);
            persistCurrentState(activity, bounds, lineThickness, showLines, pocketRadius, puckRadius, strikerRadius, pocketInset, physics, advPhysics, pocketPhys);
        });
        contentLayout.addView(scStrikerMass.view);

        // 6. Shot Power Multiplier (0.20x to 3.00x, default 1.00x, step 0.02)
        FloatSliderControl scPowerMult = new FloatSliderControl(
                activity, "Shot Power Multiplier (Global Launch Velocity)",
                0.20f, 3.00f, physics[5], "%.2fx", 140, "#00E5FF", val -> {
            physics[5] = val;
            NativeCore.setFineTunePhysics(physics[0], physics[1], physics[2], physics[3], physics[4], physics[5]);
            persistCurrentState(activity, bounds, lineThickness, showLines, pocketRadius, puckRadius, strikerRadius, pocketInset, physics, advPhysics, pocketPhys);
        });
        contentLayout.addView(scPowerMult.view);

        // 7. Tangential Cut Friction (0.000 to 0.300, default 0.050, step 0.002)
        FloatSliderControl scContactFric = new FloatSliderControl(
                activity, "Tangential Cut Friction (Puck Throw / Cut Angle)",
                0.000f, 0.300f, advPhysics[0], "%.3f", 150, "#FF55AA", val -> {
            advPhysics[0] = val;
            NativeCore.setAdvancedPhysics(advPhysics[0], advPhysics[1], advPhysics[2]);
            persistCurrentState(activity, bounds, lineThickness, showLines, pocketRadius, puckRadius, strikerRadius, pocketInset, physics, advPhysics, pocketPhys);
        });
        contentLayout.addView(scContactFric.view);

        // 8. Cushion Glancing Tangent Velocity Retention (0.10 to 1.00, default 0.90, step 0.01)
        FloatSliderControl scCushionTang = new FloatSliderControl(
                activity, "Cushion Glancing Tangent Retention (Wall Friction)",
                0.10f, 1.00f, advPhysics[1], "%.2f", 90, "#00FF88", val -> {
            advPhysics[1] = val;
            NativeCore.setAdvancedPhysics(advPhysics[0], advPhysics[1], advPhysics[2]);
            persistCurrentState(activity, bounds, lineThickness, showLines, pocketRadius, puckRadius, strikerRadius, pocketInset, physics, advPhysics, pocketPhys);
        });
        contentLayout.addView(scCushionTang.view);

        // 9. Striker Full-Force Power Bonus (0.0 to 200.0 cm/s, default 69.0)
        FloatSliderControl scStrikerBonus = new FloatSliderControl(
                activity, "Striker Full-Pull Power Bonus (Max Pull Boost)",
                0.0f, 200.0f, advPhysics[2], "%.1f cm/s", 200, "#00E5FF", val -> {
            advPhysics[2] = val;
            NativeCore.setAdvancedPhysics(advPhysics[0], advPhysics[1], advPhysics[2]);
            persistCurrentState(activity, bounds, lineThickness, showLines, pocketRadius, puckRadius, strikerRadius, pocketInset, physics, advPhysics, pocketPhys);
        });
        contentLayout.addView(scStrikerBonus.view);

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
                0.10f, 3.00f, lineThickness[0], "%.2f", 145, "#FFD700", val -> {
            lineThickness[0] = val;
            NativeCore.setGuidelineThickness(val);
            persistCurrentState(activity, bounds, lineThickness, showLines, pocketRadius, puckRadius, strikerRadius, pocketInset, physics, advPhysics, pocketPhys);
        });
        contentLayout.addView(scThickness.view);

        // Pocket Detection Hole Radius Slider & Visual Ring Size
        TextView tvPocketHeader = new TextView(activity);
        tvPocketHeader.setText("🎯 Pocket Capture & Visual Ring Size");
        tvPocketHeader.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
        tvPocketHeader.setTextColor(Color.parseColor("#00FF66"));
        tvPocketHeader.setTypeface(tvPocketHeader.getTypeface(), android.graphics.Typeface.BOLD);
        tvPocketHeader.setShadowLayer(4f, 0f, 2f, Color.BLACK);
        tvPocketHeader.setPadding(0, dpToPx(activity, 10), 0, dpToPx(activity, 4));
        contentLayout.addView(tvPocketHeader);

        FloatSliderControl scPocket = new FloatSliderControl(
                activity, "Pocket Detection Radius (Strict Fall Zone)",
                0.50f, 5.00f, pocketRadius[0], "%.2f cm", 90, "#00FF66", val -> {
            pocketRadius[0] = val;
            NativeCore.setPocketRadius(val);
            NativeCore.setObjectGeometry(strikerRadius[0], puckRadius[0], pocketInset[0], val);
            persistCurrentState(activity, bounds, lineThickness, showLines, pocketRadius, puckRadius, strikerRadius, pocketInset, physics, advPhysics, pocketPhys);
        });
        contentLayout.addView(scPocket.view);

        FloatSliderControl scPuck = new FloatSliderControl(
                activity, "Puck Collision Radius (Target Disc Size)",
                0.50f, 5.00f, puckRadius[0], "%.2f cm", 90, "#FFD700", val -> {
            puckRadius[0] = val;
            NativeCore.setPuckRadius(val);
            NativeCore.setObjectGeometry(strikerRadius[0], val, pocketInset[0], pocketRadius[0]);
            persistCurrentState(activity, bounds, lineThickness, showLines, pocketRadius, puckRadius, strikerRadius, pocketInset, physics, advPhysics, pocketPhys);
        });
        contentLayout.addView(scPuck.view);

        FloatSliderControl scStrikerRad = new FloatSliderControl(
                activity, "Striker Collision Radius (White Disc Size)",
                0.50f, 5.00f, strikerRadius[0], "%.2f cm", 90, "#FFFFFF", val -> {
            strikerRadius[0] = val;
            NativeCore.setObjectGeometry(val, puckRadius[0], pocketInset[0], pocketRadius[0]);
            persistCurrentState(activity, bounds, lineThickness, showLines, pocketRadius, puckRadius, strikerRadius, pocketInset, physics, advPhysics, pocketPhys);
        });
        contentLayout.addView(scStrikerRad.view);

        FloatSliderControl scPocketInset = new FloatSliderControl(
                activity, "Pocket Corner Inset (Hole Placement)",
                20.00f, 45.00f, pocketInset[0], "%.2f cm", 250, "#00FF66", val -> {
            pocketInset[0] = val;
            NativeCore.setObjectGeometry(strikerRadius[0], puckRadius[0], val, pocketRadius[0]);
            persistCurrentState(activity, bounds, lineThickness, showLines, pocketRadius, puckRadius, strikerRadius, pocketInset, physics, advPhysics, pocketPhys);
        });
        contentLayout.addView(scPocketInset.view);

        FloatSliderControl scPocketDropSpeed = new FloatSliderControl(
                activity, "Pocket Drop Speed Cutoff (Puck skips hole above this)",
                30.0f, 250.0f, pocketPhys[0], "%.0f cm/s", 44, "#00FF66", val -> {
            pocketPhys[0] = val;
            NativeCore.setPocketPhysics(pocketPhys[0], pocketPhys[1]);
            persistCurrentState(activity, bounds, lineThickness, showLines, pocketRadius, puckRadius, strikerRadius, pocketInset, physics, advPhysics, pocketPhys);
        });
        contentLayout.addView(scPocketDropSpeed.view);

        FloatSliderControl scPocketGravity = new FloatSliderControl(
                activity, "Pocket Funnel Gravitational Suction (Inward Pull)",
                200.0f, 2000.0f, pocketPhys[1], "%.0f cm/s²", 90, "#00FF66", val -> {
            pocketPhys[1] = val;
            NativeCore.setPocketPhysics(pocketPhys[0], pocketPhys[1]);
            persistCurrentState(activity, bounds, lineThickness, showLines, pocketRadius, puckRadius, strikerRadius, pocketInset, physics, advPhysics, pocketPhys);
        });
        contentLayout.addView(scPocketGravity.view);

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
            persistCurrentState(activity, bounds, lineThickness, showLines, pocketRadius, puckRadius, strikerRadius, pocketInset, physics, advPhysics, pocketPhys);
        });
        contentLayout.addView(cbShowBorder);

        // Pass & Play Both Sides Toggle CheckBox
        CheckBox cbPassAndPlay = new CheckBox(activity);
        cbPassAndPlay.setText("Pass & Play: Show Guidelines on Both Sides (Friend Turn)");
        cbPassAndPlay.setTextColor(Color.WHITE);
        cbPassAndPlay.setTypeface(cbPassAndPlay.getTypeface(), android.graphics.Typeface.BOLD);
        cbPassAndPlay.setShadowLayer(3f, 0f, 1f, Color.BLACK);
        cbPassAndPlay.setButtonTintList(ColorStateList.valueOf(Color.parseColor("#00E5FF")));
        cbPassAndPlay.setChecked(NativeCore.getPassAndPlayBothSides());
        cbPassAndPlay.setPadding(dpToPx(activity, 8), dpToPx(activity, 8), dpToPx(activity, 8), dpToPx(activity, 8));
        cbPassAndPlay.setOnCheckedChangeListener((btn, isChecked) -> {
            NativeCore.setPassAndPlayBothSides(isChecked);
            SharedPreferences sp = activity.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
            sp.edit().putBoolean("calib_pass_and_play", isChecked).apply();
        });
        contentLayout.addView(cbPassAndPlay);



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

            float boardSize = screenW * 0.942f;
            float defL = (screenW - boardSize) * 0.5f;
            float defR = (screenW + boardSize) * 0.5f;
            float bCenterY = screenH * 0.5f;
            float defT = bCenterY - boardSize * 0.5f;
            float defB = bCenterY + boardSize * 0.5f;

            bounds[0] = defL;
            bounds[1] = defR;
            bounds[2] = defB;
            bounds[3] = defT;

            scLeft.setValue((int) defL);
            scRight.setValue((int) defR);
            scBottom.setValue((int) defB);
            scTop.setValue((int) defT);

            physics[0] = 0.1600f;
            physics[1] = 0.1080f;
            physics[2] = 1.00f;
            physics[3] = 0.88f;
            physics[4] = 3.00f;
            physics[5] = 1.00f;

            advPhysics[0] = 0.050f;
            advPhysics[1] = 0.90f;
            advPhysics[2] = 69.0f;

            strikerRadius[0] = 2.65f;
            pocketInset[0] = 33.48f;
            puckRadius[0] = 1.95f;
            pocketRadius[0] = 2.15f;

            scStrikerDamping.setValue(physics[0]);
            scPuckDamping.setValue(physics[1]);
            scRestBody.setValue(physics[2]);
            scRestCushion.setValue(physics[3]);
            scStrikerMass.setValue(physics[4]);
            scPowerMult.setValue(physics[5]);
            scContactFric.setValue(advPhysics[0]);
            scCushionTang.setValue(advPhysics[1]);
            scStrikerBonus.setValue(advPhysics[2]);

            scStrikerRad.setValue(strikerRadius[0]);
            scPocketInset.setValue(pocketInset[0]);
            scPuck.setValue(puckRadius[0]);
            scPocket.setValue(pocketRadius[0]);

            NativeCore.setPuckRadius(puckRadius[0]);
            NativeCore.setPocketRadius(pocketRadius[0]);
            NativeCore.setObjectGeometry(strikerRadius[0], puckRadius[0], pocketInset[0], pocketRadius[0]);
            NativeCore.setBoardBounds(bounds[0], bounds[1], bounds[2], bounds[3], showLines[0]);
            NativeCore.setFineTunePhysics(physics[0], physics[1], physics[2], physics[3], physics[4], physics[5]);
            NativeCore.setAdvancedPhysics(advPhysics[0], advPhysics[1], advPhysics[2]);
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
            bounds[2] -= 20;
            bounds[3] -= 20;
            scBottom.setValue((int) bounds[2]);
            scTop.setValue((int) bounds[3]);
            NativeCore.setBoardBounds(bounds[0], bounds[1], bounds[2], bounds[3], showLines[0]);
            persistCurrentState(activity, bounds, lineThickness, showLines, pocketRadius, puckRadius, strikerRadius, pocketInset, physics, advPhysics, pocketPhys);
        });
        nudgeRow.addView(btnNudgeUp);

        Button btnNudgeDown = new Button(activity);
        btnNudgeDown.setText("▼ Shift Board DOWN (-20px)");
        btnNudgeDown.setTextColor(Color.WHITE);
        btnNudgeDown.setBackground(createButtonBg("#252535", "#8888AA", dpToPx(activity, 8)));
        LinearLayout.LayoutParams lpNudgeDown = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
        btnNudgeDown.setLayoutParams(lpNudgeDown);
        btnNudgeDown.setOnClickListener(v -> {
            bounds[2] += 20;
            bounds[3] += 20;
            scBottom.setValue((int) bounds[2]);
            scTop.setValue((int) bounds[3]);
            NativeCore.setBoardBounds(bounds[0], bounds[1], bounds[2], bounds[3], showLines[0]);
            persistCurrentState(activity, bounds, lineThickness, showLines, pocketRadius, puckRadius, strikerRadius, pocketInset, physics, advPhysics, pocketPhys);
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
            float bSize = screenW * 0.942f;
            float defL = (screenW - bSize) * 0.5f;
            float defR = (screenW + bSize) * 0.5f;
            float bCenter = screenH * 0.5f;
            float defT = bCenter - bSize * 0.5f;
            float defB = bCenter + bSize * 0.5f;

            bounds[0] = defL;
            bounds[1] = defR;
            bounds[2] = defB;
            bounds[3] = defT;

            scLeft.setValue((int) defL);
            scRight.setValue((int) defR);
            scBottom.setValue((int) defB);
            scTop.setValue((int) defT);

            physics[0] = 0.1600f;
            physics[1] = 0.1080f;
            physics[2] = 1.00f;
            physics[3] = 0.88f;
            physics[4] = 3.00f;
            physics[5] = 1.00f;

            advPhysics[0] = 0.050f;
            advPhysics[1] = 0.90f;
            advPhysics[2] = 0.0f;

            if (pocketPhys != null && pocketPhys.length >= 2) {
                pocketPhys[0] = 250.0f;
                pocketPhys[1] = 980.0f;
                scPocketDropSpeed.setValue(pocketPhys[0]);
                scPocketGravity.setValue(pocketPhys[1]);
            }

            strikerRadius[0] = 2.65f;
            pocketInset[0] = 33.48f;
            puckRadius[0] = 1.95f;
            pocketRadius[0] = 2.15f;
            lineThickness[0] = 0.70f;
            showLines[0] = false;

            scStrikerDamping.setValue(physics[0]);
            scPuckDamping.setValue(physics[1]);
            scRestBody.setValue(physics[2]);
            scRestCushion.setValue(physics[3]);
            scStrikerMass.setValue(physics[4]);
            scPowerMult.setValue(physics[5]);
            scContactFric.setValue(advPhysics[0]);
            scCushionTang.setValue(advPhysics[1]);
            scStrikerBonus.setValue(advPhysics[2]);

            scStrikerRad.setValue(strikerRadius[0]);
            scPocketInset.setValue(pocketInset[0]);
            scThickness.setValue(lineThickness[0]);
            scPocket.setValue(pocketRadius[0]);
            scPuck.setValue(puckRadius[0]);
            cbShowBorder.setChecked(false);

            NativeCore.setBoardBounds(bounds[0], bounds[1], bounds[2], bounds[3], false);
            NativeCore.setPocketRadius(pocketRadius[0]);
            NativeCore.setPuckRadius(puckRadius[0]);
            NativeCore.setObjectGeometry(strikerRadius[0], puckRadius[0], pocketInset[0], pocketRadius[0]);
            NativeCore.setFineTunePhysics(physics[0], physics[1], physics[2], physics[3], physics[4], physics[5]);
            NativeCore.setAdvancedPhysics(advPhysics[0], advPhysics[1], advPhysics[2]);
            NativeCore.setPocketPhysics(pocketPhys[0], pocketPhys[1]);
            NativeCore.setGuidelineThickness(lineThickness[0]);
            persistCurrentState(activity, bounds, lineThickness, showLines, pocketRadius, puckRadius, strikerRadius, pocketInset, physics, advPhysics, pocketPhys);
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
                            lineThickness[0], showLines[0], pocketRadius[0], puckRadius[0],
                            strikerRadius[0], pocketInset[0],
                            physics[0], physics[1], physics[2], physics[3], physics[4], physics[5],
                            advPhysics[0], advPhysics[1], advPhysics[2],
                            pocketPhys[0], pocketPhys[1]);
            NativeCore.setBoardBounds(bounds[0], bounds[1], bounds[2], bounds[3], sShowBorder || NativeCore.getShowBorder());
            NativeCore.setPocketRadius(sPocketRadius);
            NativeCore.setPuckRadius(sPuckRadius);
            NativeCore.setObjectGeometry(sStrikerRadius, sPuckRadius, sPocketInset, sPocketRadius);
            NativeCore.setGuidelineThickness(sGuidelineThickness);
            NativeCore.setFineTunePhysics(sStrikerDamping, sPuckDamping, sRestitutionBody, sRestitutionCushion, sStrikerMass, sPowerMultiplier);
            NativeCore.setPocketPhysics(sPocketDropSpeed, sPocketGravity);
            NativeCore.setAdvancedPhysics(sContactFriction, sCushionTangentFriction, sStrikerBonus);
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
            // Keep user adjustments and persist them
            persistCurrentState(activity, bounds, lineThickness, showLines, pocketRadius, puckRadius, strikerRadius, pocketInset, physics, advPhysics, pocketPhys);
            NativeCore.setBoardBounds(bounds[0], bounds[1], bounds[2], bounds[3], showLines[0]);
            NativeCore.setPocketRadius(pocketRadius[0]);
            NativeCore.setPuckRadius(puckRadius[0]);
            NativeCore.setObjectGeometry(strikerRadius[0], puckRadius[0], pocketInset[0], pocketRadius[0]);
            NativeCore.setGuidelineThickness(lineThickness[0]);
            NativeCore.setFineTunePhysics(physics[0], physics[1], physics[2], physics[3], physics[4], physics[5]);
            NativeCore.setAdvancedPhysics(advPhysics[0], advPhysics[1], advPhysics[2]);
            sActiveDialog = null;
        });
        builder.setOnDismissListener(dialog -> {
            persistCurrentState(activity, bounds, lineThickness, showLines, pocketRadius, puckRadius, strikerRadius, pocketInset, physics, advPhysics, pocketPhys);
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

            float clampedInit = Math.max(min, Math.min(max, initialValue));
            int curStep = Math.round(((clampedInit - min) / (max - min)) * steps);
            curStep = Math.max(0, Math.min(steps, curStep));

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
            val = Math.max(min, Math.min(max, val));
            int step = Math.round(((val - min) / (max - min)) * steps);
            step = Math.max(0, Math.min(steps, step));
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
        private final Paint mPaintBitmap = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
        private final Paint mPaintText = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final RectF mBounds = new RectF();
        private final Path mClipPath = new Path();
        private Bitmap mLogoBitmap = null;

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

            mPaintCircle.setColor(Color.parseColor("#EE050B14"));
            mPaintCircle.setStyle(Paint.Style.FILL);

            mPaintBorder.setColor(Color.parseColor("#00E5FF"));
            mPaintBorder.setStyle(Paint.Style.STROKE);
            mPaintBorder.setStrokeWidth(dpToPx(getContext(), 2.5f));

            mPaintText.setColor(Color.parseColor("#00E5FF"));
            mPaintText.setTextSize(dpToPx(getContext(), 15));
            mPaintText.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);
            mPaintText.setTextAlign(Paint.Align.CENTER);

            setElevation(dpToPx(getContext(), 8));

            loadLogoBitmap();
        }

        private void loadLogoBitmap() {
            // 1. Try host context resources if accessible
            try {
                if (top.niunaijun.blackbox.BlackBoxCore.getContext() != null) {
                    Context hostCtx = top.niunaijun.blackbox.BlackBoxCore.getContext();
                    int resId = hostCtx.getResources().getIdentifier("catengine_floating_icon", "drawable", hostCtx.getPackageName());
                    if (resId != 0) {
                        mLogoBitmap = BitmapFactory.decodeResource(hostCtx.getResources(), resId);
                    }
                }
            } catch (Throwable ignored) {}

            // 2. Try target context resources
            if (mLogoBitmap == null) {
                try {
                    int resId = getResources().getIdentifier("catengine_floating_icon", "drawable", getContext().getPackageName());
                    if (resId != 0) {
                        mLogoBitmap = BitmapFactory.decodeResource(getResources(), resId);
                    }
                } catch (Throwable ignored) {}
            }

            // 3. Guaranteed embedded CatEngine logo fallback (never fails, zero external dependency)
            if (mLogoBitmap == null) {
                mLogoBitmap = CatEngineLogoData.getLogoBitmap();
            }
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

            // Draw circular background
            canvas.drawCircle(cx, cy, radius, mPaintCircle);

            // Draw circular CatEngine logo
            if (mLogoBitmap != null && !mLogoBitmap.isRecycled()) {
                mClipPath.reset();
                mClipPath.addCircle(cx, cy, radius, Path.Direction.CW);
                canvas.save();
                canvas.clipPath(mClipPath);
                mBounds.set(cx - radius, cy - radius, cx + radius, cy + radius);
                canvas.drawBitmap(mLogoBitmap, null, mBounds, mPaintBitmap);
                canvas.restore();
            } else {
                // Fallback to stylized CatEngine text if image resource is missing
                Paint.FontMetrics fm = mPaintText.getFontMetrics();
                float textY = cy - (fm.ascent + fm.descent) * 0.5f;
                canvas.drawText("CE", cx, textY, mPaintText);
            }

            // Draw electric cyan circular border
            canvas.drawCircle(cx, cy, radius, mPaintBorder);
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
                    if (sAutoButton != null) {
                        sAutoButton.setX(newX + (getWidth() - sAutoButton.getWidth()) * 0.5f);
                        sAutoButton.setY(newY - sAutoButton.getHeight() - dpToPx(getContext(), 8));
                    }
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

    // ---------------------------------------------------------------------------------------------
    // Draggable Auto-Shot Toggle Button View (Positioned Above BB Button)
    // ---------------------------------------------------------------------------------------------
    private static class AutoShotButtonView extends View {
        private final Paint mPaintCircle = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint mPaintBorder = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint mPaintIcon = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Path mTrianglePath = new Path();
        private final RectF mBarLeft = new RectF();
        private final RectF mBarRight = new RectF();

        private float mDownRawX, mDownRawY;
        private float mTouchDownX, mTouchDownY;
        private boolean mIsDragging = false;
        private OnClickListener mClickListener;

        public AutoShotButtonView(Context context) {
            super(context);
            init();
        }

        private void init() {
            setClickable(true);
            setFocusable(true);

            mPaintCircle.setStyle(Paint.Style.FILL);
            mPaintBorder.setStyle(Paint.Style.STROKE);
            mPaintBorder.setStrokeWidth(dpToPx(getContext(), 2.2f));

            mPaintIcon.setStyle(Paint.Style.FILL);

            setElevation(dpToPx(getContext(), 9));
            updateStyle();
        }

        private float dpToPx(Context context, float dp) {
            return TypedValue.applyDimension(
                    TypedValue.COMPLEX_UNIT_DIP, dp, context.getResources().getDisplayMetrics());
        }

        public void setActive(boolean active) {
            updateStyle();
            invalidate();
        }

        private void updateStyle() {
            if (sAutoShotEnabled) {
                // Auto is ON: Emerald green glow with parallel pause lines (||)
                mPaintCircle.setColor(Color.parseColor("#EE052618"));
                mPaintBorder.setColor(Color.parseColor("#00E676")); // Neon green border
                mPaintIcon.setColor(Color.parseColor("#00E676"));   // Neon green bars
            } else {
                // Auto is OFF: Electric blue theme with play triangle (▶)
                mPaintCircle.setColor(Color.parseColor("#EE0F172A"));
                mPaintBorder.setColor(Color.parseColor("#00E5FF")); // Electric cyan border
                mPaintIcon.setColor(Color.parseColor("#00E5FF"));   // Electric cyan play triangle
            }
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
            float radius = (Math.min(w, h) * 0.5f) - dpToPx(getContext(), 2.5f);

            // Draw circular background and border
            canvas.drawCircle(cx, cy, radius, mPaintCircle);
            canvas.drawCircle(cx, cy, radius, mPaintBorder);

            if (sAutoShotEnabled) {
                // AUTO ON: Show two parallel vertical lines representing ON / pause state
                float barW = dpToPx(getContext(), 3.5f);
                float barH = dpToPx(getContext(), 14.0f);
                float gap = dpToPx(getContext(), 5.0f);
                float cornerR = dpToPx(getContext(), 1.75f);

                float halfH = barH * 0.5f;
                float halfGap = gap * 0.5f;

                mBarLeft.set(cx - halfGap - barW, cy - halfH, cx - halfGap, cy + halfH);
                mBarRight.set(cx + halfGap, cy - halfH, cx + halfGap + barW, cy + halfH);

                canvas.drawRoundRect(mBarLeft, cornerR, cornerR, mPaintIcon);
                canvas.drawRoundRect(mBarRight, cornerR, cornerR, mPaintIcon);
            } else {
                // AUTO OFF: Show right-pointing play triangle representing OFF / start state
                // Optical Centering: The geometric center of a triangle places excessive visual weight
                // on the base (left side), making it look shifted to the left.
                // We add an optical centroid offset to the right so it is visually centered.
                float triW = dpToPx(getContext(), 13.0f);
                float triH = dpToPx(getContext(), 14.0f);
                float halfW = triW * 0.5f;
                float halfH = triH * 0.5f;
                float opticalShiftX = triW * 0.16f; // Visual center correction (+~2.1dp)

                float xLeft = cx - halfW + opticalShiftX;
                float xRight = cx + halfW + opticalShiftX;
                float yTop = cy - halfH;
                float yBottom = cy + halfH;

                mTrianglePath.reset();
                mTrianglePath.moveTo(xLeft, yTop);
                mTrianglePath.lineTo(xRight, cy);
                mTrianglePath.lineTo(xLeft, yBottom);
                mTrianglePath.close();

                canvas.drawPath(mTrianglePath, mPaintIcon);
            }
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

                    ViewGroup parent = (ViewGroup) getParent();
                    if (parent != null) {
                        int parentW = parent.getWidth();
                        int parentH = parent.getHeight();
                        newX = Math.max(0, Math.min(newX, parentW - getWidth()));
                        newY = Math.max(0, Math.min(newY, parentH - getHeight()));
                    }

                    setX(newX);
                    setY(newY);

                    // Move BB button companion right below it
                    if (sFloatingButton != null) {
                        sFloatingButton.setX(newX - (sFloatingButton.getWidth() - getWidth()) * 0.5f);
                        sFloatingButton.setY(newY + getHeight() + dpToPx(getContext(), 8));
                    }
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
