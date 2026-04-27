#pragma once

#include <QImage>
#include <QQuickItem>

class PreviewItem : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(QImage frame READ frame WRITE setFrame NOTIFY frameChanged)

public:
    explicit PreviewItem(QQuickItem *parent = nullptr);

    QImage frame() const { return m_frame; }
    void setFrame(const QImage &frame);

signals:
    void frameChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;

private:
    QImage m_frame;
};
