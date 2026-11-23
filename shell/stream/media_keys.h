// НаомиФон — клавиши плеера с клавиатуры (пуск / пауза, «назад», «вперёд»; docs/DESKTOP.md, «Клавиши плеера») — тому,
// кто играет: плееры объявляют себя на сессионной шине по MPRIS (org.mpris.MediaPlayer2.*; «Браузер» — сам, это
// Chromium: YouTube и любой сайт с MediaSession). Пуск / пауза: кто-то играет — пауза всем играющим, никто — пуск
// тому, кого останавливали последним (иначе первому). «Назад» / «вперёд» — играющему, а нет его — тому же последнему.
#pragma once

#include <QObject>
#include <QStringList>

class MediaKeys : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    void press(int qtKey);   // Qt::Key_MediaTogglePlayPause, Key_MediaNext, Key_MediaPrevious

private:
    QString m_last;   // кого останавливали или листали последним
};
