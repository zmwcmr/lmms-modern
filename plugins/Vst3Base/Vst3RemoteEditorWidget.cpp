/*
 * Vst3RemoteEditorWidget.cpp - widget for the out-of-process editor
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

#include "Vst3RemoteEditorWidget.h"

#include <QCloseEvent>
#include <QDebug>
#include <QGuiApplication>
#include <QLabel>
#include <QResizeEvent>
#include <QVBoxLayout>

#include "Vst3EditorWidget.h"
#include "Vst3Host.h"
#include "Vst3RemoteEditorClient.h"

namespace lmms
{

Vst3RemoteEditorWidget::Vst3RemoteEditorWidget(Vst3HostBase* host,
	std::function<void(uint32_t, double)> parameterHandler, QWidget* parent) :
	QWidget(parent),
	m_host(host),
	m_parameterHandler(std::move(parameterHandler))
{
	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	if (!isSupported() || m_host == nullptr)
	{
		layout->addWidget(new QLabel(
			tr("The out-of-process VST3 editor is not available."), this));
		return;
	}

	setAttribute(Qt::WA_NativeWindow);
	setAttribute(Qt::WA_NoSystemBackground);
	const auto windowId = static_cast<quintptr>(winId());

	m_client = new Vst3RemoteEditorClient(m_host, windowId, this);
	m_client->editorResized = [this](int width, int height) {
		m_resizing = true;
		setFixedSize(width, height);
		m_resizing = false;
		updateGeometry();
	};
	m_client->editorClosed = [this] {
		if (window() != this)
		{
			window()->close();
		}
	};
	m_client->parameterChanged = [this](uint32_t id, double value) {
		if (m_parameterHandler)
		{
			m_parameterHandler(id, value);
		}
	};
	m_client->editorError = [this](const QString& error) {
		qWarning("VST3 remote editor: %s", error.toUtf8().constData());
		if (m_client == nullptr)
		{
			return;
		}
		auto* label = new QLabel(tr("VST3 editor error: %1").arg(error), this);
		label->setWordWrap(true);
		this->layout()->addWidget(label);
	};

	QString error;
	if (!m_client->start(windowTitle(), &error))
	{
		delete m_client;
		m_client = nullptr;
		layout->addWidget(new QLabel(
			tr("Unable to start the VST3 editor: %1").arg(error), this));
	}
}

Vst3RemoteEditorWidget::~Vst3RemoteEditorWidget()
{
	if (m_client != nullptr)
	{
		m_client->editorResized = {};
		m_client->editorClosed = {};
		m_client->editorError = {};
		m_client->parameterChanged = {};
		m_client->stop();
	}
}

bool Vst3RemoteEditorWidget::isSupported()
{
	return Vst3EditorWidget::isSupported()
		&& !Vst3RemoteEditorClient::editorHostExecutable().isEmpty()
		&& qEnvironmentVariableIsEmpty("LMMS_VST3_EDITOR_LOCAL");
}

bool Vst3RemoteEditorWidget::isRunning() const
{
	return m_client != nullptr && m_client->isRunning();
}

QSize Vst3RemoteEditorWidget::sizeHint() const
{
	return QSize(640, 480);
}

void Vst3RemoteEditorWidget::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);
	if (m_client != nullptr && !m_resizing)
	{
		m_client->sendResize(event->size().width(), event->size().height());
	}
}

void Vst3RemoteEditorWidget::closeEvent(QCloseEvent* event)
{
	if (m_client != nullptr)
	{
		m_client->stop();
	}
	QWidget::closeEvent(event);
}

} // namespace lmms
