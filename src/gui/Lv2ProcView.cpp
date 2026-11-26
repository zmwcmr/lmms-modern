/*
 * Lv2ProcView.cpp - base class for Lv2 plugin views
 *
 * Copyright (c) 2018-2026 Johannes Lorenz <jlsf2013$users.sourceforge.net, $=@>
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

#include "Lv2ProcView.h"

#ifdef LMMS_HAVE_LV2

#include <algorithm>
#include <cmath>

#include <QByteArray>

#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QPushButton>
#include <QSignalBlocker>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include <lilv/lilv.h>
#include <lv2/atom/atom.h>
#include <lv2/instance-access/instance-access.h>
#include <lv2/options/options.h>
#include <lv2/parameters/parameters.h>
#include <lv2/options/options.h>
#include <lv2/parameters/parameters.h>
#include <lv2/port-props/port-props.h>
#include <lv2/ui/ui.h>

#ifdef LMMS_HAVE_SUIL
#include <suil/suil.h>
#endif

#include "AudioEngine.h"
#include "ControlLayout.h"
#include "ConfigManager.h"
#include "Controls.h"
#include "embed.h"
#include "Engine.h"
#include "FontHelper.h"
#include "GuiApplication.h"
#include "lmms_math.h"
#include "Lv2Manager.h"
#include "Lv2Ports.h"
#include "Lv2Proc.h"
#include "MainWindow.h"
#include "SubWindow.h"

#if defined(LMMS_BUILD_LINUX)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif


namespace lmms::gui
{

namespace
{
// UIs occasionally report bogus sizes (0, negative or near UINT_MAX) through
// ui:resize. Clamp them before they reach the plugin, which may assert.
int clampUiDimension(int value, int maximum)
{
	return std::clamp(value, 1, std::max(maximum, 1));
}

// Temporarily switch the working directory for plugin calls that resolve
// assets relative to the CWD (Zyn Fusion's qml/ and libzest.so).
class CurrentDirGuard
{
public:
	explicit CurrentDirGuard(const QString& dir)
		: m_old(QDir::currentPath())
	{
		if (!dir.isEmpty() && dir != m_old)
		{
			m_changed = QDir::setCurrent(dir);
		}
	}
	~CurrentDirGuard()
	{
		if (m_changed) { QDir::setCurrent(m_old); }
	}
private:
	QString m_old;
	bool m_changed = false;
};
}

#if defined(LMMS_BUILD_LINUX)
namespace
{
XErrorHandler g_previousXErrorHandler = nullptr;

// Some plugin UIs (Zyn Fusion) look for qml/, font/ and schema/ relative to
// the current directory. Walk up from the bundle until the asset root is
// found; return an empty string for ordinary bundles.
QString findUiDataRoot(const QString& bundleDir)
{
	// Bundle paths are often symlinks into an install tree; resolve them
	// before walking up to the asset root.
	const QString canonical = QFileInfo(bundleDir).canonicalFilePath();
	QDir dir(canonical.isEmpty() ? bundleDir : canonical);
	for (int depth = 0; depth < 4; ++depth)
	{
		if (QFile::exists(dir.filePath(QStringLiteral("qml/MainWindow.qml"))))
		{
			return dir.absolutePath();
		}
		if (!dir.cdUp()) { break; }
	}
	return {};
}

int lmmsXErrorHandler(Display* display, XErrorEvent* event)
{
	if (event->error_code == BadWindow || event->error_code == BadDrawable)
	{
		qWarning() << "Ignoring stale X11 window operation on resource"
			<< static_cast<qulonglong>(event->resourceid)
			<< "(error" << event->error_code << ")";
		return 0;
	}
	return g_previousXErrorHandler
		? g_previousXErrorHandler(display, event)
		: 0;
}


}
#endif


Lv2ProcView::Lv2ProcView(QWidget* meAsWidget, Lv2Proc *proc) :
	ModelGroupView(meAsWidget, proc),
	m_proc(proc),
	m_pluginWidget(meAsWidget),
	m_helpWindowEventFilter(this)
{
	// Destroy the native UI before the plugin instance is shut down.
	if (m_proc)
	{
		m_proc->setViewDetacher([this] {
			destroyUi();
			m_proc = nullptr;
		});
	}

	auto btnBox = new QHBoxLayout();
	if (/* DISABLES CODE */ (false))
	{
		m_reloadPluginButton = new QPushButton(QObject::tr("Reload Plugin"),
			meAsWidget);
		m_reloadPluginButton->setObjectName("btn");
		btnBox->addWidget(m_reloadPluginButton, 0);
	}

	if (proc && !proc->presets().empty())
	{
		auto* presetLabel = new QLabel(QObject::tr("Preset:"), meAsWidget);
		m_presetCombo = new QComboBox(meAsWidget);
		m_presetCombo->setSizeAdjustPolicy(
			QComboBox::AdjustToMinimumContentsLengthWithIcon);
		m_presetCombo->setMinimumContentsLength(24);
		m_presetCombo->setSizePolicy(QSizePolicy::Expanding,
			QSizePolicy::Fixed);
		m_presetCombo->setToolTip(QObject::tr(
			"Load a factory preset from the plugin bundle"));
		m_presetCombo->addItem(QObject::tr("Choose preset..."));
		for (const Lv2Proc::Preset& preset : proc->presets())
		{
			m_presetCombo->addItem(preset.label);
		}
		btnBox->addWidget(presetLabel);
		btnBox->addWidget(m_presetCombo, 1);
		QObject::connect(m_presetCombo,
			QOverload<int>::of(&QComboBox::currentIndexChanged),
			meAsWidget,
			[this](int index)
			{
				if (index <= 0 || !m_proc)
				{
					return;
				}
				if (!m_proc->loadPreset(static_cast<std::size_t>(index - 1)))
				{
					qWarning() << "Could not load LV2 preset" << index;
				}
			});
	}

	if (hasPluginUi())
	{
		m_toggleUIButton = new QPushButton(QObject::tr("Show GUI"),
			meAsWidget);
		m_toggleUIButton->setObjectName("btn");
		m_toggleUIButton->setCheckable(true);
		m_toggleUIButton->setChecked(false);
		m_toggleUIButton->setFont(adjustedToPixelSize(m_toggleUIButton->font(), SMALL_FONT_SIZE));
		btnBox->addWidget(m_toggleUIButton, 0);
	}
	btnBox->addStretch(1);

	meAsWidget->setAcceptDrops(true);

	// note: the lifetime of C++ objects ends after the top expression in the
	// expression syntax tree, so the AutoLilvNode gets freed after the function
	// has been called
	AutoLilvNodes props(lilv_plugin_get_value(proc->getPlugin(),
				uri(LILV_NS_RDFS "comment").get()));
	LILV_FOREACH(nodes, itr, props.get())
	{
		const LilvNode* node = lilv_nodes_get(props.get(), itr);
		auto infoLabel = new QLabel(QString(lilv_node_as_string(node)).trimmed() + "\n");
		infoLabel->setWordWrap(true);
		infoLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);

		m_helpButton = new QPushButton(QObject::tr("Help"));
		m_helpButton->setObjectName("btn");
		m_helpButton->setCheckable(true);
		btnBox->addWidget(m_helpButton);

		m_helpWindow = getGUI()->mainWindow()->addWindowedWidget(infoLabel);
		m_helpWindow->setSizePolicy(QSizePolicy::Expanding,
									QSizePolicy::Expanding);
		m_helpWindow->installEventFilter(&m_helpWindowEventFilter);
		m_helpWindow->setAttribute(Qt::WA_DeleteOnClose, false);
		m_helpWindow->hide();

		break;
	}

	if (m_reloadPluginButton || m_toggleUIButton || m_helpButton
		|| m_presetCombo)
	{
		auto* buttonBar = new QWidget(meAsWidget);
		// ControlLayout sorts items by object name. This key sorts after the
		// search bar but before regular controls, and requests a full row.
		buttonBar->setObjectName("!lv2ButtonBar");
		buttonBar->setProperty("lmmsControlLayoutFullWidth", true);
		buttonBar->setLayout(btnBox);
		if (auto* layout = meAsWidget->layout())
		{
			layout->addWidget(buttonBar);
		}
		else
		{
			auto* vlayout = new QVBoxLayout(meAsWidget);
			vlayout->addWidget(buttonBar);
		}
	}
	else { delete btnBox; }

	class SetupTheWidget : public Lv2Ports::ConstVisitor
	{
	public:
		QWidget* m_parent; // input
		const LilvNode* m_commentUri; // input
		Control* m_control = nullptr; // output
		void visit(const Lv2Ports::Control& port) override
		{
			if (port.m_flow == Lv2Ports::Flow::Input)
			{
				using PortVis = Lv2Ports::Vis;

				switch (port.m_vis)
				{
					case PortVis::Generic:
						m_control = new KnobControl(port.name(), m_parent);
						break;
					case PortVis::Integer:
					{
						sample_rate_t sr = Engine::audioEngine()->outputSampleRate();
						auto pMin = port.min(sr);
						auto pMax = port.max(sr);
						int numDigits = std::max(numDigitsAsInt(pMin), numDigitsAsInt(pMax));
						m_control = new LcdControl(numDigits, m_parent);
						break;
					}
					case PortVis::Enumeration:
						m_control = new ComboControl(m_parent);
						break;
					case PortVis::Toggled:
						m_control = new CheckControl(m_parent);
						break;
				}
				m_control->setText(port.name());

				AutoLilvNodes props(lilv_port_get_value(
					port.m_plugin, port.m_port, m_commentUri));
				LILV_FOREACH(nodes, itr, props.get())
				{
					const LilvNode* nod = lilv_nodes_get(props.get(), itr);
					m_control->topWidget()->setToolTip(lilv_node_as_string(nod));
					break;
				}
			}
		}
	};

	AutoLilvNode commentUri = uri(LILV_NS_RDFS "comment");
	proc->foreach_port(
		[this, &commentUri, &meAsWidget](const Lv2Ports::PortBase* port)
		{
			if(!lilv_port_has_property(port->m_plugin, port->m_port,
										uri(LV2_PORT_PROPS__notOnGUI).get()))
			{
				SetupTheWidget setup;
				setup.m_parent = meAsWidget;
				setup.m_commentUri = commentUri.get();
				port->accept(setup);

				if (setup.m_control)
				{
					addControl(meAsWidget, setup.m_control,
						lilv_node_as_string(lilv_port_get_symbol(
							port->m_plugin, port->m_port)),
						port->name().toUtf8().data(),
						false);
				}
			}
		});
}




