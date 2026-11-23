package com.popka.naomiphone.tablet;

import android.app.admin.DeviceAdminReceiver;

/**
 * Владелец устройства для киоска (docs/TABLET.md, «Киоск»): сам ничего не делает — он нужен, чтобы приложению можно
 * было закрепить себя на экране без вопросов и убрать шторку, «домой» и жесты системы (MainActivity.kiosk).
 * Владельцем приложение назначает пользователь одной командой adb (tools/tablet kiosk on).
 */
public final class AdminReceiver extends DeviceAdminReceiver {
}
