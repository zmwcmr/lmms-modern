#include "EditorShortcuts.h"

#include <iterator>

#include <QAction>
#include <QDebug>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMap>
#include <QWidget>

#include "ConfigManager.h"
#include "DeprecationHelper.h"

namespace lmms::gui
{

namespace
{
const auto transportPlayStop = QStringLiteral("transport_play_stop");
const auto transportPause = QStringLiteral("transport_pause");
const auto transportRecord = QStringLiteral("transport_record");
const auto transportRecordAccompany = QStringLiteral("transport_record_accompany");
const auto transportRecordStep = QStringLiteral("transport_record_step");
const QKeySequence defaultPlayStop(Qt::Key_Space);
const auto defaultPause = keySequence(Qt::SHIFT, Qt::Key_Space);

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
	{QStringLiteral("pattern_next"), QKeySequence(Qt::Key_Plus)},
	{QStringLiteral("pattern_previous"), QKeySequence(Qt::Key_Minus)},
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

bool isValidTransportBinding(const QKeySequence& sequence, const TransportShortcuts* playback = nullptr)
{
	if (!isValid(sequence) || sequence.count() != 1) { return false; }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	const auto key = sequence[0].key();
	const auto modifiers = sequence[0].keyboardModifiers();
#else
	const auto key = sequence[0] & ~Qt::KeyboardModifierMask;
	const auto modifiers = sequence[0] & Qt::KeyboardModifierMask;
#endif
	// Default playback also handles Space with extra modifiers.
	if (playback && key == Qt::Key_Space)
	{
		const bool shift = modifiers & Qt::ShiftModifier;
		if (shift ? playback->pause == defaultPause : playback->playStop == defaultPlayStop) { return false; }
	}
	const auto pianoRollValueModifiers = Qt::AltModifier | Qt::ControlModifier | Qt::KeypadModifier;
	return key < Qt::Key_0 || key > Qt::Key_9 || !(modifiers & pianoRollValueModifiers);
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

template<typename Accepts = decltype(&isValid)>
QMap<QString, QKeySequence> resolveShortcuts(const QMap<QString, QKeySequence>& groupDefaults,
	const QList<QKeySequence>& reserved, Accepts accepts = isValid)
{
	auto shortcuts = groupDefaults;
	const auto section = QStringLiteral("shortcuts");
	for (auto item = shortcuts.begin(); item != shortcuts.end(); ++item)
	{
		const auto configured = ConfigManager::inst()->value(section, item.key()).trimmed();
		const auto sequence = QKeySequence::fromString(configured, QKeySequence::PortableText);
		if (accepts(sequence)) { item.value() = sequence; }
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
const QString patternPrefix = QStringLiteral("pattern_");

const QMap<QString, QKeySequence>& menuDefaults()
{
	static const QMap<QString, QKeySequence> shortcuts{
		{QStringLiteral("file_new"), QKeySequence::New},
		{QStringLiteral("file_open"), QKeySequence::Open},
		{QStringLiteral("file_save"), QKeySequence::Save},
		{QStringLiteral("file_save_as"), keySequence(Qt::CTRL, Qt::SHIFT, Qt::Key_S)},
		{QStringLiteral("file_save_new_version"), keySequence(Qt::CTRL, Qt::ALT, Qt::Key_S)},
		{QStringLiteral("file_export"), keySequence(Qt::CTRL, Qt::Key_E)},
		{QStringLiteral("file_export_tracks"), keySequence(Qt::CTRL, Qt::SHIFT, Qt::Key_E)},
		{QStringLiteral("file_export_midi"), keySequence(Qt::CTRL, Qt::Key_M)},
		{QStringLiteral("file_quit"), keySequence(Qt::CTRL, Qt::Key_Q)},
		{QStringLiteral("edit_undo"), QKeySequence::Undo},
		{QStringLiteral("edit_redo"), QKeySequence::Redo},
	};
	return shortcuts;
}

const QMap<QString, QKeySequence>& windowLayoutDefaults()
{
	static const QMap<QString, QKeySequence> shortcuts{
		{QStringLiteral("layout_fullscreen"), QKeySequence(Qt::Key_F11)},
		{QStringLiteral("layout_maximize_editor"), keySequence(Qt::SHIFT, Qt::Key_F11)},
		{QStringLiteral("layout_detach_all"), keySequence(Qt::CTRL, Qt::SHIFT, Qt::Key_D)},
		{QStringLiteral("layout_attach_all"), keySequence(Qt::CTRL, Qt::ALT, Qt::SHIFT, Qt::Key_D)},
	};
	return shortcuts;
}

QList<QKeySequence> effectiveTransportShortcuts()
{
	const auto shortcuts = transportShortcuts();
	return {shortcuts.playStop, shortcuts.pause, shortcuts.record, shortcuts.recordAccompany, shortcuts.recordStep};
}

QList<QKeySequence> reservedForEditors()
{
	return defaultShortcuts(windowPrefix).values() + menuDefaults().values() + effectiveTransportShortcuts()
		+ windowLayoutShortcuts().values();
}

QMap<QString, QKeySequence> editorShortcuts(const QString& prefix)
{
	return resolveShortcuts(defaultShortcuts(prefix), reservedForEditors() + defaultShortcuts(patternPrefix).values());
}

QList<QKeySequence> effectiveEditorShortcuts()
{
	return editorShortcuts(pianoPrefix).values() + editorShortcuts(automationPrefix).values();
}
}

QMap<QString, QKeySequence> windowLayoutShortcuts()
{
	return resolveShortcuts(windowLayoutDefaults(),
		menuDefaults().values() + defaultShortcuts(windowPrefix).values() + defaultShortcuts(patternPrefix).values()
		+ defaultShortcuts(pianoPrefix).values() + defaultShortcuts(automationPrefix).values() + QList<QKeySequence>{defaultPlayStop, defaultPause});
}

TransportShortcuts transportShortcuts()
{
	const auto reserved = menuDefaults().values() + defaultShortcuts(windowPrefix).values()
		+ defaultShortcuts(patternPrefix).values() + defaultShortcuts(pianoPrefix).values()
		+ defaultShortcuts(automationPrefix).values() + windowLayoutShortcuts().values();
	const auto playback = resolveShortcuts({{transportPlayStop, defaultPlayStop}, {transportPause, defaultPause}},
		reserved, [](const QKeySequence& sequence) { return isValidTransportBinding(sequence); });
	TransportShortcuts shortcuts{playback.value(transportPlayStop), playback.value(transportPause), {}, {}, {}};
	const auto recording = resolveShortcuts({{transportRecord, {}}, {transportRecordAccompany, {}}, {transportRecordStep, {}}},
		reserved + playback.values(),
		[&shortcuts](const QKeySequence& sequence) { return isValidTransportBinding(sequence, &shortcuts); });
	shortcuts.record = recording.value(transportRecord);
	shortcuts.recordAccompany = recording.value(transportRecordAccompany);
	shortcuts.recordStep = recording.value(transportRecordStep);
	return shortcuts;
}

TransportShortcuts::Action TransportShortcuts::match(const QKeyEvent& event) const
{
	const QKeySequence pressed(event.key() | static_cast<int>(event.modifiers()));
	if (pressed == playStop) { return Action::PlayStop; }
	if (pressed == pause) { return Action::Pause; }
	if (event.key() == Qt::Key_Space)
	{
		if (event.modifiers() & Qt::ShiftModifier) { return pause == defaultPause ? Action::Pause : Action::None; }
		if (playStop == defaultPlayStop) { return Action::PlayStop; }
	}
	return Action::None;
}

QMap<QString, QKeySequence> patternNavigationShortcuts()
{
	return resolveShortcuts(defaultShortcuts(patternPrefix), reservedForEditors() + effectiveEditorShortcuts());
}

QMap<QString, QKeySequence> windowToggleShortcuts(const QList<QKeySequence>& reserved)
{
	return resolveShortcuts(defaultShortcuts(windowPrefix),
		reserved + effectiveEditorShortcuts() + patternNavigationShortcuts().values() + effectiveTransportShortcuts()
		+ windowLayoutShortcuts().values());
}

QMap<QString, QKeySequence> mainMenuShortcuts(const QList<QKeySequence>& reserved)
{
	return resolveShortcuts(menuDefaults(), reserved + defaultShortcuts(windowPrefix).values()
		+ effectiveEditorShortcuts() + patternNavigationShortcuts().values() + effectiveTransportShortcuts()
		+ windowLayoutShortcuts().values());
}

void bindEditorShortcuts(QWidget& editor,
	std::initializer_list<std::pair<QString, QAction*>> actions)
{
	QMap<QString, QKeySequence> groupDefaults;
	for (const auto& binding : actions) { groupDefaults.insert(binding.first, defaults.value(binding.first)); }
	const auto shortcuts = resolveShortcuts(groupDefaults, reservedForEditors() + defaultShortcuts(patternPrefix).values());
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