Lv2ProcView::~Lv2ProcView()
{
	if (m_proc)
	{
		m_proc->setViewDetacher(nullptr);
		m_proc = nullptr;
	}
	destroyUi();
	closeHelpWindow();
}




bool Lv2ProcView::hasPluginUi() const
{
#ifdef LMMS_HAVE_SUIL
	if (!m_proc || !Lv2Manager::wantUi())
	{
		return false;
	}

	if (Lv2Manager::pluginUiIsUnstable(m_proc->pluginUri())
		&& !qEnvironmentVariableIsSet("LMMS_ENABLE_UNSTABLE_LV2_UI"))
	{
		return false;
	}

	const QString platform = QGuiApplication::platformName();
	if (platform != "xcb" && platform != "wayland")
	{
		return false;
	}
	if (platform == "wayland" && !qEnvironmentVariableIsSet("DISPLAY"))
	{
		// Wayland without XWayland cannot host X11 plugin UIs.
		return false;
	}

	LilvUIs* uis = m_proc->getUis();
	if (!uis)
	{
		return false;
	}

	AutoLilvNode hostType = uri(LV2_UI__X11UI);
	LILV_FOREACH(uis, it, uis)
	{
		const LilvUI* ui = lilv_uis_get(uis, it);
		const LilvNode* uiType = nullptr;
		if (lilv_ui_is_supported(ui, suil_ui_supported, hostType.get(), &uiType))
		{
			return true;
		}
	}
#endif // LMMS_HAVE_SUIL
	return false;
}




