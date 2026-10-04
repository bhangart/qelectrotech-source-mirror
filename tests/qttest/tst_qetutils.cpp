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
#include "qet.h"
#include "qetapp.h"
#include "qetversion.h"

#include <QDomDocument>
#include <QTest>

/**
	qet.cpp needs this one symbol from the application; standing in for it
	here is what lets its helpers be tested without linking (or starting)
	the whole of QElectroTech.
*/
QString QETApp::m_interface_language;

Q_DECLARE_METATYPE(Qet::Orientation)
Q_DECLARE_METATYPE(QET::DiagramArea)

// The small pure helpers of qet.cpp that tst_qetstrings does not reach:
// the attribute checks every element and project reader relies on (a
// coordinate that is not a finite number must be refused, or loading
// hangs), the string forms of orientations and diagram areas as files
// store them, angle normalisation and the point-on-segment geometry
// behind conductor editing. And QetVersion, which decides whether a
// project or element is older than 0.6 or newer than this build.
class tst_qetutils : public QObject
{
	Q_OBJECT

	QDomDocument m_doc;

	// an element <e> with one attribute a="value", or none when value is null
	QDomElement element(const QString &value)
	{
		QDomElement e = m_doc.createElement("e");
		if (!value.isNull())
			e.setAttribute("a", value);
		return e;
	}

private slots:
	void attributeIsAnInteger_data()
	{
		QTest::addColumn<QString>("value");
		QTest::addColumn<bool>("ok");
		QTest::addColumn<int>("expected");

		QTest::newRow("positive") << "12" << true << 12;
		QTest::newRow("negative") << "-3" << true << -3;
		QTest::newRow("missing")  << QString() << false << -1;
		QTest::newRow("empty")    << "" << false << -1;
		QTest::newRow("text")     << "abc" << false << -1;
		QTest::newRow("real")     << "1.5" << false << -1;
	}

	/// on failure the caller's value is left alone
	void attributeIsAnInteger()
	{
		QFETCH(QString, value);
		QFETCH(bool, ok);
		QFETCH(int, expected);

		int read = -1;
		QCOMPARE(QET::attributeIsAnInteger(element(value), "a", &read), ok);
		QCOMPARE(read, expected);
	}

	void attributeIsAReal_data()
	{
		QTest::addColumn<QString>("value");
		QTest::addColumn<bool>("ok");
		QTest::addColumn<qreal>("expected");

		QTest::newRow("decimal")  << "1.5" << true << qreal(1.5);
		QTest::newRow("negative") << "-0.25" << true << qreal(-0.25);
		QTest::newRow("integer")  << "3" << true << qreal(3);
		QTest::newRow("missing")  << QString() << false << qreal(-1);
		QTest::newRow("empty")    << "" << false << qreal(-1);
		QTest::newRow("comma")    << "1,5" << false << qreal(-1);
		// toDouble() parses these, but a position that is not finite
		// hangs Conductor::shape() while the project loads
		QTest::newRow("nan")      << "nan" << false << qreal(-1);
		QTest::newRow("inf")      << "inf" << false << qreal(-1);
		QTest::newRow("-inf")     << "-inf" << false << qreal(-1);
	}

	void attributeIsAReal()
	{
		QFETCH(QString, value);
		QFETCH(bool, ok);
		QFETCH(qreal, expected);

		qreal read = -1;
		QCOMPARE(QET::attributeIsAReal(element(value), "a", &read), ok);
		QCOMPARE(read, expected);
	}

	/// only geometry attributes count, polygon points (x1, y12...) included
	void hasNonFiniteGeometry()
	{
		QDomElement e = m_doc.createElement("line");
		e.setAttribute("x1", "0");
		e.setAttribute("length1", "1.5");
		e.setAttribute("name", "nan");
		QVERIFY(!QET::hasNonFiniteGeometry(e));

		e.setAttribute("y2", "inf");
		QVERIFY(QET::hasNonFiniteGeometry(e));

		QDomElement polygon = m_doc.createElement("polygon");
		polygon.setAttribute("x12", "nan");
		QVERIFY(QET::hasNonFiniteGeometry(polygon));
	}

	void escapeSpaces_data()
	{
		QTest::addColumn<QString>("plain");
		QTest::addColumn<QString>("escaped");

		QTest::newRow("empty")      << "" << "";
		QTest::newRow("no space")   << "file.qet" << "file.qet";
		QTest::newRow("space")      << "my file.qet" << "my\\ file.qet";
		QTest::newRow("backslash")  << "a\\b" << "a\\\\b";
		// the backslash is escaped first, so its space stays distinct
		QTest::newRow("backslash then space") << "a\\ b" << "a\\\\\\ b";
	}

	void escapeSpaces()
	{
		QFETCH(QString, plain);
		QFETCH(QString, escaped);

		QCOMPARE(QET::escapeSpaces(plain), escaped);
		QCOMPARE(QET::unescapeSpaces(escaped), plain);
	}

