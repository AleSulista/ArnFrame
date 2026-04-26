#include "DriftImageProvider.h"

#include <QFileInfo>
#include <QUrl>

DriftImageProvider::DriftImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage DriftImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QString path = QUrl::fromPercentEncoding(id.toUtf8());
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        if (size)
            *size = QSize();
        return {};
    }

    QImage image(path);
    if (image.isNull()) {
        if (size)
            *size = QSize();
        return {};
    }

    if (requestedSize.isValid() && requestedSize.width() > 0 && requestedSize.height() > 0) {
        image = image.scaled(requestedSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    }

    if (size)
        *size = image.size();
    return image;
}
