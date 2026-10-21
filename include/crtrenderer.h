/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 *
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef CRTRENDERER_H
#define CRTRENDERER_H

#include <QImage>
#include <QSettings>
#include <QSize>
#include <QThreadPool>
#include <QVector>
#include <array>

// CRT look applied by Fujisan itself: libatari800 only hands back the raw frame,
// and atari800's own scanline code lives in its SDL display drivers.
struct CrtSettings {
    bool enabled = false;
    int scanlines = 60;   // 0-100: darkness of the gaps between scanlines
    int mask = 30;        // 0-100: aperture grille (RGB stripe) strength
    int curvature = 25;   // 0-100: tube curvature, vignette and rounded corners
    int glow = 30;        // 0-100: phosphor glow around bright pixels

    // Persisted as video/crt* keys in the application QSettings.
    static CrtSettings load(const QSettings& settings);
    void save(QSettings& settings) const;

    bool operator==(const CrtSettings& other) const;
    bool operator!=(const CrtSettings& other) const { return !(*this == other); }
};

class CrtRenderer
{
public:
    CrtRenderer();

    void setSettings(const CrtSettings& settings);
    const CrtSettings& settings() const { return m_settings; }

    // Renders source (any size, RGB32/ARGB32) into an RGB32 image of outputSize.
    QImage render(const QImage& source, const QSize& outputSize);

private:
    void rebuildTables(const QSize& sourceSize, const QSize& outputSize);
    void buildGlow(const QImage& source);
    void renderRows(const QImage& source, QImage& output, int firstRow, int lastRow) const;

    CrtSettings m_settings;
    QSize m_tableSourceSize;
    QSize m_tableOutputSize;
    bool m_tablesValid = false;

    // Per output pixel: source position in 1/256 source pixels (-1 = outside the tube)
    // and a 0-256 shade for vignette and anti-aliased tube edges.
    QVector<qint32> m_srcX;
    QVector<qint32> m_srcY;
    QVector<quint16> m_shade;

    std::array<quint16, 256> m_beam{};        // scanline weight by position within a line, 8.8 fixed point
    std::array<quint8, 256> m_sharpen{};      // horizontal interpolation curve
    std::array<std::array<quint16, 3>, 3> m_maskWeights{};  // [column % 3][channel], 0-256
    int m_glowAmount = 0;                     // 0-256

    QVector<quint32> m_glowBuffer;            // blurred source, same size as the source
    QThreadPool m_pool;
};

#endif // CRTRENDERER_H
