package com.popka.naomiphone.tablet;

import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.SystemClock;
import android.util.Log;
import android.view.Surface;

import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.util.ArrayDeque;
import java.util.HashMap;
import java.util.Map;

/**
 * Распаковка кадров стола — аппаратный декодер HEVC планшета (MediaCodec), прямо на поверхность SurfaceView.
 * Поток телефона без B-кадров: кадр уходит на экран, как только распакован (без очереди по меткам). Запускается с
 * первого опорного кадра — из него же параметры потока (VPS, SPS, PPS → csd-0). Сбился — заново с опорного.
 */
final class VideoDecoder {
    interface Callback {
        void onNeedKey();
        /** первый кадр потока (после reset) на экране — заставке пора таять; зовётся из потока декодера */
        void onFirstFrame();
    }

    private static final String MIME = "video/hevc";
    static int OPERATING_RATE = 120;
    static volatile boolean trace;   // каждый кадр — в журнал (files/trace есть — tools/tablet trace)

    private final Callback callback;
    private final HandlerThread thread = new HandlerThread("naomi-decode");
    private final Handler handler;
    private MediaCodec codec;
    private Surface surface;
    private int width, height;
    private boolean waitKey = true;
    private final ArrayDeque<byte[]> pending = new ArrayDeque<>();
    private final ArrayDeque<long[]> pendingInfo = new ArrayDeque<>();   // pts, опорный
    private final ArrayDeque<Integer> freeInputs = new ArrayDeque<>();
    private final Map<Long, Long> queuedAt = new HashMap<>();             // pts → когда отдан декодеру (мкс)
    // Замер «от кадра на телефоне до экрана» — без блокировки декодера: слушатель показа кадра MediaCodec зовёт,
    // держа свою (mListenerLock), а под ней поток MediaCodec шлёт и прочие события. Слушатель, ждавший блокировку
    // декодера, пока поток связи под ней же сидел в queueInputBuffer (тот ждёт поток MediaCodec), — взаимная
    // блокировка: картинка на планшете застыла (21 ноября 2026, открывали «Настройки»; стеки — jdb)
    private final java.util.concurrent.ConcurrentHashMap<Long, Long> captured = new java.util.concurrent.ConcurrentHashMap<>();
    private final java.util.ArrayList<Double> totals = new java.util.ArrayList<>();   // от кадра на телефоне до экрана, мс
    private final Object totalsLock = new Object();                                   // только для totals
    private final java.util.concurrent.atomic.AtomicBoolean firstWanted = new java.util.concurrent.atomic.AtomicBoolean();

    // цифры за секунду
    private int frames, keys, drops;
    private long bytes;
    private double decodeSum, decodeMax;
    private long statFrom = SystemClock.elapsedRealtime();

    VideoDecoder(Callback callback) {
        this.callback = callback;
        thread.start();
        handler = new Handler(thread.getLooper());
    }

    synchronized void setSurface(Surface s) {
        if (s == surface)
            return;
        release();
        surface = s;
        waitKey = true;
        if (s != null)
            callback.onNeedKey();
    }

    /** Новый поток (Hello): размер кадра; декодер — заново с опорного. */
    synchronized void reset(int w, int h) {
        release();
        width = w;
        height = h;
        waitKey = true;
        firstWanted.set(true);
    }

    synchronized void quit() {
        release();
        thread.quitSafely();
    }

    synchronized void feed(byte[] data, int off, int len, long pts, long capturedHereUs, boolean key) {
        bytes += len;
        if (waitKey && !key) {
            ++drops;
            return;
        }
        if (codec == null) {
            if (!key || surface == null || width <= 0 || !start(data, off, len)) {
                waitKey = true;
                return;
            }
        }
        if (key)
            ++keys;
        if (pending.size() > 30) {   // декодер встал — всё прочь, дальше с опорного
            Log.w(Link.TAG, "декодер не успевает — с опорного");
            drops += pending.size();
            pending.clear();
            pendingInfo.clear();
            waitKey = true;
            callback.onNeedKey();
            if (!key)
                return;
            waitKey = false;
        }
        waitKey = false;
        byte[] copy = new byte[len];
        System.arraycopy(data, off, copy, 0, len);
        pending.add(copy);
        pendingInfo.add(new long[] {pts, key ? 1 : 0});
        if (capturedHereUs != Long.MIN_VALUE)
            captured.put(pts, capturedHereUs);
        drain();
    }