	void correctAngle_data()
	{
		QTest::addColumn<qreal>("angle");
		QTest::addColumn<bool>("positive");
		QTest::addColumn<qreal>("expected");

		QTest::newRow("0")           << qreal(0) << false << qreal(0);
		QTest::newRow("90")          << qreal(90) << false << qreal(90);
		QTest::newRow("359.5")       << qreal(359.5) << false << qreal(359.5);
		QTest::newRow("360")         << qreal(360) << false << qreal(0);
		QTest::newRow("810")         << qreal(810) << false << qreal(90);
		// negative angles above -360 are kept unless asked for positive
		QTest::newRow("-90")         << qreal(-90) << false << qreal(-90);
		QTest::newRow("-360")        << qreal(-360) << false << qreal(0);
		QTest::newRow("-450")        << qreal(-450) << false << qreal(-90);
		QTest::newRow("-90 positive")  << qreal(-90) << true << qreal(270);
		QTest::newRow("-720 positive") << qreal(-720) << true << qreal(0);
		QTest::newRow("725 positive")  << qreal(725) << true << qreal(5);
	}

	void correctAngle()
	{
		QFETCH(qreal, angle);
		QFETCH(bool, positive);
		QFETCH(qreal, expected);

		QCOMPARE(QET::correctAngle(angle, positive), expected);
	}

	void lineContainsPoint_data()
	{
		QTest::addColumn<QLineF>("line");
		QTest::addColumn<QPointF>("point");
		QTest::addColumn<bool>("contains");

		const QLineF h(0, 0, 10, 0);
		QTest::newRow("start")        << h << QPointF(0, 0) << true;
		QTest::newRow("end")          << h << QPointF(10, 0) << true;
		QTest::newRow("middle")       << h << QPointF(4, 0) << true;
		QTest::newRow("past the end") << h << QPointF(11, 0) << false;
		QTest::newRow("before start") << h << QPointF(-1, 0) << false;
		QTest::newRow("beside")       << h << QPointF(5, 1) << false;
		QTest::newRow("diagonal")     << QLineF(0, 0, 4, 4) << QPointF(2, 2) << true;
		QTest::newRow("reversed")     << QLineF(10, 0, 0, 0) << QPointF(3, 0) << true;
	}

	void lineContainsPoint()
	{
		QFETCH(QLineF, line);
		QFETCH(QPointF, point);
		QFETCH(bool, contains);

		QCOMPARE(QET::lineContainsPoint(line, point), contains);
	}

	/// the projection is handed back even when it falls outside the segment
	void orthogonalProjection()
	{
		const QLineF line(0, 0, 10, 0);
		QPointF p;
		QVERIFY(QET::orthogonalProjection(QPointF(4, 7), line, &p));
		QCOMPARE(p, QPointF(4, 0));

		QVERIFY(!QET::orthogonalProjection(QPointF(15, -3), line, &p));
		QCOMPARE(p, QPointF(15, 0));

		QVERIFY(QET::orthogonalProjection(QPointF(0, 4), QLineF(0, 0, 4, 4), &p));
		QCOMPARE(p, QPointF(2, 2));
	}

	void diagramArea_data()
	{
		QTest::addColumn<QString>("string");
		QTest::addColumn<QET::DiagramArea>("area");

		QTest::newRow("border")   << "border" << QET::BorderArea;
		QTest::newRow("BORDER")   << "BORDER" << QET::BorderArea;
		QTest::newRow("elements") << "elements" << QET::ElementsArea;
		// anything that is not "border" is the elements area
		QTest::newRow("empty")    << "" << QET::ElementsArea;
		QTest::newRow("unknown")  << "folio" << QET::ElementsArea;
	}

	void diagramArea()
	{
		QFETCH(QString, string);
		QFETCH(QET::DiagramArea, area);

		QCOMPARE(QET::diagramAreaFromString(string), area);
		QCOMPARE(QET::diagramAreaFromString(QET::diagramAreaToString(area)), area);
	}

	void orientationFromString_data()
	{
		QTest::addColumn<QString>("string");
		QTest::addColumn<Qet::Orientation>("orientation");

		QTest::newRow("n") << "n" << Qet::North;
		QTest::newRow("e") << "e" << Qet::East;
		QTest::newRow("s") << "s" << Qet::South;
		QTest::newRow("w") << "w" << Qet::West;
		// only the first character is read
		QTest::newRow("east") << "east" << Qet::East;
		// unknown or upper case: North
		QTest::newRow("x") << "x" << Qet::North;
		QTest::newRow("E") << "E" << Qet::North;
	}

	void orientationFromString()
	{
		QFETCH(QString, string);
		QFETCH(Qet::Orientation, orientation);

		QCOMPARE(Qet::orientationFromString(string), orientation);
	}

