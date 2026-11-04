/********************************************************************
**  TrackDeck, based on Nulloy Music Player, http://nulloy.com
**
**  This program can be distributed under the terms of the GNU
**  General Public License version 3.0 as published by the Free
**  Software Foundation and appearing in the file LICENSE.GPL3
**  included in the packaging of this file.  Please review the
**  following information to ensure the GNU General Public License
**  version 3.0 requirements will be met:
**
**  http://www.gnu.org/licenses/gpl-3.0.html
**
*********************************************************************/

#ifndef N_SPECTRUM_WIDGET_H
#define N_SPECTRUM_WIDGET_H

#include <QColor>
#include <QElapsedTimer>
#include <QPixmap>
#include <QVector>
#include <QWidget>

class QTimer;

// Live spectrum bars with falling peak caps, fed by the playback engine's
// spectrumChanged() signal. Bands are spaced logarithmically from 30 Hz to
// 16 kHz; the number of bars follows the widget's width.
class NSpectrumWidget : public QWidget
{
    Q_OBJECT

public:
    NSpectrumWidget(QWidget *parent = 0);
    ~NSpectrumWidget() {}

    void setColorScheme(bool rgb, const QString &scheme, const QColor &accent);

public slots:
    void setSpectrum(const QVector<float> &magnitudes, int sampleRate);
    void clear();

protected:
    void paintEvent(QPaintEvent *event);
    void resizeEvent(QResizeEvent *event);

private slots:
    void tick();

private:
    int barCount() const;
    QColor barColor(int bar, int bars) const;
    void renderCache();

    // bars pre-rendered at full height (lit and unlit); each frame copies
    // the visible part of each bar instead of painting gradients
    QPixmap m_litBars;
    QPixmap m_unlitBars;
    QVector<QColor> m_capColors;
    bool m_cacheDirty;

    QTimer *m_timer;
    QElapsedTimer m_clock;
    qint64 m_lastTick;
    qint64 m_lastFrame;

    QVector<float> m_target; // 0..1 per bar, from the latest frame
    QVector<float> m_level;  // what's drawn, eased towards m_target
    QVector<float> m_peak;
    QVector<float> m_peakHold; // seconds left before the cap starts falling

    bool m_rgb;
    QString m_scheme;
    QColor m_accent;
};

#endif
