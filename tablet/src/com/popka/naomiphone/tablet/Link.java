package com.popka.naomiphone.tablet;

import android.content.Context;
import android.os.Build;
import android.util.Log;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.DataInputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.cert.CertificateException;
import java.security.cert.X509Certificate;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509TrustManager;

/**
 * Связь с приставкой (pi/dock/tablet.h — там же протокол): TLS 1.3, сертификат приставки сверяется по отпечатку из
 * сопряжения, первое слово — ключ планшета. Сообщение: u32 длина (тип + тело), u8 тип, тело; little-endian.
 * Читает свой поток, пишет — свой (касания приходят с потока интерфейса); пинг — раз в 0,5 с. Оборвалось —
 * подключиться снова через секунду, пока приложение на экране.
 * Дежурный телевизора (TvService, docs/TV.md) — тот же Link на своём порту (agent в сопряжении): кадров ему нет, только
 * Launch — «включи экран и открой приложение».
 */
final class Link {
    static final String TAG = "NaomiTablet";

    // приставка → планшет
    static final int HELLO = 1, VIDEO = 2, PONG = 3, CURSOR = 4, CURSOR_IMAGE = 5, SPLASH = 6, WALLPAPER = 7, LIGHT = 8,
            LEAVE = 9, LAUNCH = 20;
    // планшет → приставка
    static final int AUTH = 10, PING = 11, POINTER = 12, SCROLL = 13, KEYFRAME = 14, VIEW = 15, WAKE = 16, SCREEN = 17;

    static final int UNPAIRED = 0, SEARCHING = 1, CONNECTED = 2;

    interface Listener {
        void onState(int state, String detail);
        default void onHello(String json) {
        }
        /** shownBy — когда кадр снят на телефоне, в часах планшета (System.nanoTime, мкс); Long.MIN_VALUE — не знаем */
        default void onVideo(byte[] data, int off, int len, long captureUs, long capturedHereUs, boolean key) {
        }
        default void onCursor(boolean shown, float x, float y) {
        }
        default void onCursorImage(int n, int hotX, int hotY, int[] argb) {
        }
        /** заставка без дока: JSON — часы есть, где они, ширина стола (docs/SCREENS.md) */
        default void onSplash(String json) {
        }
        /** обои заставки: хэш (16 знаков; «none» — обоев нет) и JPEG (пусто — нет) */
        default void onWallpaper(String hash, byte[] jpeg) {
        }
        /** яркость экрана со стола: auto — вести самому по датчику света, иначе level, 1…100 % */
        default void onLight(boolean auto, int level) {
        }
        default void onRtt(double ms) {
        }
        /** стол с этого экрана сняли («нет» в «Экранах», выключили, переехал): телевизор уходит с экрана */
        default void onLeave() {
        }
        /** дежурному телевизора: включи экран и открой приложение */
        default void onLaunch() {
        }
    }

    private final Context context;
    private final Listener listener;
    private final boolean agent;   // дежурный телевизора: порт agent из сопряжения
    // номер запуска: поток связи работает, пока он тот же (свернули и тут же открыли — прежний поток не мешает новому)
    private final java.util.concurrent.atomic.AtomicInteger generation = new java.util.concurrent.atomic.AtomicInteger();
    private volatile int current = -1;
    private Thread reader;
    private volatile Socket socket;
    private final LinkedBlockingQueue<byte[]> out = new LinkedBlockingQueue<>();

    Link(Context context, Listener listener) {
        this(context, listener, false);
    }

    Link(Context context, Listener listener, boolean agent) {
        this.context = context;
        this.listener = listener;
        this.agent = agent;
    }

    void start() {
        final int gen = generation.incrementAndGet();
        current = gen;
        reader = new Thread(() -> loop(gen), agent ? "naomi-agent" : "naomi-link");
        reader.start();
    }

    void stop() {
        current = -1;
        closeSocket();
        if (reader != null)
            reader.interrupt();
        reader = null;
    }

    boolean connected() {
        return socket != null;
    }

    void send(int type, ByteBuffer body) {
        if (socket == null)
            return;
        int n = body == null ? 0 : body.position();
        byte[] m = new byte[5 + n];
        ByteBuffer b = ByteBuffer.wrap(m).order(ByteOrder.LITTLE_ENDIAN);
        b.putInt(n + 1).put((byte) type);
        if (n > 0)
            b.put(body.array(), 0, n);
        out.offer(m);
    }

    static ByteBuffer body(int size) {
        return ByteBuffer.allocate(size).order(ByteOrder.LITTLE_ENDIAN);
    }

