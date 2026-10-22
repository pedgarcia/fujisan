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

    // Everything about an output pixel that depends only on geometry and settings.
    struct Tap {
        qint32 sx;       // source x in 1/256 pixels, -1 = outside the tube
        quint16 row;     // source row
        quint16 weight;  // scanline beam x vignette x edge, 8.8 fixed point
        quint16 glow;    // glow strength x vignette x edge, 8.8 fixed point
    };
    QVector<Tap> m_taps;

    std::array<quint8, 256> m_sharpen{};      // horizontal interpolation curve
    std::array<std::array<quint16, 3>, 3> m_maskWeights{};  // [column % 3][channel], 0-256
    bool m_glowEnabled = false;

    QVector<quint32> m_glowBuffer;            // blurred source, same size as the source

    // Emulated screens are often static; identical frames reuse the last result.
    QImage m_lastSource;
    QImage m_lastOutput;
    QThreadPool m_pool;
};

#endif // CRTRENDERER_H
