/*
 * Fujisan Test Suite - CRT Effect Renderer Tests
 *
 * Checks the visible properties of the software CRT effect: output geometry,
 * scanline modulation, aperture grille, tube curvature and glow.
 */

#include "crtrenderer.h"
#include <QElapsedTimer>
#include <QImage>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest/QtTest>

namespace {

QImage solidFrame(QRgb color)
{
    QImage image(384, 240, QImage::Format_RGB32);
    image.fill(color);
    return image;
}

int brightness(QRgb pixel)
{
    return qRed(pixel) + qGreen(pixel) + qBlue(pixel);
}

CrtSettings onlyEffect(int scanlines, int mask, int curvature, int glow)
{
    CrtSettings settings;
    settings.enabled = true;
    settings.scanlines = scanlines;
    settings.mask = mask;
    settings.curvature = curvature;
    settings.glow = glow;
    return settings;
}

}  // namespace

class TestCrtRenderer : public QObject {
    Q_OBJECT

private slots:
    void outputHasRequestedSize()
    {
        CrtRenderer renderer;
        renderer.setSettings(onlyEffect(60, 30, 25, 30));
        const QImage output = renderer.render(solidFrame(qRgb(128, 128, 128)), QSize(1000, 700));
        QCOMPARE(output.size(), QSize(1000, 700));
        QCOMPARE(output.format(), QImage::Format_RGB32);
    }

    void emptyInputGivesNullImage()
    {
        CrtRenderer renderer;
        QVERIFY(renderer.render(QImage(), QSize(100, 100)).isNull());
        QVERIFY(renderer.render(solidFrame(qRgb(1, 2, 3)), QSize()).isNull());
    }

    void settingsAreClamped()
    {
        CrtRenderer renderer;
        renderer.setSettings(onlyEffect(150, -10, 101, 200));
        QCOMPARE(renderer.settings().scanlines, 100);
        QCOMPARE(renderer.settings().mask, 0);
        QCOMPARE(renderer.settings().curvature, 100);
        QCOMPARE(renderer.settings().glow, 100);
    }

    void settingsPersistThroughQSettings()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("crt.ini");