	/// each orientation, written and read back; next/previous go round
	void orientationRoundTrips()
	{
		const QList<Qet::Orientation> all {
			Qet::North, Qet::East, Qet::South, Qet::West};
		for (int i = 0 ; i < all.size() ; ++i) {
			const Qet::Orientation o = all.at(i);
			QCOMPARE(Qet::orientationFromString(Qet::orientationToString(o)), o);
			QCOMPARE(Qet::nextOrientation(o), all.at((i + 1) % 4));
			QCOMPARE(Qet::previousOrientation(o), all.at((i + 3) % 4));
			QCOMPARE(Qet::isOpposed(o, all.at((i + 2) % 4)), true);
			QCOMPARE(Qet::isOpposed(o, all.at((i + 1) % 4)), false);
			QCOMPARE(Qet::isOpposed(o, o), false);
			QCOMPARE(Qet::surLeMemeAxe(o, all.at((i + 2) % 4)), true);
			QCOMPARE(Qet::surLeMemeAxe(o, all.at((i + 1) % 4)), false);
			QCOMPARE(Qet::isHorizontal(o), o == Qet::East || o == Qet::West);
			QCOMPARE(Qet::isVertical(o), !Qet::isHorizontal(o));
		}
	}

	void endTypeAndCollectionStrings()
	{
		for (auto t : {Qet::None, Qet::Simple, Qet::Triangle, Qet::Circle,
					   Qet::Diamond})
			QCOMPARE(Qet::endTypeFromString(Qet::endTypeToString(t)), t);
		QCOMPARE(Qet::endTypeFromString("arrow"), Qet::None);

		for (auto c : {QET::Common, QET::Company, QET::Custom, QET::Embedded})
			QCOMPARE(QET::qetCollectionFromString(QET::qetCollectionToString(c)), c);
		QCOMPARE(QET::qetCollectionFromString("Company"), QET::Common);
	}

	void infoFlagIsTrue_data()
	{
		QTest::addColumn<QString>("value");
		QTest::addColumn<bool>("isTrue");

		QTest::newRow("true")     << "true" << true;
		QTest::newRow("1")        << "1" << true;
		QTest::newRow("yes")      << "yes" << true;
		QTest::newRow("on")       << "on" << true;
		QTest::newRow(" TRUE ")   << " TRUE " << true;
		QTest::newRow("false")    << "false" << false;
		QTest::newRow("0")        << "0" << false;
		QTest::newRow("empty")    << "" << false;
	}

	void infoFlagIsTrue()
	{
		QFETCH(QString, value);
		QFETCH(bool, isTrue);
		QCOMPARE(QET::infoFlagIsTrue(value), isTrue);
	}

	void versionFromXmlAttribute_data()
	{
		QTest::addColumn<QString>("value");
		QTest::addColumn<QVersionNumber>("version");

		QTest::newRow("missing")   << QString() << QVersionNumber();
		QTest::newRow("empty")     << "" << QVersionNumber();
		QTest::newRow("garbage")   << "abc" << QVersionNumber();
		QTest::newRow("0.80")      << "0.80" << QVersionNumber(0, 80);
		QTest::newRow("0.100.0")   << "0.100.0" << QVersionNumber(0, 100, 0);
		// a suffix ends the number rather than spoiling it
		QTest::newRow("0.90-dev")  << "0.90-dev" << QVersionNumber(0, 90);
	}

	void versionFromXmlAttribute()
	{
		QFETCH(QString, value);
		QFETCH(QVersionNumber, version);

		QDomElement e = m_doc.createElement("project");
		if (!value.isNull())
			e.setAttribute("version", value);
		QCOMPARE(QetVersion::fromXmlAttribute(e), version);
	}

	/// what this build writes it reads back as itself, and it is newer
	/// than 0.6, the oldest version QetProject still opens without warning
	void versionRoundTrips()
	{
		QDomElement e = m_doc.createElement("project");
		QetVersion::toXmlAttribute(e);
		QCOMPARE(QetVersion::fromXmlAttribute(e), QetVersion::currentVersion());
		QVERIFY(QetVersion::versionZeroDotSix() < QetVersion::currentVersion());
		QVERIFY(QetVersion::displayedVersion().startsWith(
					QetVersion::currentVersion().toString()));

		// the comparisons QetProject makes on what it reads
		QVERIFY(QVersionNumber::fromString("0.5") <= QetVersion::versionZeroDotSix());
		QVERIFY(QVersionNumber::fromString("0.60") <= QetVersion::versionZeroDotSix());
		QVERIFY(!(QVersionNumber::fromString("0.80") <= QetVersion::versionZeroDotSix()));
	}
};

QTEST_GUILESS_MAIN(tst_qetutils)
#include "tst_qetutils.moc"
