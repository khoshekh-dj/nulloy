/********************************************************************
**  TrackDeck, https://github.com/khoshekh-dj/trackdeck
**  Copyright (C) 2026 Khoshekh
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

#include "bpmDetector.h"

#include <algorithm>
#include <cmath>

// A 128-sample hop keeps aubio's beat-period quantisation small (~0.3% at
// 44.1 kHz); the default 512 is ~1.2%, which showed up as tracks reading 1.5 BPM
// high in testing.
static const int WIN_SIZE = 512;
static const int HOP_SIZE = 128;

// Ignore the first seconds while the tracker locks on.
static const double SETTLE_SECONDS = 4.0;
// A gap counts as "steady" if it is within this fraction of the median gap.
static const double STEADY_TOLERANCE = 0.04;
// Shortest steady run (in beats) that contributes to the estimate.
static const int MIN_RUN_BEATS = 8;
// Results are folded into this range by doubling or halving.
static const double MIN_BPM = 75.0;
static const double MAX_BPM = 185.0;

void NBpmDetector::Biquad::setHighPass(double freq, double rate)
{
    // RBJ cookbook, Q = 1/sqrt(2)
    double w = 2 * M_PI * freq / rate;
    double alpha = std::sin(w) / (2 * M_SQRT1_2);
    double c = std::cos(w);
    double a0 = 1 + alpha;
    b0 = (1 + c) / 2 / a0;
    b1 = -(1 + c) / a0;
    b2 = (1 + c) / 2 / a0;
    a1 = -2 * c / a0;
    a2 = (1 - alpha) / a0;
    z1 = z2 = 0;
}

void NBpmDetector::Biquad::setLowPass(double freq, double rate)
{
    double w = 2 * M_PI * freq / rate;
    double alpha = std::sin(w) / (2 * M_SQRT1_2);
    double c = std::cos(w);
    double a0 = 1 + alpha;
    b0 = (1 - c) / 2 / a0;
    b1 = (1 - c) / a0;
    b2 = (1 - c) / 2 / a0;
    a1 = -2 * c / a0;
    a2 = (1 - alpha) / a0;
    z1 = z2 = 0;
}

NBpmDetector::NBpmDetector()
{
    m_sampleRate = 0;
    m_frames = 0;
    m_abandoned = false;
    m_fill = 0;
}

NBpmDetector::~NBpmDetector()
{
    destroyTrackers();
}

void NBpmDetector::destroyTrackers()
{
    for (Tracker *t : {&m_full, &m_kick}) {
        if (t->tempo) {
            del_aubio_tempo(t->tempo);
        }
        if (t->in) {
            del_fvec(t->in);
        }
        if (t->out) {
            del_fvec(t->out);
        }
        t->tempo = nullptr;
        t->in = nullptr;
        t->out = nullptr;
        t->beats.clear();
    }
}

void NBpmDetector::createTrackers(int sampleRate)
{
    destroyTrackers();
    m_sampleRate = sampleRate;
    for (Tracker *t : {&m_full, &m_kick}) {
        t->tempo = new_aubio_tempo("default", WIN_SIZE, HOP_SIZE, sampleRate);
        t->in = new_fvec(HOP_SIZE);
        t->out = new_fvec(2);
        t->beats.reserve(4096);
    }
    m_highPass.setHighPass(30, sampleRate);
    m_lowPass.setLowPass(200, sampleRate);
    m_fill = 0;
}

void NBpmDetector::reset()
{
    destroyTrackers();
    m_sampleRate = 0;
    m_frames = 0;
    m_abandoned = false;
    m_fill = 0;
}

void NBpmDetector::process(const short *pcm, int nChannels, int nFrames, int sampleRate)
{
    if (m_abandoned || nChannels <= 0 || sampleRate <= 0) {
        return;
    }
    if (m_sampleRate != sampleRate) {
        if (m_sampleRate != 0) { // rate changed mid-track; can't trust the result
            m_abandoned = true;
            destroyTrackers();
            return;
        }
        createTrackers(sampleRate);
    }
    if (!m_full.tempo || !m_kick.tempo) {
        m_abandoned = true;
        return;
    }

    for (int i = 0; i < nFrames; ++i) {
        int sum = 0;
        for (int c = 0; c < nChannels; ++c) {
            sum += pcm[i * nChannels + c];
        }
        float mono = (float)sum / nChannels / 32768.0f;
        m_full.in->data[m_fill] = mono;
        m_kick.in->data[m_fill] = m_lowPass.process(m_highPass.process(mono));
        if (++m_fill == HOP_SIZE) {
            runHop();
            m_fill = 0;
        }
    }

    m_frames += nFrames;
    if (m_frames > (qint64)maxSeconds() * sampleRate) { // most likely a DJ mix
        m_abandoned = true;
        destroyTrackers();
    }
}

void NBpmDetector::runHop()
{
    for (Tracker *t : {&m_full, &m_kick}) {
        aubio_tempo_do(t->tempo, t->in, t->out);
        if (t->out->data[0] != 0) {
            t->beats.append(aubio_tempo_get_last_s(t->tempo));
        }
    }
}

double NBpmDetector::bpmFromBeats(const QVector<double> &beats)
{
    QVector<double> gaps;
    for (int i = 1; i < beats.size(); ++i) {
        if (beats[i - 1] > SETTLE_SECONDS) {
            gaps.append(beats[i] - beats[i - 1]);
        }
    }
    if (gaps.size() < MIN_RUN_BEATS) {
        return 0;
    }
    std::sort(gaps.begin(), gaps.end());
    double median = gaps[gaps.size() / 2];
    if (median <= 0) {
        return 0;
    }

    // Split the beats into steady runs and measure each run end to end; the
    // tracker's breakdowns and phase jumps fall between runs.
    double span = 0;
    long count = 0;
    int start = -1;
    for (int i = 1; i <= beats.size(); ++i) {
        bool steady = i < beats.size() && beats[i - 1] > SETTLE_SECONDS &&
                      std::fabs((beats[i] - beats[i - 1]) - median) < STEADY_TOLERANCE * median;
        if (steady && start < 0) {
            start = i - 1;
        }
        if (!steady && start >= 0) {
            int len = (i - 1) - start;
            if (len >= MIN_RUN_BEATS) {
                span += beats[i - 1] - beats[start];
                count += len;
            }
            start = -1;
        }
    }

    double period = count > 0 ? span / count : median;
    return 60.0 / period;
}

double NBpmDetector::finish()
{
    if (m_abandoned || !m_full.tempo) {
        reset();
        return 0;
    }

    double full = bpmFromBeats(m_full.beats);
    double kick = bpmFromBeats(m_kick.beats);
    reset();

    double bpm = full > 0 ? full : kick;
    // aubio sometimes locks onto half the tempo of fast tracks on the full
    // mix; the kick band usually doesn't
    if (full > 0 && kick > 0 && std::fabs(kick - 2 * full) < 0.03 * 2 * full) {
        bpm = 2 * full;
    }
    if (bpm <= 0) {
        return 0;
    }
    while (bpm < MIN_BPM) {
        bpm *= 2;
    }
    while (bpm > MAX_BPM) {
        bpm /= 2;
    }
    return bpm;
}
