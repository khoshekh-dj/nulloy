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
#ifndef N_BPM_DETECTOR_H
#define N_BPM_DETECTOR_H

#include <QVector>

#include <aubio/aubio.h>

// Feeds decoded audio to two aubio tempo trackers, one on the full mix and one
// on the kick-drum band (30-200 Hz), and works out a BPM once the track ends.
//
// aubio quantises its beat period to whole hops, so the BPM is not read from
// aubio directly: beat times are collected and the period is measured over
// the steady stretches of the track (span / beats), which averages the
// quantisation away. The full mix gives the precise value; the kick band is
// used to catch aubio locking onto half the tempo.
class NBpmDetector
{
public:
    NBpmDetector();
    ~NBpmDetector();

    // Discards any state and starts a new track.
    void reset();

    // Interleaved signed 16-bit samples.
    void process(const short *pcm, int nChannels, int nFrames, int sampleRate);

    // Call once at the end of the track. Returns 0 if no tempo was found
    // or the track was longer than maxSeconds().
    double finish();

    bool isAbandoned() const { return m_abandoned; }
    static int maxSeconds() { return 15 * 60; }

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1 = 0, z2 = 0;
        void setHighPass(double freq, double rate);
        void setLowPass(double freq, double rate);
        inline float process(float x)
        {
            double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return (float)y;
        }
    };

    struct Tracker
    {
        aubio_tempo_t *tempo = nullptr;
        fvec_t *in = nullptr;
        fvec_t *out = nullptr;
        QVector<double> beats;
    };

    void createTrackers(int sampleRate);
    void destroyTrackers();
    void runHop();
    static double bpmFromBeats(const QVector<double> &beats);

    int m_sampleRate;
    qint64 m_frames;
    bool m_abandoned;
    int m_fill;
    Tracker m_full;
    Tracker m_kick;
    Biquad m_highPass;
    Biquad m_lowPass;
};

#endif
