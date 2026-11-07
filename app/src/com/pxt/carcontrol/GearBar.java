package com.pxt.carcontrol;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.view.View;

/** 7 个圆点组成的档位条。 */
public class GearBar extends View {

    private final Paint on = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint off = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint ring = new Paint(Paint.ANTI_ALIAS_FLAG);
    private int count = 7;
    private int index = 3;

    public GearBar(Context c) {
        this(c, null);
    }

    public GearBar(Context c, AttributeSet a) {
        super(c, a);
        float d = getResources().getDisplayMetrics().density;
        on.setColor(Ui.PRIMARY);
        off.setColor(Ui.TRACK);
        ring.setStyle(Paint.Style.STROKE);
        ring.setStrokeWidth(1 * d);
        ring.setColor(0x220F172A);
    }

    public void set(int count, int index) {
        this.count = Math.max(1, count);
        this.index = Math.max(0, Math.min(this.count - 1, index));
        invalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        float w = getWidth();
        float h = getHeight();
        float gap = w * 0.02f;
        float cell = (w - gap * (count - 1)) / count;
        float r = Math.min(cell / 2f, h / 2f);
        float cy = h / 2f;
        for (int i = 0; i < count; i++) {
            float cx = cell / 2f + i * (cell + gap);
            if (i == index) {
                canvas.drawRoundRect(new RectF(cx - cell / 2f, cy - r, cx + cell / 2f, cy + r),
                        r, r, on);
            } else {
                canvas.drawRoundRect(new RectF(cx - cell / 2f, cy - r, cx + cell / 2f, cy + r),
                        r, r, off);
                canvas.drawRoundRect(new RectF(cx - cell / 2f, cy - r, cx + cell / 2f, cy + r),
                        r, r, ring);
            }
        }
    }
}
