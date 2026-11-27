#ifndef LMMS_EDITORSHORTCUTS_H
#define LMMS_EDITORSHORTCUTS_H

#include <initializer_list>
#include <utility>

#include <QKeySequence>
#include <QList>
#include <QMap>
#include <QString>

class QAction;
class QWidget;

namespace lmms::gui
{

void bindEditorShortcuts(QWidget& editor,
	std::initializer_list<std::pair<QString, QAction*>> actions);

QMap<QString, QKeySequence> windowToggleShortcuts(const QList<QKeySequence>& reserved);

}

#endif
