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
#include "qetxml.h"
#include "qetapp.h"

#include <QDomDocument>
#include <QTest>

/**
	nameslist.cpp and qetutils.cpp, which qetxml.cpp calls into, need these
	few symbols from the application; standing in for them here is what
	lets the QETXML helpers be tested without linking (or starting) the
	whole of QElectroTech. None of the functions tested below reaches them.
*/

QString QETApp::langFromSetting()
{
	return QStringLiteral("en");
}

QETApp *QETApp::instance()
{
	return nullptr;
}

QETDiagramEditor *QETApp::diagramEditorAncestorOf(const QWidget *)
{
	return nullptr;
}

Q_DECLARE_METATYPE(QETXML::PropertyFlags)

// The QETXML helpers read and write the properties, pens, margins and
// attributes of nearly every object a project or element file stores. A
// reader that accepts a bad value, or reports a missing property as found,
// quietly changes what a file opens as; a writer whose output its own
// reader does not take back loses the value on the next save. These cases
// pin down what each reader accepts, what it reports when it does not
// (NotFound / NoValidConversion), and that each writer round-trips.
class tst_qetxml : public QObject
{
	Q_OBJECT

	QDomDocument m_doc;

	// an element <e> holding the given attributes and child elements
	QDomElement parse(const QString &xml)
	{
		QDomDocument doc;
		const auto result = doc.setContent(xml);
		if (!result)
			qFatal("bad fixture: %s", qPrintable(xml));
		m_doc = doc;
		return doc.documentElement();
	}

private slots:
	void propertyInteger_data()
	{
		QTest::addColumn<QString>("value");
		QTest::addColumn<QETXML::PropertyFlags>("flag");
		QTest::addColumn<int>("expected");

		QTest::newRow("positive") << "42" << QETXML::Success << 42;
		QTest::newRow("negative") << "-7" << QETXML::Success << -7;
		QTest::newRow("zero")     << "0" << QETXML::Success << 0;
		QTest::newRow("empty")    << "" << QETXML::NoValidConversion << -1;
		QTest::newRow("text")     << "abc" << QETXML::NoValidConversion << -1;
		QTest::newRow("real")     << "3.5" << QETXML::NoValidConversion << -1;
		// larger than an int: rejected rather than truncated
		QTest::newRow("overflow") << "99999999999" << QETXML::NoValidConversion << -1;
	}

	/// a failed conversion leaves the caller's value untouched
	void propertyInteger()
	{
		QFETCH(QString, value);
		QFETCH(QETXML::PropertyFlags, flag);
		QFETCH(int, expected);

		int read = -1;
		QCOMPARE(QETXML::propertyInteger(value, &read), flag);
		QCOMPARE(read, expected);
	}

	void propertyDouble_data()
	{
		QTest::addColumn<QString>("value");
		QTest::addColumn<QETXML::PropertyFlags>("flag");
		QTest::addColumn<double>("expected");

		QTest::newRow("decimal")     << "1.5" << QETXML::Success << 1.5;
		QTest::newRow("exponent")    << "-2e3" << QETXML::Success << -2000.0;
		QTest::newRow("integer")     << "12" << QETXML::Success << 12.0;
		// files are written with a dot whatever the locale
		QTest::newRow("comma")       << "1,5" << QETXML::NoValidConversion << -1.0;
		QTest::newRow("empty")       << "" << QETXML::NoValidConversion << -1.0;
		QTest::newRow("text")        << "abc" << QETXML::NoValidConversion << -1.0;
	}

	void propertyDouble()
	{
		QFETCH(QString, value);
		QFETCH(QETXML::PropertyFlags, flag);
		QFETCH(double, expected);

		double read = -1.0;
		QCOMPARE(QETXML::propertyDouble(value, &read), flag);
		QCOMPARE(read, expected);
	}

	void propertyBool_data()
	{
		QTest::addColumn<QString>("value");
		QTest::addColumn<QETXML::PropertyFlags>("flag");
		QTest::addColumn<bool>("expected");

		QTest::newRow("true")  << "true" << QETXML::Success << true;
		QTest::newRow("false") << "false" << QETXML::Success << false;
		QTest::newRow("1")     << "1" << QETXML::Success << true;
		QTest::newRow("0")     << "0" << QETXML::Success << false;
		// read as an integer first: any non-zero number is true
		QTest::newRow("2")     << "2" << QETXML::Success << true;
		QTest::newRow("TRUE")  << "TRUE" << QETXML::NoValidConversion << false;
		QTest::newRow("yes")   << "yes" << QETXML::NoValidConversion << false;
		QTest::newRow("empty") << "" << QETXML::NoValidConversion << false;
	}

