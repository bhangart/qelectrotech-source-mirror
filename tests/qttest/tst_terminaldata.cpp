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
#include "properties/terminaldata.h"
#include "qetapp.h"

#include <QDomDocument>
#include <QGraphicsObject>
#include <QPainter>
#include <QTest>

/**
	terminaldata.cpp, qet.cpp and qetutils.cpp need these few symbols from
	the application; standing in for them here is what lets TerminalData
	be tested without linking (or starting) the whole of QElectroTech.
*/
QString QETApp::m_interface_language;

QFont QETApp::diagramTextsFont(qreal)
{
	return QFont();
}

QETApp *QETApp::instance()
{
	return nullptr;
}

QETDiagramEditor *QETApp::diagramEditorAncestorOf(const QWidget *)
{
	return nullptr;
}

namespace {
	/// TerminalData::toXml() reads the position from its parent item
	class Part : public QGraphicsObject
	{
		public:
			QRectF boundingRect() const override { return {}; }
			void paint(QPainter *, const QStyleOptionGraphicsItem *,
				   QWidget *) override {}
	};
}

// TerminalData::fromXml() and toXml(): the path every <terminal> of an
// element definition takes through the element editor (PartTerminal) and
// into a placed element (Element::parseTerminal). Checked here on its own,
// so the optional `class` attribute is seen to survive the real reader and
// writer, not only a project file stored verbatim.
class tst_terminaldata : public QObject
{
	Q_OBJECT

	// read @p attributes into a TerminalData, then write it back out
	static QDomElement roundTrip(const QString &attributes, TerminalData *read = nullptr)
	{
		QDomDocument in;
		const QString xml = QStringLiteral("<terminal x=\"10\" y=\"-5\" "
			"orientation=\"e\" type=\"Generic\" %1/>").arg(attributes);
		if (!in.setContent(xml))
			return {};

		Part part;
		TerminalData data(&part);
		if (!data.fromXml(in.documentElement()))
			return {};
		part.setPos(data.m_pos);
		if (read)
			*read = data;

		QDomDocument out;
		return data.toXml(out);
	}

private slots:
	// What was read is written back exactly, a value this build does not
	// know included, and an empty value writes no attribute at all.
	void classRoundTrip_data()
	{
		QTest::addColumn<QString>("attributes");
		QTest::addColumn<bool>("written");
		QTest::addColumn<QString>("value");

		QTest::newRow("known")    << "class=\"hydraulic\"" << true  << "hydraulic";
		QTest::newRow("unknown")  << "class=\"steam\""     << true  << "steam";
		QTest::newRow("absent")   << ""                    << false << "";
		QTest::newRow("empty")    << "class=\"\""          << false << "";
	}

	void classRoundTrip()
	{
		QFETCH(QString, attributes);
		QFETCH(bool, written);
		QFETCH(QString, value);

		const QDomElement out = roundTrip(attributes);
		QVERIFY2(!out.isNull(), "the terminal did not read back");
		QCOMPARE(out.hasAttribute(QStringLiteral("class")), written);
		QCOMPARE(out.attribute(QStringLiteral("class")), value);
	}

	void terminalClass_data()
	{
		QTest::addColumn<QString>("attributes");
		QTest::addColumn<int>("expected");

		QTest::newRow("electrical") << "class=\"electrical\"" << int(TerminalClass::Electrical);
		QTest::newRow("air")        << "class=\"air\""        << int(TerminalClass::Air);
		QTest::newRow("unknown")    << "class=\"steam\""      << int(TerminalClass::Unknown);
		QTest::newRow("absent")     << ""                     << int(TerminalClass::Unspecified);
	}

	void terminalClass()
	{
		QFETCH(QString, attributes);
		QFETCH(int, expected);

		TerminalData read;
		QVERIFY(!roundTrip(attributes, &read).isNull());
		QCOMPARE(int(read.terminalClass()), expected);
	}

	// The attribute changes nothing else the terminal writes.
	void otherAttributesKept()
	{
		const QDomElement out = roundTrip(QStringLiteral(
			"class=\"gas\" name=\"PE\" "
			"uuid=\"{3f2504e0-4f89-11d3-9a0c-0305e82c3301}\""));
		QVERIFY(!out.isNull());
		QCOMPARE(out.attribute(QStringLiteral("x")), QStringLiteral("10"));
		QCOMPARE(out.attribute(QStringLiteral("y")), QStringLiteral("-5"));
		QCOMPARE(out.attribute(QStringLiteral("orientation")), QStringLiteral("e"));
		QCOMPARE(out.attribute(QStringLiteral("type")), QStringLiteral("Generic"));
		QCOMPARE(out.attribute(QStringLiteral("name")), QStringLiteral("PE"));
		QCOMPARE(out.attribute(QStringLiteral("uuid")),
			 QStringLiteral("{3f2504e0-4f89-11d3-9a0c-0305e82c3301}"));
	}
};

QTEST_MAIN(tst_terminaldata)

#include "tst_terminaldata.moc"
