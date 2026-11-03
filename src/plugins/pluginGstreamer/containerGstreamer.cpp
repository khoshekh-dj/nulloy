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

#include "containerGstreamer.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include "common.h"
#include "playbackEngineGstreamer.h"
#include "waveformBuilderGstreamer.h"
#ifdef _N_GSTREAMER_TAGREADER_PLUGIN_
#include "tagReaderGstreamer.h"
#endif

NContainerGstreamer::NContainerGstreamer(QObject *parent) : QObject(parent)
{
    qputenv("GST_REGISTRY", QString("%1/gstreamer-1.0.registry.bin").arg(NCore::rcDir()).toUtf8());

#ifdef Q_OS_MAC
    // Self-contained app bundle: use GStreamer plugins shipped inside it
    // instead of a system (e.g. Homebrew) installation.
    QDir bundledPlugins(QCoreApplication::applicationDirPath() + "/../PlugIns/gstreamer");
    if (bundledPlugins.exists()) {
        qputenv("GST_PLUGIN_SYSTEM_PATH", bundledPlugins.absolutePath().toUtf8());
        qunsetenv("GST_PLUGIN_PATH");
        QString scanner = QCoreApplication::applicationDirPath() + "/gst-plugin-scanner";
        if (QFileInfo::exists(scanner)) {
            qputenv("GST_PLUGIN_SCANNER", scanner.toUtf8());
        }
    }
#endif

    m_plugins << new NPlaybackEngineGStreamer()
#ifdef _N_GSTREAMER_TAGREADER_PLUGIN_
              << new NTagReaderGstreamer()
#endif
              << new NWaveformBuilderGstreamer();
}

NContainerGstreamer::~NContainerGstreamer()
{
    foreach (NPlugin *plugin, m_plugins)
        delete plugin;
}

QList<NPlugin *> NContainerGstreamer::plugins() const
{
    return m_plugins;
}