	void propertyBool()
	{
		QFETCH(QString, value);
		QFETCH(QETXML::PropertyFlags, flag);
		QFETCH(bool, expected);

		// start from the opposite so a write is seen
		bool read = !expected;
		QCOMPARE(QETXML::propertyBool(value, &read), flag);
		QCOMPARE(read, flag == QETXML::Success ? expected : !expected);
	}

	void propertyUuid_data()
	{
		QTest::addColumn<QString>("value");
		QTest::addColumn<QETXML::PropertyFlags>("flag");

		QTest::newRow("braces")
			<< "{8c3e7c4e-6a5b-4f0e-9a1d-2b3c4d5e6f70}" << QETXML::Success;
		QTest::newRow("no braces")
			<< "8c3e7c4e-6a5b-4f0e-9a1d-2b3c4d5e6f70" << QETXML::Success;
		// the nil uuid is what QUuid gives for anything it cannot parse,
		// so it is refused as well
		QTest::newRow("nil")
			<< "{00000000-0000-0000-0000-000000000000}"
			<< QETXML::NoValidConversion;
		QTest::newRow("garbage") << "not-a-uuid" << QETXML::NoValidConversion;
		QTest::newRow("empty") << "" << QETXML::NoValidConversion;
	}

	void propertyUuid()
	{
		QFETCH(QString, value);
		QFETCH(QETXML::PropertyFlags, flag);

		QUuid read;
		QCOMPARE(QETXML::propertyUuid(value, &read), flag);
		QCOMPARE(read, flag == QETXML::Success ? QUuid(value) : QUuid());
	}

	void propertyColor_data()
	{
		QTest::addColumn<QString>("value");
		QTest::addColumn<QETXML::PropertyFlags>("flag");
		QTest::addColumn<QColor>("expected");

		QTest::newRow("hex")      << "#ff0000" << QETXML::Success << QColor(255, 0, 0);
		QTest::newRow("svg name") << "blue" << QETXML::Success << QColor(0, 0, 255);
		QTest::newRow("bad hex")  << "#12345" << QETXML::NoValidConversion << QColor();
		QTest::newRow("unknown")  << "notacolor" << QETXML::NoValidConversion << QColor();
		QTest::newRow("empty")    << "" << QETXML::NoValidConversion << QColor();
	}

	void propertyColor()
	{
		QFETCH(QString, value);
		QFETCH(QETXML::PropertyFlags, flag);
		QFETCH(QColor, expected);

		QColor read;
		QCOMPARE(QETXML::propertyColor(value, &read), flag);
		QCOMPARE(read, expected);
	}

	/// Reading from an element: a <property> child, else the legacy
	/// attribute of the same name, else NotFound.
	void elementReaders()
	{
		const QDomElement e = parse(QStringLiteral(
			"<e width=\"7\" legacy_only=\"3\" bad=\"x\">"
			"<property name=\"width\" type=\"int\" value=\"12\"/>"
			"<property name=\"typed\" type=\"string\" value=\"5\"/>"
			"<property name=\"broken\" type=\"int\" value=\"abc\"/>"
			"<property name=\"incomplete\" type=\"int\"/>"
			"</e>"));

		int i = -1;
		// the property child wins over the legacy attribute
		QCOMPARE(QETXML::propertyInteger(e, "width", &i), QETXML::Success);
		QCOMPARE(i, 12);
		QCOMPARE(QETXML::propertyInteger(e, "legacy_only", &i), QETXML::Success);
		QCOMPARE(i, 3);

		i = -1;
		QCOMPARE(QETXML::propertyInteger(e, "absent", &i), QETXML::NotFound);
		// a property of another type is reported as not found, not converted
		QCOMPARE(QETXML::propertyInteger(e, "typed", &i), QETXML::NotFound);
		QCOMPARE(QETXML::propertyInteger(e, "broken", &i),
				 QETXML::NoValidConversion);
		QCOMPARE(QETXML::propertyInteger(e, "bad", &i),
				 QETXML::NoValidConversion);
		// a child without a value is no property at all
		QCOMPARE(QETXML::propertyInteger(e, "incomplete", &i), QETXML::NotFound);
		QCOMPARE(i, -1);

		QString s;
		QCOMPARE(QETXML::propertyString(e, "typed", &s), QETXML::Success);
		QCOMPARE(s, QStringLiteral("5"));
		QCOMPARE(QETXML::propertyString(e, "absent", &s), QETXML::NotFound);
		QCOMPARE(QETXML::propertyString(e, "width", &s), QETXML::NotFound);

		QVERIFY(QETXML::property(e, "typed").hasAttribute("value"));
		QVERIFY(QETXML::property(e, "incomplete").isNull());
		QVERIFY(QETXML::property(e, "absent").isNull());
	}

