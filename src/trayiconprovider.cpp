#include "trayiconprovider.h"

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPen>

namespace {
QString normalizeIconId(QString id)
{
    if (id.startsWith('/')) {
        id.remove(0, 1);
    }

    return id;
}

QRect badgeRectFor(const QSize& size)
{
    const int minSide = qMin(size.width(), size.height());
    const int badgeDiameter = qMax(8, qRound(minSide * 0.5));
    const int margin = qMax(0, qRound(minSide * 0.03));

    return QRect(size.width() - badgeDiameter - margin,
                 size.height() - badgeDiameter - margin,
                 badgeDiameter,
                 badgeDiameter);
}
}

TrayIconProvider::TrayIconProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage TrayIconProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize)
{
    const QString iconId = normalizeIconId(id);
    const QImage baseImage(QString(":/icons/%1").arg(iconId));

    if (baseImage.isNull()) {
        if (size) {
            *size = QSize();
        }

        return QImage();
    }

    QImage result = requestedSize.isValid()
        ? baseImage.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)
        : baseImage;

    if (size) {
        *size = result.size();
    }

    const QRect badgeRect = badgeRectFor(result.size());
    const int borderWidth = qMax(1, qRound(qMin(result.width(), result.height()) * 0.08));
    const int innerIconSize = qMax(5, qRound(badgeRect.width() * 0.58));
    const QRect innerRect(badgeRect.center().x() - (innerIconSize / 2),
                          badgeRect.center().y() - (innerIconSize / 2),
                          innerIconSize,
                          innerIconSize);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setPen(QPen(QColor(0, 0, 0, 150), borderWidth));
    painter.setBrush(QColor(255, 255, 255, 245));
    painter.drawEllipse(badgeRect.adjusted(0, 0, -1, -1));

    const QImage headsetImage(QStringLiteral(":/icons/devices/headset.png"));
    if (!headsetImage.isNull()) {
        painter.drawImage(innerRect, headsetImage);
    }

    return result;
}