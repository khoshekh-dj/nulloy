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

#include "bpmCache.h"

#include "common.h"

#include <QDataStream>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <cmath>

// bump when the detector changes enough that old results should be redone
static const quint32 CACHE_VERSION = 1;

NBpmCache *NBpmCache::instance()
{
    static NBpmCache cache;
    return &cache;
}

NBpmCache::NBpmCache()
{
    m_loaded = false;
    m_path = NCore::rcDir() + "/" + NCore::applicationBinaryName() + ".bpm";
}

static qint64 modifiedTime(const QString &file)
{
    return QFileInfo(file).lastModified().toMSecsSinceEpoch();
}

void NBpmCache::load()
{
    m_loaded = true;
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }
    QDataStream in(&f);
    quint32 version = 0;
    in >> version;
    if (version != CACHE_VERSION) {
        return;
    }
    qint32 count = 0;
    in >> count;
    for (int i = 0; i < count && in.status() == QDataStream::Ok; ++i) {
        QString file;
        Entry e;
        in >> file >> e.modified >> e.bpm;
        m_entries.insert(file, e);
    }
}

void NBpmCache::save() const
{
    QSaveFile f(m_path);
    if (!f.open(QIODevice::WriteOnly)) {
        return;
    }
    QDataStream out(&f);
    out << CACHE_VERSION << (qint32)m_entries.size();
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        out << it.key() << it.value().modified << it.value().bpm;
    }
    f.commit();
}

bool NBpmCache::lookup(const QString &file, double *bpm)
{
    if (!m_loaded) {
        load();
    }
    auto it = m_entries.constFind(file);
    if (it == m_entries.constEnd() || it.value().modified != modifiedTime(file)) {
        return false;
    }
    if (bpm) {
        *bpm = it.value().bpm;
    }
    return true;
}

void NBpmCache::insert(const QString &file, double bpm)
{
    if (!m_loaded) {
        load();
    }
    m_entries.insert(file, Entry{modifiedTime(file), bpm});
    save();
}

QString NBpmCache::format(double bpm)
{
    if (bpm <= 0) {
        return QString();
    }
    double rounded = std::round(bpm * 10) / 10;
    if (std::fabs(rounded - std::round(rounded)) < 0.05) {
        return QString::number((int)std::round(rounded));
    }
    return QString::number(rounded, 'f', 1);
}
