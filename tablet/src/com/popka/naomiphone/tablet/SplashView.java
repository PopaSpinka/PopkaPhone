package com.popka.naomiphone.tablet;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.Shader;
import android.graphics.Typeface;
import android.view.View;
import android.view.animation.DecelerateInterpolator;

import java.util.Calendar;

/**
 * Заставка без дока (docs/SCREENS.md): пока стол на планшет не выведен — обои стола во весь экран и часы с датой,
 * тот же вид и то же место, что у монитора приставки (pi/dock/clock.cpp: цифры Google Sans Flex 150 точек стола,
 * тонкие, скруглённые; дата капсом 19 точек; место — левый верх чернил в долях экрана из настроек стола «Без дока»).
 * Лежит поверх кадра. Обои и часы проявляются и тают порознь: стол приходит — обои тают за 200 мс, часы уходят вверх
 * на 4 % своей высоты за 140; стол уходит — обои за 250, часы съезжают на место за 180 (как у приставки, lock.cpp).
 */
final class SplashView extends View {
    private static final String[] DAYS = {"ВОСКРЕСЕНЬЕ", "ПОНЕДЕЛЬНИК", "ВТОРНИК", "СРЕДА", "ЧЕТВЕРГ", "ПЯТНИЦА", "СУББОТА"};
    private static final String[] MONTHS = {"ЯНВАРЯ", "ФЕВРАЛЯ", "МАРТА", "АПРЕЛЯ", "МАЯ", "ИЮНЯ", "ИЮЛЯ", "АВГУСТА",
            "СЕНТЯБРЯ", "ОКТЯБРЯ", "НОЯБРЯ", "ДЕКАБРЯ"};
    private static final int INK = 0xECE9E3;

