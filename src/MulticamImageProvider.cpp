#include "MulticamImageProvider.h"

#include "MulticamImageStore.h"

MulticamImageProvider::MulticamImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage MulticamImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    // "<angleIndex>"; the ?rev= query QML appends is stripped before we are called, and exists
    // only to defeat QML's URL-keyed image cache.
    bool ok = false;
    const int angle = id.section(QLatin1Char('/'), 0, 0).toInt(&ok);

    const QImage image = ok ? MulticamImageStore::tile(angle) : QImage();
    if (image.isNull()) {
        if (size)
            *size = QSize();
        return {};
    }

    if (requestedSize.isValid() && requestedSize.width() > 0 && requestedSize.height() > 0) {
        const QImage scaled = image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        if (size)
            *size = scaled.size();
        return scaled;
    }

    if (size)
        *size = image.size();
    return image;
}
