#include "EditorShortcuts.h"

#include <iterator>

#include <QAction>
#include <QDebug>
#include <QKeySequence>
#include <QMap>
#include <QWidget>

#include "ConfigManager.h"
#include "DeprecationHelper.h"

namespace lmms::gui
{

namespace
{
const QMap<QString, QKeySequence> defaults{
	{QStringLiteral("piano_roll_draw"), keySequence(Qt::SHIFT, Qt::Key_D)},
	{QStringLiteral("piano_roll_erase"), keySequence(Qt::SHIFT, Qt::Key_E)},
	{QStringLiteral("piano_roll_select"), keySequence(Qt::SHIFT, Qt::Key_S)},
	{QStringLiteral("piano_roll_pitch_bend"), keySequence(Qt::SHIFT, Qt::Key_T)},
	{QStringLiteral("piano_roll_cut"), keySequence(Qt::CTRL, Qt::Key_X)},
	{QStringLiteral("piano_roll_copy"), keySequence(Qt::CTRL, Qt::Key_C)},
	{QStringLiteral("piano_roll_paste"), keySequence(Qt::CTRL, Qt::Key_V)},
	{QStringLiteral("piano_roll_glue"), keySequence(Qt::SHIFT, Qt::Key_G)},
	{QStringLiteral("piano_roll_knife"), keySequence(Qt::SHIFT, Qt::Key_K)},
	{QStringLiteral("piano_roll_strum"), keySequence(Qt::SHIFT, Qt::Key_J)},
	{QStringLiteral("piano_roll_fill"), keySequence(Qt::SHIFT, Qt::Key_F)},
	{QStringLiteral("piano_roll_cut_overlaps"), keySequence(Qt::SHIFT, Qt::Key_C)},
	{QStringLiteral("piano_roll_reverse"), keySequence(Qt::SHIFT, Qt::Key_R)},
	{QStringLiteral("automation_editor_draw"), keySequence(Qt::SHIFT, Qt::Key_D)},
	{QStringLiteral("automation_editor_erase"), keySequence(Qt::SHIFT, Qt::Key_E)},
	{QStringLiteral("automation_editor_draw_out"), keySequence(Qt::SHIFT, Qt::Key_C)},
	{QStringLiteral("automation_editor_edit_tangents"), keySequence(Qt::SHIFT, Qt::Key_T)},
	{QStringLiteral("window_song_editor"), keySequence(Qt::CTRL, Qt::Key_1)},
	{QStringLiteral("window_pattern_editor"), keySequence(Qt::CTRL, Qt::Key_2)},
	{QStringLiteral("window_piano_roll"), keySequence(Qt::CTRL, Qt::Key_3)},
	{QStringLiteral("window_automation_editor"), keySequence(Qt::CTRL, Qt::Key_4)},
	{QStringLiteral("window_mixer"), keySequence(Qt::CTRL, Qt::Key_5)},
	{QStringLiteral("window_controller_rack"), keySequence(Qt::CTRL, Qt::Key_6)},
	{QStringLiteral("window_project_notes"), keySequence(Qt::CTRL, Qt::Key_7)},
};

bool isValid(const QKeySequence& sequence)
{
	if (sequence.isEmpty()) { return false; }
	for (int index = 0; index < sequence.count(); ++index)
	{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		const auto key = sequence[index].key();
#else
		const int key = sequence[index] & ~Qt::KeyboardModifierMask;
#endif
		if (key == 0 || key == Qt::Key_unknown) { return false; }
	}
	return true;
}
bool shortcutsOverlap(const QKeySequence& first, const QKeySequence& second)
{
	return !first.isEmpty() && !second.isEmpty()
		&& (first.matches(second) != QKeySequence::NoMatch || second.matches(first) != QKeySequence::NoMatch);
}

QMap<QString, QKeySequence> defaultShortcuts(const QString& prefix)
{
	QMap<QString, QKeySequence> shortcuts;
	for (auto item = defaults.cbegin(); item != defaults.cend(); ++item)
	{
		if (item.key().startsWith(prefix)) { shortcuts.insert(item.key(), item.value()); }
	}
	return shortcuts;
}

QMap<QString, QKeySequence> resolveShortcuts(const QMap<QString, QKeySequence>& groupDefaults,
	const QList<QKeySequence>& reserved)
{
	auto shortcuts = groupDefaults;
	const auto section = QStringLiteral("shortcuts");
	for (auto item = shortcuts.begin(); item != shortcuts.end(); ++item)
	{
		const auto configured = ConfigManager::inst()->value(section, item.key()).trimmed();
		const auto sequence = QKeySequence::fromString(configured, QKeySequence::PortableText);
		if (isValid(sequence)) { item.value() = sequence; }
		else if (!configured.isEmpty())
		{
			qWarning() << "Ignoring invalid shortcut for" << item.key() << ':' << configured;
		}
	}

	for (auto first = shortcuts.cbegin(); first != shortcuts.cend(); ++first)
	{
		for (const auto& sequence : reserved)
		{
			if (shortcutsOverlap(first.value(), sequence))
			{
				qWarning() << "Shortcut for" << first.key() << "conflicts with" << sequence << "; using group defaults";
				return groupDefaults;
			}
		}
		for (auto second = std::next(first); second != shortcuts.cend(); ++second)
		{
			if (shortcutsOverlap(first.value(), second.value()))
			{
				qWarning() << "Conflicting shortcuts for" << first.key() << "and" << second.key()
					<< "; using group defaults";
				return groupDefaults;
			}
		}
	}
	return shortcuts;
}

const QString windowPrefix = QStringLiteral("window_");
const QString pianoPrefix = QStringLiteral("piano_roll_");
const QString automationPrefix = QStringLiteral("automation_editor_");

QMap<QString, QKeySequence> editorShortcuts(const QString& prefix)
{
	return resolveShortcuts(defaultShortcuts(prefix), defaultShortcuts(windowPrefix).values());
}
}

QMap<QString, QKeySequence> windowToggleShortcuts(const QList<QKeySequence>& reserved)
{
	return resolveShortcuts(defaultShortcuts(windowPrefix),
		reserved + editorShortcuts(pianoPrefix).values() + editorShortcuts(automationPrefix).values());
}

void bindEditorShortcuts(QWidget& editor,
	std::initializer_list<std::pair<QString, QAction*>> actions)
{
	QMap<QString, QKeySequence> groupDefaults;
	for (const auto& binding : actions) { groupDefaults.insert(binding.first, defaults.value(binding.first)); }
	const auto shortcuts = resolveShortcuts(groupDefaults, defaultShortcuts(windowPrefix).values());
	for (const auto& [id, action] : actions)
	{
		const auto sequence = shortcuts.value(id);
		action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
		action->setShortcut(sequence);
		editor.addAction(action);
		if (!sequence.isEmpty())
		{
			action->setToolTip(QWidget::tr("%1 (%2)").arg(action->toolTip(), sequence.toString(QKeySequence::NativeText)));
		}
	}
}

}
