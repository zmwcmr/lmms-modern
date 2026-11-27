#include <QGraphicsColorizeEffect>
#include <QGraphicsDropShadowEffect>
#include <QProxyStyle>
#include <QtTest>

#include "UiFeedback.h"

class UiFeedbackTestStyle : public QProxyStyle
{
public:
	explicit UiFeedbackTestStyle(int duration) : m_duration(duration) {}

	int styleHint(StyleHint hint, const QStyleOption* option, const QWidget* widget,
		QStyleHintReturn* result) const override
	{
		return hint == SH_Widget_Animation_Duration
			? m_duration : QProxyStyle::styleHint(hint, option, widget, result);
	}

private:
	int m_duration;
};

class UiFeedbackTest : public QObject
{
	Q_OBJECT

private slots:
	void tintsTargetAndFades()
	{
		QWidget target;
		auto* style = new UiFeedbackTestStyle(100);
		style->setParent(&target);
		target.setStyle(style);
		target.resize(80, 40);
		target.show();
		QCoreApplication::processEvents();

		lmms::gui::UiFeedback::flash(&target);
		auto* effect = qobject_cast<QGraphicsColorizeEffect*>(target.graphicsEffect());
		QVERIFY(effect);
		QVERIFY(effect->strength() > 0.0);
		QTRY_VERIFY(!target.graphicsEffect());
	}

	void respectsDisabledAnimations()
	{
		QWidget target;
		auto* style = new UiFeedbackTestStyle(0);
		style->setParent(&target);
		target.setStyle(style);
		target.resize(80, 40);

		lmms::gui::UiFeedback::flash(&target);
		QVERIFY(!target.graphicsEffect());
	}

	void keepsUnknownEffects()
	{
		QWidget target;
		auto* style = new UiFeedbackTestStyle(100);
		style->setParent(&target);
		target.setStyle(style);
		target.resize(80, 40);

		auto* existing = new QGraphicsDropShadowEffect(&target);
		target.setGraphicsEffect(existing);
		lmms::gui::UiFeedback::flash(&target);
		QCOMPARE(target.graphicsEffect(), existing);
	}
};

QTEST_MAIN(UiFeedbackTest)
#include "UiFeedbackTest.moc"
