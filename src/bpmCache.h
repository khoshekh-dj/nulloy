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

#ifndef N_BPM_CACHE_H
#define N_BPM_CACHE_H

#include <QHash>
#include <QString>

// Detected BPMs, keyed by file path and checked against the file's
// modification time. Stored next to the waveform cache, so tracks are only
// analysed once. A value of 0 means "analysed, no tempo" (e.g. a DJ mix over
// the length limit), so those aren't retried either.
class NBpmCache
{
public:
    static NBpmCache *instance();

    // true if the file has been analysed (bpm may still be 0)
    bool lookup(const QString &file, double *bpm);
    void insert(const QString &file, double bpm);

    // "122", "122.5": whole numbers without decimals
    static QString format(double bpm);

private:
    struct Entry
    {
        qint64 modified;
        double bpm;
    };

    NBpmCache();
    void load();
    void save() const;

    bool m_loaded;
    QString m_path;
    QHash<QString, Entry> m_entries;
};

#endif
