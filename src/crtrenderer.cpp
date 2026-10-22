/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 *
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include "crtrenderer.h"
#include <QRunnable>
#include <QThread>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <functional>

namespace {

constexpr int kMaxThreads = 8;
// Fewer output rows than this aren't worth splitting across threads.
constexpr int kMinRowsPerBand = 64;

// Beam profile: brightness across one scanline, 1 at its center falling to
// exp(-kBeamFalloff) at the edge between two lines.
constexpr double kBeamFalloff = 2.8;
// Fraction of the brightness lost to scanlines that is given back.
constexpr double kScanlineCompensation = 0.7;
constexpr double kMaskDepth = 0.6;
constexpr double kMaxGlow = 0.5;
constexpr double kMaxWarpX = 0.08;
constexpr double kMaxWarpY = 0.11;
constexpr double kMaxCornerRadius = 0.05;  // of the output height
constexpr double kMaxVignettePower = 0.25;
constexpr double kPi = 3.14159265358979323846;

class BandTask : public QRunnable
{
public:
    explicit BandTask(std::function<void()> work) : m_work(std::move(work)) {}
    void run() override { m_work(); }

private:
    std::function<void()> m_work;
};

inline int clamp255(int value)
{
    return value > 255 ? 255 : value;
}

}  // namespace

CrtSettings CrtSettings::load(const QSettings& settings)
{
    const CrtSettings defaults;
    CrtSettings crt;
    crt.enabled = settings.value("video/crtEnabled", defaults.enabled).toBool();
    crt.scanlines = qBound(0, settings.value("video/crtScanlines", defaults.scanlines).toInt(), 100);
    crt.mask = qBound(0, settings.value("video/crtMask", defaults.mask).toInt(), 100);
    crt.curvature = qBound(0, settings.value("video/crtCurvature", defaults.curvature).toInt(), 100);
    crt.glow = qBound(0, settings.value("video/crtGlow", defaults.glow).toInt(), 100);
    return crt;
}

void CrtSettings::save(QSettings& settings) const
{
    settings.setValue("video/crtEnabled", enabled);
    settings.setValue("video/crtScanlines", scanlines);
    settings.setValue("video/crtMask", mask);
    settings.setValue("video/crtCurvature", curvature);
    settings.setValue("video/crtGlow", glow);
}

bool CrtSettings::operator==(const CrtSettings& other) const
{
    return enabled == other.enabled && scanlines == other.scanlines && mask == other.mask
           && curvature == other.curvature && glow == other.glow;
}

CrtRenderer::CrtRenderer()
{
    m_pool.setMaxThreadCount(qBound(1, QThread::idealThreadCount() - 1, kMaxThreads - 1));
}

void CrtRenderer::setSettings(const CrtSettings& settings)
{
    CrtSettings clamped = settings;
    clamped.scanlines = qBound(0, clamped.scanlines, 100);
    clamped.mask = qBound(0, clamped.mask, 100);
    clamped.curvature = qBound(0, clamped.curvature, 100);
    clamped.glow = qBound(0, clamped.glow, 100);
    if (clamped != m_settings) {
        m_settings = clamped;
        m_tablesValid = false;
    }
}

