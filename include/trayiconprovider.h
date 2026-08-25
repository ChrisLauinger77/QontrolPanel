#pragma once

#include <QQuickImageProvider>

class TrayIconProvider : public QQuickImageProvider
{
public:
    TrayIconProvider();

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;
};