package com.snakemod;

import android.app.Service;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.IBinder;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;
import java.util.Locale;

public final class OverlayService extends Service {
    private WindowManager windowManager;
    private View root;
    private WindowManager.LayoutParams params;
    private boolean expanded = false;
    private TextView valueText;

    private static native void nativeSetMultiplier(float value);

    @Override public void onCreate() {
        super.onCreate();
        windowManager = (WindowManager) getSystemService(WINDOW_SERVICE);
        buildOverlay();
    }

    private GradientDrawable background(int color, int radius) {
        GradientDrawable d = new GradientDrawable();
        d.setColor(color);
        d.setCornerRadius(radius);
        d.setStroke(1, 0xFF53677F);
        return d;
    }

    private void buildOverlay() {
        LinearLayout panel = new LinearLayout(this);
        panel.setOrientation(LinearLayout.VERTICAL);
        panel.setPadding(14, 10, 14, 10);
        panel.setBackground(background(0xEE111827, 18));

        Button toggle = new Button(this);
        toggle.setText("snake mod  •  speed");
        toggle.setTextColor(Color.WHITE);
        toggle.setAllCaps(false);
        toggle.setTextSize(13);
        toggle.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        toggle.setBackground(background(0xFF263449, 12));
        panel.addView(toggle, new LinearLayout.LayoutParams(220, 48));

        LinearLayout controls = new LinearLayout(this);
        controls.setOrientation(LinearLayout.VERTICAL);
        valueText = new TextView(this);
        valueText.setTextColor(Color.WHITE);
        valueText.setTextSize(15);
        valueText.setPadding(4, 12, 4, 4);
        valueText.setText("speed: 1.0x");
        controls.addView(valueText);

        SeekBar slider = new SeekBar(this);
        slider.setMax(40); // 1.0x .. 5.0x in 0.1 increments
        slider.setProgress(0);
        slider.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar bar, int progress, boolean fromUser) {
                float multiplier = 1.0f + progress / 10.0f;
                valueText.setText(String.format(Locale.US, "speed: %.1fx", multiplier));
                nativeSetMultiplier(multiplier);
            }
            @Override public void onStartTrackingTouch(SeekBar bar) {}
            @Override public void onStopTrackingTouch(SeekBar bar) {}
        });
        controls.addView(slider, new LinearLayout.LayoutParams(220, 52));
        TextView note = new TextView(this);
        note.setText("1.0x — normal speed\n5.0x — maximum");
        note.setTextColor(0xFFB8C5D6);
        note.setTextSize(11);
        controls.addView(note);
        panel.addView(controls);

        toggle.setOnClickListener(v -> {
            expanded = !expanded;
            controls.setVisibility(expanded ? View.VISIBLE : View.GONE);
            toggle.setText(expanded ? "snake mod  •  hide ▲" : "snake mod  •  speed ▼");
        });
        controls.setVisibility(View.GONE);
        root = panel;

        params = new WindowManager.LayoutParams(
                WindowManager.LayoutParams.WRAP_CONTENT,
                WindowManager.LayoutParams.WRAP_CONTENT,
                WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,
                PixelFormat.TRANSLUCENT);
        params.gravity = Gravity.TOP | Gravity.START;
        params.x = 24;
        params.y = 120;
        panel.setOnTouchListener(new View.OnTouchListener() {
            private int startX, startY;
            private float downX, downY;
            @Override public boolean onTouch(View view, MotionEvent event) {
                if (event.getAction() == MotionEvent.ACTION_DOWN) {
                    startX = params.x; startY = params.y;
                    downX = event.getRawX(); downY = event.getRawY();
                    return false;
                }
                if (event.getAction() == MotionEvent.ACTION_MOVE && event.getPointerCount() == 1) {
                    params.x = startX + (int)(event.getRawX() - downX);
                    params.y = startY + (int)(event.getRawY() - downY);
                    try { windowManager.updateViewLayout(root, params); } catch (Exception ignored) {}
                }
                return false;
            }
        });
        try { windowManager.addView(root, params); }
        catch (Exception error) { stopSelf(); }
    }

    @Override public void onDestroy() {
        if (root != null && windowManager != null) {
            try { windowManager.removeView(root); } catch (Exception ignored) {}
        }
        super.onDestroy();
    }

    @Override public IBinder onBind(Intent intent) { return null; }
}