void CrtRenderer::rebuildTables(const QSize& sourceSize, const QSize& outputSize)
{
    const int sw = sourceSize.width();
    const int sh = sourceSize.height();
    const int ow = outputSize.width();
    const int oh = outputSize.height();
    const double curvature = m_settings.curvature / 100.0;
    const double warpX = curvature * kMaxWarpX;
    const double warpY = curvature * kMaxWarpY;
    const double cornerRadius = curvature * kMaxCornerRadius * oh;
    const double vignettePower = curvature * kMaxVignettePower;

    const double scanlines = m_settings.scanlines / 100.0;
    const double averageBeam =
        1.0 - scanlines * (1.0 - std::sqrt(kPi / kBeamFalloff) / 2.0 * std::erf(std::sqrt(kBeamFalloff)));
    const double beamGain = std::pow(1.0 / averageBeam, kScanlineCompensation);
    std::array<double, 256> beam;
    for (int i = 0; i < 256; ++i) {
        const double distance = std::fabs((i + 0.5) / 256.0 - 0.5) * 2.0;
        const double profile = std::exp(-kBeamFalloff * distance * distance);
        beam[i] = (1.0 - scanlines * (1.0 - profile)) * beamGain;
    }
    const double glowAmount = m_settings.glow / 100.0 * kMaxGlow;
    m_glowEnabled = glowAmount > 0.0;

    m_taps.resize(ow * oh);

    for (int y = 0; y < oh; ++y) {
        const double cy = 2.0 * (y + 0.5) / oh - 1.0;
        for (int x = 0; x < ow; ++x) {
            const double cx = 2.0 * (x + 0.5) / ow - 1.0;
            const double u = (cx * (1.0 + cy * cy * warpX)) * 0.5 + 0.5;
            const double v = (cy * (1.0 + cx * cx * warpY)) * 0.5 + 0.5;
            Tap& tap = m_taps[y * ow + x];

            // Distance in output pixels to the edge of the (rounded) tube face.
            const double dx = std::min(u, 1.0 - u) * ow;
            const double dy = std::min(v, 1.0 - v) * oh;
            double distance = std::min(dx, dy);
            if (cornerRadius > 0.0 && dx < cornerRadius && dy < cornerRadius) {
                distance = cornerRadius - std::hypot(cornerRadius - dx, cornerRadius - dy);
            }
            const double edge = std::clamp(distance + 0.5, 0.0, 1.0);
            if (edge <= 0.0) {
                tap = Tap{-1, 0, 0, 0};
                continue;
            }

            double vignette = 1.0;
            if (vignettePower > 0.0) {
                vignette = std::pow(std::max(16.0 * u * (1.0 - u) * v * (1.0 - v), 0.0), vignettePower);
            }

            const double sx = std::clamp(u * sw - 0.5, 0.0, sw - 1.0);
            const double sy = std::clamp(v * sh, 0.0, sh - 1.0 / 256.0);
            const double shade = edge * vignette;
            const int syFixed = static_cast<int>(sy * 256.0);
            tap.sx = static_cast<qint32>(sx * 256.0);
            tap.row = static_cast<quint16>(syFixed >> 8);
            tap.weight = static_cast<quint16>(std::lround(beam[syFixed & 0xff] * shade * 256.0));
            tap.glow = static_cast<quint16>(std::lround(glowAmount * shade * 256.0));
        }
    }

    // Wide horizontal steps get a steeper blend so pixels stay sharp but anti-aliased.
    const double scaleX = static_cast<double>(ow) / sw;
    const double sharpness = std::clamp(scaleX / 1.5, 1.0, 4.0);
    for (int i = 0; i < 256; ++i) {
        const double t = std::clamp((i / 256.0 - 0.5) * sharpness + 0.5, 0.0, 1.0);
        m_sharpen[i] = static_cast<quint8>(std::min(255L, std::lround(t * 256.0)));
    }

    // A 3-pixel grille needs room: fade it out as the image approaches 1:1.
    const double maskRoom = std::clamp((scaleX - 1.0) / 2.0, 0.0, 1.0);
    const double offChannel = 1.0 - kMaskDepth * (m_settings.mask / 100.0) * maskRoom;
    const double maskGain = 3.0 / (1.0 + 2.0 * offChannel);
    for (int phase = 0; phase < 3; ++phase) {
        for (int channel = 0; channel < 3; ++channel) {
            const double weight = (phase == channel ? 1.0 : offChannel) * maskGain;
            m_maskWeights[phase][channel] = static_cast<quint16>(std::lround(weight * 256.0));
        }
    }
    m_tableSourceSize = sourceSize;
    m_tableOutputSize = outputSize;
    m_tablesValid = true;
}

void CrtRenderer::buildGlow(const QImage& source)
{
    // Separable [1 4 6 4 1] blur at source resolution.
    const int w = source.width();
    const int h = source.height();
    static constexpr int kTaps[5] = {1, 4, 6, 4, 1};

    QVector<quint32> horizontal(w * h);
    for (int y = 0; y < h; ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(source.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            int r = 0, g = 0, b = 0;
            for (int t = 0; t < 5; ++t) {
                const QRgb p = line[qBound(0, x + t - 2, w - 1)];
                r += qRed(p) * kTaps[t];
                g += qGreen(p) * kTaps[t];
                b += qBlue(p) * kTaps[t];
            }
            horizontal[y * w + x] = qRgb(r >> 4, g >> 4, b >> 4);
        }
    }

    m_glowBuffer.resize(w * h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int r = 0, g = 0, b = 0;
            for (int t = 0; t < 5; ++t) {
                const QRgb p = horizontal[qBound(0, y + t - 2, h - 1) * w + x];
                r += qRed(p) * kTaps[t];
                g += qGreen(p) * kTaps[t];
                b += qBlue(p) * kTaps[t];
            }
            m_glowBuffer[y * w + x] = qRgb(r >> 4, g >> 4, b >> 4);
        }
    }
}

