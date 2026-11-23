package com.popka.naomiphone.tablet;

import android.app.Activity;
import android.app.admin.DevicePolicyManager;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.media.MediaMetadata;
import android.media.session.MediaSession;
import android.media.session.PlaybackState;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.json.JSONObject;

import java.io.File;
import java.io.FileOutputStream;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;

/**
 * НаомиФон для планшета (docs/TABLET.md): рабочий стол телефона с приставки на Pi во весь экран, в своих пропорциях
 * (по краям — чёрные поля); курсор мыши и трекпада приставки — поверх; касания — мышь стола (Gestures). Связь только
 * пока приложение на экране: ушло — приставка ничего не шлёт.
 * Планшет — один из экранов стола (docs/SCREENS.md): стол на него не выведен — заставка без дока (SplashView: обои и
 * часы, как у монитора приставки), двойное касание — «стол сюда»; вывели — заставка тает в картинку.
 * Киоск (приложение — владелец устройства, tools/tablet kiosk): на экране только оно, без шторки, «домой» и жестов
 * системы; кнопка питания гасит экран и включает сразу сюда.
 * Телевизор Android TV (docs/TV.md) — то же приложение: экран «tv». Открыли плиткой или кнопкой пульта — «стол сюда»
 * (как двойное касание планшета); открыла служба по слову приставки (TvService: телевизор выбрали в «Экранах») —
 * стол придёт сам. Стол с телевизора сняли (Leave) — приложение уходит: на экране снова то, что было до него
 * (HDMI-вход, другая программа), а если телевизор будили мы — он засыпает. «Назад» на пульте — уйти. Пока стол на
 * экране, приложение значится играющим (MediaSession) — чтобы дом не погасил телевизор как забытый.
 */
public final class MainActivity extends Activity implements Link.Listener, Gestures.Sink, SurfaceHolder.Callback {
    /** открыли кнопкой пульта (TvService): как подключимся — «стол сюда» */
    static final String EXTRA_WAKE = "naomi.wake";
    /** открыла служба, и телевизор при этом спал: стол снимут — усыпить обратно */
    static final String EXTRA_ASLEEP = "naomi.asleep";
    private static volatile MainActivity front;   // приложение на экране (между onResume и onPause)

    private boolean tv;                      // телевизор Android TV: пульт вместо касаний, без киоска
    private boolean wakeWanted;              // подключимся — просить стол сюда
    private boolean sleepOnLeave;            // телевизор будили мы
    private MediaSession session;            // ТВ: «играет», пока стол на экране
    private FrameLayout root;
    private SurfaceView video;
    private CursorView cursor;
    private SplashView splash;
    private TextView status;
    private long wakeDownMs;                 // прошлое касание заставки: второе быстро и рядом — «стол сюда»
    private float wakeX, wakeY;
    private volatile String wallHash = "";   // какие обои заставки у нас есть (хэш от приставки)
    private final Runnable frameLate = () -> reveal();   // первый кадр не отметился — заставке всё равно таять
    private Link link;
    private VideoDecoder decoder;
    private Gestures gestures;
    private WifiManager.WifiLock wifiLock;
    private TestFinger finger;

    private volatile int frameW, frameH;
    private volatile boolean desk;
    private int linkState = -1;
    private final float[] videoRect = new float[4];   // левый верх и размер кадра на экране, пиксели
    private volatile boolean cursorShown;
    private volatile float cursorX, cursorY;
    private boolean cursorPosted;
    private int[] sentView = new int[2];
    private double rttMin = 1e9;
    private int rttCount;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        tv = getPackageManager().hasSystemFeature(PackageManager.FEATURE_LEANBACK);
        if (!tv)   // телевизору — только пока на нём стол (busy): заставка не мешает ему уснуть самому
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        intent(getIntent());
        if (Build.VERSION.SDK_INT >= 28)
            getWindow().getAttributes().layoutInDisplayCutoutMode =
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;

