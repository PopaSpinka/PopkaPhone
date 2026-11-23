#include "shell_compositor.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>
#include <QWaylandSeat>
#include <QWaylandSurface>

#include <QtWaylandCompositor/private/qwaylandseat_p.h>
#include <QtWaylandCompositor/private/qwldatadevice_p.h>

ShellCompositor::ShellCompositor(QObject *parent)
    : QWaylandQuickCompositor(parent)
{
    setRetainedSelectionEnabled(true);
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &ShellCompositor::shellClipboardChanged);
}

void ShellCompositor::create()
{
    QWaylandQuickCompositor::create();
    if (QWaylandSeat *seat = defaultSeat())
        connect(seat, &QWaylandSeat::keyboardFocusChanged, this, [this] {
            if (m_shellOwns)
                offerShellClipboard();
        });
}

// программа положила в буфер (Qt уже вычитал все виды данных) — в буфер оболочки
void ShellCompositor::retainedSelectionReceived(QMimeData *mime)
{
    if (m_offering || !mime)
        return;
    m_shellOwns = false;
    auto *copy = new QMimeData;
    for (const QString &format : mime->formats())
        copy->setData(format, mime->data(format));
    m_fromClient = true;
    QGuiApplication::clipboard()->setMimeData(copy);   // dataChanged — сразу, внутри
    m_fromClient = false;
}

// оболочка положила в буфер — программам
void ShellCompositor::shellClipboardChanged()
{
    if (m_fromClient || !isCreated())
        return;
    m_shellOwns = true;
    offerShellClipboard();
}

void ShellCompositor::offerShellClipboard()
{
    const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
    // overrideSelection сразу шлёт своё окну в фокусе и падает, если у его программы нет wl_data_device: тогда —
    // при следующей смене фокуса
    if (!mime || mime->formats().isEmpty() || !focusHasDataDevice())
        return;
    QMimeData offer;
    for (const QString &format : mime->formats())
        offer.setData(format, mime->data(format));
    // текст — и с кодировкой: её ждут программы не на Qt (GTK, SDL); Qt понимает оба вида
    const QString utf8 = QStringLiteral("text/plain;charset=utf-8");
    if (offer.hasFormat(QStringLiteral("text/plain")) && !offer.hasFormat(utf8))
        offer.setData(utf8, mime->data(QStringLiteral("text/plain")));
    m_offering = true;
    overrideSelection(&offer);
    m_offering = false;
}

bool ShellCompositor::focusHasDataDevice() const
{
    QWaylandSeat *seat = defaultSeat();
    const QWaylandSurface *focus = seat ? seat->keyboardFocus() : nullptr;
    if (!focus)
        return true;
    QtWayland::DataDevice *device = QWaylandSeatPrivate::get(seat)->dataDevice();
    return device && device->resourceMap().value(focus->waylandClient());
}
