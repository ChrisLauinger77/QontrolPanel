#include "nativeimage.h"
#include <QBuffer>
#include <cstring>

namespace NativeImage
{
    QString imageToDataUri(const QImage& image)
    {
        if (image.isNull())
        {
            return {};
        }

        QByteArray imageData;
        QBuffer buffer(&imageData);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        return QStringLiteral("data:image/png;base64,") + imageData.toBase64();
    }

    QImage bitmapToImage(HBITMAP bitmap)
    {
        BITMAP bitmapInfo{};
        if (!bitmap || GetObject(bitmap, sizeof(bitmapInfo), &bitmapInfo) == 0 || bitmapInfo.bmWidth <= 0 ||
            bitmapInfo.bmHeight <= 0 || bitmapInfo.bmWidth > 4096 || bitmapInfo.bmHeight > 4096)
        {
            return {};
        }

        BITMAPINFO dibInfo{};
        dibInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        dibInfo.bmiHeader.biWidth = bitmapInfo.bmWidth;
        dibInfo.bmiHeader.biHeight = -bitmapInfo.bmHeight;
        dibInfo.bmiHeader.biPlanes = 1;
        dibInfo.bmiHeader.biBitCount = 32;
        dibInfo.bmiHeader.biCompression = BI_RGB;

        QImage image(bitmapInfo.bmWidth, bitmapInfo.bmHeight, QImage::Format_ARGB32);
        if (image.isNull())
            return {};
        HDC deviceContext = GetDC(nullptr);
        if (!deviceContext)
            return {};
        const int copiedLines =
            GetDIBits(deviceContext, bitmap, 0, bitmapInfo.bmHeight, image.bits(), &dibInfo, DIB_RGB_COLORS);
        ReleaseDC(nullptr, deviceContext);

        return copiedLines == bitmapInfo.bmHeight ? image : QImage{};
    }

    QImage iconToImage(HICON icon, int size)
    {
        if (!icon || size <= 0 || size > 4096)
        {
            return {};
        }

        BITMAPINFO bitmapInfo{};
        bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmapInfo.bmiHeader.biWidth = size;
        bitmapInfo.bmiHeader.biHeight = -size;
        bitmapInfo.bmiHeader.biPlanes = 1;
        bitmapInfo.bmiHeader.biBitCount = 32;
        bitmapInfo.bmiHeader.biCompression = BI_RGB;

        void* pixels = nullptr;
        HDC screenContext = GetDC(nullptr);
        HBITMAP bitmap = CreateDIBSection(screenContext, &bitmapInfo, DIB_RGB_COLORS, &pixels, nullptr, 0);
        HDC drawContext = CreateCompatibleDC(screenContext);
        if (!screenContext || !bitmap || !pixels || !drawContext)
        {
            if (drawContext)
            {
                DeleteDC(drawContext);
            }
            if (bitmap)
            {
                DeleteObject(bitmap);
            }
            if (screenContext)
            {
                ReleaseDC(nullptr, screenContext);
            }
            return {};
        }

        HGDIOBJ oldBitmap = SelectObject(drawContext, bitmap);
        memset(pixels, 0, static_cast<size_t>(size * size * 4));
        const bool drawn = oldBitmap && oldBitmap != HGDI_ERROR &&
                           DrawIconEx(drawContext, 0, 0, icon, size, size, 0, nullptr, DI_NORMAL);

        QImage image(static_cast<uchar*>(pixels), size, size, QImage::Format_ARGB32);
        QImage result = drawn ? image.copy() : QImage{};

        SelectObject(drawContext, oldBitmap);
        DeleteDC(drawContext);
        DeleteObject(bitmap);
        ReleaseDC(nullptr, screenContext);
        return result;
    }

} // namespace NativeImage
