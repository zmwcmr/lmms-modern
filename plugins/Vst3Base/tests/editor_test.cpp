/*
 * editor_test.cpp - native VST3 editor smoke test
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

#include <memory>

#include <QApplication>
#include <QTextStream>
#include <QTimer>

#include "Vst3EditorWindow.h"
#include "Vst3Host.h"
#include "Vst3RemoteHostClient.h"

int main(int argc, char* argv[])
{
	QApplication application(argc, argv);
	const auto arguments = application.arguments();
	if (arguments.size() < 3)
	{
		QTextStream(stderr)
			<< "Usage: vst3-editor-test <bundle.vst3> <class-id>\n";
		return 2;
	}

	std::unique_ptr<lmms::Vst3HostBase> host;
	QString error;
	if (qEnvironmentVariableIntValue("LMMS_VST3_REMOTE_DSP") == 1)
	{
		auto remote = std::make_unique<lmms::Vst3RemoteHostClient>();
		if (!remote->load(arguments.at(1), arguments.at(2), 48000.0, 512, &error))
		{
			QTextStream(stderr) << "Remote load failed: " << error << '\n';
			return 1;
		}
		host = std::move(remote);
	}
	else
	{
		auto local = std::make_unique<lmms::Vst3Host>();
		if (!local->load(arguments.at(1), arguments.at(2), 48000.0, 512, &error))
		{
			QTextStream(stderr) << "Load failed: " << error << '\n';
			return 1;
		}
		host = std::move(local);
	}
	if (!host->hasEditor())
	{
		QTextStream(stderr) << "Plugin has no editor\n";
		return 3;
	}

	lmms::Vst3EditorWindow window(host.get(), QStringLiteral("VST3 editor test"));
	if (!window.isX11Forwarded())
	{
		window.show();
	}

	int seconds = qEnvironmentVariableIntValue("VST3_EDITOR_TEST_SECONDS");
	if (seconds <= 0)
	{
		seconds = 3;
	}
	QTimer::singleShot(seconds * 1000, &application, &QApplication::quit);
	application.exec();

	QTextStream(stdout) << "editor-open=" << host->isEditorOpen()
		<< " size=" << window.width() << 'x' << window.height() << '\n';
	return 0;
}
