/*
 * Vst3EditorWidget.cpp - native VST3 editor widget
 *
 * Copyright (c) 2026 LMMS developers
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

#include "Vst3EditorWidget.h"

#include <QCloseEvent>
#include <QFocusEvent>
#include <QGuiApplication>
#include <QLabel>
#include <QMetaObject>
#include <QResizeEvent>
#include <QVBoxLayout>

#include "Vst3Host.h"

namespace lmms
{

Vst3EditorWidget::Vst3EditorWidget(Vst3HostBase* host, QWidget* parent) :
	QWidget(parent),
	m_host(host)
{
	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	if (!isSupported())
	{
		layout->addWidget(new QLabel(
			tr("The VST3 editor requires the X11 (xcb) Qt platform."), this));
		return;
	}
	if (m_host == nullptr || !m_host->hasEditor())
	{
		layout->addWidget(new QLabel(tr("This VST3 plugin has no editor."), this));
		return;
	}

	setAttribute(Qt::WA_NativeWindow);
	setAttribute(Qt::WA_NoSystemBackground);
	setFocusPolicy(Qt::StrongFocus);

	m_host->setEditorResizeCallback([this](int width, int height) {
		QMetaObject::invokeMethod(this, [this, width, height] {
			resizeFromPlugin(width, height);
		}, Qt::QueuedConnection);
	});

	const auto windowId = static_cast<quintptr>(winId());
	QString error;
	if (!m_host->openEditor(reinterpret_cast<void*>(windowId), &error))
	{
		m_host->setEditorResizeCallback({});
		layout->addWidget(new QLabel(
			tr("Unable to open the VST3 editor: %1").arg(error), this));
		return;
	}

	m_open = true;
	int width = 0;
	int height = 0;
	if (m_host->editorSize(width, height))
	{
		setFixedSize(width, height);
	}
	m_host->setEditorContentScale(devicePixelRatioF());
}

Vst3EditorWidget::~Vst3EditorWidget()
{
	if (m_host != nullptr)
	{
		m_host->setEditorResizeCallback({});
		if (m_open)
		{
			m_host->closeEditor();
		}
	}
}

bool Vst3EditorWidget::isSupported()
{
	const auto platform = QGuiApplication::platformName();
	return platform.startsWith(QStringLiteral("xcb"), Qt::CaseInsensitive)
		|| platform.startsWith(QStringLiteral("x11"), Qt::CaseInsensitive);
}

QSize Vst3EditorWidget::sizeHint() const
{
	if (m_host != nullptr && m_open)
	{
		int width = 0;
		int height = 0;
		if (m_host->editorSize(width, height))
		{
			return QSize(width, height);
		}
	}
	return QSize(480, 360);
}

void Vst3EditorWidget::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);
	if (m_open && !m_resizing && m_host != nullptr)
	{
		m_host->resizeEditor(event->size().width(), event->size().height());
	}
}

void Vst3EditorWidget::closeEvent(QCloseEvent* event)
{
	if (m_host != nullptr && m_open)
	{
		m_host->closeEditor();
		m_open = false;
	}
	QWidget::closeEvent(event);
}

void Vst3EditorWidget::focusInEvent(QFocusEvent* event)
{
	QWidget::focusInEvent(event);
	if (m_host != nullptr)
	{
		m_host->focusEditor(true);
	}
}

void Vst3EditorWidget::focusOutEvent(QFocusEvent* event)
{
	QWidget::focusOutEvent(event);
	if (m_host != nullptr)
	{
		m_host->focusEditor(false);
	}
}

void Vst3EditorWidget::resizeFromPlugin(int width, int height)
{
	if (!m_open || width <= 0 || height <= 0)
	{
		return;
	}
	m_resizing = true;
	setFixedSize(width, height);
	m_resizing = false;
	updateGeometry();
}

} // namespace lmms
