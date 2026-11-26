/*
 * Lv2ProcView.h - base class for Lv2 plugin views
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

#ifndef LMMS_GUI_LV2_VIEW_BASE_H
#define LMMS_GUI_LV2_VIEW_BASE_H

#include "Lv2Proc.h"
#include "lmmsconfig.h"

#ifdef LMMS_HAVE_LV2

#include <QWidget>
#include <optional>
#include <vector>

#include "ModelGroupView.h"
#include "lmms_export.h"
#include "Lv2Basics.h"

#ifdef LMMS_HAVE_SUIL
#include <suil/suil.h>
#include <lv2/options/options.h>
#include <lv2/ui/ui.h>
#endif

class QPushButton;
class QComboBox;
class QMdiSubWindow;
class QTimer;

namespace lmms
{

class Lv2Proc;

namespace gui
{

class HelpWindowEventFilter : public QObject
{
	Q_OBJECT
	class Lv2ProcView* const m_procView;
protected:
	bool eventFilter(QObject* obj, QEvent* event) override;
public:
	HelpWindowEventFilter(class Lv2ProcView* viewBase);
};




//! Base class for view for one Lv2 plugin
class LMMS_EXPORT Lv2ProcView : public ModelGroupView
{
	friend class HelpWindowEventFilter;
protected:
	//! @param pluginWidget A child class which inherits QWidget
	Lv2ProcView(class QWidget *pluginWidget, Lv2Proc *proc);
	~Lv2ProcView();

	// these widgets must be connected by child widgets
	QPushButton* m_reloadPluginButton = nullptr;
	QPushButton* m_toggleUIButton = nullptr;
	QPushButton* m_helpButton = nullptr;
	QComboBox* m_presetCombo = nullptr;

	void toggleUI();
	void toggleHelp(bool visible);
	void closeHelpWindow();

	// to be called by child virtuals
	//! Reconnect models if model changed
	void modelChanged(Lv2Proc *proc);

private:
	static AutoLilvNode uri(const char *uriStr);
	void onHelpWindowClosed();

	//! Whether a supported native UI exists for this plugin
	bool hasPluginUi() const;
	void createUi();
	void createRemoteUi();
	void destroyUi();
	void setUiVisible(bool visible);
	void updateUi();
	void resizeUiToPlugin();
	int resizeUiRequest(int width, int height);
	void applyUiResize(int width, int height);
	void updateToggleButton();
#if defined(LMMS_BUILD_LINUX) && defined(LMMS_HAVE_SUIL)
	void processX11Events();
	void resizePluginChild(int width, int height);
#endif
	void touchPort(uint32_t portIndex, bool grabbed);
	void writeToPlugin(uint32_t portIndex, uint32_t bufferSize,
		uint32_t protocol, const void* buffer);
	uint32_t indexOfPort(const char* symbol);

	Lv2Proc* m_proc = nullptr;
	QWidget* m_pluginWidget = nullptr;

#ifdef LMMS_HAVE_SUIL
	SuilHost* m_uiHost = nullptr;
	SuilInstance* m_uiInstance = nullptr;
	QTimer* m_uiTimer = nullptr;

	// The plugin UI may keep pointers to these features for its whole
	// lifetime, so they must outlive suil_instance_new().
	LV2_Feature m_parentFeature{};
	LV2_Feature m_instanceFeature{};
	LV2_Feature m_dataFeature{};
	LV2_Feature m_resizeFeature{};
	LV2_Feature m_uiOptionsFeature{};
	LV2_Options_Option m_uiOptions[3]{};
	LV2UI_Resize m_resize{};
	const LV2UI_Show_Interface* m_uiShow = nullptr;
	//! Asset root some UIs (Zyn Fusion) resolve relative to the CWD
	QString m_uiDataRoot;
	float m_uiScale = 1.0f;
	float m_uiSampleRate = 0.0f;
	bool m_uiVisible = false;
	bool m_uiResizable = true;
	//! Size requested by the UI while suil_instance_new() was running
	int m_pendingUiWidth = 0;
	int m_pendingUiHeight = 0;
	int m_lastUiWidth = 0;
	int m_lastUiHeight = 0;
#endif

#if defined(LMMS_BUILD_LINUX)
	//! Standalone X11 window used when Qt runs on Wayland
	void* m_x11Display = nullptr;
	unsigned long m_x11Window = 0;
	unsigned long m_x11WmDelete = 0;
#endif

	QMdiSubWindow* m_helpWindow = nullptr;
	HelpWindowEventFilter m_helpWindowEventFilter;
};


} // namespace gui

} // namespace lmms

#endif // LMMS_HAVE_LV2

#endif // LMMS_GUI_LV2_VIEW_BASE_H
