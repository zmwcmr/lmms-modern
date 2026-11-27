#include <QGraphicsColorizeEffect>
#include <QGraphicsDropShadowEffect>
#include <QProxyStyle>
#include <QVariantAnimation>
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
		lmms::gui::UiFeedback::setPulsingHighlight(&target, true);
		lmms::gui::UiFeedback::setPulsingHighlight(&target, false);
		QCOMPARE(target.graphicsEffect(), existing);
	}

	void pulseUsesBrighterAccentAndPersistsUntilCleared()
	{
		using lmms::gui::UiFeedback;
		QWidget target;
		auto* style = new UiFeedbackTestStyle(100);
		style->setParent(&target);
		target.setStyle(style);
		auto palette = target.palette();
		palette.setColor(QPalette::Window, QColor(24, 24, 24));
		palette.setColor(QPalette::Highlight, QColor::fromHsl(220, 170, 120));
		target.setPalette(palette);
		target.setAutoFillBackground(true);
		target.resize(80, 40);
		const auto centerPixel = [&target] {
			const auto image = target.grab().toImage();
			return image.pixelColor(image.rect().center());
		};
		const auto original = centerPixel();
		UiFeedback::flash(&target);
		auto* flash = qobject_cast<QGraphicsColorizeEffect*>(target.graphicsEffect());
		QVERIFY(flash);
		const QColor color = flash->color();

		UiFeedback::setPulsingHighlight(&target, true);
		auto* pulse = qobject_cast<QGraphicsColorizeEffect*>(target.graphicsEffect());
		QVERIFY(pulse);
		QCOMPARE(pulse->color().hslHue(), color.hslHue());
		QVERIFY(pulse->color().lightness() > color.lightness());
		const qreal peakStrength = pulse->strength();
		UiFeedback::flash(&target);
		QTest::qWait(120);
		QCOMPARE(target.graphicsEffect(), pulse);

		auto* animation = pulse->findChild<QVariantAnimation*>();
		QVERIFY(animation);
		QCOMPARE(animation->loopCount(), -1);
		animation->setCurrentTime(animation->duration() / 2);
		QVERIFY(centerPixel().lightness() < original.lightness());
		animation->setCurrentTime(animation->duration());
		QCOMPARE(pulse->strength(), peakStrength);
		QVERIFY(centerPixel().lightness() > original.lightness());
		UiFeedback::setPulsingHighlight(&target, false);
		QVERIFY(!target.graphicsEffect());
		QCOMPARE(centerPixel(), original);
	}

	void selectionUsesStaticTintWithAnimationsDisabled()
	{
		QWidget target;
		auto* style = new UiFeedbackTestStyle(0);
		style->setParent(&target);
		target.setStyle(style);
		lmms::gui::UiFeedback::setPulsingHighlight(&target, true);
		auto* tint = qobject_cast<QGraphicsColorizeEffect*>(target.graphicsEffect());
		QVERIFY(tint && tint->strength() > 0.0);
		QVERIFY(!tint->findChild<QVariantAnimation*>());
		lmms::gui::UiFeedback::setPulsingHighlight(&target, false);
		QVERIFY(!target.graphicsEffect());
	}
};

QTEST_MAIN(UiFeedbackTest)
#include "UiFeedbackTest.moc"
