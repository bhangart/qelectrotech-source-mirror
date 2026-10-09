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
#include "statictextwidth.h"
#include "textlines.h"

#include <QtTest>
#include <QDomDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QTextOption>

#include <limits>

/**
	A static text of a symbol (<text> in a .elmt) can have a width it wraps
	to, its text_width attribute (statictextwidth.h). The folio draws it
	wrapped as the element editor does; fixtures/static_text_width.qet has a
	symbol with one text per case, the five words of each text starting
	with the same four letters.
*/
class tst_statictextwidth : public QObject
{
	Q_OBJECT

	QTemporaryDir m_dir;
	QStringList m_dxf;

	/// Run the qelectrotech binary on the fixture project
	bool runQet(const QString &option)
	{
		QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
		env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
		QProcess proc;
		proc.setProcessEnvironment(env);
		proc.start(QStringLiteral(QET_TEST_BINARY_PATH),
				   {option, QFINDTESTDATA("fixtures/static_text_width.qet"), m_dir.path()});
		return proc.waitForFinished(60000)
				&& proc.exitStatus() == QProcess::NormalExit
				&& proc.exitCode() == 0;
	}

	/// The x of each DXF TEXT entity whose text starts with prefix, in
	/// order. scale: the DXF units of one pixel, from the text height of
	/// the 9 pt font of the fixture.
	QList<qreal> dxfLines(const QString &prefix, qreal *scale = nullptr) const
	{
		QList<qreal> xs;
		for (int i = 0 ; i + 1 < m_dxf.size() ; i += 2)
		{
			if (m_dxf.at(i).trimmed() != QLatin1String("0")
				|| m_dxf.at(i + 1).trimmed() != QLatin1String("TEXT"))
				continue;
			qreal x = 0;
			QString text;
			for (int j = i + 2 ; j + 1 < m_dxf.size() && m_dxf.at(j).trimmed() != QLatin1String("0") ; j += 2) {
				if (m_dxf.at(j).trimmed() == QLatin1String("10"))
					x = m_dxf.at(j + 1).trimmed().toDouble();
				else if (m_dxf.at(j).trimmed() == QLatin1String("1"))
					text = m_dxf.at(j + 1).trimmed();
				else if (scale && m_dxf.at(j).trimmed() == QLatin1String("40"))
					*scale = m_dxf.at(j + 1).trimmed().toDouble() / 9;
			}
			if (text.startsWith(prefix))
				xs << x;
		}
		return xs;
	}

	/// A document laid out as the element editor lays out a static text
	static void editorLayout(QTextDocument &document, qreal width, Qt::Alignment alignment)
	{
		document.setDocumentMargin(StaticTextWidth::editorMargin);
		QTextOption option = document.defaultTextOption();
		option.setAlignment(alignment);
		option.setWrapMode(QTextOption::WordWrap);
		document.setDefaultTextOption(option);
		document.setTextWidth(width);
	}

	/// A document laid out as the folio lays out a static text
	static void folioLayout(QTextDocument &document, qreal width, Qt::Alignment alignment)
	{
		document.setDocumentMargin(0);
		QTextOption option = document.defaultTextOption();
		option.setAlignment(alignment);
		option.setWrapMode(QTextOption::WordWrap);
		document.setDefaultTextOption(option);
		document.setTextWidth(StaticTextWidth::lineWidth(width));
	}

private slots:
	void initTestCase()
	{
		QVERIFY(m_dir.isValid());
		QVERIFY2(QFile::exists(QStringLiteral(QET_TEST_BINARY_PATH)), "qelectrotech binary not found");
		QVERIFY2(!QFINDTESTDATA("fixtures/static_text_width.qet").isEmpty(), "fixture project not found");

		QVERIFY2(runQet(QStringLiteral("--export-dxf")), "--export-dxf failed");
		QFile file(m_dir.filePath(QStringLiteral("01_diagram.dxf")));
		QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
		m_dxf = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
	}

