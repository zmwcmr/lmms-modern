#ifndef LMMS_GUI_DROP_DOWN_INTERACTION_H
#define LMMS_GUI_DROP_DOWN_INTERACTION_H

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QMouseEvent>
#include <QPointer>
#include <QSlider>
#include <QStyleOptionToolButton>
#include <QTimer>
#include <QToolButton>

#include "DeprecationHelper.h"
#include "SetupDialog.h"

namespace lmms::gui
{

class DropDownInteraction : public QObject
{
public:
	using QObject::QObject;

protected:
	bool eventFilter(QObject* watched, QEvent* event) override
	{
		const auto type = event->type();
		if (type == QEvent::Wheel)
		{
			if (qobject_cast<QComboBox*>(watched) || isSettingsNumberControl(watched))
			{
				event->ignore();
				return true;
			}
			return false;
		}

		if (type == QEvent::Hide)
		{
			if (watched == m_pending) { m_pending.clear(); }
			if (watched == m_popupViewport) { m_popupViewport.clear(); }
			return false;
		}
		const bool pressing = type == QEvent::MouseButtonPress || type == QEvent::MouseButtonDblClick;
		if ((!pressing && type != QEvent::MouseButtonRelease) || !watched->isWidgetType()) { return false; }

		auto* mouse = static_cast<QMouseEvent*>(event);
		if (mouse->button() != Qt::LeftButton) { return false; }
		auto* combo = qobject_cast<QComboBox*>(watched);
		auto* button = qobject_cast<QToolButton*>(watched);
		if (pressing)
		{
			m_pending.clear();
			m_popupPressed = watched == m_popupViewport;
			if (combo && !combo->isEditable() && combo->isEnabled() && !combo->view()->isVisible())
			{
				m_pending = combo;
			}
			else if (button && button->isEnabled() && opensMenuAt(button, position(mouse)))
			{
				m_pending = button;
			}
			if (!m_pending) { return false; }
		}
		else
		{
			const auto pending = m_pending;
			m_pending.clear();
			const bool popupPressed = m_popupPressed;
			m_popupPressed = false;
			if (watched == m_popupViewport && !popupPressed)
			{
				event->accept();
				return true;
			}
			if (!pending || watched != pending) { return false; }
			if (pending->isEnabled() && QRectF(pending->rect()).contains(position(mouse)))
			{
				if (combo && combo->count() > 0)
				{
					if (combo->focusPolicy() & Qt::ClickFocus) { combo->setFocus(Qt::MouseFocusReason); }
					if (pending) { combo->showPopup(); }
					if (pending && combo->view()->isVisible()) { m_popupViewport = combo->view()->viewport(); }
				}
				else if (button && opensMenuAt(button, position(mouse)))
				{
					QTimer::singleShot(0, button, &QToolButton::showMenu);
				}
			}
		}
		event->accept();
		return true;
	}

private:
	static bool isSettingsNumberControl(QObject* object)
	{
		if (!qobject_cast<QAbstractSpinBox*>(object) && !qobject_cast<QSlider*>(object)) { return false; }
		for (auto* parent = object->parent(); parent; parent = parent->parent())
		{
			if (qobject_cast<SetupDialog*>(parent)) { return true; }
		}
		return false;
	}

	static bool opensMenuAt(const QToolButton* button, const QPointF& point)
	{
		if (!button->menu()) { return false; }
		if (button->popupMode() == QToolButton::InstantPopup) { return true; }
		if (button->popupMode() != QToolButton::MenuButtonPopup) { return false; }
		QStyleOptionToolButton option;
		option.initFrom(button);
		option.features = QStyleOptionToolButton::MenuButtonPopup | QStyleOptionToolButton::HasMenu;
		option.subControls = QStyle::SC_ToolButton | QStyle::SC_ToolButtonMenu;
		return button->style()->hitTestComplexControl(QStyle::CC_ToolButton, &option, point.toPoint(), button)
			== QStyle::SC_ToolButtonMenu;
	}

	QPointer<QWidget> m_pending;
	QPointer<QWidget> m_popupViewport;
	bool m_popupPressed = false;
};

} // namespace lmms::gui

#endif
