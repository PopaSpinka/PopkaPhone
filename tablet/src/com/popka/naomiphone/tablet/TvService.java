package com.popka.naomiphone.tablet;

import android.accessibilityservice.AccessibilityService;
import android.content.Intent;
import android.os.Build;
import android.os.PowerManager;
import android.util.Log;
import android.view.KeyEvent;
import android.view.accessibility.AccessibilityEvent;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;

/**
 * Служба телевизора (docs/TV.md) — «дежурный»: работает, пока телевизор не спит, и когда приложения на экране нет.
 * Система держит её сама (служба специальных возможностей; включает пользователь — tools/tv access on), ей же
 * разрешено открывать приложение не с экрана — поверх HDMI-входа и других программ. Делает три вещи:
 *  · кнопка пульта (files/tv.conf: key — код клавиши Android, scan — код самой кнопки; tools/tv button) открывает
 *    «НаомиФон» со словом «стол сюда»; нажали при открытом — закрывает (на экране то, что было до него);
 *  · держит связь с приставкой (Link на порту дежурного): в «Экранах» стола телевизор поставили на «стол» — приставка
 *    говорит Launch: включить экран (телевизор спал — его сеть приставка подняла Wake-on-LAN) и открыть приложение;
 *  · гасит телевизор, который сама будила, когда стол с него сняли (MainActivity.onLeave).
 * Из экрана служба видит только нажатия кнопок; события окон и содержимое не запрашивает (res/xml/tv_service.xml).
 */
public final class TvService extends AccessibilityService implements Link.Listener {
    private static volatile TvService instance;

    private Link agent;
    private PowerManager power;
    private int linkState = -1;
    private long confTime = -1;
    private int buttonKey = -1, buttonScan = -1;

    @Override
    protected void onServiceConnected() {
        instance = this;
        power = (PowerManager) getSystemService(POWER_SERVICE);
        agent = new Link(getApplicationContext(), this, true);
        agent.start();
        Log.i(Link.TAG, "служба ТВ: включена");
    }

    @Override
    public boolean onUnbind(Intent intent) {
        stop();
        return super.onUnbind(intent);
    }

    @Override
    public void onDestroy() {
        stop();
        super.onDestroy();
    }

    private void stop() {
        if (instance == this)
            instance = null;
        if (agent != null) {
            agent.stop();
            agent = null;
            Log.i(Link.TAG, "служба ТВ: выключена");
        }
    }

    @Override
    public void onAccessibilityEvent(AccessibilityEvent event) {
    }

    @Override
    public void onInterrupt() {
    }

    // ── кнопка пульта ──

    @Override
    protected boolean onKeyEvent(KeyEvent e) {
        if (new File(getFilesDir(), "keys").exists())   // tools/tv keys: какая кнопка что шлёт
            Log.i(Link.TAG, "пульт: " + (e.getAction() == KeyEvent.ACTION_DOWN ? "нажата" : "отпущена") + " key="
                    + e.getKeyCode() + " (" + KeyEvent.keyCodeToString(e.getKeyCode()) + ") scan=" + e.getScanCode());
        loadButton();
        if (buttonKey < 0 && buttonScan < 0)
            return false;
        if ((buttonKey >= 0 && e.getKeyCode() != buttonKey) || (buttonScan >= 0 && e.getScanCode() != buttonScan))
            return false;
        Log.i(Link.TAG, "кнопка пульта: " + (e.getAction() == KeyEvent.ACTION_DOWN ? "нажата" : "отпущена")
                + (e.getRepeatCount() > 0 ? " (повтор " + e.getRepeatCount() + ")" : "") + ", приложение "
                + (MainActivity.onScreen() ? "на экране" : "не на экране"));
        if (e.getAction() == KeyEvent.ACTION_DOWN && e.getRepeatCount() == 0) {
            if (MainActivity.onScreen()) {
                Log.i(Link.TAG, "кнопка пульта: приложение на экране — закрываю");
                MainActivity.close();
            } else {
                Log.i(Link.TAG, "кнопка пульта: открываю приложение");
                open(true);
            }
        }
        return true;   // кнопка — наша: то, что на ней было (приложение производителя), не открывается
    }

    /** files/tv.conf — какая кнопка наша; файл сменился (tools/tv button) — читаем заново. */
    private void loadButton() {
        File f = new File(getFilesDir(), "tv.conf");
        long t = f.exists() ? f.lastModified() : 0;
        if (t == confTime)
            return;
        confTime = t;
        buttonKey = buttonScan = -1;
        if (t == 0)
            return;
        try (BufferedReader r = new BufferedReader(new FileReader(f))) {
            for (String line; (line = r.readLine()) != null; ) {
                int eq = line.indexOf('=');
                if (eq < 0)
                    continue;
                String k = line.substring(0, eq).trim(), v = line.substring(eq + 1).trim();
                if (k.equals("key"))
                    buttonKey = Integer.parseInt(v);
                else if (k.equals("scan"))
                    buttonScan = Integer.parseInt(v);
            }
        } catch (Exception e) {
            buttonKey = buttonScan = -1;
        }
        Log.i(Link.TAG, "кнопка пульта: " + (buttonKey < 0 && buttonScan < 0 ? "не выбрана" : "key=" + buttonKey
                + " scan=" + buttonScan));
    }

    // ── открыть приложение: экран включить (спал — будим), приложение — поверх того, что на экране ──

    private void open(boolean byRemote) {
        boolean asleep = !power.isInteractive();
        if (asleep) {
            // экран включает сама активность (setTurnScreenOn), это — чтобы телевизор не уснул обратно, пока она
            // открывается: после Wake-on-LAN он не спит всего несколько секунд
            PowerManager.WakeLock w = power.newWakeLock(PowerManager.SCREEN_BRIGHT_WAKE_LOCK
                    | PowerManager.ACQUIRE_CAUSES_WAKEUP, "naomi:tv-wake");
            w.acquire(10000);
        }
        startActivity(new Intent(this, MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                .putExtra(MainActivity.EXTRA_WAKE, byRemote).putExtra(MainActivity.EXTRA_ASLEEP, asleep));
    }

    /** Телевизор — спать (будили его мы, а стол с него сняли). Служба выключена — не умеем. */
    static boolean sleep() {
        TvService s = instance;
        return s != null && Build.VERSION.SDK_INT >= 28 && s.performGlobalAction(GLOBAL_ACTION_LOCK_SCREEN);
    }

    static boolean running() {
        return instance != null;
    }

    // ── связь с приставкой (поток связи) ──

    @Override
    public void onState(int state, String detail) {
        if (state == linkState)
            return;
        linkState = state;
        Log.i(Link.TAG, "служба ТВ: " + (state == Link.CONNECTED ? "приставка на связи (" + detail + ")"
                : state == Link.UNPAIRED ? "не сопряжена (tools/tv pair)" : "ищу приставку"));
    }

    @Override
    public void onLaunch() {
        if (MainActivity.onScreen())
            return;
        Log.i(Link.TAG, "служба ТВ: приставка просит открыть приложение" + (power.isInteractive() ? "" : " — телевизор спал"));
        open(false);
    }
}