	/// What each createXmlProperty() writes is read back by the reader of
	/// the same type, through a parent element as a file holds it.
	void createXmlPropertyRoundTrips()
	{
		const QUuid uuid = QUuid::createUuid();
		QDomElement parent = m_doc.createElement("parent");
		parent.appendChild(QETXML::createXmlProperty("i", -15));
		parent.appendChild(QETXML::createXmlProperty("d", 2.25));
		parent.appendChild(QETXML::createXmlProperty("t", true));
		parent.appendChild(QETXML::createXmlProperty("f", false));
		parent.appendChild(QETXML::createXmlProperty("u", uuid));
		parent.appendChild(QETXML::createXmlProperty("c", QColor("#123456")));
		parent.appendChild(QETXML::createXmlProperty("s", QString("a b")));
		parent.appendChild(QETXML::createXmlProperty("p", "plain"));

		int i = 0;
		double d = 0;
		bool t = false, f = true;
		QUuid u;
		QColor c;
		QString s, p;
		QCOMPARE(QETXML::propertyInteger(parent, "i", &i), QETXML::Success);
		QCOMPARE(QETXML::propertyDouble(parent, "d", &d), QETXML::Success);
		QCOMPARE(QETXML::propertyBool(parent, "t", &t), QETXML::Success);
		QCOMPARE(QETXML::propertyBool(parent, "f", &f), QETXML::Success);
		QCOMPARE(QETXML::propertyUuid(parent, "u", &u), QETXML::Success);
		QCOMPARE(QETXML::propertyColor(parent, "c", &c), QETXML::Success);
		QCOMPARE(QETXML::propertyString(parent, "s", &s), QETXML::Success);
		QCOMPARE(QETXML::propertyString(parent, "p", &p), QETXML::Success);

		QCOMPARE(i, -15);
		QCOMPARE(d, 2.25);
		QCOMPARE(t, true);
		QCOMPARE(f, false);
		QCOMPARE(u, uuid);
		QCOMPARE(c, QColor("#123456"));
		QCOMPARE(s, QStringLiteral("a b"));
		QCOMPARE(p, QStringLiteral("plain"));

		// what is actually stored for a bool and its type name
		const QDomElement b = QETXML::property(parent, "t");
		QCOMPARE(b.attribute("type"), QETXML::boolS);
		QCOMPARE(b.attribute("value"), QStringLiteral("1"));
	}

	void validXmlProperty_data()
	{
		QTest::addColumn<QString>("xml");
		QTest::addColumn<bool>("valid");

		QTest::newRow("complete")
			<< "<property name=\"n\" type=\"int\" value=\"1\"/>" << true;
		// an empty value is still a value
		QTest::newRow("empty value")
			<< "<property name=\"n\" type=\"string\" value=\"\"/>" << true;
		QTest::newRow("no name")
			<< "<property type=\"int\" value=\"1\"/>" << false;
		QTest::newRow("no type")
			<< "<property name=\"n\" value=\"1\"/>" << false;
		QTest::newRow("no value")
			<< "<property name=\"n\" type=\"int\"/>" << false;
	}

	void validXmlProperty()
	{
		QFETCH(QString, xml);
		QFETCH(bool, valid);
		QCOMPARE(QETXML::validXmlProperty(parse(xml)), valid);
	}

