package com.popka.naomiphone.tablet;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;

import java.util.ArrayList;
import java.util.List;

/**
 * Палец для проверок (tools/tablet finger, docs/TABLET.md): жест — событиями MotionEvent в тот же разбор касаний
 * (Gestures), шагом 8 мс, как у сенсора. Сенсор планшета adb писать не даёт (SELinux), а `input tap` — один палец и
 * по полсекунды на касание (запуск Java) — двойного щелчка и двух пальцев им не сделать.
 * Принимает только отправителя с правом DUMP — его есть у adb (shell), у приложений на планшете его нет.
 *
 *   am broadcast -a com.popka.naomiphone.tablet.FINGER --es g dtap|rtap|scroll|drag --ei x X --ei y Y
 *                [--ei dx DX --ei dy DY --ei ms ДЛИТЕЛЬНОСТЬ]       (пиксели экрана, мс)
 */
final class TestFinger extends BroadcastReceiver {
    static final String ACTION = "com.popka.naomiphone.tablet.FINGER";
    private static final int STEP_MS = 8;

    private final View target;
    private final Handler handler = new Handler(Looper.getMainLooper());

    TestFinger(View target) {
        this.target = target;
    }

    void register(Context c) {
        c.registerReceiver(this, new IntentFilter(ACTION), "android.permission.DUMP", handler);
    }

    void unregister(Context c) {
        c.unregisterReceiver(this);
    }

    /** шаг жеста: через сколько мс от начала, действие, точки пальцев */
    private static final class Step {
        final long at;
        final int action;
        final float[][] points;

        Step(long at, int action, float[]... points) {
            this.at = at;
            this.action = action;
            this.points = points;
        }
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        String g = intent.getStringExtra("g");
        float x = intent.getIntExtra("x", 0), y = intent.getIntExtra("y", 0);
        float dx = intent.getIntExtra("dx", 0), dy = intent.getIntExtra("dy", 0);
        int ms = Math.max(STEP_MS, intent.getIntExtra("ms", 500));
        List<Step> steps = new ArrayList<>();
        int pointerDown1 = MotionEvent.ACTION_POINTER_DOWN | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT);
        int pointerUp1 = MotionEvent.ACTION_POINTER_UP | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT);
        if ("dtap".equals(g)) {   // два касания: лежит 50 мс, между ними 120 мс, второе — на 3 пикселя в стороне
            for (int k = 0; k < 2; ++k) {
                long t0 = k * 170L;
                float[] p = {x + 3 * k, y + 2 * k};
                steps.add(new Step(t0, MotionEvent.ACTION_DOWN, p));
                steps.add(new Step(t0 + 50, MotionEvent.ACTION_UP, p));
            }
        } else if ("rtap".equals(g)) {   // двумя пальцами на 80 мс
            float[] a = {x - 60, y}, b = {x + 60, y};
            steps.add(new Step(0, MotionEvent.ACTION_DOWN, a));
            steps.add(new Step(16, pointerDown1, a, b));
            steps.add(new Step(96, pointerUp1, a, b));
            steps.add(new Step(112, MotionEvent.ACTION_UP, a));
        } else if ("scroll".equals(g)) {   // два пальца ведут на dx, dy за ms
            float[] a = {x - 100, y}, b = {x + 100, y};
            steps.add(new Step(0, MotionEvent.ACTION_DOWN, a));
            steps.add(new Step(16, pointerDown1, a, b));
            int n = ms / STEP_MS;
            for (int i = 1; i <= n; ++i) {
                float k = (float) i / n;
                steps.add(new Step(16 + i * STEP_MS, MotionEvent.ACTION_MOVE, new float[] {a[0] + dx * k, a[1] + dy * k},
                        new float[] {b[0] + dx * k, b[1] + dy * k}));
            }
            float[] ea = {a[0] + dx, a[1] + dy}, eb = {b[0] + dx, b[1] + dy};
            steps.add(new Step(16 + n * STEP_MS + 8, pointerUp1, ea, eb));
            steps.add(new Step(16 + n * STEP_MS + 16, MotionEvent.ACTION_UP, ea));
        } else if ("drag".equals(g)) {   // один палец ведёт на dx, dy за ms
            float[] a = {x, y};
            steps.add(new Step(0, MotionEvent.ACTION_DOWN, a));
            int n = ms / STEP_MS;
            for (int i = 1; i <= n; ++i) {
                float k = (float) i / n;
                steps.add(new Step(i * STEP_MS, MotionEvent.ACTION_MOVE, new float[] {x + dx * k, y + dy * k}));
            }
            steps.add(new Step(n * STEP_MS + 8, MotionEvent.ACTION_UP, new float[] {x + dx, y + dy}));
        } else {
            Log.w(Link.TAG, "палец: жест «" + g + "» не знаю");
            return;
        }
        Log.i(Link.TAG, "палец: " + g + " " + (int) x + "," + (int) y);
        long down = SystemClock.uptimeMillis() + 20;
        for (Step s : steps)
            handler.postAtTime(() -> dispatch(down, s), down + s.at);
    }

    private void dispatch(long down, Step s) {
        int n = s.points.length;
        MotionEvent.PointerProperties[] props = new MotionEvent.PointerProperties[n];
        MotionEvent.PointerCoords[] coords = new MotionEvent.PointerCoords[n];
        for (int i = 0; i < n; ++i) {
            props[i] = new MotionEvent.PointerProperties();
            props[i].id = i;
            props[i].toolType = MotionEvent.TOOL_TYPE_FINGER;
            coords[i] = new MotionEvent.PointerCoords();
            coords[i].x = s.points[i][0];
            coords[i].y = s.points[i][1];
            coords[i].pressure = 1;
            coords[i].size = 0.05f;
        }
        MotionEvent e = MotionEvent.obtain(down, down + s.at, s.action, n, props, coords, 0, 0, 1, 1, 0, 0,
                InputDevice.SOURCE_TOUCHSCREEN, 0);
        target.dispatchTouchEvent(e);
        e.recycle();
    }
}
