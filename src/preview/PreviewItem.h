#pragma once

#include <QQuickItem>
#include <QSize>

// Displays the compositor's output. The frame is already a GL texture in the
// shared context, so this wraps it directly instead of uploading a QImage on
// every frame.
class PreviewItem : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(int textureId READ textureId WRITE setTextureId NOTIFY frameChanged)
    Q_PROPERTY(QSize textureSize READ textureSize WRITE setTextureSize NOTIFY frameChanged)

public:
    explicit PreviewItem(QQuickItem *parent = nullptr);

    int textureId() const { return m_textureId; }
    void setTextureId(int id);

    QSize textureSize() const { return m_textureSize; }
    void setTextureSize(const QSize &size);

signals:
    void frameChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;

private:
    int m_textureId = 0;
    QSize m_textureSize;
};