	void boolFromString_data()
	{
		QTest::addColumn<QString>("value");
		QTest::addColumn<bool>("default_value");
		QTest::addColumn<bool>("expected");
		QTest::addColumn<bool>("ok");

		QTest::newRow("true")    << "true" << false << true << true;
		QTest::newRow("1")       << "1" << false << true << true;
		QTest::newRow("false")   << "false" << true << false << true;
		QTest::newRow("0")       << "0" << true << false << true;
		// unlike propertyBool(), only the four exact spellings are known
		QTest::newRow("2")       << "2" << false << false << false;
		QTest::newRow("True")    << "True" << false << false << false;
		QTest::newRow("empty, default true")  << "" << true << true << false;
		QTest::newRow("empty, default false") << "" << false << false << false;
	}

	void boolFromString()
	{
		QFETCH(QString, value);
		QFETCH(bool, default_value);
		QFETCH(bool, expected);
		QFETCH(bool, ok);

		bool conv_ok = !ok;
		QCOMPARE(QETXML::boolFromString(value, default_value, &conv_ok), expected);
		QCOMPARE(conv_ok, ok);
	}

	void boolToStringRoundTrips()
	{
		QCOMPARE(QETXML::boolToString(true), QStringLiteral("true"));
		QCOMPARE(QETXML::boolToString(false), QStringLiteral("false"));
		QCOMPARE(QETXML::boolFromString(QETXML::boolToString(false)), false);
		QCOMPARE(QETXML::boolFromString(QETXML::boolToString(true), false), true);
	}

	void penRoundTrips_data()
	{
		QTest::addColumn<int>("style");

		QTest::newRow("solid")        << int(Qt::SolidLine);
		QTest::newRow("dash")         << int(Qt::DashLine);
		QTest::newRow("dot")          << int(Qt::DotLine);
		QTest::newRow("dash dot")     << int(Qt::DashDotLine);
		QTest::newRow("dash dot dot") << int(Qt::DashDotDotLine);
	}

	void penRoundTrips()
	{
		QFETCH(int, style);

		QPen pen(QColor("#00aa33"), 2.5, Qt::PenStyle(style));
		const QPen read = QETXML::penFromXml(QETXML::penToXml(m_doc, pen));
		QCOMPARE(read.style(), pen.style());
		QCOMPARE(read.color(), pen.color());
		QCOMPARE(read.widthF(), 2.5);
	}

	/// A pen that cannot be read is a dashed one, whatever went wrong:
	/// that is how the readers of shapes and wires have always drawn it.
	void penFromXmlFallsBackToDashLine()
	{
		QCOMPARE(QETXML::penFromXml(QDomElement()).style(), Qt::DashLine);
		QCOMPARE(QETXML::penFromXml(parse("<brush style=\"SolidLine\"/>")).style(),
				 Qt::DashLine);
		QCOMPARE(QETXML::penFromXml(parse("<pen style=\"Wavy\"/>")).style(),
				 Qt::DashLine);

		// no attribute at all: dashed, black, width 1
		const QPen bare = QETXML::penFromXml(parse("<pen/>"));
		QCOMPARE(bare.style(), Qt::DashLine);
		QCOMPARE(bare.color(), QColor(Qt::black));
		QCOMPARE(bare.widthF(), 1.0);

		// Qt::NoPen has no name of its own: written as Unknown, read dashed
		const QDomElement none = QETXML::penToXml(m_doc, QPen(Qt::NoPen));
		QCOMPARE(none.attribute("style"), QStringLiteral("Unknown"));
		QCOMPARE(QETXML::penFromXml(none).style(), Qt::DashLine);
	}

	void brushRoundTrips()
	{
		const QBrush brush(QColor("#abcdef"), Qt::Dense4Pattern);
		const QBrush read = QETXML::brushFromXml(QETXML::brushToXml(m_doc, brush));
		QCOMPARE(read.style(), Qt::Dense4Pattern);
		QCOMPARE(read.color(), QColor("#abcdef"));

		// unlike a pen, a brush that cannot be read is no brush
		QCOMPARE(QETXML::brushFromXml(QDomElement()).style(), Qt::NoBrush);
		QCOMPARE(QETXML::brushFromXml(parse("<brush/>")).style(), Qt::NoBrush);
	}

