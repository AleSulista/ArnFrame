#include "PreviewItem.h"

#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QSGTexture>

PreviewItem::PreviewItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

void PreviewItem::setTextureId(int id)
{
    if (m_textureId == id)
        return;
    m_textureId = id;
    emit frameChanged();
    update();
}

void PreviewItem::setTextureSize(const QSize &size)
{
    if (m_textureSize == size)
        return;
    m_textureSize = size;
    emit frameChanged();
    update();
}

QSGNode *PreviewItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    auto *node = static_cast<QSGSimpleTextureNode *>(oldNode);

    // Qt 6's QSGSimpleTextureNode::setTexture(nullptr) segfaults. When there is
    // nothing to show, drop the node entirely instead of clearing the texture.
    if (m_textureId == 0 || m_textureSize.isEmpty() || !window()) {
        delete node;
        m_boundTextureId = 0;
        m_boundTextureSize = {};
        return nullptr;
    }

    if (!node)
        node = new QSGSimpleTextureNode();

    const bool needNewWrapper =
        !node->texture() || m_boundTextureId != m_textureId || m_boundTextureSize != m_textureSize;
    if (needNewWrapper) {
        // Wraps the compositor's framebuffer texture — no copy, no upload. The GL
        // object stays owned by the engine's presentation ring, so the scene graph
        // must not take ownership of it; setOwnsTexture only frees this wrapper.
        QSGTexture *texture = QNativeInterface::QSGOpenGLTexture::fromNative(
            static_cast<GLuint>(m_textureId), window(), m_textureSize);
        if (!texture) {
            delete node;
            m_boundTextureId = 0;
            m_boundTextureSize = {};
            return nullptr;
        }
        node->setTexture(texture);
        node->setOwnsTexture(true);
        node->setFiltering(QSGTexture::Linear);
        m_boundTextureId = m_textureId;
        m_boundTextureSize = m_textureSize;
    }

    // No flip: the compositor promotes every source into its framebuffer such
    // that row 0 holds the image's top row (which is why toImage(false) comes out
    // upright), and the scene graph likewise samples v=0 at the top.

    const QRectF bounds = boundingRect();
    const qreal scale = qMin(bounds.width() / m_textureSize.width(),
                             bounds.height() / m_textureSize.height());
    const qreal drawW = m_textureSize.width() * scale;
    const qreal drawH = m_textureSize.height() * scale;
    const qreal x = bounds.x() + (bounds.width() - drawW) / 2.0;
    const qreal y = bounds.y() + (bounds.height() - drawH) / 2.0;
    node->setRect(QRectF(x, y, drawW, drawH));

    return node;
}