    private final Paint wallPaint = new Paint(Paint.FILTER_BITMAP_FLAG);
    private final Paint gradient = new Paint();
    private final Paint time = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.SUBPIXEL_TEXT_FLAG);
    private final Paint date = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.SUBPIXEL_TEXT_FLAG);
    private final Matrix matrix = new Matrix();
    private final Rect bounds = new Rect();
    private Bitmap wall;
    private boolean clock = true;
    private int clockX = 275, clockY = 378, deskWidth = 1600;   // настройки стола «Без дока» (tablet::Splash)
    private float wallAlpha = 1, clockAlpha = 1;
    private boolean shown = true;
    private ValueAnimator wallAnim, clockAnim;
    private final Runnable minute = this::tick;

    SplashView(Context context) {
        super(context);
        time.setFontFeatureSettings("tnum");
        date.setFontFeatureSettings("tnum");
        date.setLetterSpacing(0.1f);
    }

    void setFonts(Typeface flex, Typeface sans) {
        if (flex != null)
            time.setTypeface(flex);
        if (sans != null)
            date.setTypeface(sans);
        invalidate();
    }

    void setWallpaper(Bitmap b) {
        wall = b;
        invalidate();
    }

    void setConfig(boolean clock, int x, int y, int width) {
        this.clock = clock;
        clockX = x;
        clockY = y;
        deskWidth = Math.max(640, width);
        invalidate();
    }

    boolean shown() {
        return shown;
    }

    /** Показать или убрать; animate — плавно. */
    void show(boolean on, boolean animate) {
        if (on == shown && animate)
            return;
        shown = on;
        if (wallAnim != null)
            wallAnim.cancel();
        if (clockAnim != null)
            clockAnim.cancel();
        if (on)
            setVisibility(VISIBLE);
        if (!animate) {
            wallAlpha = clockAlpha = on ? 1 : 0;
            if (!on)
                setVisibility(INVISIBLE);
            invalidate();
            return;
        }
        wallAnim = run(wallAlpha, on ? 1 : 0, on ? 250 : 200, true);
        clockAnim = run(clockAlpha, on ? 1 : 0, on ? 180 : 140, false);
    }

    private ValueAnimator run(float from, float to, long ms, boolean wallpaper) {
        ValueAnimator a = ValueAnimator.ofFloat(from, to);
        a.setDuration(ms);
        a.setInterpolator(new DecelerateInterpolator(1.5f));   // как OutCubic анимаций стола
        a.addUpdateListener(v -> {
            if (wallpaper)
                wallAlpha = (float) v.getAnimatedValue();
            else
                clockAlpha = (float) v.getAnimatedValue();
            if (!shown && wallAlpha <= 0 && clockAlpha <= 0)
                setVisibility(INVISIBLE);
            invalidate();
        });
        a.start();
        return a;
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        tick();
    }

    @Override
    protected void onDetachedFromWindow() {
        removeCallbacks(minute);
        super.onDetachedFromWindow();
    }

    private void tick() {   // новая минута — перерисовать
        removeCallbacks(minute);
        invalidate();
        postDelayed(minute, 60000 - System.currentTimeMillis() % 60000 + 30);
    }

    @Override
    protected void onSizeChanged(int w, int h, int ow, int oh) {
        gradient.setShader(new LinearGradient(0, 0, 0, h, 0xFF1B1424, 0xFF08070B, Shader.TileMode.CLAMP));
    }

    @Override
    protected void onDraw(Canvas c) {
        final int w = getWidth(), h = getHeight();
        if (w <= 0 || h <= 0)
            return;
        if (wallAlpha > 0) {
            final int a = Math.round(wallAlpha * 255);
            if (wall != null) {   // во весь экран с обрезкой, как обои стола (PreserveAspectCrop)
                final float k = Math.max((float) w / wall.getWidth(), (float) h / wall.getHeight());
                matrix.setScale(k, k);
                matrix.postTranslate((w - wall.getWidth() * k) / 2, (h - wall.getHeight() * k) / 2);
                wallPaint.setAlpha(a);
                c.drawBitmap(wall, matrix, wallPaint);
            } else {
                gradient.setAlpha(a);
                c.drawRect(0, 0, w, h, gradient);
            }
        }
        if (clockAlpha <= 0 || !clock)
            return;
        final float pt = (float) w / deskWidth;   // пикселей экрана в точке стола
        final float tpx = 150 * pt, dpx = 19 * pt;
        final Calendar now = Calendar.getInstance();
        final String hm = String.format("%02d:%02d", now.get(Calendar.HOUR_OF_DAY), now.get(Calendar.MINUTE));
        final String day = DAYS[now.get(Calendar.DAY_OF_WEEK) - 1] + " · " + now.get(Calendar.DAY_OF_MONTH) + " "
                + MONTHS[now.get(Calendar.MONTH)];
        time.setTextSize(tpx);
        time.setFontVariationSettings("'wght' 300, 'ROND' 100, 'opsz' " + Math.round(Math.max(6, Math.min(144, tpx))));
        date.setTextSize(dpx);
        date.setFontVariationSettings("'wght' 500, 'opsz' " + Math.round(Math.max(6, Math.min(144, dpx))));
        final int ta = Math.round(clockAlpha * 255), da = Math.round(clockAlpha * 0.85f * 255);
        final int shadow = Math.round(clockAlpha * 0.42f * 255) << 24;
        time.setColor(ta << 24 | INK);
        date.setColor(da << 24 | INK);
        time.setShadowLayer(4 * pt, 0, 2 * pt, shadow);
        date.setShadowLayer(4 * pt, 0, 2 * pt, shadow);
        // место — по чернилам: левый верх чернил цифр — доля экрана; дата — под цифрами через 16 точек
        final float left = clockX / 10000f * w, top = clockY / 10000f * h;
        time.getTextBounds(hm, 0, hm.length(), bounds);
        final float timeH = bounds.height();
        final float tx = left - bounds.left, ty = top - bounds.top;
        date.getTextBounds(day, 0, day.length(), bounds);
        final float dx = left - bounds.left, dy = top + timeH + 16 * pt - bounds.top;
        final float shift = -(1 - clockAlpha) * 0.04f * (timeH + 16 * pt + bounds.height());
        c.drawText(hm, tx, ty + shift, time);
        c.drawText(day, dx, dy + shift, date);
    }
}
