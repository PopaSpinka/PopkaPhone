package com.popka.naomiphone.tablet;

import android.content.Context;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/**
 * Сопряжение с приставкой — файл files/pair.conf в личной папке приложения (кладёт tools/tablet pair с Mac через
 * run-as; на телевизоре — tools/tv pair): адреса приставки, порт, отпечаток её сертификата (SHA-256) и ключ планшета;
 * у телевизора ещё agent — порт дежурного (TvService). Читается при каждом подключении: сопрягли заново — следующая
 * попытка уже с новым.
 */
final class Pair {
    final List<String> hosts = new ArrayList<>();
    int port = 8490;
    int agent;            // порт дежурного телевизора; 0 — нет (планшет)
    byte[] fingerprint;   // 32 байта
    byte[] token;         // 32 байта

    static Pair load(Context context) {
        File f = new File(context.getFilesDir(), "pair.conf");
        Pair p = new Pair();
        try (BufferedReader r = new BufferedReader(new InputStreamReader(new FileInputStream(f), StandardCharsets.UTF_8))) {
            for (String line; (line = r.readLine()) != null; ) {
                int eq = line.indexOf('=');
                if (eq < 0)
                    continue;
                String k = line.substring(0, eq).trim(), v = line.substring(eq + 1).trim();
                switch (k) {
                    case "hosts":
                        for (String h : v.split(","))
                            if (!h.trim().isEmpty())
                                p.hosts.add(h.trim());
                        break;
                    case "port":
                        p.port = Integer.parseInt(v);
                        break;
                    case "agent":
                        p.agent = Integer.parseInt(v);
                        break;
                    case "fingerprint":
                        p.fingerprint = hex(v.replace(":", ""));
                        break;
                    case "token":
                        p.token = hex(v);
                        break;
                    default:
                        break;
                }
            }
        } catch (Exception e) {
            return null;
        }
        if (p.hosts.isEmpty() || p.fingerprint == null || p.fingerprint.length != 32 || p.token == null
                || p.token.length != 32)
            return null;
        return p;
    }

    private static byte[] hex(String s) {
        if (s.length() % 2 != 0)
            return null;
        byte[] b = new byte[s.length() / 2];
        for (int i = 0; i < b.length; ++i)
            b[i] = (byte) Integer.parseInt(s.substring(2 * i, 2 * i + 2), 16);
        return b;
    }
}