        root = new FrameLayout(this);
        root.setBackgroundColor(Color.BLACK);
        video = new SurfaceView(this);
        video.getHolder().addCallback(this);
        root.addView(video, new FrameLayout.LayoutParams(1, 1, Gravity.TOP | Gravity.START));
        // заставка без дока — поверх кадра: с запуска и пока стол на планшет не выведен
        splash = new SplashView(this);
        splash.setFonts(font("fonts/GoogleSansFlex.ttf"), font("fonts/GoogleSans.ttf"));
        root.addView(splash, new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));
        loadSplash();
        applyLight();
        cursor = new CursorView(this);
        cursor.setVisibility(View.INVISIBLE);
        root.addView(cursor, new FrameLayout.LayoutParams(64, 64, Gravity.TOP | Gravity.START));
        status = new TextView(this);   // приставки не видно, не сопряжён — строкой внизу заставки
        status.setTextColor(0xFFA9A49A);
        status.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        status.setGravity(Gravity.CENTER);
        status.setLineSpacing(0, 1.3f);
        status.setLetterSpacing(0.08f);
        status.setShadowLayer(8, 0, 1, 0x99000000);
        FrameLayout.LayoutParams sp = new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.WRAP_CONTENT, Gravity.BOTTOM);
        sp.bottomMargin = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 28, getResources().getDisplayMetrics());
        root.addView(status, sp);
        setContentView(root);
        if (Build.VERSION.SDK_INT >= 27) {   // кнопка питания включила экран — сразу сюда, без экрана блокировки
            setShowWhenLocked(true);
            setTurnScreenOn(true);
        }

        root.addOnLayoutChangeListener((v, l, t, r, b, ol, ot, or, ob) -> {
            layoutVideo();
            if (Build.VERSION.SDK_INT >= 29) {   // жесты системы от краёв — не посреди перетаскивания у края
                int edge = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 40, getResources().getDisplayMetrics());
                root.setSystemGestureExclusionRects(Arrays.asList(new Rect(0, 0, edge, b - t),
                        new Rect(r - l - edge, 0, r - l, b - t)));
            }
        });
        // стол на планшете — касания ему (мышь стола); заставка — двойное касание просит стол сюда
        root.setOnTouchListener((v, e) -> desk && !splash.shown() ? gestures.onTouch(e) : wakeTouch(e));
        root.setOnHoverListener((v, e) -> desk && !splash.shown() && gestures.onHover(e));

        gestures = new Gestures(this, ViewConfiguration.get(this).getScaledTouchSlop());
        finger = new TestFinger(root);
        decoder = new VideoDecoder(new VideoDecoder.Callback() {
            @Override
            public void onNeedKey() {
                link.send(Link.KEYFRAME, null);
            }

            @Override
            public void onFirstFrame() {
                runOnUiThread(MainActivity.this::reveal);
            }
        });
        link = new Link(getApplicationContext(), this);
        WifiManager wm = (WifiManager) getApplicationContext().getSystemService(Context.WIFI_SERVICE);
        if (wm != null)
            wifiLock = wm.createWifiLock(Build.VERSION.SDK_INT >= 29 ? WifiManager.WIFI_MODE_FULL_LOW_LATENCY
                    : WifiManager.WIFI_MODE_FULL_HIGH_PERF, "naomi-tablet");
        if (tv) {
            session = new MediaSession(this, "NaomiPhone");
            session.setMetadata(new MediaMetadata.Builder()
                    .putString(MediaMetadata.METADATA_KEY_TITLE, "Рабочий стол НаомиФона").build());
        }
        showStatus();
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        intent(intent);
        askDesk();
    }

    // Кто открыл (ТВ): служба по слову приставки (EXTRA_WAKE = false) — стол придёт сам; кнопка пульта или плитка на
    // главном экране — «стол сюда»
    private void intent(Intent intent) {
        if (!tv)
            return;
        wakeWanted = intent.getBooleanExtra(EXTRA_WAKE, true);
        sleepOnLeave = intent.getBooleanExtra(EXTRA_ASLEEP, false);
    }

    /** ТВ: «стол сюда» — раз, как только есть связь, и только если стола на экране ещё нет. */
    private void askDesk() {
        if (!tv || !wakeWanted || linkState != Link.CONNECTED)
            return;
        wakeWanted = false;
        if (!desk) {
            Log.i(Link.TAG, "открыли с пульта — прошу стол сюда");
            link.send(Link.WAKE, null);
        }
    }

    static boolean onScreen() {
        return front != null;
    }

    /** Уйти с экрана (кнопка пульта при открытом приложении): на экране — то, что было до него. */
    static void close() {
        MainActivity a = front;
        if (a != null)
            a.runOnUiThread(a::finish);
    }

    // ТВ: стол на экране — экран не гаснет, и приложение значится играющим (MediaSession: дом — НаомиОС — не гасит
    // телевизор, на котором что-то идёт, как забытый); заставка — нет: телевизор без дела уснёт сам
    private void busy() {
        if (!tv)
            return;
        boolean on = front == this && desk && linkState == Link.CONNECTED;
        if (on)
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        else
            getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        if (session == null || session.isActive() == on)
            return;
        session.setPlaybackState(new PlaybackState.Builder()
                .setState(on ? PlaybackState.STATE_PLAYING : PlaybackState.STATE_STOPPED, 0, on ? 1f : 0f).build());
        session.setActive(on);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (tv && keyCode == KeyEvent.KEYCODE_BACK)
            sleepOnLeave = false;   // «назад» — уходит сам пользователь
        if (tv && (keyCode == KeyEvent.KEYCODE_DPAD_CENTER || keyCode == KeyEvent.KEYCODE_ENTER
                || keyCode == KeyEvent.KEYCODE_NUMPAD_ENTER)) {
            if (linkState == Link.CONNECTED && !desk && event.getRepeatCount() == 0) {   // «ОК» на заставке — стол сюда
                Log.i(Link.TAG, "«ОК» на заставке — прошу стол сюда");
                link.send(Link.WAKE, null);
            }
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public void onLeave() {
        runOnUiThread(() -> {
            if (!tv || isFinishing())
                return;
            boolean sleep = sleepOnLeave;
            Log.i(Link.TAG, "стол с телевизора сняли — ухожу с экрана" + (sleep ? "; будили его мы — спать" : ""));
            finish();
            if (sleep && !TvService.sleep())
                Log.w(Link.TAG, "усыпить телевизор некому: служба ТВ выключена");
        });
    }

    @Override
    protected void onResume() {
        super.onResume();
        VideoDecoder.trace = new java.io.File(getFilesDir(), "trace").exists();
        hideBars();
        if (wifiLock != null && !wifiLock.isHeld())
            wifiLock.acquire();
        finger.register(this);
        registerReceiver(kioskOrder, new IntentFilter(KIOSK), "android.permission.DUMP", null);
        kiosk();
        front = this;
        link.start();
    }

    @Override
    protected void onPause() {
        gestures.cancel();
        finger.unregister(this);
        unregisterReceiver(kioskOrder);
        link.stop();
        if (wifiLock != null && wifiLock.isHeld())
            wifiLock.release();
        if (front == this)
            front = null;
        if (tv)
            Log.i(Link.TAG, "приложение ушло с экрана" + (isFinishing() ? " (закрыто)" : " (поверх открылось другое)"));
        busy();
        super.onPause();
    }

    // С экрана ушёл сам пользователь («домой», другое приложение) — телевизор дальше ведёт он: усыплять его за него не
    // будем. (Не в onPause: пока телевизор просыпается, приложение уходит с экрана и возвращается само)
    @Override
    protected void onUserLeaveHint() {
        sleepOnLeave = false;
        super.onUserLeaveHint();
    }

    @Override
    protected void onDestroy() {
        if (session != null)
            session.release();
        decoder.quit();
        super.onDestroy();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus)
            hideBars();
    }

    private void hideBars() {
        if (Build.VERSION.SDK_INT >= 30) {
            WindowInsetsController c = getWindow().getInsetsController();
            if (c != null) {
                c.hide(WindowInsets.Type.systemBars());
                c.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
        }
    }

    // кадр — в середину экрана в своих пропорциях
    private void layoutVideo() {
        int w = root.getWidth(), h = root.getHeight();
        if (w <= 0 || h <= 0)
            return;
        int fw = frameW > 0 ? frameW : 16, fh = frameH > 0 ? frameH : 9;
        int vw = w, vh = Math.round((float) w * fh / fw);
        if (vh > h) {
            vh = h;
            vw = Math.round((float) h * fw / fh);
        }
        int left = (w - vw) / 2, top = (h - vh) / 2;
        videoRect[0] = left;
        videoRect[1] = top;
        videoRect[2] = vw;
        videoRect[3] = vh;
        FrameLayout.LayoutParams lp = (FrameLayout.LayoutParams) video.getLayoutParams();
        if (lp.width != vw || lp.height != vh || lp.leftMargin != left || lp.topMargin != top) {
            lp.width = vw;
            lp.height = vh;
            lp.leftMargin = left;
            lp.topMargin = top;
            video.setLayoutParams(lp);
        }
        if (frameW > 0)
            video.getHolder().setFixedSize(frameW, frameH);
        sendView();
        placeCursor();
    }

    private void sendView() {
        if (!link.connected() || videoRect[2] <= 0)
            return;
        int w = (int) videoRect[2], h = (int) videoRect[3];
        if (w == sentView[0] && h == sentView[1])
            return;
        ByteBuffer b = Link.body(4);
        b.putShort((short) w).putShort((short) h);
        link.send(Link.VIEW, b);
        sentView = new int[] {w, h};
    }

    private void placeCursor() {
        cursorPosted = false;
        boolean shown = cursorShown && desk && linkState == Link.CONNECTED && !splash.shown();
        if (shown)
            cursor.moveTo(videoRect[0] + cursorX * videoRect[2], videoRect[1] + cursorY * videoRect[3]);
        cursor.setVisibility(shown ? View.VISIBLE : View.INVISIBLE);
    }

    private void showStatus() {
        String s = null;
        if (linkState == Link.UNPAIRED)
            s = tv ? "ТЕЛЕВИЗОР НЕ СОПРЯЖЁН С ПРИСТАВКОЙ · НА MAC: TOOLS/TV PAIR"
                   : "ПЛАНШЕТ НЕ СОПРЯЖЁН С ПРИСТАВКОЙ · НА MAC: TOOLS/TABLET PAIR";
        else if (linkState != Link.CONNECTED)
            s = "ИЩУ ПРИСТАВКУ…";
        status.setText(s);
        status.setVisibility(s == null ? View.GONE : View.VISIBLE);
    }

    // ── заставка без дока (docs/SCREENS.md) ──

    private Typeface font(String asset) {
        try {
            return Typeface.createFromAsset(getAssets(), asset);
        } catch (Exception e) {
            return null;   // собрано без шрифтов — системный
        }
    }

    /** Обои и настройки заставки, что запомнены с прошлого раза: заставка — с первого кадра приложения. */
    private void loadSplash() {
        SharedPreferences p = getSharedPreferences("splash", MODE_PRIVATE);
        splash.setConfig(p.getBoolean("clock", true), p.getInt("clockX", 275), p.getInt("clockY", 378), p.getInt("width", 1600));
        wallHash = p.getString("wall", "");
        File f = new File(getFilesDir(), "wall.jpg");
        if (!wallHash.isEmpty() && !wallHash.equals("none") && f.exists()) {
            Bitmap b = BitmapFactory.decodeFile(f.getPath());
            if (b != null)
                splash.setWallpaper(b);
            else
                wallHash = "";
        }
    }

    /** Первый кадр потока на экране (или не отметился за полсекунды): стол на планшете — заставка тает. */
    private void reveal() {
        root.removeCallbacks(frameLate);
        if (desk && linkState == Link.CONNECTED && splash.shown()) {
            splash.show(false, true);
            placeCursor();
        }
    }

    /** Касания заставки: второе за 400 мс и рядом с первым — «стол сюда» (приставка скажет телефону). */
    private boolean wakeTouch(MotionEvent e) {
        if (e.getActionMasked() != MotionEvent.ACTION_DOWN)
            return true;
        long now = e.getEventTime();
        float slop = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 80, getResources().getDisplayMetrics());
        if (now - wakeDownMs < 400 && Math.hypot(e.getX() - wakeX, e.getY() - wakeY) < slop) {
            wakeDownMs = 0;
            if (linkState == Link.CONNECTED && !desk) {
                Log.i(Link.TAG, "двойное касание заставки — прошу стол сюда");
                link.send(Link.WAKE, null);
            }
        } else {
            wakeDownMs = now;
            wakeX = e.getX();
            wakeY = e.getY();
        }
        return true;
    }

    /** Приставке: какой у планшета экран и какие обои заставки уже есть (tablet::Screen). */
    private void sendScreen() {
        Point size = new Point();
        getWindowManager().getDefaultDisplay().getRealSize(size);
        if (tv)
            panel(size);
        ByteBuffer b = Link.body(20);
        b.putShort((short) Math.max(size.x, size.y)).putShort((short) Math.min(size.x, size.y));
        byte[] hash = wallHash.getBytes(StandardCharsets.UTF_8);
        b.put(hash, 0, Math.min(16, hash.length));
        b.position(20);
        link.send(Link.SCREEN, b);
    }

    /** Телевизор 4K рисует интерфейс в 1920 × 1080, а кадр стола показывает во всю панель: её размер — свойство
     *  системы sys.display-size («3840x2160»); нет его — как сказал экран. */
    private static void panel(Point size) {
        try {
            String s = (String) Class.forName("android.os.SystemProperties").getMethod("get", String.class)
                    .invoke(null, "sys.display-size");
            String[] wh = s == null ? new String[0] : s.trim().split("x");
            if (wh.length == 2) {
                int w = Integer.parseInt(wh[0]), h = Integer.parseInt(wh[1]);
                if (w >= size.x && h >= size.y && w <= 8192 && h <= 8192)
                    size.set(w, h);
            }
        } catch (Exception ignored) {
        }
    }

    @Override
    public void onSplash(String json) {
        try {
            JSONObject j = new JSONObject(json);
            boolean clock = j.optInt("clock", 1) != 0;
            int x = j.optInt("clockX", 275), y = j.optInt("clockY", 378), width = j.optInt("width", 1600);
            getSharedPreferences("splash", MODE_PRIVATE).edit().putBoolean("clock", clock).putInt("clockX", x)
                    .putInt("clockY", y).putInt("width", width).apply();
            runOnUiThread(() -> splash.setConfig(clock, x, y, width));
        } catch (Exception e) {
            Log.w(Link.TAG, "Splash: " + e);
        }
    }

    @Override
    public void onWallpaper(String hash, byte[] jpeg) {
        Bitmap b = jpeg.length > 0 ? BitmapFactory.decodeByteArray(jpeg, 0, jpeg.length) : null;
        if (jpeg.length > 0 && b == null)
            return;
        try {
            if (jpeg.length > 0)
                try (FileOutputStream out = new FileOutputStream(new File(getFilesDir(), "wall.jpg"))) {
                    out.write(jpeg);
                }
            getSharedPreferences("splash", MODE_PRIVATE).edit().putString("wall", hash).apply();
            wallHash = hash;
        } catch (Exception e) {
            Log.w(Link.TAG, "обои: " + e);
        }
        Log.i(Link.TAG, "обои заставки: " + (jpeg.length > 0 ? jpeg.length / 1024 + " КБ" : "нет (градиент)"));
        runOnUiThread(() -> splash.setWallpaper(b));
    }

    // ── яркость экрана со стола (docs/SCREENS.md, «Яркость планшета») ──
    // Ползунок в «Экранах» стола: уровень — яркость этого окна (другие программы и настройку планшета не трогает);
    // «авто» — окно яркость не задаёт, её ведёт сам планшет по датчику света (автояркость Android; приложение —
    // владелец устройства в киоске — включает её в настройках планшета само). Помнится: с запуска — как стояло.

    @Override
    public void onLight(boolean auto, int level) {
        getSharedPreferences("light", MODE_PRIVATE).edit().putBoolean("auto", auto).putInt("level", level).apply();
        Log.i(Link.TAG, "яркость со стола: " + (auto ? "сам, по датчику света" : level + " %"));
        runOnUiThread(this::applyLight);
    }

    private void applyLight() {
        SharedPreferences p = getSharedPreferences("light", MODE_PRIVATE);
        boolean auto = p.getBoolean("auto", true);
        int level = Math.max(1, Math.min(100, p.getInt("level", 60)));
        WindowManager.LayoutParams lp = getWindow().getAttributes();
        // ползунок — на глаз ровный: подсветка — квадрат его доли (половина ползунка — четверть подсветки)
        lp.screenBrightness = auto ? WindowManager.LayoutParams.BRIGHTNESS_OVERRIDE_NONE
                                   : Math.max(0.004f, (level / 100f) * (level / 100f));
        getWindow().setAttributes(lp);
        if (!auto || Build.VERSION.SDK_INT < 28)
            return;
        try {
            DevicePolicyManager dpm = (DevicePolicyManager) getSystemService(Context.DEVICE_POLICY_SERVICE);
            if (dpm != null && dpm.isDeviceOwnerApp(getPackageName()))
                dpm.setSystemSetting(new ComponentName(this, AdminReceiver.class),
                        android.provider.Settings.System.SCREEN_BRIGHTNESS_MODE, "1");
        } catch (Exception e) {
            Log.w(Link.TAG, "автояркость: " + e);
        }
    }

    // ── киоск (docs/TABLET.md, «Киоск») ──
    // Приложение — владелец устройства (tools/tablet kiosk on: команду adb запускает пользователь): закрепляет себя на
    // экране (lock task) без шторки, «домой», недавних и уведомлений, становится домашним экраном (после перезагрузки
    // планшет открывается сразу в него), экран блокировки выключен — кнопка питания гасит экран и включает сюда же.
    // Меню кнопки питания (выключить, перезагрузить) остаётся. Выйти: tools/tablet kiosk off (файл files/kiosk-off —
    // владелец остаётся, киоск не включается) или kiosk remove (приложение перестаёт быть владельцем).
    private static final String KIOSK = "com.popka.naomiphone.tablet.KIOSK";

    private void kiosk() {
        DevicePolicyManager dpm = (DevicePolicyManager) getSystemService(Context.DEVICE_POLICY_SERVICE);
        if (dpm == null || !dpm.isDeviceOwnerApp(getPackageName()))
            return;
        ComponentName admin = new ComponentName(this, AdminReceiver.class);
        boolean on = !new File(getFilesDir(), "kiosk-off").exists();
        try {
            if (on) {
                dpm.setLockTaskPackages(admin, new String[] {getPackageName()});
                if (Build.VERSION.SDK_INT >= 28)
                    dpm.setLockTaskFeatures(admin, DevicePolicyManager.LOCK_TASK_FEATURE_GLOBAL_ACTIONS);
                dpm.setKeyguardDisabled(admin, true);
                dpm.setStatusBarDisabled(admin, true);
                IntentFilter home = new IntentFilter(Intent.ACTION_MAIN);
                home.addCategory(Intent.CATEGORY_HOME);
                home.addCategory(Intent.CATEGORY_DEFAULT);
                dpm.addPersistentPreferredActivity(admin, home, new ComponentName(this, MainActivity.class));
                startLockTask();
            } else {
                stopLockTask();
                dpm.setStatusBarDisabled(admin, false);
                dpm.setKeyguardDisabled(admin, false);
                dpm.clearPackagePersistentPreferredActivities(admin, getPackageName());
            }
            Log.i(Link.TAG, "киоск: " + (on ? "включён" : "выключен (files/kiosk-off)"));
        } catch (Exception e) {
            Log.w(Link.TAG, "киоск: " + e);
        }
    }

    /** Приказ с Mac (adb, право DUMP — как у пальца для проверок): remove — перестать быть владельцем устройства. */
    private final BroadcastReceiver kioskOrder = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            DevicePolicyManager dpm = (DevicePolicyManager) getSystemService(Context.DEVICE_POLICY_SERVICE);
            if (dpm == null || !dpm.isDeviceOwnerApp(getPackageName()) || !"remove".equals(intent.getStringExtra("do")))
                return;
            try {
                ComponentName admin = new ComponentName(MainActivity.this, AdminReceiver.class);
                stopLockTask();
                dpm.setStatusBarDisabled(admin, false);
                dpm.setKeyguardDisabled(admin, false);
                dpm.clearPackagePersistentPreferredActivities(admin, getPackageName());
                dpm.clearDeviceOwnerApp(getPackageName());
                Log.i(Link.TAG, "киоск: приложение больше не владелец устройства");
            } catch (Exception e) {
                Log.w(Link.TAG, "киоск: " + e);
            }
        }
    };

    // ── связь (поток связи) ──

    @Override
    public void onState(int state, String detail) {
        runOnUiThread(() -> {
            linkState = state;
            if (state == Link.CONNECTED) {
                sentView = new int[2];
                sendScreen();
                sendView();
                askDesk();
            } else {
                desk = false;
                root.removeCallbacks(frameLate);
                splash.show(true, true);   // приставки не видно — заставка поверх застывшего кадра
            }
            showStatus();
            placeCursor();
            busy();
        });
    }

    @Override
    public void onHello(String json) {
        try {
            JSONObject j = new JSONObject(json);
            boolean d = j.optInt("desk") != 0;
            int w = j.optInt("width"), h = j.optInt("height");
            Log.i(Link.TAG, "приставка: " + json);
            if (d) {
                frameW = w;
                frameH = h;
                decoder.reset(w, h);   // здесь же, в потоке связи: следующий кадр — уже новому декодеру
            }
            desk = d;
            runOnUiThread(() -> {
                layoutVideo();
                showStatus();
                root.removeCallbacks(frameLate);
                if (!d)
                    splash.show(true, true);            // стол с планшета ушёл — заставка
                else if (splash.shown())
                    root.postDelayed(frameLate, 1500);  // тает с первым кадром (reveal); не отметился — через 1,5 с
                placeCursor();
                busy();
            });
        } catch (Exception e) {
            Log.w(Link.TAG, "Hello: " + e);
        }
    }

    @Override
    public void onVideo(byte[] data, int off, int len, long captureUs, long capturedHereUs, boolean key) {
        decoder.feed(data, off, len, captureUs, capturedHereUs, key);
    }

    @Override
    public void onCursor(boolean shown, float x, float y) {
        cursorShown = shown;
        cursorX = x;
        cursorY = y;
        synchronized (this) {
            if (cursorPosted)
                return;
            cursorPosted = true;
        }
        runOnUiThread(this::placeCursor);
    }

    @Override
    public void onCursorImage(int n, int hotX, int hotY, int[] argb) {
        Bitmap b = Bitmap.createBitmap(argb, n, n, Bitmap.Config.ARGB_8888);
        runOnUiThread(() -> {
            FrameLayout.LayoutParams lp = (FrameLayout.LayoutParams) cursor.getLayoutParams();
            if (lp.width != n) {
                lp.width = lp.height = n;
                cursor.setLayoutParams(lp);
            }
            cursor.setImage(b, hotX, hotY);
            placeCursor();
        });
    }

    @Override
    public void onRtt(double ms) {
        rttMin = Math.min(rttMin, ms);
        if (++rttCount >= 10) {   // раз в 5 с
            Log.i(Link.TAG, String.format("пинг до приставки: %.1f мс (лучший за 5 с)", rttMin));
            rttMin = 1e9;
            rttCount = 0;
        }
    }

    // ── касания → приставке ──

    @Override
    public void pointer(int buttons, float x, float y, long tUs) {
        ByteBuffer b = Link.body(17);
        b.put((byte) buttons).putFloat(x).putFloat(y).putLong(tUs);
        link.send(Link.POINTER, b);
    }

    @Override
    public void scroll(float x, float y, float dx, float dy, boolean fingers, long tUs) {
        ByteBuffer b = Link.body(25);
        b.putFloat(x).putFloat(y).putFloat(dx).putFloat(dy).put((byte) (fingers ? 1 : 0)).putLong(tUs);
        link.send(Link.SCROLL, b);
    }

    @Override
    public float[] video() {
        return videoRect;
    }

    // ── поверхность кадра ──

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        decoder.setSurface(holder.getSurface());
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        decoder.setSurface(null);
    }
}
