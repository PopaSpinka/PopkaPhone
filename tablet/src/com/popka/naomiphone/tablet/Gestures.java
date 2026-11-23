package com.popka.naomiphone.tablet;

import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.view.MotionEvent;

/**
 * Касания планшета → мышь стола (docs/TABLET.md, «Касания»):
 *  • коснулся и отпустил — щелчок там, где палец; второй щелчок рядом и быстро встаёт ровно на место первого (стол
 *    узнаёт двойной);
 *  • коснулся и повёл — хват: кнопка нажата в точке касания, дальше мышь идёт за пальцем (окна, выделение,
 *    ползунки), отпустил — кнопка отпущена;
 *  • два пальца повели — прокрутка за пальцами (как трекпадом приставки, с инерцией после);
 *  • двумя пальцами коснулся и отпустил — правый щелчок;
 *  • перо над экраном — наведение (без кнопки).
 * Кнопку палец нажимает не сразу: ждёт kHoldMs (или пока не сдвинется) — второй палец за это время делает
 * прокрутку или правый щелчок, а не хват первым. Координаты — доли кадра (0…1), время — метка касания, мкс.
 */
final class Gestures {
    interface Sink {
        void pointer(int buttons, float x, float y, long tUs);
        void scroll(float x, float y, float dx, float dy, boolean fingers, long tUs);
        /** куда на экране кадр: левый верх и размер, пикселей */
        float[] video();
    }

    private static final long HOLD_MS = 90;          // столько палец лежит без движения — хват (кнопка нажата)
    private static final long TWO_TAP_MS = 350;      // касание двумя короче — правый щелчок
    private static final long DOUBLE_MS = 400;       // второй щелчок быстрее и ближе — на место первого
    private static final int IDLE = 0, PENDING = 1, DRAG = 2, TWO = 3, DONE = 4;

    private final Sink sink;
    private final float slop;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private int state = IDLE;
    private int primaryId = -1;
    private float downX, downY, lastX, lastY;   // экран, пиксели
    private long downUs;
    private float twoX, twoY;                    // середина двух пальцев (прошлая)
    private long twoDownMs;
    private boolean twoScrolling;
    private long lastTapMs;
    private float lastTapX = -1, lastTapY = -1;  // доли кадра

    Gestures(Sink sink, float slop) {
        this.sink = sink;
        this.slop = slop;
    }

    private final Runnable hold = () -> {
        if (state == PENDING)
            press(downX, downY, downUs);
    };

    private static long timeUs(MotionEvent e) {
        return Build.VERSION.SDK_INT >= 34 ? e.getEventTimeNanos() / 1000 : e.getEventTime() * 1000;
    }

    private static long historicalUs(MotionEvent e, int h) {
        return Build.VERSION.SDK_INT >= 34 ? e.getHistoricalEventTimeNanos(h) / 1000 : e.getHistoricalEventTime(h) * 1000;
    }

    private float fx(float x) {
        float[] v = sink.video();
        return Math.max(0, Math.min(1, (x - v[0]) / Math.max(1, v[2])));
    }

    private float fy(float y) {
        float[] v = sink.video();
        return Math.max(0, Math.min(1, (y - v[1]) / Math.max(1, v[3])));
    }

    private void press(float x, float y, long tUs) {
        state = DRAG;
        sink.pointer(1, fx(x), fy(y), tUs);
    }