bool Lv2ProcView::useFloatingWindow() const
{
	const QString mode = ConfigManager::inst()->lv2UiEmbedMethod();
	return mode == "floating"
		|| QGuiApplication::platformName() != "xcb";
}




void Lv2ProcView::createUi()
{
#ifdef LMMS_HAVE_SUIL
	if (m_uiInstance || !m_proc || !hasPluginUi())
	{
		return;
	}

	LilvUIs* uis = m_proc->getUis();
	AutoLilvNode hostType = uri(LV2_UI__X11UI);
	const LilvUI* ui = nullptr;
	const LilvNode* uiType = nullptr;
	LILV_FOREACH(uis, it, uis)
	{
		const LilvUI* candidate = lilv_uis_get(uis, it);
		const LilvNode* candidateType = nullptr;
		if (lilv_ui_is_supported(candidate, suil_ui_supported, hostType.get(), &candidateType))
		{
			ui = candidate;
			uiType = candidateType;
			break;
		}
	}

	if (!ui)
	{
		return;
	}

	Lv2Manager* mgr = Engine::getLv2Manager();
	AutoLilvNode fixedSize = uri(LV2_UI__fixedSize);
	AutoLilvNodes fixedSizeNodes = mgr->findNodes(
		lilv_ui_get_uri(ui), fixedSize.get(), nullptr);
	m_uiResizable = !fixedSizeNodes
		|| lilv_nodes_size(fixedSizeNodes.get()) == 0;

#if defined(LMMS_BUILD_LINUX)
	if (g_previousXErrorHandler == nullptr)
	{
		g_previousXErrorHandler = XSetErrorHandler(lmmsXErrorHandler);
	}
#endif

	WId parentId = 0;
	if (!useFloatingWindow())
	{
		m_uiHostWidget = new QWidget;
		m_uiHostWidget->setAttribute(Qt::WA_NativeWindow);
		m_uiHostWidget->setAttribute(Qt::WA_OpaquePaintEvent);
		m_uiHostWidget->setMinimumSize(320, 240);
		m_uiHostWidget->resize(900, 700);

		m_uiWindow = getGUI()->mainWindow()->addWindowedWidget(m_uiHostWidget);
		if (!m_uiWindow)
		{
			destroyUi();
			return;
		}
		m_uiWindow->setAttribute(Qt::WA_DeleteOnClose, false);
		m_uiWindow->setWindowTitle(qStringFromPluginNode(m_proc->getPlugin(),
			lilv_plugin_get_name));
		m_uiWindow->show();
		m_uiHostWidget->show();
		QApplication::processEvents();

		parentId = m_uiHostWidget->winId();
	}
	else
	{
#if defined(LMMS_BUILD_LINUX)
		// Create a standalone X11 window (Wayland, or floating mode).
		Display* dpy = XOpenDisplay(nullptr);
		if (!dpy)
		{
			qWarning() << "Could not open an X11 display for the LV2 UI";
			return;
		}

		const int screen = DefaultScreen(dpy);
		m_x11Window = XCreateSimpleWindow(dpy, RootWindow(dpy, screen),
			100, 100, 900, 700, 1,
			BlackPixel(dpy, screen), WhitePixel(dpy, screen));
		if (!m_x11Window)
		{
			qWarning() << "Could not create an X11 window for the LV2 UI";
			XCloseDisplay(dpy);
			return;
		}

		const QByteArray title = qStringFromPluginNode(m_proc->getPlugin(),
			lilv_plugin_get_name).toUtf8();
		XStoreName(dpy, m_x11Window, title.constData());

		Atom wmDelete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
		m_x11WmDelete = wmDelete;
		XSetWMProtocols(dpy, m_x11Window, &wmDelete, 1);
		XSelectInput(dpy, m_x11Window, StructureNotifyMask);

		XMapWindow(dpy, m_x11Window);
		XFlush(dpy);

		m_x11Display = dpy;
		parentId = static_cast<WId>(m_x11Window);
#else
		return;
#endif
	}

	if (parentId == 0)
	{
		qWarning() << "Could not create a native window for the LV2 UI";
		destroyUi();
		return;
	}

	m_uiHost = suil_host_new(
		[](void* handle, uint32_t port, uint32_t size, uint32_t protocol, const void* buffer)
		{
			static_cast<Lv2ProcView*>(handle)->writeToPlugin(port, size, protocol, buffer);
		},
		[](void* handle, const char* symbol) -> uint32_t
		{
			return static_cast<Lv2ProcView*>(handle)->indexOfPort(symbol);
		},
		nullptr, nullptr);
	if (!m_uiHost)
	{
		qWarning() << "Could not create the LV2 UI host";
		destroyUi();
		return;
	}

	suil_host_set_touch_func(m_uiHost,
		[](void* handle, uint32_t port, bool grabbed)
		{
			static_cast<Lv2ProcView*>(handle)->touchPort(port, grabbed);
		});

	m_parentFeature = LV2_Feature{
		LV2_UI__parent,
		reinterpret_cast<void*>(static_cast<uintptr_t>(parentId))
	};
	m_instanceFeature = LV2_Feature{
		LV2_INSTANCE_ACCESS_URI,
		const_cast<void*>(lilv_instance_get_handle(
			m_proc->getInstanceForInstanceFeatureOnly()))
	};
	m_dataFeature = LV2_Feature{
		LV2_DATA_ACCESS_URI,
		m_proc->extdataFeature()
	};
	m_resize.handle = this;
	m_resize.ui_resize = [](LV2UI_Handle handle, int width, int height) -> int
	{
		return static_cast<Lv2ProcView*>(handle)->resizeUiRequest(width, height);
	};
	m_resizeFeature = LV2_Feature{LV2_UI__resize, &m_resize};

	QWidget* const scaleWidget = m_uiHostWidget
		? static_cast<QWidget*>(m_uiHostWidget) : m_pluginWidget;
	// The floating UI is a raw X11 window managed outside Qt's Wayland
	// surfaces. Qt's device pixel ratio is rounded up (often to 2.0) for
	// fractional desktops, which would make the plugin render at double size
	// and push its lower part below the screen. Use 1.0 so the plugin's
	// natural pixel size matches VST2 editors; the environment override can
	// still request a different scale.
	qreal scale = m_uiHostWidget
		? (scaleWidget ? scaleWidget->devicePixelRatioF() : 1.0) : 1.0;
	if (const QByteArray override = qgetenv("LMMS_LV2_UI_SCALE");
		!override.isEmpty())
	{
		bool ok = false;
		const double value = override.toDouble(&ok);
		if (ok && value > 0.0) { scale = value; }
	}
	m_uiScale = static_cast<float>(scale);
	m_uiSampleRate = static_cast<float>(
		Engine::audioEngine()->outputSampleRate());
	const LV2_URID floatType = mgr->uridMap().map(LV2_ATOM__Float);
	m_uiOptions[0] = LV2_Options_Option{
		LV2_OPTIONS_INSTANCE, 0,
		mgr->uridMap().map(LV2_UI__scaleFactor),
		sizeof(float), floatType, &m_uiScale
	};
	m_uiOptions[1] = LV2_Options_Option{
		LV2_OPTIONS_INSTANCE, 0,
		mgr->uridMap().map(LV2_PARAMETERS__sampleRate),
		sizeof(float), floatType, &m_uiSampleRate
	};
	m_uiOptions[2] = LV2_Options_Option{
		LV2_OPTIONS_INSTANCE, 0, 0, 0, 0, nullptr
	};
	m_uiOptionsFeature = LV2_Feature{LV2_OPTIONS__options, m_uiOptions};

	const LV2_Feature* features[] = {
		&m_parentFeature,
		&m_instanceFeature,
		&m_dataFeature,
		&m_resizeFeature,
		&m_uiOptionsFeature,
		m_proc->mapFeature(),
		m_proc->unmapFeature(),
		nullptr
	};

	AutoLilvPtr<char> bundlePath(lilv_file_uri_parse(
		lilv_node_as_uri(lilv_ui_get_bundle_uri(ui)), nullptr));
	AutoLilvPtr<char> binaryPath(lilv_file_uri_parse(
		lilv_node_as_uri(lilv_ui_get_binary_uri(ui)), nullptr));

	// Zyn Fusion loads qml/ and libzest.so relative to the working directory.
	// Switch to its asset root while the UI is created and updated.
	const QString dataRoot = findUiDataRoot(
		QString::fromLocal8Bit(bundlePath.get()));
	m_uiDataRoot = dataRoot;
	CurrentDirGuard cwdGuard(m_uiDataRoot);

	m_uiInstance = suil_instance_new(
		m_uiHost,
		this,
		LV2_UI__X11UI,
		m_proc->pluginUri(),
		lilv_node_as_uri(lilv_ui_get_uri(ui)),
		uiType ? lilv_node_as_uri(uiType) : LV2_UI__X11UI,
		bundlePath.get(),
		binaryPath.get(),
		features);

	if (!m_uiInstance)
	{
		qWarning() << "Could not instantiate the LV2 UI for"
			<< m_proc->pluginUri();
		destroyUi();
		return;
	}

	// DrumGizmo and other UIs request their natural size from inside
	// suil_instance_new(), before m_uiInstance existed. Apply that request now;
	// otherwise the UI stays at the 1x1 size of its initial X11 window.
	if (m_pendingUiWidth > 0 && m_pendingUiHeight > 0)
	{
		const int width = m_pendingUiWidth;
		const int height = m_pendingUiHeight;
		m_pendingUiWidth = 0;
		m_pendingUiHeight = 0;
		applyUiResize(width, height);
	}

	m_uiShow = static_cast<const LV2UI_Show_Interface*>(
		suil_instance_extension_data(m_uiInstance, LV2_UI__showInterface));
	if (m_uiShow && m_uiShow->show)
	{
		m_uiShow->show(suil_instance_get_handle(m_uiInstance));
	}

	m_proc->connectToPluginEvents();

	resizeUiToPlugin();
	QTimer::singleShot(200, m_pluginWidget, [this] { resizeUiToPlugin(); });
	QTimer::singleShot(1000, m_pluginWidget, [this] { resizeUiToPlugin(); });

	m_uiTimer = new QTimer(m_pluginWidget);
	QObject::connect(m_uiTimer, &QTimer::timeout, m_pluginWidget, [this] {
		updateUi();
	});
	m_uiTimer->start(33);
	m_uiVisible = true;
#endif // LMMS_HAVE_SUIL
}




