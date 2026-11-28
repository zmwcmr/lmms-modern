/*
 * PluginLoadingWindow.cpp - immediate SVG loading placeholder
 *
 * Copyright (c) 2026 LMMS contributors
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include "PluginLoadingWindow.h"

#include "GuiApplication.h"
#include "MainWindow.h"

#include <QApplication>
#include <QByteArray>
#include <QGuiApplication>
#include <QLabel>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>
#include <QtSvg/QSvgWidget>

namespace lmms::gui
{

namespace
{
const char* const kLogoSvg = R"SVG(<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<svg xmlns="http://www.w3.org/2000/svg" xmlns:svg="http://www.w3.org/2000/svg" xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#" xmlns:cc="http://creativecommons.org/ns#" xmlns:dc="http://purl.org/dc/elements/1.1/" version="1.1" width="48" height="48">
	<metadata>
		<rdf:RDF>
			<cc:Work>
				<dc:title>LMMS plugin logo</dc:title>
				<dc:creator><cc:Agent><dc:title>Rebecca Noel Ati</dc:title></cc:Agent></dc:creator>
			</cc:Work>
			<cc:License rdf:about="http://creativecommons.org/publicdomain/zero/1.0/">
				<cc:permits rdf:resource="http://creativecommons.org/ns#Reproduction" />
				<cc:permits rdf:resource="http://creativecommons.org/ns#Distribution" />
				<cc:permits rdf:resource="http://creativecommons.org/ns#DerivativeWorks" />
			</cc:License>
		</rdf:RDF>
	</metadata>
	<path fill="#fff" d="M7.86719 2C3.95608 2 2 3.95608 2 7.86719V40.1328C2 44.04392 3.95608 46 7.86719 46H40.1328C44.04392 46 46 44.04392 46 40.13281V7.8672C46 3.95608 44.04392 2 40.13281 2H7.8672zM24 9l15 8.4375V35.25l-5.625 2.8125L27.75 35.25v-6.5625l5.625-2.8125V20.25L24 15.5625 14.625 20.25v5.625l5.625 2.8125V35.25l-5.625 2.8125L9 35.25V17.4375L24 9z"/>
</svg>
)SVG";

QPointer<PluginLoadingWindow> s_instance;
}

PluginLoadingWindow::PluginLoadingWindow(const QString& pluginName,
	QWidget* parent) :
	QWidget(parent)
{
	setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint
		| Qt::WindowStaysOnTopHint);
	setAttribute(Qt::WA_ShowWithoutActivating);
	setAttribute(Qt::WA_DeleteOnClose);
	setFixedSize(360, 200);
	setStyleSheet(QStringLiteral(
		"QWidget { background: #2F2F2F; color: white; }"
		"QLabel { color: white; }"));

	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(20, 18, 20, 18);
	layout->setSpacing(6);

	auto* logo = new QSvgWidget(this);
	logo->load(QByteArray(kLogoSvg));
	logo->setFixedSize(96, 96);
	layout->addWidget(logo, 0, Qt::AlignHCenter);

	auto* name = new QLabel(pluginName.isEmpty()
		? tr("Plugin") : pluginName, this);
	name->setAlignment(Qt::AlignHCenter);
	QFont nameFont = name->font();
	nameFont.setBold(true);
	name->setFont(nameFont);
	layout->addWidget(name);

	auto* status = new QLabel(tr("Loading plugin GUI..."), this);
	status->setAlignment(Qt::AlignHCenter);
	layout->addWidget(status);

	if (auto* screen = QGuiApplication::primaryScreen())
	{
		move(screen->availableGeometry().center() - rect().center());
	}

	// A native plugin window can be mapped after this window. On X11 keep the
	// placeholder above it; Wayland does not support activation/raise for
	// ordinary clients and repeated requests can kill the connection.
	if (!QGuiApplication::platformName().contains(
			QStringLiteral("wayland"), Qt::CaseInsensitive))
	{
		m_raiseTimer = new QTimer(this);
		m_raiseTimer->setInterval(100);
		connect(m_raiseTimer, &QTimer::timeout, this, [this] { raise(); });
		m_raiseTimer->start();
	}
}

PluginLoadingWindow::~PluginLoadingWindow() = default;

void PluginLoadingWindow::showFor(const QString& pluginName)
{
	if (QApplication::instance() == nullptr || gui::getGUI() == nullptr)
	{
		return;
	}
	QWidget* parent = QApplication::activeWindow();
	if (parent == nullptr)
	{
		parent = gui::getGUI()->mainWindow();
	}
	if (!s_instance.isNull())
	{
		s_instance->close();
		s_instance = nullptr;
	}
	auto* window = new PluginLoadingWindow(pluginName, parent);
	s_instance = window;
	window->show();
	if (!QGuiApplication::platformName().contains(
			QStringLiteral("wayland"), Qt::CaseInsensitive))
	{
		window->raise();
	}
}

void PluginLoadingWindow::hide()
{
	if (!s_instance.isNull())
	{
		s_instance->close();
		s_instance = nullptr;
	}
}

} // namespace lmms::gui
