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

#include "spectrumWidget.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

#include <cmath>

static const double MIN_FREQ = 30.0;
static const double MAX_FREQ = 16000.0;
// Music loses roughly 5 dB per octave towards the top; tilt it back so the
// treble bars move as much as the bass ones.
static const double TILT_DB_PER_OCTAVE = 5.0;
// Tilted levels mapped onto the bar height.
static const double FLOOR_DB = -66.0;
static const double CEILING_DB = -26.0;

static const int BAR_WIDTH = 9;
static const int BAR_GAP = 3;
static const int MIN_BARS = 12;
static const int MAX_BARS = 64;

static const double FALL_PER_SEC = 1.6;      // bar height per second
static const double PEAK_HOLD_SEC = 0.35;
static const double PEAK_FALL_PER_SEC = 0.55;
static const qint64 FRAME_TIMEOUT_MSEC = 150; // no frames: paused or stopped

NSpectrumWidget::NSpectrumWidget(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setMinimumSize(80, 40);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_rgb = true;
    m_scheme = "rgb";
    m_accent = QColor("#3D8BFF");

    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);
    m_timer->setInterval(16);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(tick()));
    m_clock.start();
    m_lastTick = 0;
    m_lastFrame = -FRAME_TIMEOUT_MSEC;
    m_cacheDirty = true;
}

int NSpectrumWidget::barCount() const
{
    int n = (width() + BAR_GAP) / (BAR_WIDTH + BAR_GAP);
    return qBound(MIN_BARS, n, MAX_BARS);
}

void NSpectrumWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    int n = barCount();
    if (n != m_level.size()) {
        m_target.fill(0, n);
        m_level.fill(0, n);
        m_peak.fill(0, n);
        m_peakHold.fill(0, n);
    }
    m_cacheDirty = true;
}

void NSpectrumWidget::setColorScheme(bool rgb, const QString &scheme, const QColor &accent)
{
    m_rgb = rgb;
    m_scheme = scheme;
    if (accent.isValid()) {
        m_accent = accent;
    }
    m_cacheDirty = true;
    update();
}

void NSpectrumWidget::setSpectrum(const QVector<float> &magnitudes, int sampleRate)
{
    int n = barCount();
    if (m_target.size() != n) {
        m_target.fill(0, n);
        m_level.fill(0, n);
        m_peak.fill(0, n);
        m_peakHold.fill(0, n);
    }
    int bins = magnitudes.size();
    if (bins < 2 || sampleRate <= 0) {
        return;
    }
    double binWidth = sampleRate / 2.0 / bins;
    double top = qMin(MAX_FREQ, sampleRate / 2.0);

    for (int b = 0; b < n; ++b) {
        double f0 = MIN_FREQ * std::pow(top / MIN_FREQ, (double)b / n);
        double f1 = MIN_FREQ * std::pow(top / MIN_FREQ, (double)(b + 1) / n);
        int i0 = (int)std::floor(f0 / binWidth);
        int i1 = (int)std::ceil(f1 / binWidth);
        i0 = qBound(0, i0, bins - 1);
        i1 = qBound(i0 + 1, i1, bins);
        // loudest bin in the band (narrow bass bands fall within one bin)
        float db = -200;
        for (int i = i0; i < i1; ++i) {
            db = qMax(db, magnitudes.at(i));
        }
        double centre = std::sqrt(f0 * f1);
        double tilted = db + TILT_DB_PER_OCTAVE * std::log2(centre / 1000.0);
        m_target[b] = (float)qBound(0.0, (tilted - FLOOR_DB) / (CEILING_DB - FLOOR_DB), 1.0);
    }

    m_lastFrame = m_clock.elapsed();
    if (!m_timer->isActive()) {
        m_lastTick = m_clock.elapsed();
        m_timer->start();
    }
}

void NSpectrumWidget::clear()
{
    m_target.fill(0);
    m_lastFrame = -FRAME_TIMEOUT_MSEC;
}