void Lv2ProcView::destroyUi()
{
#ifdef LMMS_HAVE_SUIL
	CurrentDirGuard cwdGuard(m_uiDataRoot);
	if (m_proc)
	{
		m_proc->setUiActive(false);
	}
	m_uiVisible = false;

	if (m_uiShow && m_uiShow->hide && m_uiInstance)
	{
		m_uiShow->hide(suil_instance_get_handle(m_uiInstance));
	}
	m_uiShow = nullptr;
	m_uiResizable = true;
	m_pendingUiWidth = 0;
	m_pendingUiHeight = 0;
	m_lastUiWidth = 0;
	m_lastUiHeight = 0;

	if (m_uiTimer)
	{
		m_uiTimer->stop();
		delete m_uiTimer;
		m_uiTimer = nullptr;
	}
	if (m_uiInstance)
	{
		suil_instance_free(m_uiInstance);
		m_uiInstance = nullptr;
	}
	if (m_uiHost)
	{
		suil_host_free(m_uiHost);
		m_uiHost = nullptr;
	}
#if defined(LMMS_BUILD_LINUX)
	if (m_x11Window && m_x11Display)
	{
		Display* dpy = static_cast<Display*>(m_x11Display);
		XDestroyWindow(dpy, m_x11Window);
		XCloseDisplay(dpy);
		m_x11Window = 0;
		m_x11Display = nullptr;
	}
	m_x11WmDelete = 0;
#endif
	if (m_uiWindow)
	{
		m_uiWindow->hide();
		m_uiWindow->deleteLater();
		m_uiWindow = nullptr;
		m_uiHostWidget = nullptr;
	}
	if (m_uiHostWidget)
	{
		delete m_uiHostWidget;
		m_uiHostWidget = nullptr;
	}
	updateToggleButton();
#endif // LMMS_HAVE_SUIL
}