    private boolean start(byte[] data, int off, int len) {
        try {
            MediaFormat f = MediaFormat.createVideoFormat(MIME, width, height);
            f.setByteBuffer("csd-0", ByteBuffer.wrap(parameterSets(data, off, len)));
            f.setInteger(MediaFormat.KEY_MAX_INPUT_SIZE, 4 << 20);
            // Частоты декодера: драйвер ставит их по частоте кадров, а вывести её из меток (захват на телефоне, кадры
            // неровные — стол рисует только перемены) не может («Unable to convey fps info to driver»). Скорость —
            // числом: 60 кадров, работать как для OPERATING_RATE; Short.MAX_VALUE драйвер Qualcomm отвергает
            // («Failed to set operating rate 32767 fps») — распаковка шла 56 мс на кадр в движении
            f.setInteger(MediaFormat.KEY_PRIORITY, 0);   // реального времени
            f.setInteger(MediaFormat.KEY_FRAME_RATE, 60);
            f.setInteger(MediaFormat.KEY_OPERATING_RATE, OPERATING_RATE);
            if (Build.VERSION.SDK_INT >= 30)
                f.setInteger(MediaFormat.KEY_LOW_LATENCY, 1);
            // Qualcomm: кадры — в порядке распаковки, без запаса под перестановку (B-кадров у телефона нет), и режим
            // малой задержки; Exynos — свой ключ. Незнакомые ключи декодер пропускает
            f.setInteger("vendor.qti-ext-dec-picture-order.enable", 1);
            f.setInteger("vendor.qti-ext-dec-low-latency.enable", 1);
            f.setInteger("vendor.rtc-ext-dec-low-latency.enable", 1);
            codec = MediaCodec.createDecoderByType(MIME);
            MediaCodecInfo info = codec.getCodecInfo();
            codec.setCallback(new MediaCodec.Callback() {
                @Override
                public void onInputBufferAvailable(MediaCodec c, int index) {
                    synchronized (VideoDecoder.this) {
                        if (c != codec)
                            return;
                        freeInputs.add(index);
                        drain();
                    }
                }

                @Override
                public void onOutputBufferAvailable(MediaCodec c, int index, MediaCodec.BufferInfo bi) {
                    synchronized (VideoDecoder.this) {
                        if (c != codec)
                            return;
                        Long at = queuedAt.remove(bi.presentationTimeUs);
                        if (at != null) {
                            double ms = (System.nanoTime() / 1000 - at) / 1000.0;
                            decodeSum += ms;
                            decodeMax = Math.max(decodeMax, ms);
                        }
                        ++frames;
                        try {
                            // показать с меткой «сейчас»: накопились — экран берёт самый свежий, прежние выбрасывает.
                            // releaseOutputBuffer(index, true) ставил метку кадра — часы телефона, — и кадры стояли
                            // в очереди экрана по одному на обновление: декодеру не хватало буферов, задержка росла
                            c.releaseOutputBuffer(index, System.nanoTime());
                        } catch (IllegalStateException ignored) {
                        }
                        if (trace)
                            Log.v(Link.TAG, "кадр " + bi.presentationTimeUs + " распакован за "
                                    + (at != null ? (System.nanoTime() / 1000 - at) / 1000.0 : -1) + " мс");
                        stats();
                    }
                }

                @Override
                public void onError(MediaCodec c, MediaCodec.CodecException e) {
                    Log.w(Link.TAG, "декодер: " + e);
                    synchronized (VideoDecoder.this) {
                        if (c != codec)
                            return;
                        release();
                        waitKey = true;
                    }
                    callback.onNeedKey();
                }

                @Override
                public void onOutputFormatChanged(MediaCodec c, MediaFormat format) {
                }
            }, handler);
            // Кадр на экране — метка показа (System.nanoTime): от снятия на телефоне до экрана планшета
            codec.setOnFrameRenderedListener((c, pts, nanoTime) -> {   // VideoDecoder.this здесь не брать (выше)
                if (firstWanted.compareAndSet(true, false))
                    callback.onFirstFrame();
                Long cap = captured.remove(pts);
                if (cap != null)
                    synchronized (totalsLock) {
                        totals.add((nanoTime / 1000 - cap) / 1000.0);
                    }
                if (captured.size() > 240)
                    captured.clear();
            }, handler);
            codec.configure(f, surface, null, 0);
            codec.start();
            Log.i(Link.TAG, "декодер " + info.getName() + ": " + width + "×" + height
                    + (Build.VERSION.SDK_INT >= 30 && info.getCapabilitiesForType(MIME)
                    .isFeatureSupported(MediaCodecInfo.CodecCapabilities.FEATURE_LowLatency) ? ", малая задержка" : ""));
            return true;
        } catch (Exception e) {
            Log.w(Link.TAG, "декодер не запустить: " + e);
            release();
            return false;
        }
    }