        CrtSettings saved = onlyEffect(80, 15, 0, 45);
        {
            QSettings settings(path, QSettings::IniFormat);
            saved.save(settings);
        }
        QSettings settings(path, QSettings::IniFormat);
        QCOMPARE(settings.value("video/crtEnabled").toBool(), true);
        QVERIFY(CrtSettings::load(settings) == saved);
    }

    void missingOrInvalidSettingsUseSafeValues()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QSettings settings(dir.filePath("crt.ini"), QSettings::IniFormat);
        QVERIFY(CrtSettings::load(settings) == CrtSettings());
        QCOMPARE(CrtSettings::load(settings).enabled, false);

        settings.setValue("video/crtScanlines", 900);
        settings.setValue("video/crtGlow", -4);
        QCOMPARE(CrtSettings::load(settings).scanlines, 100);
        QCOMPARE(CrtSettings::load(settings).glow, 0);
    }

    void allEffectsOffIsNeutral()
    {
        CrtRenderer renderer;
        renderer.setSettings(onlyEffect(0, 0, 0, 0));
        const QImage output = renderer.render(solidFrame(qRgb(100, 150, 200)), QSize(1152, 720));
        for (int y = 0; y < output.height(); y += 37) {
            for (int x = 0; x < output.width(); x += 41) {
                QCOMPARE(output.pixel(x, y), qRgb(100, 150, 200));
            }
        }
    }

    void scanlinesDarkenGapsBetweenLines()
    {
        CrtRenderer renderer;
        renderer.setSettings(onlyEffect(100, 0, 0, 0));
        // 3x vertical scale: output rows 0 and 2 straddle a gap, row 1 is a line center.
        const QImage output = renderer.render(solidFrame(qRgb(200, 200, 200)), QSize(1152, 720));
        const int x = 576;
        const int center = brightness(output.pixel(x, 301));
        const int edge = brightness(output.pixel(x, 300));
        QVERIFY2(center > edge + 60, qPrintable(QString("center %1 edge %2").arg(center).arg(edge)));
    }

    void scanlineStrengthIsAdjustable()
    {
        CrtRenderer renderer;
        renderer.setSettings(onlyEffect(30, 0, 0, 0));
        QImage output = renderer.render(solidFrame(qRgb(200, 200, 200)), QSize(1152, 720));
        const int weakContrast = brightness(output.pixel(576, 301)) - brightness(output.pixel(576, 300));

        renderer.setSettings(onlyEffect(100, 0, 0, 0));
        output = renderer.render(solidFrame(qRgb(200, 200, 200)), QSize(1152, 720));
        const int strongContrast = brightness(output.pixel(576, 301)) - brightness(output.pixel(576, 300));

        QVERIFY(weakContrast > 0);
        QVERIFY(strongContrast > weakContrast);
    }

    void apertureGrilleTintsColumns()
    {
        CrtRenderer renderer;
        renderer.setSettings(onlyEffect(0, 100, 0, 0));
        const QImage output = renderer.render(solidFrame(qRgb(150, 150, 150)), QSize(1152, 720));
        const int y = 360;
        // Each of three neighbouring columns favours a different channel.
        const QRgb p0 = output.pixel(600, y);
        const QRgb p1 = output.pixel(601, y);
        const QRgb p2 = output.pixel(602, y);
        QVERIFY(qRed(p0) > qGreen(p0) && qRed(p0) > qBlue(p0));
        QVERIFY(qGreen(p1) > qRed(p1) && qGreen(p1) > qBlue(p1));
        QVERIFY(qBlue(p2) > qRed(p2) && qBlue(p2) > qGreen(p2));
    }

    void apertureGrilleFadesAtNativeSize()
    {
        CrtRenderer renderer;
        renderer.setSettings(onlyEffect(0, 100, 0, 0));
        const QImage output = renderer.render(solidFrame(qRgb(150, 150, 150)), QSize(384, 240));
        const QRgb p = output.pixel(100, 100);
        QCOMPARE(qRed(p), qGreen(p));
        QCOMPARE(qGreen(p), qBlue(p));
    }

    void curvatureBlacksOutCorners()
    {
        CrtRenderer renderer;
        renderer.setSettings(onlyEffect(0, 0, 100, 0));
        const QImage output = renderer.render(solidFrame(qRgb(255, 255, 255)), QSize(1152, 720));
        QCOMPARE(output.pixel(0, 0), qRgb(0, 0, 0));
        QCOMPARE(output.pixel(1151, 0), qRgb(0, 0, 0));
        QCOMPARE(output.pixel(0, 719), qRgb(0, 0, 0));
        QCOMPARE(output.pixel(1151, 719), qRgb(0, 0, 0));
        // The middle of the tube stays lit, and is brighter than near the edge (vignette).
        const int middle = brightness(output.pixel(576, 360));
        QVERIFY(middle > 600);
        QVERIFY(middle > brightness(output.pixel(40, 360)));
    }

    void noCurvatureKeepsCorners()
    {
        CrtRenderer renderer;
        renderer.setSettings(onlyEffect(0, 0, 0, 0));
        const QImage output = renderer.render(solidFrame(qRgb(255, 255, 255)), QSize(1152, 720));
        QCOMPARE(output.pixel(0, 0), qRgb(255, 255, 255));
        QCOMPARE(output.pixel(1151, 719), qRgb(255, 255, 255));
    }

    void glowSpreadsLightIntoDarkNeighbours()
    {
        QImage frame = solidFrame(qRgb(0, 0, 0));
        for (int y = 100; y < 140; ++y) {
            for (int x = 180; x < 200; ++x) {
                frame.setPixel(x, y, qRgb(255, 255, 255));
            }
        }
        CrtRenderer renderer;
        renderer.setSettings(onlyEffect(0, 0, 0, 0));
        const QImage plain = renderer.render(frame, QSize(1152, 720));
        renderer.setSettings(onlyEffect(0, 0, 0, 100));
        const QImage glowing = renderer.render(frame, QSize(1152, 720));

        // One source pixel left of the lit block.
        const QPoint nearBlock(178 * 3 + 1, 120 * 3 + 1);
        QCOMPARE(brightness(plain.pixel(nearBlock)), 0);
        QVERIFY(brightness(glowing.pixel(nearBlock)) > 0);
    }

    void unchangedFrameReusesOutputButChangesAreRendered()
    {
        CrtRenderer renderer;
        renderer.setSettings(onlyEffect(60, 30, 25, 30));
        QImage frame = solidFrame(qRgb(100, 100, 100));
        const QSize size(800, 500);
        const QImage first = renderer.render(frame, size);
        const QImage again = renderer.render(solidFrame(qRgb(100, 100, 100)), size);
        QCOMPARE(again.constBits(), first.constBits());

        frame.setPixel(192, 120, qRgb(255, 255, 255));
        const QImage changedFrame = renderer.render(frame, size);
        QVERIFY(changedFrame != first);

        renderer.setSettings(onlyEffect(60, 30, 25, 0));
        const QImage changedSettings = renderer.render(frame, size);
        QVERIFY(changedSettings != changedFrame);

        QCOMPARE(renderer.render(frame, QSize(400, 250)).size(), QSize(400, 250));
    }

    void largeFrameRendersQuickly()
    {
        CrtRenderer renderer;
        renderer.setSettings(onlyEffect(60, 30, 25, 30));
        // Alternate frames so every render does the full work.
        const QImage frames[2] = {solidFrame(qRgb(90, 120, 160)), solidFrame(qRgb(160, 120, 90))};
        const QSize size(1440, 900);
        renderer.render(frames[1], size);  // builds lookup tables

        QElapsedTimer timer;
        timer.start();
        const int count = 20;
        for (int i = 0; i < count; ++i) {
            renderer.render(frames[i % 2], size);
        }
        const double msPerFrame = timer.nsecsElapsed() / 1e6 / count;
        qInfo("CRT render %dx%d: %.2f ms/frame", size.width(), size.height(), msPerFrame);
        // Generous bound so slow CI machines pass; real budget is ~16 ms per frame.
        QVERIFY(msPerFrame < 200.0);
    }
};

QTEST_GUILESS_MAIN(TestCrtRenderer)
#include "test_crt_renderer.moc"