void CrtRenderer::renderRows(const QImage& source, QImage& output, int firstRow, int lastRow) const
{
    const int sw = source.width();
    const int ow = output.width();
    const uchar* sourceBits = source.constBits();
    const qsizetype sourceStride = source.bytesPerLine();
    const quint32* glowBuffer = m_glowBuffer.constData();

    for (int y = firstRow; y < lastRow; ++y) {
        QRgb* out = reinterpret_cast<QRgb*>(output.scanLine(y));
        const Tap* taps = m_taps.constData() + static_cast<qsizetype>(y) * ow;
        int phase = 0;
        for (int x = 0; x < ow; ++x, ++phase) {
            if (phase == 3) {
                phase = 0;
            }
            const Tap& tap = taps[x];
            if (tap.sx < 0) {
                out[x] = 0xff000000;
                continue;
            }
            const int ix = tap.sx >> 8;
            const int ix1 = ix + 1 < sw ? ix + 1 : ix;
            const int fx = m_sharpen[tap.sx & 0xff];

            const QRgb* line = reinterpret_cast<const QRgb*>(sourceBits + tap.row * sourceStride);
            const QRgb p0 = line[ix];
            const QRgb p1 = line[ix1];
            const int r = qRed(p0) + (((qRed(p1) - qRed(p0)) * fx) >> 8);
            const int g = qGreen(p0) + (((qGreen(p1) - qGreen(p0)) * fx) >> 8);
            const int b = qBlue(p0) + (((qBlue(p1) - qBlue(p0)) * fx) >> 8);

            const int weight = tap.weight;
            const auto& mask = m_maskWeights[phase];
            int outR = (r * weight * mask[0]) >> 16;
            int outG = (g * weight * mask[1]) >> 16;
            int outB = (b * weight * mask[2]) >> 16;

            if (m_glowEnabled) {
                const QRgb halo = glowBuffer[tap.row * sw + (fx >= 128 ? ix1 : ix)];
                outR += (qRed(halo) * tap.glow) >> 8;
                outG += (qGreen(halo) * tap.glow) >> 8;
                outB += (qBlue(halo) * tap.glow) >> 8;
            }

            out[x] = qRgb(clamp255(outR), clamp255(outG), clamp255(outB));
        }
    }
}

QImage CrtRenderer::render(const QImage& source, const QSize& outputSize)
{
    if (source.isNull() || outputSize.isEmpty()) {
        return QImage();
    }

    QImage src = source;
    if (src.format() != QImage::Format_RGB32 && src.format() != QImage::Format_ARGB32) {
        src = src.convertToFormat(QImage::Format_RGB32);
    }

    if (!m_tablesValid || m_tableSourceSize != src.size() || m_tableOutputSize != outputSize) {
        rebuildTables(src.size(), outputSize);
    } else if (src == m_lastSource) {
        return m_lastOutput;
    }
    if (m_glowEnabled) {
        buildGlow(src);
    }

    QImage output(outputSize, QImage::Format_RGB32);
    const int rows = outputSize.height();
    const int bands = qBound(1, rows / kMinRowsPerBand, m_pool.maxThreadCount() + 1);
    const int rowsPerBand = (rows + bands - 1) / bands;

    for (int band = 1; band < bands; ++band) {
        const int first = band * rowsPerBand;
        const int last = std::min(rows, first + rowsPerBand);
        if (first < last) {
            m_pool.start(new BandTask([this, &src, &output, first, last]() {
                renderRows(src, output, first, last);
            }));
        }
    }
    renderRows(src, output, 0, std::min(rows, rowsPerBand));
    m_pool.waitForDone();

    m_lastSource = src;
    m_lastOutput = output;
    return output;
}