    boolean onTouch(MotionEvent e) {
        switch (e.getActionMasked()) {
            case MotionEvent.ACTION_DOWN: {
                state = PENDING;
                primaryId = e.getPointerId(0);
                downX = lastX = e.getX();
                downY = lastY = e.getY();
                downUs = timeUs(e);
                handler.postDelayed(hold, HOLD_MS);
                break;
            }
            case MotionEvent.ACTION_POINTER_DOWN: {
                if (state == PENDING && e.getPointerCount() == 2) {
                    handler.removeCallbacks(hold);
                    state = TWO;
                    twoX = (e.getX(0) + e.getX(1)) / 2;
                    twoY = (e.getY(0) + e.getY(1)) / 2;
                    twoDownMs = e.getEventTime();
                    twoScrolling = false;
                }
                break;
            }
            case MotionEvent.ACTION_MOVE: {
                int i = e.findPointerIndex(primaryId);
                if (state == PENDING && i >= 0) {
                    float x = e.getX(i), y = e.getY(i);
                    if (Math.hypot(x - downX, y - downY) > slop) {
                        handler.removeCallbacks(hold);
                        press(downX, downY, downUs);
                    }
                }
                if (state == DRAG && i >= 0) {
                    for (int h = 0; h < e.getHistorySize(); ++h)
                        sink.pointer(1, fx(e.getHistoricalX(i, h)), fy(e.getHistoricalY(i, h)), historicalUs(e, h));
                    lastX = e.getX(i);
                    lastY = e.getY(i);
                    sink.pointer(1, fx(lastX), fy(lastY), timeUs(e));
                } else if (state == TWO && e.getPointerCount() >= 2) {
                    float cx = (e.getX(0) + e.getX(1)) / 2, cy = (e.getY(0) + e.getY(1)) / 2;
                    if (!twoScrolling && Math.hypot(cx - twoX, cy - twoY) > slop)
                        twoScrolling = true;
                    if (twoScrolling) {
                        float[] v = sink.video();
                        sink.scroll(fx(cx), fy(cy), (cx - twoX) / Math.max(1, v[2]), (cy - twoY) / Math.max(1, v[3]),
                                true, timeUs(e));
                        twoX = cx;
                        twoY = cy;
                    }
                }
                break;
            }
            case MotionEvent.ACTION_POINTER_UP: {
                if (state == TWO)
                    endTwo(e);
                else if (state == DRAG && e.getPointerId(e.getActionIndex()) == primaryId)
                    release(timeUs(e));
                break;
            }
            case MotionEvent.ACTION_UP: {
                handler.removeCallbacks(hold);
                if (state == PENDING) {   // щелчок: нажал и отпустил в точке касания
                    float x = fx(downX), y = fy(downY);
                    float[] v = sink.video();
                    if (e.getEventTime() - lastTapMs < DOUBLE_MS && lastTapX >= 0
                            && Math.hypot((x - lastTapX) * v[2], (y - lastTapY) * v[3]) < slop * 3) {
                        x = lastTapX;
                        y = lastTapY;
                    }
                    sink.pointer(1, x, y, downUs);
                    sink.pointer(0, x, y, timeUs(e));
                    lastTapMs = e.getEventTime();
                    lastTapX = x;
                    lastTapY = y;
                } else if (state == DRAG) {
                    release(timeUs(e));
                } else if (state == TWO) {
                    endTwo(e);
                }
                state = IDLE;
                break;
            }
            case MotionEvent.ACTION_CANCEL: {
                handler.removeCallbacks(hold);
                cancel();
                break;
            }
            default:
                break;
        }
        return true;
    }

    /** Перо над экраном — наведение. */
    boolean onHover(MotionEvent e) {
        if (state != IDLE)
            return false;
        int a = e.getActionMasked();
        if (a == MotionEvent.ACTION_HOVER_MOVE || a == MotionEvent.ACTION_HOVER_ENTER) {
            sink.pointer(0, fx(e.getX()), fy(e.getY()), timeUs(e));
            return true;
        }
        return false;
    }

    /** Касание пропало (окно ушло с экрана) — отпустить, что держали. */
    void cancel() {
        if (state == DRAG)
            sink.pointer(0, fx(lastX), fy(lastY), System.nanoTime() / 1000);
        else if (state == TWO && twoScrolling)
            sink.scroll(fx(twoX), fy(twoY), 0, 0, false, System.nanoTime() / 1000);
        state = IDLE;
    }

    private void release(long tUs) {
        sink.pointer(0, fx(lastX), fy(lastY), tUs);
        state = DONE;
    }

    private void endTwo(MotionEvent e) {
        if (twoScrolling) {
            sink.scroll(fx(twoX), fy(twoY), 0, 0, false, timeUs(e));
        } else if (e.getEventTime() - twoDownMs < TWO_TAP_MS) {   // касание двумя — правый щелчок
            sink.pointer(2, fx(twoX), fy(twoY), timeUs(e));
            sink.pointer(0, fx(twoX), fy(twoY), timeUs(e));
        }
        state = DONE;
    }
}
