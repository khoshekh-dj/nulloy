/********************************************************************
**  Nulloy Music Player, http://nulloy.com
**  Copyright (C) 2010-2024 Sergey Vlasov <sergey@vlasov.me>
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

#ifndef N_WAVEFORM_BUILDER_INTERFACE_H
#define N_WAVEFORM_BUILDER_INTERFACE_H

#include <QtCore>

#include "waveformPeaks.h"

#define WAVEFORM_INTERFACE "Nulloy/NWaveformBuilderInterface/0.8"

class NWaveformBuilderInterface : public QThread
{
public:
    NWaveformBuilderInterface(QObject *parent = 0) : QThread(parent) {}
    virtual ~NWaveformBuilderInterface() {}

    virtual void start(const QString &file) = 0;
    virtual void stop() = 0;
    virtual void positionAndIndex(float &pos, int &index) = 0;
    virtual const NWaveformPeaks &peaks() const = 0;

    // Ask the next start() to also detect the track's BPM. Builders that support
    // it emit bpmDetected(QString file, qreal bpm) when the track has been
    // decoded; bpm is 0 when no tempo was found or the track was too long.
    virtual void setBpmDetection(bool enabled) { Q_UNUSED(enabled); }

    static QString interfaceString() { return WAVEFORM_INTERFACE; }
};

Q_DECLARE_INTERFACE(NWaveformBuilderInterface, WAVEFORM_INTERFACE)

#endif