	void margins()
	{
		const QMargins m(1, -2, 30, 400);
		const QDomElement xml = QETXML::marginsToXml(m_doc, m);
		QCOMPARE(xml.tagName(), QStringLiteral("margins"));
		QCOMPARE(xml.text(), QStringLiteral("1;-2;30;400"));
		QCOMPARE(QETXML::marginsFromXml(xml), m);

		// wrong tag, wrong count: null margins rather than a partial read
		QCOMPARE(QETXML::marginsFromXml(parse("<padding>1;2;3;4</padding>")),
				 QMargins());
		QCOMPARE(QETXML::marginsFromXml(parse("<margins>1;2;3</margins>")),
				 QMargins());
		QCOMPARE(QETXML::marginsFromXml(parse("<margins>1;2;3;4;5</margins>")),
				 QMargins());
		// a field that is no number reads as 0, the others are kept
		QCOMPARE(QETXML::marginsFromXml(parse("<margins>1;x;3;4</margins>")),
				 QMargins(1, 0, 3, 4));
	}

	void orientationAttribute()
	{
		QDomElement e = m_doc.createElement("e");
		QETXML::orientationToAttribute(Qt::Horizontal, e);
		QCOMPARE(e.attribute("orientation"), QStringLiteral("Horizontal"));
		QCOMPARE(QETXML::orientationFromAttribute(e, Qt::Vertical), Qt::Horizontal);

		QETXML::orientationToAttribute(Qt::Vertical, e);
		QCOMPARE(QETXML::orientationFromAttribute(e, Qt::Horizontal), Qt::Vertical);

		// missing or unknown (the match is case-sensitive): the default
		QCOMPARE(QETXML::orientationFromAttribute(parse("<e/>"), Qt::Horizontal),
				 Qt::Horizontal);
		QCOMPARE(QETXML::orientationFromAttribute(parse("<e/>")), Qt::Vertical);
		QCOMPARE(QETXML::orientationFromAttribute(
					 parse("<e orientation=\"horizontal\"/>"), Qt::Vertical),
				 Qt::Vertical);
	}

	void alignmentRoundTrips_data()
	{
		QTest::addColumn<int>("alignment");

		QTest::newRow("left")          << int(Qt::AlignLeft);
		QTest::newRow("right")         << int(Qt::AlignRight);
		QTest::newRow("hcenter")       << int(Qt::AlignHCenter);
		QTest::newRow("justify")       << int(Qt::AlignJustify);
		QTest::newRow("top")           << int(Qt::AlignTop);
		QTest::newRow("baseline")      << int(Qt::AlignBaseline);
		QTest::newRow("left | top")    << int(Qt::AlignLeft | Qt::AlignTop);
		QTest::newRow("right | top")   << int(Qt::AlignRight | Qt::AlignTop);
		QTest::newRow("none")          << 0;
		// alignmentToAttribute() used to write VCenter for AlignBottom and
		// nothing for AlignVCenter, which terminal strips use by default
		QTest::newRow("bottom")        << int(Qt::AlignBottom);
		QTest::newRow("vcenter")       << int(Qt::AlignVCenter);
		QTest::newRow("center")        << int(Qt::AlignCenter);
		QTest::newRow("right | vcenter") << int(Qt::AlignRight | Qt::AlignVCenter);
		QTest::newRow("left | bottom") << int(Qt::AlignLeft | Qt::AlignBottom);
	}

	void alignmentRoundTrips()
	{
		QFETCH(int, alignment);

		QDomElement e = m_doc.createElement("e");
		QETXML::alignmentToAttribute(Qt::Alignment(alignment), e);
		QCOMPARE(QETXML::alignmentFromAttribute(e), Qt::Alignment(alignment));
	}

	void alignmentFromAttribute()
	{
		// no attribute: no flag
		QCOMPARE(QETXML::alignmentFromAttribute(parse("<e/>")), Qt::Alignment());
		// the words may come in any order; unknown words are ignored
		QCOMPARE(QETXML::alignmentFromAttribute(
					 parse("<e alignment=\"VCenter Middle HCenter\"/>")),
				 Qt::AlignVCenter | Qt::AlignHCenter);
	}
};

QTEST_GUILESS_MAIN(tst_qetxml)
#include "tst_qetxml.moc"
