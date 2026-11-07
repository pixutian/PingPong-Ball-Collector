package com.pxt.carcontrol;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.StateListDrawable;
import android.util.TypedValue;
import android.view.View;
import android.widget.TextView;

/** 统一的配色与控件样式（蓝白灰）。 */
final class Ui {

    static final int BG        = 0xFFF3F6FA;
    static final int CARD      = 0xFFFFFFFF;
    static final int LINE      = 0xFFE6ECF3;
    static final int PRIMARY   = 0xFF1565C0;
    static final int PRIMARY_D = 0xFF0B3D91;
    static final int ACCENT    = 0xFF42A5F5;
    static final int TEXT      = 0xFF0F172A;
    static final int MUTED     = 0xFF5B6B7F;
    static final int PALE      = 0xFFE8F0FE;
    static final int STOP      = 0xFFD32F2F;
    static final int TRACK     = 0xFFDCE3EC;

    private Ui() {
    }

    static int dp(Context c, float v) {
        return Math.round(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v,
                c.getResources().getDisplayMetrics()));
    }

    static GradientDrawable round(Context c, int color, float radiusDp) {
        GradientDrawable d = new GradientDrawable();
        d.setColor(color);
        d.setCornerRadius(dp(c, radiusDp));
        return d;
    }

    static GradientDrawable roundStroke(Context c, int color, int stroke, float radiusDp) {
        GradientDrawable d = round(c, color, radiusDp);
        d.setStroke(dp(c, 1), stroke);
        return d;
    }

    static GradientDrawable vertical(Context c, int top, int bottom, float radiusDp) {
        GradientDrawable d = new GradientDrawable(GradientDrawable.Orientation.TOP_BOTTOM,
                new int[]{top, bottom});
        d.setCornerRadius(dp(c, radiusDp));
        return d;
    }

    /** 带按下反馈的圆角按钮背景。 */
    static StateListDrawable pressable(Context c, int normal, int pressed,
                                       int strokeColor, float radiusDp) {
        StateListDrawable s = new StateListDrawable();
        s.addState(new int[]{android.R.attr.state_pressed},
                roundStroke(c, pressed, strokeColor, radiusDp));
        s.addState(new int[]{}, roundStroke(c, normal, strokeColor, radiusDp));
        return s;
    }

    static TextView label(Context c, String text, float size, int color, boolean medium) {
        TextView v = new TextView(c);
        v.setText(text);
        v.setTextSize(size);
        v.setTextColor(color);
        v.setTypeface(medium ? android.graphics.Typeface.create("sans-serif-medium",
                android.graphics.Typeface.NORMAL) : android.graphics.Typeface.DEFAULT);
        return v;
    }

    static void card(View v, Context c) {
        v.setBackground(roundStroke(c, CARD, LINE, 18));
        v.setElevation(dp(c, 2));
    }

    static int blend(int a, int b, float t) {
        int ar = Color.red(a), ag = Color.green(a), ab = Color.blue(a);
        int br = Color.red(b), bg = Color.green(b), bb = Color.blue(b);
        return Color.rgb(
                (int) (ar + (br - ar) * t),
                (int) (ag + (bg - ag) * t),
                (int) (ab + (bb - ab) * t));
    }
}