void Lv2ProcView::resizeUiToPlugin()
{
#ifdef LMMS_HAVE_SUIL
	if (!m_uiInstance)
	{
		return;
	}

#if defined(LMMS_BUILD_LINUX)
	if (m_x11Window && m_x11Display)
	{
		Display* dpy = static_cast<Display*>(m_x11Display);
		XWindowAttributes attr{};
		const auto widget = reinterpret_cast<uintptr_t>(
			suil_instance_get_widget(m_uiInstance));
		if (widget
			&& XGetWindowAttributes(dpy, static_cast<::Window>(widget), &attr)
			&& attr.width > 0 && attr.height > 0)
		{
			int widgetWidth = attr.width;
			int widgetHeight = attr.height;
			// Some UIs are still 1x1 until the host applies the size they
			// requested from inside suil_instance_new().
			if ((widgetWidth <= 1 || widgetHeight <= 1)
				&& m_lastUiWidth > 1 && m_lastUiHeight > 1)
			{
				widgetWidth = m_lastUiWidth;
				widgetHeight = m_lastUiHeight;
				resizePluginChild(widgetWidth, widgetHeight);
			}
			// Keep the outer window on screen even if the plugin asks for a
			// larger size. Resizable UIs are told to use the clamped size.
			const int screen = DefaultScreen(dpy);
			const int maxWidth = std::max(DisplayWidth(dpy, screen) - 80, 320);
			const int maxHeight = std::max(DisplayHeight(dpy, screen) - 120, 240);
			const int windowWidth = std::min(widgetWidth, maxWidth);
			const int windowHeight = std::min(widgetHeight, maxHeight);
			XResizeWindow(dpy, m_x11Window, windowWidth, windowHeight);
			if (m_uiResizable
				&& (windowWidth != widgetWidth || windowHeight != widgetHeight))
			{
				resizePluginChild(windowWidth, windowHeight);
			}
			m_lastUiWidth = windowWidth;
			m_lastUiHeight = windowHeight;

			if (!m_uiResizable)
			{
				XSizeHints hints{};
				hints.flags = PMinSize | PMaxSize;
				hints.min_width = hints.max_width = windowWidth;
				hints.min_height = hints.max_height = windowHeight;
				XSetNormalHints(dpy, m_x11Window, &hints);
			}
			XFlush(dpy);
		}
		return;
	}

	if (m_uiHostWidget)
	{
		Display* dpy = XOpenDisplay(nullptr);
		if (!dpy)
		{
			return;
		}

		XWindowAttributes attr{};
		const auto widget = reinterpret_cast<uintptr_t>(
			suil_instance_get_widget(m_uiInstance));
		if (widget
			&& XGetWindowAttributes(dpy, static_cast<::Window>(widget), &attr)
			&& attr.width > 0 && attr.height > 0)
		{
			int widgetWidth = attr.width;
			int widgetHeight = attr.height;
			if ((widgetWidth <= 1 || widgetHeight <= 1)
				&& m_lastUiWidth > 1 && m_lastUiHeight > 1)
			{
				widgetWidth = m_lastUiWidth;
				widgetHeight = m_lastUiHeight;
				resizePluginChild(widgetWidth, widgetHeight);
			}
			const qreal dpr = m_uiHostWidget->devicePixelRatioF();
			const int width = std::max(static_cast<int>(std::lround(widgetWidth / dpr)), 320);
			const int height = std::max(static_cast<int>(std::lround(widgetHeight / dpr)), 240);
			m_lastUiWidth = widgetWidth;
			m_lastUiHeight = widgetHeight;
			if (m_uiResizable)
			{
				m_uiHostWidget->setMinimumSize(width, height);
				m_uiHostWidget->resize(width, height);
			}
			else
			{
				m_uiHostWidget->setFixedSize(width, height);
			}
			resizeUiWindow(width, height);
			clampUiWindowToWorkspace();
		}
		XCloseDisplay(dpy);
	}
#endif // LMMS_BUILD_LINUX
#endif // LMMS_HAVE_SUIL
}




