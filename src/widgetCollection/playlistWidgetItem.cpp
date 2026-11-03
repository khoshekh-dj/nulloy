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

#include "playlistWidgetItem.h"

#include <QApplication>
#include <QFileInfo>
#include <QPainter>

#include "global.h"
#include "playlistWidget.h"

NPlaylistWidgetItem::NPlaylistWidgetItem(QListWidget *parent) : QListWidgetItem(parent) {}

NPlaylistWidgetItem::NPlaylistWidgetItem(const QFileInfo &fileinfo, QListWidget *parent)
    : QListWidgetItem(parent)
{
    m_data.path = fileinfo.filePath();
}

NPlaylistWidgetItem::NPlaylistWidgetItem(const NPlaylistDataItem &dataItem, QListWidget *parent)
    : QListWidgetItem(parent)
{
    m_data = dataItem;
    setText(m_data.title);
}

void NPlaylistWidgetItem::setText(const QString &text)
{
    m_data.title = text;
    QListWidgetItem::setText(text);
}

QVariant NPlaylistWidgetItem::data(int role) const
{
    switch (role) {
        case (N::PlayingRole):
            return m_data.playing;
        case (N::FailedRole):
            return m_data.failed;
        case (N::PathRole):
            return m_data.path;
        case (N::DurationRole):
            return m_data.duration;
        case (N::CountRole):
            return m_data.playbackCount;
        case (N::PositionRole):
            return m_data.playbackPosition;
        case (N::TitleFormatRole):
            return m_data.titleFormat;
        case (N::TrackIndexRole):
            return m_data.trackIndex;
        case (Qt::DisplayRole):
            return QVariant(
                QListWidgetItem::data(Qt::DisplayRole)
                    .toString()
                    .replace("%i", QString::number(data(N::TrackIndexRole).toInt() + 1)));
        case (Qt::FontRole): {
            QFont font = qvariant_cast<QFont>(QListWidgetItem::data(Qt::FontRole));
            font.setBold(m_data.playing);
            return font;
        }
        case (N::IdRole):
            return m_data.id;

        default:
            return QListWidgetItem::data(role);
    }
}

void NPlaylistWidgetItem::setData(int role, const QVariant &value)
{
    switch (role) {
        case (N::PlayingRole):
            m_data.playing = value.toBool();
            break;
        case (N::FailedRole):
            m_data.failed = value.toBool();
            break;
        case (N::PathRole):
            m_data.path = value.toString();
            break;
        case (N::DurationRole):
            m_data.duration = value.toInt();
            break;
        case (N::CountRole):
            m_data.playbackCount = value.toInt();
            break;
        case (N::PositionRole):
            m_data.playbackPosition = value.toFloat();
            break;
        case (N::TitleFormatRole):
            m_data.titleFormat = value.toString();
            break;
        case (N::TrackIndexRole):
            m_data.trackIndex = value.toInt();
            break;
        case (N::IdRole):
            m_data.id = value.toInt();
            break;
        default:
            QListWidgetItem::setData(role, value);
            break;
    }
}

NPlaylistDataItem NPlaylistWidgetItem::dataItem() const
{
    return m_data;
}

void NPlaylistWidgetItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                        const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;
    const NPlaylistWidget *playlistWidget = qobject_cast<const NPlaylistWidget *>(opt.widget);

    if (index.data(N::FailedRole).toBool()) { // FailedRole has higher priority than PlayingRole
        QColor color = playlistWidget->failedTextColor();
        if (color.isValid()) {
            opt.palette.setColor(QPalette::HighlightedText, color);
            opt.palette.setColor(QPalette::Text, color);
        }
    } else if (index.data(N::PlayingRole).toBool()) {
        QColor color = playlistWidget->playingTextColor();
        if (color.isValid()) {
            opt.palette.setColor(QPalette::HighlightedText, color);
            opt.palette.setColor(QPalette::Text, color);
        }
    }

    // bold font set via Qt::FontRole

    if (!playlistWidget || !playlistWidget->columnsMode()) {
        QStyledItemDelegate::paint(painter, opt, index);
        return;
    }

    // columns mode: background/selection/focus as usual, without text...
    initStyleOption(&opt, index);
    QStyleOptionViewItem bg = opt;
    bg.text.clear();
    const QWidget *widget = opt.widget;
    QStyle *style = widget ? widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &bg, painter, widget);

    // ...then each column's text clipped to its header section
    QStringList columns = index.data(N::ColumnsRole).toStringList();
    QString trackIndex = QString::number(index.data(N::TrackIndexRole).toInt() + 1);
    QPalette::ColorRole role = (opt.state & QStyle::State_Selected) ? QPalette::HighlightedText
                                                                     : QPalette::Text;
    painter->save();
    painter->setFont(opt.font);
    painter->setPen(opt.palette.color(QPalette::Active, role));
    const int padding = 4;
    for (int i = 0; i < playlistWidget->columnCount() && i < columns.size(); ++i) {
        QRect cell(opt.rect.left() + playlistWidget->columnX(i), opt.rect.top(),
                   playlistWidget->columnWidth(i), opt.rect.height());
        QRect textRect = cell.adjusted(padding, 0, -padding, 0);
        if (textRect.width() <= 0) {
            continue;
        }
        QString text = QString(columns.at(i)).replace("%i", trackIndex);
        text = opt.fontMetrics.elidedText(text, Qt::ElideRight, textRect.width());
        painter->setClipRect(cell);
        painter->drawText(textRect, playlistWidget->columnAlignment(i) | Qt::AlignVCenter, text);
    }
    painter->restore();
}