    private void drain() {
        while (codec != null && !freeInputs.isEmpty() && !pending.isEmpty()) {
            int index = freeInputs.poll();
            byte[] d = pending.poll();
            long[] info = pendingInfo.poll();
            try {
                ByteBuffer b = codec.getInputBuffer(index);
                b.clear();
                b.put(d);
                queuedAt.put(info[0], System.nanoTime() / 1000);
                if (queuedAt.size() > 120)
                    queuedAt.clear();
                codec.queueInputBuffer(index, 0, d.length, info[0], info[1] != 0 ? MediaCodec.BUFFER_FLAG_KEY_FRAME : 0);
            } catch (Exception e) {
                Log.w(Link.TAG, "в декодер: " + e);
                release();
                waitKey = true;
                callback.onNeedKey();
                return;
            }
        }
    }

    private void release() {
        if (codec != null) {
            try {
                codec.stop();
            } catch (Exception ignored) {
            }
            codec.release();
            codec = null;
        }
        pending.clear();
        pendingInfo.clear();
        freeInputs.clear();
        queuedAt.clear();
        captured.clear();
    }

    private void stats() {
        long now = SystemClock.elapsedRealtime();
        if (now - statFrom < 1000)
            return;
        double sec = (now - statFrom) / 1000.0;
        String total = "";
        synchronized (totalsLock) {
            if (!totals.isEmpty()) {
                java.util.Collections.sort(totals);
                total = String.format("; от кадра на телефоне до экрана %.1f мс (95%%: %.1f), показано %d",
                        totals.get(totals.size() / 2),
                        totals.get(Math.min(totals.size() - 1, (int) (totals.size() * 0.95))), totals.size());
                totals.clear();
            }
        }
        Log.i(Link.TAG, String.format("кадров %.0f/с (опорных %d, пропущено до опорного %d), %.1f Мбит/с, "
                        + "распаковка %.1f мс (худшая %.1f)%s", frames / sec, keys, drops, bytes * 8 / sec / 1e6,
                frames > 0 ? decodeSum / frames : 0.0, decodeMax, total));
        frames = keys = drops = 0;
        bytes = 0;
        decodeSum = decodeMax = 0;
        statFrom = now;
    }

    /** VPS, SPS и PPS из опорного кадра (Annex B) — параметры потока для декодера. */
    static byte[] parameterSets(byte[] d, int off, int len) {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        int end = off + len;
        int i = off;
        int nal = -1;
        while (i + 3 <= end) {
            boolean sc3 = d[i] == 0 && d[i + 1] == 0 && d[i + 2] == 1;
            if (sc3) {
                if (nal >= 0)
                    putNal(out, d, nal, trimZeros(d, nal, i));
                nal = i + 3;
                i += 3;
            } else {
                ++i;
            }
        }
        if (nal >= 0)
            putNal(out, d, nal, end);
        return out.toByteArray();
    }

    private static int trimZeros(byte[] d, int from, int to) {
        while (to > from && d[to - 1] == 0)
            --to;
        return to;
    }

    private static void putNal(ByteArrayOutputStream out, byte[] d, int from, int to) {
        if (to <= from)
            return;
        int type = (d[from] >> 1) & 0x3f;
        if (type == 32 || type == 33 || type == 34) {   // VPS, SPS, PPS
            out.write(0);
            out.write(0);
            out.write(0);
            out.write(1);
            out.write(d, from, to - from);
        }
    }
}
