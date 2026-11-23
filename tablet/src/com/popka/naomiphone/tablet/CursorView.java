package com.popka.naomiphone.tablet;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.view.View;

/**
 * Курсор приставки поверх кадра: картинку («Мягкая», под размер кадра на этом экране) рисует приставка, здесь — 1:1.
 * Двигается сдвигом вида (setTranslation), без перерисовки.
 */
final class CursorView extends View {
    private Bitmap bitmap;
    private int hotX, hotY;

    CursorView(Context context) {
        super(context);
        setWillNotDraw(false);
    }

    void setImage(Bitmap b, int hx, int hy) {
        bitmap = b;
        hotX = hx;
        hotY = hy;
        invalidate();
    }

    /** x, y — точка курсора на экране, пиксели */
    void moveTo(float x, float y) {
        setTranslationX(x - hotX);
        setTranslationY(y - hotY);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        if (bitmap != null)
            canvas.drawBitmap(bitmap, 0, 0, null);
    }
}
