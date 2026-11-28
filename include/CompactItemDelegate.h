/*
 * CompactItemDelegate.h - item delegate with a content-sized selection pill
 *
 * Copyright (c) 2026 LMMS contributors
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */

#ifndef LMMS_GUI_COMPACT_ITEM_DELEGATE_H
#define LMMS_GUI_COMPACT_ITEM_DELEGATE_H

#include <QColor>
#include <QFontMetrics>
#include <QIcon>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>

namespace lmms::gui
{

//! Draws the tree item content with a compact selection/hover pill instead of
//! filling the whole stretched column. Colours match the tree item rules in
//! data/themes/*/style.css.
class CompactItemDelegate : public QStyledItemDelegate
{
public:
	explicit CompactItemDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

	QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
	{
		const auto base = QStyledItemDelegate::sizeHint(option, index);
		return QSize(base.width(), qMax(20, QFontMetrics(option.font).height() + 4));
	}

	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
	{
		QStyleOptionViewItem opt = option;
		initStyleOption(&opt, index);

		const bool selected = opt.state & QStyle::State_Selected;
		const bool hovered = opt.state & QStyle::State_MouseOver;
		const auto metrics = QFontMetrics(opt.font);
		const int iconSize = opt.decorationSize.isValid() ? opt.decorationSize.width() : 16;
		const int margin = 4;
		const int gap = 6;

		const int availableTextWidth = qMax(0, opt.rect.width() - 2 * margin
			- (opt.icon.isNull() ? 0 : iconSize + gap));
		QString text = opt.text;
		if (metrics.horizontalAdvance(text) > availableTextWidth)
		{
			text = metrics.elidedText(text, opt.textElideMode, availableTextWidth);
		}

		const int textWidth = metrics.horizontalAdvance(text);
		const int contentWidth = (opt.icon.isNull() ? 0 : iconSize + gap) + textWidth;
		const QRect pillRect(opt.rect.left() + 2, opt.rect.top() + 1,
			qMin(opt.rect.width() - 4, contentWidth + 2 * margin), opt.rect.height() - 2);

		painter->save();
		painter->setRenderHint(QPainter::Antialiasing);
		if (selected || hovered)
		{
			painter->setPen(Qt::NoPen);
			painter->setBrush(selected ? QColor(15, 92, 44) : QColor(60, 68, 78));
			painter->drawRoundedRect(pillRect, 3, 3);
		}
		if (selected && opt.widget && opt.widget->hasFocus())
		{
			painter->setBrush(Qt::NoBrush);
			painter->setPen(QPen(QColor(11, 213, 86), 1));
			painter->drawRoundedRect(pillRect.adjusted(0, 0, -1, -1), 3, 3);
		}

		int textLeft = pillRect.left() + margin;
		if (!opt.icon.isNull())
		{
			const QRect iconRect(textLeft, opt.rect.center().y() - iconSize / 2, iconSize, iconSize);
			opt.icon.paint(painter, iconRect, Qt::AlignCenter,
				selected ? QIcon::Selected : QIcon::Normal);
			textLeft = iconRect.right() + gap + 1;
		}

		const QRect textRect(textLeft, opt.rect.top(),
			qMax(0, pillRect.right() - textLeft - margin), opt.rect.height());
		painter->setFont(opt.font);
		painter->setPen(opt.palette.color(selected ? QPalette::HighlightedText : QPalette::Text));
		painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);
		painter->restore();
	}
};

} // namespace lmms::gui

#endif // LMMS_GUI_COMPACT_ITEM_DELEGATE_H