		/// -1 is the automatic width, for every value that is not a width
	void normalized_data()
	{
		QTest::addColumn<qreal>("width");
		QTest::addColumn<qreal>("expected");
		QTest::newRow("0") << qreal(0) << qreal(-1);
		QTest::newRow("-1") << qreal(-1) << qreal(-1);
		QTest::newRow("-0.5") << qreal(-0.5) << qreal(-1);
		QTest::newRow("nan") << std::numeric_limits<qreal>::quiet_NaN() << qreal(-1);
		QTest::newRow("inf") << std::numeric_limits<qreal>::infinity() << qreal(-1);
		QTest::newRow("-inf") << -std::numeric_limits<qreal>::infinity() << qreal(-1);
		QTest::newRow("0.5") << qreal(0.5) << qreal(0.5);
		QTest::newRow("61.3") << qreal(61.3) << qreal(61.3);
		QTest::newRow("61.7") << qreal(61.7) << qreal(61.7);
		QTest::newRow("80") << qreal(80) << qreal(80);
		QTest::newRow("1e6") << qreal(1e6) << qreal(1e6);
	}

	void normalized()
	{
		QFETCH(qreal, width);
		QFETCH(qreal, expected);
		QCOMPARE(StaticTextWidth::normalized(width), expected);
	}

		/// The attribute is written only for a width, read back unchanged
	void xml_data()
	{
		QTest::addColumn<QString>("saved");	//empty: no attribute
		QTest::addColumn<qreal>("width");
		QTest::addColumn<QString>("written");	//empty: no attribute
		QTest::newRow("absent") << QString() << qreal(-1) << QString();
		QTest::newRow("-1") << QStringLiteral("-1") << qreal(-1) << QString();
		QTest::newRow("0") << QStringLiteral("0") << qreal(-1) << QString();
		QTest::newRow("nan") << QStringLiteral("nan") << qreal(-1) << QString();
		QTest::newRow("inf") << QStringLiteral("inf") << qreal(-1) << QString();
		QTest::newRow("text") << QStringLiteral("wide") << qreal(-1) << QString();
		QTest::newRow("61.3") << QStringLiteral("61.3") << qreal(61.3) << QStringLiteral("61.3");
		QTest::newRow("61.7") << QStringLiteral("61.7") << qreal(61.7) << QStringLiteral("61.7");
		QTest::newRow("80") << QStringLiteral("80") << qreal(80) << QStringLiteral("80");
	}

	void xml()
	{
		QFETCH(QString, saved);
		QFETCH(qreal, width);
		QFETCH(QString, written);

		QDomDocument document;
		QDomElement in = document.createElement(QStringLiteral("text"));
		if (!saved.isNull())
			in.setAttribute(QStringLiteral("text_width"), saved);
		QCOMPARE(StaticTextWidth::fromXml(in), width);

		QDomElement out = document.createElement(QStringLiteral("text"));
		StaticTextWidth::toXml(out, StaticTextWidth::fromXml(in));
		QCOMPARE(out.hasAttribute(QStringLiteral("text_width")), !written.isNull());
		QCOMPARE(out.attribute(QStringLiteral("text_width")), written.isNull() ? QString() : written);
		QCOMPARE(StaticTextWidth::fromXml(out), width);
	}

		/// The folio, without margin, breaks the lines where the element
		/// editor, with its margins, does
	void sameLinesAsTheEditor_data()
	{
		QTest::addColumn<qreal>("width");
		QTest::addColumn<int>("alignment");
		const QList<qreal> widths {30, 61.3, 61.7, 80, 90, 117, 150};
		for (qreal width : widths) {
			QTest::newRow(qPrintable(QStringLiteral("%1 left").arg(width))) << width << int(Qt::AlignLeft);
			QTest::newRow(qPrintable(QStringLiteral("%1 centre").arg(width))) << width << int(Qt::AlignHCenter);
			QTest::newRow(qPrintable(QStringLiteral("%1 right").arg(width))) << width << int(Qt::AlignRight);
		}
	}

	void sameLinesAsTheEditor()
	{
		QFETCH(qreal, width);
		QFETCH(int, alignment);
		const QString text = QStringLiteral("Lorem ipsum dolor sit amet, consectetur "
											"adipiscing elit, sed do eiusmod tempor\n"
											"incididunt ut labore et dolore magna aliqua");
		QFont font(QStringLiteral("Liberation Sans"));
		font.setPointSizeF(9);

		QTextDocument editor;
		editor.setDefaultFont(font);
		editor.setPlainText(text);
		editorLayout(editor, width, Qt::Alignment(alignment));

		QTextDocument folio;
		folio.setDefaultFont(font);
		folio.setPlainText(text);
		folioLayout(folio, width, Qt::Alignment(alignment));

		const QStringList lines = TextLines::layoutLines(&folio);
		QVERIFY(lines.size() > 2);
		QCOMPARE(lines, TextLines::layoutLines(&editor));
	}