    /** Закрыть свой сокет; общий — только если он ещё этот (новый запуск уже мог поставить свой). */
    private synchronized void closeSocket(Socket own) {
        if (socket == own)
            socket = null;
        try {
            own.close();
        } catch (IOException ignored) {
        }
    }

    // Закрыть — своим потоком: stop() зовут с потока интерфейса (onPause), а закрытие TLS пишет в сеть — Android за
    // это роняет приложение (NetworkOnMainThreadException: так оно падало каждый раз, когда гас экран, — в киоске после
    // кнопки питания на экране оказывался не «НаомиФон», а рабочий стол планшета; 21 ноября 2026)
    private void closeSocket() {
        final Socket s = socket;
        socket = null;
        if (s != null)
            new Thread(() -> {
                try {
                    s.close();
                } catch (IOException ignored) {
                }
            }, "naomi-close").start();
    }

    private boolean running(int gen) {
        return current == gen;
    }

    private void loop(int gen) {
        while (running(gen)) {
            Pair pair = Pair.load(context);
            if (pair == null || (agent && pair.agent <= 0)) {
                listener.onState(UNPAIRED, null);
                sleep(agent ? 10000 : 2000);
                continue;
            }
            if (agent)
                pair.port = pair.agent;
            listener.onState(SEARCHING, null);
            SSLSocket s = connect(pair, gen);
            if (s == null) {
                sleep(1000);
                continue;
            }
            out.clear();
            pongCount = 0;
            piOffsetKnown = false;
            socket = s;
            if (!running(gen)) {
                try {
                    s.close();
                } catch (IOException ignored) {
                }
                break;
            }
            Thread writer = new Thread(() -> write(s, gen), "naomi-write");
            try {
                // первое слово — ключ и имя планшета, раньше пинга писателя
                byte[] name = (Build.MANUFACTURER + " " + Build.MODEL).getBytes(StandardCharsets.UTF_8);
                int nameLen = Math.min(64, name.length);
                byte[] auth = new byte[5 + 32 + nameLen];
                ByteBuffer.wrap(auth).order(ByteOrder.LITTLE_ENDIAN).putInt(1 + 32 + nameLen).put((byte) AUTH)
                        .put(pair.token).put(name, 0, nameLen);
                s.getOutputStream().write(auth);
                s.getOutputStream().flush();
                writer.start();
                listener.onState(CONNECTED, s.getInetAddress().getHostAddress());
                read(s, gen);
            } catch (IOException e) {
                Log.i(TAG, "связь: " + e);
            } finally {
                closeSocket(s);
                writer.interrupt();
            }
            if (running(gen)) {
                listener.onState(SEARCHING, null);
                sleep(500);
            }
        }
    }

    private SSLSocket connect(Pair pair, int gen) {
        for (String host : pair.hosts) {
            if (!running(gen))
                return null;
            Socket raw = new Socket();
            try {
                raw.setTcpNoDelay(true);
                raw.setReceiveBufferSize(4 << 20);
                raw.connect(new InetSocketAddress(host, pair.port), 1500);
                SSLContext ctx = SSLContext.getInstance("TLSv1.3");
                ctx.init(null, new TrustManager[] {new Pin(pair.fingerprint)}, null);
                SSLSocket s = (SSLSocket) ctx.getSocketFactory().createSocket(raw, host, pair.port, true);
                s.setEnabledProtocols(new String[] {"TLSv1.3"});
                s.setSoTimeout(3000);   // приставка отвечает на пинг раз в 0,5 с — тишина 3 с значит обрыв
                s.startHandshake();
                Log.i(TAG, "подключился к " + host + " (" + s.getInetAddress().getHostAddress() + ")");
                return s;
            } catch (Exception e) {
                Log.i(TAG, host + ": " + e);
                try {
                    raw.close();
                } catch (IOException ignored) {
                }
            }
        }
        return null;
    }