void Lv2ProcView::resizeUiWindow(int width, int height)
{
	if (!m_uiWindow)
	{
		return;
	}

	// QMdiSubWindow::resize() sets the whole subwindow, including its title
	// bar and frame. Add that decoration so the child gets the full plugin
	// size instead of losing its bottom part below the title bar.
	if (auto* subWindow = qobject_cast<SubWindow*>(m_uiWindow);
		subWindow && !subWindow->isDetached())
	{
		const int frame = subWindow->frameWidth();
		width += 2 * frame;
		height += subWindow->titleBarHeight() + frame;
	}

	m_uiWindow->resize(width, height);
}




void Lv2ProcView::clampUiWindowToWorkspace()
{
	if (!m_uiWindow)
	{
		return;
	}

	if (auto* subWindow = qobject_cast<SubWindow*>(m_uiWindow);
		subWindow && subWindow->isDetached())
	{
		return;
	}

	QMdiArea* area = m_uiWindow->mdiArea();
	if (!area)
	{
		return;
	}

	const QRect viewport = area->viewport()->rect();
	QPoint position = m_uiWindow->pos();
	position.setX(std::clamp(position.x(), 0,
		std::max(0, viewport.width() - m_uiWindow->width())));
	position.setY(std::clamp(position.y(), 0,
		std::max(0, viewport.height() - m_uiWindow->height())));
	m_uiWindow->move(position);
}




int Lv2ProcView::resizeUiRequest(int width, int height)
{
	if (width <= 0 || height <= 0)
	{
		return 1;
	}

	// Some UIs request a size from inside suil_instance_new(). Remember it and
	// apply it once the instance exists; the initial X11 window is only 1x1.
	if (!m_uiInstance)
	{
		m_pendingUiWidth = width;
		m_pendingUiHeight = height;
		return 0;
	}

	// Ignore the notification a UI sends after the host already applied its
	// requested size; otherwise host and UI keep resizing each other.
	if (width == m_lastUiWidth && height == m_lastUiHeight)
	{
		return 0;
	}

	if (QThread::currentThread() != m_pluginWidget->thread())
	{
		QMetaObject::invokeMethod(m_pluginWidget,
			[this, width, height] { applyUiResize(width, height); },
			Qt::QueuedConnection);
		return 0;
	}

	applyUiResize(width, height);
	return 0;
}




