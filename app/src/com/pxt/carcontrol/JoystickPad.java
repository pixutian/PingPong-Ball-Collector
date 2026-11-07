package com.pxt.carcontrol;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.DashPathEffect;
import android.graphics.Paint;
import android.graphics.RadialGradient;
import android.graphics.RectF;
import android.graphics.Shader;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

/**
 * 圆形方向盘（V3）：8 个扇区对应 A~H，松手回调 0。
 * 视觉：白盘 + 浅灰刻度 + 蓝色渐变手柄，扇区高亮使用淡蓝色。
 */
public class JoystickPad extends View {

    public interface OnDirectionListener {
        void onDirection(int code);
    }

    private static final int[] SECTOR_CODE = {3, 2, 1, 8, 7, 6, 5, 4};

    private final Paint disc = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint ring = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint inner = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint wedge = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint tick = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint knob = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint knobRing = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint dead = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);

    private float cx, cy, radius, knobX, knobY;
    private int activeCode = 0;
    private boolean enabled = true;
    private OnDirectionListener listener;

    public JoystickPad(Context c) {
        this(c, null);
    }

    public JoystickPad(Context c, AttributeSet a) {
        super(c, a);
        float d = getResources().getDisplayMetrics().density;

        disc.setColor(Color.WHITE);
        ring.setStyle(Paint.Style.STROKE);
        ring.setStrokeWidth(1.4f * d);
        ring.setColor(0xFFE6ECF3);
        inner.setStyle(Paint.Style.STROKE);
        inner.setStrokeWidth(1f * d);
        inner.setColor(0xFFEEF3F9);

        wedge.setColor(0xFFE8F0FE);

        tick.setStyle(Paint.Style.STROKE);
        tick.setStrokeWidth(1.2f * d);
        tick.setColor(0xFFDDE5EF);

        knob.setStyle(Paint.Style.FILL);
        knobRing.setStyle(Paint.Style.STROKE);
        knobRing.setStrokeWidth(2f * d);
        knobRing.setColor(Color.WHITE);

        dead.setStyle(Paint.Style.STROKE);
        dead.setStrokeWidth(1f * d);
        dead.setColor(0xFFE3EAF3);
        dead.setPathEffect(new DashPathEffect(new float[]{4 * d, 5 * d}, 0));

        text.setColor(0xFF94A3B8);
        text.setTextSize(10 * d);
        text.setTextAlign(Paint.Align.CENTER);
    }

    public void setOnDirectionListener(OnDirectionListener l) {
        listener = l;
    }

    public void setEnabledState(boolean on) {
        enabled = on;
        if (!on) {
            activeCode = 0;
            knobX = cx;
            knobY = cy;
        }
        invalidate();
    }

    @Override
    protected void onSizeChanged(int w, int h, int ow, int oh) {
        super.onSizeChanged(w, h, ow, oh);
        cx = w / 2f;
        cy = h / 2f;
        radius = Math.min(w, h) / 2f - dp(8);
        knobX = cx;
        knobY = cy;
    }

    private float dp(float v) {
        return v * getResources().getDisplayMetrics().density;
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        int alpha = enabled ? 255 : 120;

        disc.setAlpha(alpha);
        canvas.drawCircle(cx, cy, radius, disc);
        canvas.drawCircle(cx, cy, radius, ring);
        canvas.drawCircle(cx, cy, radius * 0.97f, inner);

        if (activeCode != 0) {
            int idx = indexOfCode(activeCode);
            RectF oval = new RectF(cx - radius, cy - radius, cx + radius, cy + radius);
            canvas.drawArc(oval, -(idx * 45f + 22.5f), 45f, true, wedge);
        }

        for (int i = 0; i < 8; i++) {
            double a = Math.toRadians(i * 45);
            float x1 = cx + (float) Math.cos(a) * radius * 0.87f;
            float y1 = cy - (float) Math.sin(a) * radius * 0.87f;
            float x2 = cx + (float) Math.cos(a) * radius * 0.96f;
            float y2 = cy - (float) Math.sin(a) * radius * 0.96f;
            canvas.drawLine(x1, y1, x2, y2, tick);
        }

        text.setAlpha(alpha);
        canvas.drawText("前", cx, cy - radius + dp(15), text);
        canvas.drawText("后", cx, cy + radius - dp(6), text);
        canvas.drawText("左", cx - radius + dp(13), cy + dp(3.5f), text);
        canvas.drawText("右", cx + radius - dp(13), cy + dp(3.5f), text);

        canvas.drawCircle(cx, cy, radius * 0.15f, dead);

        knob.setShader(new RadialGradient(knobX - radius * 0.06f, knobY - radius * 0.08f,
                radius * 0.26f, new int[]{0xFF64B5F6, 0xFF1565C0}, null,
                Shader.TileMode.CLAMP));
        knob.setAlpha(alpha);
        knobRing.setAlpha(alpha);
        canvas.drawCircle(knobX, knobY, radius * 0.235f, knob);
        canvas.drawCircle(knobX, knobY, radius * 0.235f, knobRing);
    }

    private int indexOfCode(int code) {
        for (int i = 0; i < SECTOR_CODE.length; i++) {
            if (SECTOR_CODE[i] == code) return i;
        }
        return 0;
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        if (!enabled) return false;
        switch (e.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_MOVE: {
                float dx = e.getX() - cx;
                float dy = e.getY() - cy;
                float dist = (float) Math.hypot(dx, dy);
                float max = radius * 0.68f;
                if (dist > max) {
                    dx = dx / dist * max;
                    dy = dy / dist * max;
                    dist = max;
                }
                knobX = cx + dx;
                knobY = cy + dy;

                int code = 0;
                if (dist > radius * 0.16f) {
                    double ang = Math.toDegrees(Math.atan2(-dy, dx));
                    int idx = (int) Math.round(ang / 45.0) % 8;
                    if (idx < 0) idx += 8;
                    code = SECTOR_CODE[idx];
                }
                if (code != activeCode) {
                    activeCode = code;
                    if (listener != null) listener.onDirection(code);
                }
                invalidate();
                return true;
            }
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL: {
                if (activeCode != 0) {
                    activeCode = 0;
                    if (listener != null) listener.onDirection(0);
                }
                knobX = cx;
                knobY = cy;
                invalidate();
                return true;
            }
        }
        return super.onTouchEvent(e);
    }
}
