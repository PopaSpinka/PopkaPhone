// НаомиФон — корень: Wayland-композитор. Программы — отдельные процессы (клиенты Wayland на сокете
// naomiWaylandSocket); их окна оболочка (ShellView.qml) показывает в своём единственном окне на экране.
// Ввод текста в программы — протоколы text-input: клавиатура оболочки (Keyboard.qml) печатает в окно
// программы так же, как в свои поля.
import QtQuick
import QtQuick.Window
import QtWayland.Compositor
import QtWayland.Compositor.XdgShell
import Naomi.Ui
import Naomi.Shell

// ShellCompositor — WaylandCompositor Qt с общим буфером обмена оболочки и программ (shell_compositor.h)
ShellCompositor {
    id: compositor
    socketName: naomiWaylandSocket

    WaylandOutput {
        sizeFollowsWindow: true
        window: Window {
            visible: true
            color: Theme.canvas
            ShellView { id: shell; anchors.fill: parent }
        }
    }

    XdgShell {
        // окно Android (docs/ANDROID.md) — столу; окно программы стола (браузер) — столу, остальные — экрану телефона
        onToplevelCreated: (toplevel, xdgSurface) => {
            const pid = xdgSurface.surface.client.processId
            if (naomiAndroid.isAndroid(pid))
                naomiAndroid.window(toplevel, xdgSurface)
            else if (naomiApps.onDesk(pid))
                naomiApps.deskWindow(toplevel, xdgSurface)
            else
                shell.addWindow(toplevel, xdgSurface)
        }
    }
    TextInputManager {}
    TextInputManagerV3 {}
    QtTextInputMethodManager {}
}
