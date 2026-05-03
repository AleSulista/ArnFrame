#include "PreviewItem.h"

#include <QQuickWindow>
#include <QSGSimpleTextureNode>

PreviewItem::PreviewItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

void PreviewItem::setFrame(const QImage &frame)
{
    if (m_frame.cacheKey() == frame.cacheKey())
        return;

    m_frame = frame;
    emit frameChanged();
    update();
}

QSGNode *PreviewItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    auto *node = static_cast<QSGSimpleTextureNode *>(oldNode);
    if (!node)
        node = new QSGSimpleTextureNode();

    if (m_frame.isNull() || !window()) {
        node->setTexture(nullptr);
        node->setRect(boundingRect());
        return node;
    }

    QSGTexture *texture = window()->createTextureFromImage(m_frame, QQuickWindow::TextureCanUseAtlas);
    node->setTexture(texture);
    node->setOwnsTexture(true);
    node->setFiltering(QSGTexture::Linear);

    const QSize frameSize = m_frame.size();
    const QRectF bounds = boundingRect();
    const qreal scale = qMin(bounds.width() / frameSize.width(), bounds.height() / frameSize.height());
    const qreal drawW = frameSize.width() * scale;
    const qreal drawH = frameSize.height() * scale;
    const qreal x = bounds.x() + (bounds.width() - drawW) / 2.0;
    const qreal y = bounds.y() + (bounds.height() - drawH) / 2.0;
    node->setRect(QRectF(x, y, drawW, drawH));

    return node;
}
