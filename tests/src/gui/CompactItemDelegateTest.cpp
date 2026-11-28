#include <QIcon>
#include <QTreeWidget>
#include <QtTest>

#include "CompactItemDelegate.h"

using namespace lmms::gui;

class CompactItemDelegateTest : public QObject
{
	Q_OBJECT

private slots:
	void textRectKeepsLastCharacter()
	{
		QTreeWidget tree;
		tree.setColumnCount(1);
		auto font = tree.font();
		font.setPointSize(12);
		tree.setFont(font);
		tree.setIconSize(QSize(16, 16));

		auto* item = new QTreeWidgetItem(&tree);
		item->setText(0, QStringLiteral("VeSTige"));
		QPixmap logo(16, 16);
		logo.fill(Qt::white);
		item->setIcon(0, QIcon(logo));

		QStyleOptionViewItem option;
		option.initFrom(&tree);
		option.rect = QRect(20, 0, 220, 24);
		option.font = tree.font();
		option.decorationSize = tree.iconSize();

		CompactItemDelegate delegate;
		const auto layout = delegate.layoutFor(option, tree.model()->index(tree.indexOfTopLevelItem(item), 0));
		const auto metrics = QFontMetrics(option.font);
		const int textWidth = metrics.horizontalAdvance(layout.text);

		QCOMPARE(layout.text, QStringLiteral("VeSTige"));
		QVERIFY2(layout.textRect.width() >= textWidth,
			qPrintable(QStringLiteral("text rect %1 is narrower than text %2")
				.arg(layout.textRect.width()).arg(textWidth)));
		QVERIFY(layout.textRect.right() < layout.pillRect.right());
	}

	void elidesLongTextWithoutClipping()
	{
		QTreeWidget tree;
		tree.setColumnCount(1);
		auto font = tree.font();
		font.setPointSize(12);
		tree.setFont(font);
		tree.setIconSize(QSize(16, 16));

		auto* item = new QTreeWidgetItem(&tree);
		item->setText(0, QStringLiteral("A very long plugin name that cannot fit"));
		QPixmap logo(16, 16);
		logo.fill(Qt::white);
		item->setIcon(0, QIcon(logo));

		QStyleOptionViewItem option;
		option.initFrom(&tree);
		option.rect = QRect(20, 0, 120, 24);
		option.font = tree.font();
		option.decorationSize = tree.iconSize();

		CompactItemDelegate delegate;
		const auto layout = delegate.layoutFor(option, tree.model()->index(tree.indexOfTopLevelItem(item), 0));
		const auto metrics = QFontMetrics(option.font);

		QVERIFY(layout.text != item->text(0));
		QVERIFY(layout.textRect.width() >= metrics.horizontalAdvance(layout.text));
		QVERIFY(layout.textRect.right() < layout.pillRect.right());
	}
};

QTEST_MAIN(CompactItemDelegateTest)
#include "CompactItemDelegateTest.moc"
