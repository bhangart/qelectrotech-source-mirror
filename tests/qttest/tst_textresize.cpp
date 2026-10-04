/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.

	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with QElectroTech.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <QtTest>
#include <QGraphicsTextItem>
#include <QTextOption>

#include "textresize.h"

static bool samePoint(const QPointF &a, const QPointF &b)
{
	return qAbs(a.x() - b.x()) < 1e-6 && qAbs(a.y() - b.y()) < 1e-6;
}

static QString describe(Qt::Alignment alignment, qreal angle, bool centre)
{
	return QStringLiteral("alignment %1 angle %2 centre %3")
			.arg(int(alignment)).arg(angle).arg(centre);
}

/// A text that wraps like the texts of QElectroTech do.
static void makeWrapping(QGraphicsTextItem &text)
{
	QTextOption option = text.document()->defaultTextOption();
	option.setWrapMode(QTextOption::WordWrap);
	text.document()->setDefaultTextOption(option);
}

class tst_textresize : public QObject
{
	Q_OBJECT

private slots:
	// The point chosen by the alignment stays where it is drawn when the
	// width changes, for every alignment, rotation and rotation point.
	void anchorStaysWhenWidthChanges()
	{
		const QList<Qt::Alignment> horizontal {Qt::AlignLeft, Qt::AlignHCenter, Qt::AlignRight};
		const QList<Qt::Alignment> vertical   {Qt::AlignTop, Qt::AlignVCenter, Qt::AlignBottom};

		for (Qt::Alignment h : horizontal)
			for (Qt::Alignment v : vertical)
				for (qreal angle : {0.0, 90.0, 37.0})
					for (bool centre : {false, true}) {
						QGraphicsTextItem text(QStringLiteral("Motor protection switch Q12"));
						makeWrapping(text);
						text.setPos(40, -25);
						text.setRotation(angle);
						text.setTransformOriginPoint(centre ? text.boundingRect().center() : QPointF());

						const Qt::Alignment alignment = h | v;
						const QPointF anchor = TextAnchor::pos(&text, alignment);

						TextResize::applyWidth(&text, 60, alignment, true, centre);
						QVERIFY2(text.boundingRect().height() > 30, "the narrow text wraps");
						QVERIFY2(samePoint(TextAnchor::pos(&text, alignment), anchor),
								 qPrintable(describe(alignment, angle, centre)));

						TextResize::applyWidth(&text, -1, alignment, true, centre);
						QVERIFY2(samePoint(TextAnchor::pos(&text, alignment), anchor),
								 qPrintable(describe(alignment, angle, centre)));
					}
	}

	// Undoing a width change puts the text back exactly: the property
	// change alone restores the position, nothing drifts.
	void widthChangeAndBackRestoresPosition()
	{
		for (qreal angle : {0.0, 90.0, 37.0})
			for (bool centre : {false, true}) {
				QGraphicsTextItem text(QStringLiteral("Motor protection switch Q12"));
				makeWrapping(text);
				text.setPos(40, -25);
				text.setRotation(angle);
				text.setTransformOriginPoint(centre ? text.boundingRect().center() : QPointF());
				const QPointF start = text.pos();
				const QPointF origin = text.transformOriginPoint();

				const Qt::Alignment right = Qt::AlignTop | Qt::AlignRight;
				for (int i = 0 ; i < 5 ; ++i) {
					TextResize::applyWidth(&text, 70, right, true, centre);
					TextResize::applyWidth(&text, -1, right, true, centre);
				}
				QVERIFY2(samePoint(text.pos(), start), qPrintable(describe(right, angle, centre)));
				QVERIFY(samePoint(text.transformOriginPoint(), origin));
			}
	}

	// With the rotation point at the centre, the pivot follows the new
	// box, so a later rotation turns the text around its own centre.
	void centrePivotFollowsTheBox()
	{
		QGraphicsTextItem text(QStringLiteral("Motor protection switch Q12"));
		makeWrapping(text);
		text.setRotation(90);
		text.setTransformOriginPoint(text.boundingRect().center());

		TextResize::applyWidth(&text, 60, Qt::AlignTop | Qt::AlignLeft, true, true);
		QVERIFY(samePoint(text.transformOriginPoint(), text.boundingRect().center()));

		const QPointF centre = text.mapToScene(text.boundingRect().center());
		text.setRotation(180);
		QVERIFY(samePoint(text.mapToScene(text.boundingRect().center()), centre));
	}

	// While a text is loaded its saved position is set afterwards: the
	// width alone does not move it.
	void loadingDoesNotMove()
	{
		QGraphicsTextItem text(QStringLiteral("Motor protection switch Q12"));
		makeWrapping(text);
		text.setPos(40, -25);
		TextResize::applyWidth(&text, 60, Qt::AlignTop | Qt::AlignRight, false, false);
		QCOMPARE(text.pos(), QPointF(40, -25));
		QCOMPARE(text.textWidth(), 60.0);
	}


	// The minimum width never cuts a word, even for a document that would
	// otherwise break a word anywhere.
	void minimumWidthKeepsWords()
	{
		QGraphicsTextItem text(QStringLiteral("a Motorschutzschalter"));
		QTextOption option = text.document()->defaultTextOption();
		option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
		text.document()->setDefaultTextOption(option);

		QGraphicsTextItem word(QStringLiteral("Motorschutzschalter"));
		QVERIFY(qAbs(TextResize::minimumWidth(text.document()) - word.boundingRect().width()) < 0.5);
	}
};

QTEST_MAIN(tst_textresize)
#include "tst_textresize.moc"