		/// One DXF line per line drawn; one line without a width
	void wrappedLines_data()
	{
		QTest::addColumn<QString>("prefix");
		QTest::addColumn<bool>("wrapped");
		QTest::newRow("automatic") << QStringLiteral("A000") << false;
		QTest::newRow("61.3") << QStringLiteral("W613") << true;
		QTest::newRow("61.7") << QStringLiteral("W617") << true;
		QTest::newRow("80") << QStringLiteral("W800") << true;
		QTest::newRow("90") << QStringLiteral("W900") << true;
		QTest::newRow("0") << QStringLiteral("Zero") << false;
		QTest::newRow("-1") << QStringLiteral("Negs") << false;
		QTest::newRow("nan") << QStringLiteral("Nans") << false;
		QTest::newRow("inf") << QStringLiteral("Infs") << false;
		QTest::newRow("rotation 90") << QStringLiteral("R090") << true;
		QTest::newRow("rotation 37") << QStringLiteral("R037") << true;
		QTest::newRow("anchored left") << QStringLiteral("Left") << true;
		QTest::newRow("anchored right") << QStringLiteral("Rght") << true;
		QTest::newRow("anchored centre") << QStringLiteral("Cent") << true;
	}

	void wrappedLines()
	{
		QFETCH(QString, prefix);
		QFETCH(bool, wrapped);

		const int lines = dxfLines(prefix).size();
		if (wrapped)
			QVERIFY2(lines > 1, qPrintable(QStringLiteral("%1 line(s)").arg(lines)));
		else
			QCOMPARE(lines, 1);
	}

		/// A wider text has no more lines than a narrower one
	void widerHasFewerLines()
	{
		const int w613 = dxfLines(QStringLiteral("W613")).size();
		const int w617 = dxfLines(QStringLiteral("W617")).size();
		const int w800 = dxfLines(QStringLiteral("W800")).size();
		const int w900 = dxfLines(QStringLiteral("W900")).size();
		QVERIFY(w613 >= w617);
		QVERIFY(w617 >= w800);
		QVERIFY(w800 >= w900);
	}

		/// With anchor="alignment", x is the right edge or the centre of
		/// the box of the width (80 = 78 px of line and the editor's two
		/// 1 px margins), not of the text on one line
	void anchoredToTheBox()
	{
		qreal scale = 0;
		const QList<qreal> left = dxfLines(QStringLiteral("Left"), &scale);
		const QList<qreal> right = dxfLines(QStringLiteral("Rght"));
		const QList<qreal> centre = dxfLines(QStringLiteral("Cent"));
		QVERIFY(!left.isEmpty() && !right.isEmpty() && !centre.isEmpty());
		QVERIFY(scale > 0);
		const qreal to_right = (left.first() - right.first()) / scale;
		const qreal to_centre = (left.first() - centre.first()) / scale;
		QVERIFY2(qAbs(to_right - 78) < 0.01, qPrintable(QString::number(to_right)));
		QVERIFY2(qAbs(to_centre - 39) < 0.01, qPrintable(QString::number(to_centre)));
	}

		/// The folio draws the words of a wrapped text on several lines,
		/// those of a text without a width on one
	void drawnWrapped()
	{
		QVERIFY2(runQet(QStringLiteral("--export-svg")), "--export-svg failed");
		QFile file(m_dir.filePath(QStringLiteral("01_diagram.svg")));
		QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
		const QString svg = QString::fromUtf8(file.readAll());

		auto drawnLines = [&svg](const QString &prefix) {
			const QRegularExpression text_re(
				QStringLiteral("<text\\b[^>]*\\by=\"([^\"]+)\"[^>]*>\\s*%1").arg(prefix));
			QSet<QString> ys;
			auto it = text_re.globalMatch(svg);
			while (it.hasNext())
				ys.insert(it.next().captured(1));
			return ys.size();
		};
		QCOMPARE(drawnLines(QStringLiteral("A000")), 1);
		QCOMPARE(drawnLines(QStringLiteral("Nans")), 1);
		QVERIFY(drawnLines(QStringLiteral("W800")) > 1);
		QVERIFY(drawnLines(QStringLiteral("W613")) > 1);
	}
};

QTEST_MAIN(tst_statictextwidth)
#include "tst_statictextwidth.moc"