    private void read(SSLSocket s, int gen) throws IOException {
        DataInputStream in = new DataInputStream(new BufferedInputStream(s.getInputStream(), 1 << 16));
        while (running(gen) && socket == s) {
            int len = Integer.reverseBytes(in.readInt());
            if (len <= 0 || len > (32 << 20))
                throw new IOException("сообщение не по протоколу: " + len);
            int type = in.readUnsignedByte();
            byte[] body = new byte[len - 1];
            in.readFully(body);
            ByteBuffer b = ByteBuffer.wrap(body).order(ByteOrder.LITTLE_ENDIAN);
            switch (type) {
                case HELLO:
                    listener.onHello(new String(body, StandardCharsets.UTF_8));
                    break;
                case VIDEO:
                    if (body.length > 17) {
                        long pi = b.getLong(8);
                        listener.onVideo(body, 17, body.length - 17, b.getLong(0),
                                pi != 0 && piOffsetKnown ? pi + piOffsetUs : Long.MIN_VALUE, (body[16] & 1) != 0);
                    }
                    break;
                case PONG:
                    if (body.length >= 16)
                        pong(b.getLong(0), b.getLong(8));
                    break;
                case CURSOR:
                    if (body.length >= 9)
                        listener.onCursor(body[0] != 0, b.getFloat(1), b.getFloat(5));
                    break;
                case SPLASH:
                    listener.onSplash(new String(body, StandardCharsets.UTF_8));
                    break;
                case LIGHT:
                    if (body.length >= 2)
                        listener.onLight(body[0] != 0, body[1] & 0xff);
                    break;
                case LEAVE:
                    listener.onLeave();
                    break;
                case LAUNCH:
                    listener.onLaunch();
                    break;
                case WALLPAPER:
                    if (body.length >= 16) {
                        int end = 0;
                        while (end < 16 && body[end] != 0)
                            ++end;
                        listener.onWallpaper(new String(body, 0, end, StandardCharsets.UTF_8),
                                java.util.Arrays.copyOfRange(body, 16, body.length));
                    }
                    break;
                case CURSOR_IMAGE:
                    if (body.length >= 6) {
                        int n = b.getShort(0) & 0xffff, hx = b.getShort(2) & 0xffff, hy = b.getShort(4) & 0xffff;
                        if (n > 0 && body.length >= 6 + n * n * 4) {
                            int[] argb = new int[n * n];
                            b.position(6);
                            b.asIntBuffer().get(argb);
                            listener.onCursorImage(n, hx, hy, argb);
                        }
                    }
                    break;
                default:
                    break;
            }
        }
    }

    // Часы приставки → часы планшета (System.nanoTime, мкс): по пингу с наименьшим путём из последних 20
    private final long[] pongRtt = new long[20], pongOffset = new long[20];
    private int pongCount;
    private volatile long piOffsetUs;
    private volatile boolean piOffsetKnown;

    private void pong(long sentUs, long piUs) {
        long now = System.nanoTime() / 1000, rtt = now - sentUs;
        int i = pongCount++ % pongRtt.length;
        pongRtt[i] = rtt;
        pongOffset[i] = (sentUs + now) / 2 - piUs;
        int best = 0;
        for (int k = 1; k < Math.min(pongCount, pongRtt.length); ++k)
            if (pongRtt[k] < pongRtt[best])
                best = k;
        piOffsetUs = pongOffset[best];
        piOffsetKnown = true;
        listener.onRtt(rtt / 1000.0);
    }

    private void write(SSLSocket s, int gen) {
        try {
            OutputStream os = new BufferedOutputStream(s.getOutputStream(), 1 << 14);
            long nextPing = 0;
            while (running(gen) && socket == s) {
                long now = System.nanoTime();
                if (now >= nextPing) {
                    byte[] m = new byte[13];
                    ByteBuffer.wrap(m).order(ByteOrder.LITTLE_ENDIAN).putInt(9).put((byte) PING).putLong(now / 1000);
                    os.write(m);
                    os.flush();
                    nextPing = now + 500_000_000L;
                }
                byte[] m = out.poll(Math.max(1, (nextPing - now) / 1_000_000), TimeUnit.MILLISECONDS);
                if (m == null)
                    continue;
                os.write(m);
                while ((m = out.poll()) != null)   // накопилось — одной посылкой
                    os.write(m);
                os.flush();
            }
        } catch (IOException | InterruptedException e) {
            closeSocket(s);
        }
    }

    private void sleep(long ms) {
        try {
            Thread.sleep(ms);
        } catch (InterruptedException ignored) {
        }
    }

    /** Сертификат приставки — только тот, чей отпечаток SHA-256 записан при сопряжении. */
    private static final class Pin implements X509TrustManager {
        private final byte[] fingerprint;

        Pin(byte[] fingerprint) {
            this.fingerprint = fingerprint;
        }

        @Override
        public void checkServerTrusted(X509Certificate[] chain, String authType) throws CertificateException {
            try {
                byte[] d = MessageDigest.getInstance("SHA-256").digest(chain[0].getEncoded());
                if (!MessageDigest.isEqual(d, fingerprint))
                    throw new CertificateException("не тот сертификат — не наша приставка");
            } catch (java.security.NoSuchAlgorithmException e) {
                throw new CertificateException(e);
            }
        }

        @Override
        public void checkClientTrusted(X509Certificate[] chain, String authType) throws CertificateException {
            throw new CertificateException("не сервер");
        }

        @Override
        public X509Certificate[] getAcceptedIssuers() {
            return new X509Certificate[0];
        }
    }
}