void Lv2ProcView::applyUiResize(int width, int height)
{
#if defined(LMMS_BUILD_LINUX)
	if (m_x11Window && m_x11Display)
	{
		Display* dpy = static_cast<Display*>(m_x11Display);
		const int screen = DefaultScreen(dpy);
		width = clampUiDimension(width, DisplayWidth(dpy, screen) - 80);
		height = clampUiDimension(height, DisplayHeight(dpy, screen) - 120);
		XResizeWindow(dpy, m_x11Window, static_cast<unsigned>(width),
			static_cast<unsigned>(height));
		m_lastUiWidth = width;
		m_lastUiHeight = height;
		XFlush(dpy);
		return;
	}
#endif

	width = clampUiDimension(width, 16384);
	height = clampUiDimension(height, 16384);

	if (m_uiHostWidget)
	{
		m_lastUiWidth = width;
		m_lastUiHeight = height;
		if (m_uiResizable)
		{
			m_uiHostWidget->setMinimumSize(width, height);
			m_uiHostWidget->resize(width, height);
		}
		else
		{
			m_uiHostWidget->setFixedSize(width, height);
		}
		resizeUiWindow(width, height);
		clampUiWindowToWorkspace();
	}
}




#if defined(LMMS_BUILD_LINUX) && defined(LMMS_HAVE_SUIL)
void Lv2ProcView::processX11Events()
{
	if (!m_x11Display || !m_x11Window)
	{
		return;
	}
	Display* const dpy = static_cast<Display*>(m_x11Display);
	const Atom wmProtocols = XInternAtom(dpy, "WM_PROTOCOLS", False);

	while (m_x11Window && XPending(dpy))
	{
		XEvent event;
		XNextEvent(dpy, &event);

		if (event.type == ClientMessage
			&& event.xclient.window == m_x11Window
			&& event.xclient.message_type == wmProtocols
			&& static_cast<Atom>(event.xclient.data.l[0]) == m_x11WmDelete)
		{
			// The window manager asked to close the floating UI. Hide it
			// instead of destroying the suil instance so it can be reopened.
			setUiVisible(false);
		}
		else if (event.type == ConfigureNotify
			&& event.xconfigure.window == m_x11Window)
		{
			const int width = event.xconfigure.width;
			const int height = event.xconfigure.height;
			if (width <= 0 || height <= 0
				|| (width == m_lastUiWidth && height == m_lastUiHeight))
			{
				continue;
			}
			if (m_uiResizable)
			{
				resizePluginChild(width, height);
			}
			else if (m_lastUiWidth > 0 && m_lastUiHeight > 0)
			{
				XResizeWindow(dpy, m_x11Window,
					static_cast<unsigned>(m_lastUiWidth),
					static_cast<unsigned>(m_lastUiHeight));
			}
		}
		else if (event.type == DestroyNotify
			&& event.xdestroywindow.window == m_x11Window)
		{
			m_x11Window = 0;
			QTimer::singleShot(0, m_pluginWidget, [this] { destroyUi(); });
			return;
		}
	}
	XFlush(dpy);
}


void Lv2ProcView::resizePluginChild(int width, int height)
{
	if (!m_uiInstance)
	{
		return;
	}
	width = clampUiDimension(width, 16384);
	height = clampUiDimension(height, 16384);
	bool ownDisplay = false;
	Display* dpy = static_cast<Display*>(m_x11Display);
	if (!dpy)
	{
		dpy = XOpenDisplay(nullptr);
		ownDisplay = true;
	}
	if (!dpy)
	{
		return;
	}
	const auto widget = reinterpret_cast<::Window>(
		suil_instance_get_widget(m_uiInstance));
	if (widget && widget != m_x11Window)
	{
		XResizeWindow(dpy, widget, static_cast<unsigned>(width),
			static_cast<unsigned>(height));
		XFlush(dpy);
	}
	m_lastUiWidth = width;
	m_lastUiHeight = height;
	if (ownDisplay) { XCloseDisplay(dpy); }
}
#endif


void Lv2ProcView::touchPort(uint32_t portIndex, bool grabbed)
{
	Q_UNUSED(portIndex);
	Q_UNUSED(grabbed);
}