void NSpectrumWidget::tick()
{
    qint64 now = m_clock.elapsed();
    double dt = qBound(0.0, (now - m_lastTick) / 1000.0, 0.1);
    m_lastTick = now;

    if (now - m_lastFrame > FRAME_TIMEOUT_MSEC) { // paused or stopped: let the bars drop
        m_target.fill(0);
    }

    bool active = false;
    for (int b = 0; b < m_level.size(); ++b) {
        float target = m_target.at(b);
        float level = m_level.at(b);
        if (target > level) {
            level += (target - level) * 0.7f; // quick, but not a hard jump
        } else {
            level = qMax(target, level - (float)(FALL_PER_SEC * dt));
        }
        m_level[b] = level;

        if (level >= m_peak.at(b)) {
            m_peak[b] = level;
            m_peakHold[b] = PEAK_HOLD_SEC;
        } else if (m_peakHold.at(b) > 0) {
            m_peakHold[b] -= dt;
        } else {
            m_peak[b] = qMax(level, m_peak.at(b) - (float)(PEAK_FALL_PER_SEC * dt));
        }
        if (level > 0.001f || m_peak.at(b) > 0.001f) {
            active = true;
        }
    }

    update();
    if (!active && now - m_lastFrame > FRAME_TIMEOUT_MSEC) {
        m_timer->stop();
    }
}

QColor NSpectrumWidget::barColor(int bar, int bars) const
{
    double t = bars > 1 ? (double)bar / (bars - 1) : 0;
    if (!m_rgb || m_scheme == "accent") {
        return m_accent;
    }
    // bass red, mids green, highs blue, as on the waveform
    double hue = 0.66 * t;
    double saturation = m_scheme == "vivid" ? 0.95 : 0.72;
    return QColor::fromHsvF(hue, saturation, 1.0);
}

void NSpectrumWidget::renderCache()
{
    m_cacheDirty = false;
    int n = m_level.size();
    qreal dpr = devicePixelRatioF();
    QSize size = this->size() * dpr;
    if (n == 0 || size.isEmpty()) {
        m_litBars = QPixmap();
        m_unlitBars = QPixmap();
        return;
    }

    m_litBars = QPixmap(size);
    m_unlitBars = QPixmap(size);
    m_litBars.setDevicePixelRatio(dpr);
    m_unlitBars.setDevicePixelRatio(dpr);
    m_litBars.fill(Qt::transparent);
    m_unlitBars.fill(Qt::transparent);
    m_capColors.resize(n);

    QPainter lit(&m_litBars);
    QPainter unlit(&m_unlitBars);
    lit.setRenderHint(QPainter::Antialiasing);
    unlit.setRenderHint(QPainter::Antialiasing);
    lit.setPen(Qt::NoPen);
    unlit.setPen(Qt::NoPen);

    double slot = (double)(width() + BAR_GAP) / n;
    double barWidth = qMax(2.0, slot - BAR_GAP);
    double h = height();
    for (int b = 0; b < n; ++b) {
        QColor color = barColor(b, n);
        QRectF rect(b * slot, 0, barWidth, h);

        // faint full-height track so the bars read as a meter even when quiet
        QColor track = color;
        track.setAlphaF(0.07);
        unlit.setBrush(track);
        unlit.drawRoundedRect(rect, 2, 2);

        QLinearGradient gradient(0, h, 0, 0);
        QColor bottom = color;
        bottom.setAlphaF(0.55);
        gradient.setColorAt(0, bottom);
        gradient.setColorAt(1, color);
        lit.setBrush(gradient);
        lit.drawRoundedRect(rect, 2, 2);

        m_capColors[b] = color.lighter(135);
    }
}

void NSpectrumWidget::paintEvent(QPaintEvent *)
{
    int n = m_level.size();
    if (n == 0) {
        return;
    }
    if (m_cacheDirty || m_litBars.isNull()) {
        renderCache();
    }
    if (m_litBars.isNull()) {
        return;
    }
    QPainter painter(this);
    painter.drawPixmap(0, 0, m_unlitBars);

    qreal dpr = m_litBars.devicePixelRatio();
    double slot = (double)(width() + BAR_GAP) / n;
    double barWidth = qMax(2.0, slot - BAR_GAP);
    double h = height();
    const double capHeight = 2.0;
    double usable = h - capHeight - 2;

    for (int b = 0; b < n; ++b) {
        double x = b * slot;
        double barHeight = m_level.at(b) * usable;
        if (barHeight >= 1) {
            // the lit bar, revealed from the bottom up to the current level
            QRectF target(x, h - barHeight, barWidth + 1, barHeight);
            QRectF source(target.x() * dpr, target.y() * dpr, target.width() * dpr,
                          target.height() * dpr);
            painter.drawPixmap(target, m_litBars, source);
        }
        if (m_peak.at(b) > 0.01) {
            double y = h - m_peak.at(b) * usable - capHeight - 1;
            painter.fillRect(QRectF(x, y, barWidth, capHeight), m_capColors.at(b));
        }
    }
}
