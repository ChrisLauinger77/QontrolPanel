#pragma once
#include <QImage>
#include <QString>
#include <windows.h>
namespace NativeImage
{
    QString imageToDataUri(const QImage& image);
    QImage bitmapToImage(HBITMAP bitmap);
    QImage iconToImage(HICON icon, int size);
} // namespace NativeImage