void Lv2ProcView::updateUi()
{
#ifdef LMMS_HAVE_SUIL
	if (!m_uiInstance || !m_proc)
	{
		return;
	}

	CurrentDirGuard cwdGuard(m_uiDataRoot);

#if defined(LMMS_BUILD_LINUX)
	if (m_uiHostWidget && m_uiResizable)
	{
		const qreal dpr = m_uiHostWidget->devicePixelRatioF();
		const int width = clampUiDimension(
			static_cast<int>(std::lround(m_uiHostWidget->width() * dpr)), 16384);
		const int height = clampUiDimension(
			static_cast<int>(std::lround(m_uiHostWidget->height() * dpr)), 16384);
		if (width != m_lastUiWidth || height != m_lastUiHeight)
		{
			resizePluginChild(width, height);
		}
	}
	processX11Events();
#endif

	for (auto& event : m_proc->takeUiEvents())
	{
		suil_instance_port_event(m_uiInstance, event.index,
			static_cast<uint32_t>(event.data.size()), event.protocol,
			event.data.empty() ? nullptr : event.data.data());
	}

	for (std::size_t i = 0; i < m_proc->portNum(); ++i)
	{
		const auto* port = m_proc->port(i);
		if (!port || port->m_type != Lv2Ports::Type::Control)
		{
			continue;
		}

		const auto* ctrl = Lv2Ports::dcast<Lv2Ports::Control>(port);
		if (!ctrl)
		{
			continue;
		}

		float value = ctrl->m_val;
		suil_instance_port_event(m_uiInstance, static_cast<uint32_t>(i),
			sizeof(float), 0, &value);
	}

	const auto* idle = static_cast<const LV2UI_Idle_Interface*>(
		suil_instance_extension_data(m_uiInstance, LV2_UI__idleInterface));
	if (idle && idle->idle)
	{
		if (idle->idle(suil_instance_get_handle(m_uiInstance)) != 0
			&& m_uiVisible)
		{
			// The UI closed itself. Keep the instance alive so it can be
			// reopened with the Show GUI button.
			setUiVisible(false);
		}
	}
#endif // LMMS_HAVE_SUIL
}




void Lv2ProcView::writeToPlugin(uint32_t portIndex, uint32_t bufferSize,
	uint32_t protocol, const void* buffer)
{
	if (m_proc)
	{
		m_proc->writeUiEvent(portIndex, protocol, bufferSize, buffer);
	}
}




uint32_t Lv2ProcView::indexOfPort(const char* symbol)
{
	if (!m_proc || !symbol)
	{
		return LV2UI_INVALID_PORT_INDEX;
	}

	const auto id = m_proc->getIdOfPort(symbol);
	return id == static_cast<std::size_t>(-1)
		? LV2UI_INVALID_PORT_INDEX
		: static_cast<uint32_t>(id);
}




void Lv2ProcView::toggleUI()
{
#ifdef LMMS_HAVE_SUIL
	if (m_uiInstance)
	{
		// Keep the native UI instance alive and only hide/show it. Some UIs
		// (e.g. Pianoteq) do not survive repeated destruction/re-creation.
		setUiVisible(!m_uiVisible);
	}
	else
	{
		createUi();
	}

	updateToggleButton();
#endif
}




void Lv2ProcView::setUiVisible(bool visible)
{
#ifdef LMMS_HAVE_SUIL
	if (!m_uiInstance || m_uiVisible == visible)
	{
		return;
	}
	m_uiVisible = visible;

	if (m_uiShow)
	{
		if (visible && m_uiShow->show)
		{
			m_uiShow->show(suil_instance_get_handle(m_uiInstance));
		}
		else if (!visible && m_uiShow->hide)
		{
			m_uiShow->hide(suil_instance_get_handle(m_uiInstance));
		}
	}

#if defined(LMMS_BUILD_LINUX)
	if (m_x11Window && m_x11Display)
	{
		Display* dpy = static_cast<Display*>(m_x11Display);
		if (visible)
		{
			XMapWindow(dpy, m_x11Window);
			XRaiseWindow(dpy, m_x11Window);
			resizeUiToPlugin();
		}
		else
		{
			XUnmapWindow(dpy, m_x11Window);
		}
		XFlush(dpy);
		updateToggleButton();
		return;
	}
#endif

	if (m_uiWindow)
	{
		if (visible) { m_uiWindow->show(); }
		else { m_uiWindow->hide(); }
	}
	updateToggleButton();
#endif // LMMS_HAVE_SUIL
}


void Lv2ProcView::updateToggleButton()
{
	if (!m_toggleUIButton)
	{
		return;
	}
	QSignalBlocker blocker(m_toggleUIButton);
	m_toggleUIButton->setChecked(m_uiVisible);
}




void Lv2ProcView::toggleHelp(bool visible)
{
	if (m_helpWindow)
	{
		if (visible) { m_helpWindow->show(); m_helpWindow->raise(); }
		else { m_helpWindow->hide(); }
	}
}




void Lv2ProcView::closeHelpWindow()
{
	if (m_helpWindow) { m_helpWindow->close(); }
}




void Lv2ProcView::modelChanged(Lv2Proc *proc)
{
	// reconnect models
	if (m_toggleUIButton)
	{
		QSignalBlocker blocker(m_toggleUIButton);
		m_toggleUIButton->setChecked(m_uiInstance != nullptr);
	}

	ModelGroupView::modelChanged(proc);
}




AutoLilvNode Lv2ProcView::uri(const char *uriStr)
{
	return Engine::getLv2Manager()->uri(uriStr);
}




void Lv2ProcView::onHelpWindowClosed()
{
	m_helpButton->setChecked(true);
}




HelpWindowEventFilter::HelpWindowEventFilter(Lv2ProcView* viewBase) :
	m_procView(viewBase) {}



bool HelpWindowEventFilter::eventFilter(QObject* , QEvent* event)
{
	if (event->type() == QEvent::Close) {
		m_procView->m_helpButton->setChecked(false);
		return true;
	}
	return false;
}


} // namespace lmms::gui


#endif // LMMS_HAVE_LV2
