#ifndef LMMS_EDITORSHORTCUTS_H
#define LMMS_EDITORSHORTCUTS_H

#include <initializer_list>
#include <utility>

#include <QKeySequence>
#include <QList>
#include <QMap>
#include <QString>

class QAction;
class QKeyEvent;
class QWidget;

namespace lmms::gui
{

struct TransportShortcuts
{
	enum class Action { None, PlayStop, Pause };
	QKeySequence playStop;
	QKeySequence pause;
	QKeySequence record;
	QKeySequence recordAccompany;
	QKeySequence recordStep;
	Action match(const QKeyEvent& event) const;
};

TransportShortcuts transportShortcuts();

struct SongTimelineShortcuts
{
	enum class Action { None, PreviousBar, NextBar, Start };
	QKeySequence previousBar;
	QKeySequence nextBar;
	QKeySequence start;
	Action match(const QKeyEvent& event) const;
};

SongTimelineShortcuts songTimelineShortcuts();

QMap<Qt::Key, QKeySequence> automationNavigationShortcuts();
Qt::Key matchAutomationNavigation(const QMap<Qt::Key, QKeySequence>& shortcuts, const QKeyEvent& event);

void bindEditorShortcuts(QWidget& editor,
	std::initializer_list<std::pair<QString, QAction*>> actions);

QMap<QString, QKeySequence> windowToggleShortcuts(const QList<QKeySequence>& reserved);
QMap<QString, QKeySequence> mainMenuShortcuts(const QList<QKeySequence>& reserved);
QMap<QString, QKeySequence> patternNavigationShortcuts();
QMap<QString, QKeySequence> windowLayoutShortcuts();
QMap<QString, QKeySequence> rackReorderShortcuts();
QMap<QString, QKeySequence> fileBrowserShortcuts();
QMap<QString, QKeySequence> projectNotesShortcuts();

}

#endif
