#include <QtTest>

#include "engine/EffectProcessor.h"

class EngineTest : public QObject
{
    Q_OBJECT

private slots:
    void effectProcessorPassthroughWithoutEffects();
    void effectProcessorBrightness();
};

void EngineTest::effectProcessorPassthroughWithoutEffects()
{
    QImage image(64, 64, QImage::Format_RGBA8888);
    image.fill(Qt::red);
    const QImage out = EffectProcessor::applyEffects(image, {});
    QCOMPARE(out.size(), image.size());
}

void EngineTest::effectProcessorBrightness()
{
    QImage image(64, 64, QImage::Format_RGBA8888);
    image.fill(QColor(100, 100, 100));

    drift::Effect effect;
    effect.name = QStringLiteral("eq");
    effect.parameters.insert(QStringLiteral("brightness"), 0.2);

    const QImage out = EffectProcessor::applyEffects(image, {effect});
    QVERIFY(!out.isNull());
    const QRgb pixel = out.pixel(32, 32);
    QVERIFY(qRed(pixel) > 100 || qGreen(pixel) > 100 || qBlue(pixel) > 100);
}

QTEST_MAIN(EngineTest)
#include "tst_engine.moc"
