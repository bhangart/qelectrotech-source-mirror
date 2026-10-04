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
#include "autoNum/assignvariables.h"
#include "autoNum/numerotationcontext.h"
#include "autoNum/numerotationcontextcommands.h"
#include "ElementsCollection/elementslocation.h"
#include "ElementsCollection/qetlabelsfile.h"
#include "diagram.h"
#include "qetapp.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/element.h"
#include "qetproject.h"

#include <QDomDocument>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

/**
	assignvariables.cpp also holds the paths that read a built folio
	(formulaToLabel() with a Diagram, genericXref(), setSequential() with
	folio parts, elementPrefixForLocation()), so it refers to Diagram,
	Element, Conductor, QETProject and the collection. None of these paths
	is run here: the formulas are given a FormulaContext instead, which is
	how the project database renders them too. Standing in for those
	symbols is what lets the numbering core be tested without linking (or
	starting) the whole of QElectroTech; each one stops the test if it is
	ever reached.
*/
#define QET_TEST_NOT_REACHED qFatal("%s: not reached by this test", Q_FUNC_INFO)

QString QETApp::m_interface_language;
QETApp *QETApp::instance() { return nullptr; }
QString QETApp::langFromSetting() { return QStringLiteral("en"); }
QETDiagramEditor *QETApp::diagramEditorAncestorOf(const QWidget *) { return nullptr; }
QString QETApp::commonElementsDir() { QET_TEST_NOT_REACHED; }
QString QETApp::customElementsDir() { QET_TEST_NOT_REACHED; }
QString QETApp::companyElementsDir() { QET_TEST_NOT_REACHED; }
QString QetLabelsFile::prefixForPath(const QString &, const QStringList &, int) { QET_TEST_NOT_REACHED; }
ElementsLocation::ElementsLocation(const ElementsLocation &) { QET_TEST_NOT_REACHED; }
ElementsLocation::~ElementsLocation() {}
ElementsLocation &ElementsLocation::operator=(const ElementsLocation &) { QET_TEST_NOT_REACHED; }
bool ElementsLocation::operator!=(const ElementsLocation &) const { QET_TEST_NOT_REACHED; }
ElementsLocation ElementsLocation::parent() const { QET_TEST_NOT_REACHED; }
QString ElementsLocation::fileName() const { QET_TEST_NOT_REACHED; }
bool ElementsLocation::isProject() const { QET_TEST_NOT_REACHED; }
DiagramContext QETProject::projectProperties() { QET_TEST_NOT_REACHED; }
DiagramPosition Diagram::convertPosition(const QPointF &) { QET_TEST_NOT_REACHED; }
int Diagram::folioIndex() const { QET_TEST_NOT_REACHED; }
QETProject *Diagram::project() const { QET_TEST_NOT_REACHED; }
Diagram *QetGraphicsItem::diagram() const { QET_TEST_NOT_REACHED; }
QString Element::getPrefix() const { QET_TEST_NOT_REACHED; }
ConductorProperties Conductor::properties() const { QET_TEST_NOT_REACHED; }

namespace {
	/// A context from parts written "type|value|increase|initialvalue|modulus|format"
	NumerotationContext make(const QStringList &parts)
	{
		NumerotationContext nc;
		for (const QString &part : parts) {
			const QStringList f = part.split(QLatin1Char('|'));
			nc.addValue(f.value(0), f.value(1), f.value(2).toInt(),
				    f.value(3).toInt(), f.value(4).toInt(), f.value(5));
		}
		return nc;
	}

	/// Every part of @p nc, as stored
	QStringList parts(const NumerotationContext &nc)
	{
		QStringList list;
		for (int i = 0; i < nc.size(); ++i)
			list << nc[i];
		return list;
	}

	/// The value of every part of @p nc
	QStringList values(const NumerotationContext &nc)
	{
		QStringList list;
		for (int i = 0; i < nc.size(); ++i)
			list << nc.itemAt(i).at(1);
		return list;
	}
}

// The autonumbering core under the element, conductor and folio numbering
// schemes: NumerotationContext (a scheme's parts, how they are stored, read
// and written, and how a part's value is padded), NumerotationContextCommands
// (one step forward or back, with the carry and borrow of cyclic "wrap"
// parts), and the formula side of autonum:: (a scheme turned into a formula,
// and a formula turned into a label). tst_elementautonumids checks the
// schemes through the binary; this checks the pieces it is made of, so a
// change to one of them is caught where it is made.
class tst_numerotationcontext : public QObject
{
	Q_OBJECT

	QTemporaryDir m_settings_dir;

private slots:
	// AssignVariables reads "border-columns_0" from QSettings: keep the
	// settings this test reads and writes in a directory of its own.
	void initTestCase()
	{
		QVERIFY(m_settings_dir.isValid());
		QCoreApplication::setOrganizationName(QStringLiteral("QElectroTech-tests"));
		QCoreApplication::setApplicationName(QStringLiteral("tst_numerotationcontext"));
		QSettings::setDefaultFormat(QSettings::IniFormat);
		QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings_dir.path());
	}

	// A part keeps its six fields; '|' is the field separator, so it is
	// taken out of the value and the format rather than shifting the fields.
	void addValueStoresSixFields()
	{
		NumerotationContext nc;
		QVERIFY(nc.isEmpty());
		QVERIFY(nc.addValue(QStringLiteral("hundredfolio"), 7, 2, 5, 0, QStringLiteral("0000")));
		QVERIFY(nc.addValue(QStringLiteral("string"), QStringLiteral("-K|1"), 1, 0, 0, QStringLiteral("0|0")));
		QCOMPARE(nc.size(), 2);
		QCOMPARE(nc.itemAt(0), (QStringList{"hundredfolio", "7", "2", "5", "0", "0000"}));
		QCOMPARE(nc.itemAt(1), (QStringList{"string", "-K1", "1", "0", "0", "00"}));
	}

	// A numeric part refuses a value which is not a number at all, and an
	// unknown type one which cannot even be shown as text.
	void addValueRefusesUnusableValues()
	{
		NumerotationContext nc;
		QVERIFY(!nc.addValue(QStringLiteral("unit"), QVariant(QPointF(1, 2))));
		QVERIFY(!nc.addValue(QStringLiteral("bogus"), QVariant(QPointF(1, 2))));
		QVERIFY(nc.isEmpty());
	}

	void typesThatAreNumbers_data()
	{
		QTest::addColumn<QString>("type");
		QTest::addColumn<bool>("number");

		QTest::newRow("unit")          << "unit"          << true;
		QTest::newRow("unitfolio")     << "unitfolio"     << true;
		QTest::newRow("ten")           << "ten"           << true;
		QTest::newRow("tenfolio")      << "tenfolio"      << true;
		QTest::newRow("hundred")       << "hundred"       << true;
		QTest::newRow("hundredfolio")  << "hundredfolio"  << true;
		QTest::newRow("wrap")          << "wrap"          << true;
		QTest::newRow("alpha")         << "alpha"         << false;
		QTest::newRow("string")        << "string"        << false;
		QTest::newRow("folio")         << "folio"         << false;
		QTest::newRow("idfolio")       << "idfolio"       << false;
		QTest::newRow("plant")         << "plant"         << false;
		QTest::newRow("locmach")       << "locmach"       << false;
		QTest::newRow("elementline")   << "elementline"   << false;
		QTest::newRow("elementcolumn") << "elementcolumn" << false;
		QTest::newRow("elementprefix") << "elementprefix" << false;
	}

	// keyIsNumber() decides which parts a wrap part carries into and
	// borrows from; every type is still an acceptable one.
	void typesThatAreNumbers()
	{
		QFETCH(QString, type);
		QFETCH(bool, number);

		NumerotationContext nc;
		QCOMPARE(nc.keyIsNumber(type), number);
		QVERIFY(nc.keyIsAcceptable(type));
		QCOMPARE(nc.validRegExpNumber().split(QLatin1Char('|')).contains(type), number);
		QVERIFY(nc.validRegExpNum().split(QLatin1Char('|')).contains(type));
	}

	// Every field a scheme has survives its writer and reader, when it is
	// one the part's type uses.
	void xmlRoundTrip()
	{
		const NumerotationContext before = make({
			"string|-K|1|0|0|",
			"unit|3|1|0|0|",
			"ten|7|2|0|0|000",
			"unitfolio|4|1|2|0|",
			"tenfolio|1|1|9|0|",
			"hundredfolio|5|10|3|0|",
			"wrap|59|1|0|60|00",
			"alpha|az|1|0|0|",
			"folio||1|0|0|",
			"elementprefix||1|0|0|"});

		QDomDocument doc;
		NumerotationContext writer = before;
		QDomElement xml = writer.toXml(doc, QStringLiteral("element_autonum"));
		QCOMPARE(xml.tagName(), QStringLiteral("element_autonum"));
		QCOMPARE(xml.elementsByTagName(QStringLiteral("part")).count(), before.size());

		const NumerotationContext after(xml);
		QCOMPARE(parts(after), parts(before));
	}

	void attributesWrittenPerType_data()
	{
		QTest::addColumn<QString>("part");
		QTest::addColumn<QStringList>("written");

		QTest::newRow("unit") << "unit|3|1|4|5|"
			<< QStringList{"increase", "type", "value"};
		QTest::newRow("unitfolio keeps its initial value") << "unitfolio|3|1|4|5|"
			<< QStringList{"increase", "initialvalue", "type", "value"};
		QTest::newRow("tenfolio keeps its initial value") << "tenfolio|3|1|4|5|"
			<< QStringList{"increase", "initialvalue", "type", "value"};
		QTest::newRow("hundredfolio keeps its initial value") << "hundredfolio|3|1|4|5|"
			<< QStringList{"increase", "initialvalue", "type", "value"};
		QTest::newRow("wrap keeps its modulus") << "wrap|3|1|4|5|"
			<< QStringList{"increase", "modulus", "type", "value"};
		QTest::newRow("a format is written for any type") << "string|x|1|0|0|00"
			<< QStringList{"format", "increase", "type", "value"};
	}

	// toXml() writes the initial value only for folio parts and the
	// modulus only for wrap parts, and a format only when there is one:
	// files without them read back the same.
	void attributesWrittenPerType()
	{
		QFETCH(QString, part);
		QFETCH(QStringList, written);

		QDomDocument doc;
		NumerotationContext nc = make({part});
		const QDomElement xml = nc.toXml(doc, QStringLiteral("autonum"))
				.firstChildElement(QStringLiteral("part"));
		QStringList names;
		const QDomNamedNodeMap attributes = xml.attributes();
		for (int i = 0; i < attributes.count(); ++i)
			names << attributes.item(i).nodeName();
		names.sort();
		QCOMPARE(names, written);
	}

	// A part written before the modulus and the format existed reads as
	// "no modulus, natural width"; fromXml() replaces what was there, and
	// the attributes a scheme carries around its parts (title, id, formula,
	// as in a copy) do not change them.
	void fromXmlReadsLegacyAndSchemeElements()
	{
		QDomDocument doc;
		QVERIFY(doc.setContent(QStringLiteral(
			"<element_autonum title=\"Equipment\" "
			"id=\"{3f2504e0-4f89-11d3-9a0c-0305e82c3301}\" formula=\"%seqt_1\">"
			"<part type=\"ten\" value=\"4\" increase=\"1\"/>"
			"</element_autonum>")));
		QDomElement root = doc.documentElement();

		NumerotationContext nc = make({"unit|9|9|9|9|9"});
		nc.fromXml(root);
		QCOMPARE(parts(nc), QStringList{"ten|4|1|0|0|"});
		QCOMPARE(NumerotationContext::formatValue(nc.itemAt(0)), QStringLiteral("04"));
	}

	void formatValue_data()
	{
		QTest::addColumn<QString>("part");
		QTest::addColumn<QString>("expected");

		QTest::newRow("unit")                    << "unit|7|1|0|0|"          << "7";
		QTest::newRow("ten pads to two")         << "ten|7|1|0|0|"           << "07";
		QTest::newRow("ten past its width")      << "ten|123|1|0|0|"         << "123";
		QTest::newRow("tenfolio pads to two")    << "tenfolio|7|1|0|0|"      << "07";
		QTest::newRow("hundred pads to three")   << "hundred|7|1|0|0|"       << "007";
		QTest::newRow("hundredfolio")            << "hundredfolio|42|1|0|0|" << "042";
		QTest::newRow("wrap")                    << "wrap|5|1|0|60|"         << "5";
		QTest::newRow("mask pads a unit")        << "unit|7|1|0|0|0000"      << "0007";
		QTest::newRow("mask overrides ten")      << "ten|7|1|0|0|0"          << "7";
		QTest::newRow("mask pads a wrap")        << "wrap|5|1|0|60|00"       << "05";
		QTest::newRow("mask shorter than value") << "hundred|1234|1|0|0|00"  << "1234";
		QTest::newRow("alpha ignores the mask")  << "alpha|ab|1|0|0|000"     << "ab";
	}

	// formatValue() is the preview of a part; setSequentialToList() pads
	// the same part for the label actually drawn. They are kept in step
	// by hand, so check them against each other as well.
	void formatValue()
	{
		QFETCH(QString, part);
		QFETCH(QString, expected);

		NumerotationContext nc = make({part});
		QCOMPARE(NumerotationContext::formatValue(nc.itemAt(0)), expected);

		QStringList drawn;
		autonum::setSequentialToList(drawn, nc, nc.itemAt(0).at(0));
		QCOMPARE(drawn, QStringList{expected});
	}

	// A part as older code holds it, with three fields: no format.
	void formatOfShortItem()
	{
		const QStringList legacy{"ten", "3", "1"};
		QCOMPARE(NumerotationContext::formatOf(legacy), QString());
		QCOMPARE(NumerotationContext::formatValue(legacy), QStringLiteral("03"));
	}

	// setSequentialToList() collects the parts of one type, in order.
	void setSequentialToListPicksOneType()
	{
		NumerotationContext nc = make({"unit|3|1|0|0|", "ten|4|1|0|0|", "string|-|1|0|0|",
					       "unit|12|1|0|0|000"});
		QStringList units;
		autonum::setSequentialToList(units, nc, QStringLiteral("unit"));
		QCOMPARE(units, (QStringList{"3", "012"}));
	}

	// replaceValue() and replaceIncrease() change their own field only.
	void replaceKeepsOtherFields()
	{
		NumerotationContext nc = make({"wrap|5|2|1|60|00"});
		nc.replaceValue(0, QStringLiteral("17"));
		QCOMPARE(nc[0], QStringLiteral("wrap|17|2|1|60|00"));
		nc.replaceIncrease(0, 5);
		QCOMPARE(nc[0], QStringLiteral("wrap|17|5|1|60|00"));

		NumerotationContext appended = make({"string|K|1|0|0|"});
		appended << nc;
		QCOMPARE(parts(appended), (QStringList{"string|K|1|0|0|", "wrap|17|5|1|60|00"}));
	}

	// The named schemes kept in the settings come back as saved; a rule
	// dropped since the previous save does not come back.
	void settingsRoundTrip()
	{
		const QString file = m_settings_dir.filePath(QStringLiteral("rules.ini"));
		QHash<QString, NumerotationContext> rules;
		rules.insert(QStringLiteral("Motors"), make({"string|M|1|0|0|", "ten|1|1|0|0|"}));
		rules.insert(QStringLiteral("Clock"), make({"unit|0|1|0|0|", "wrap|0|1|0|60|00"}));
		rules.insert(QStringLiteral("Old"), make({"unit|9|1|0|0|"}));
		{
			QSettings settings(file, QSettings::IniFormat);
			NumerotationContext::saveToSettings(rules, QStringLiteral("Clock"), settings,
							    QStringLiteral("autonum/element"));
			rules.remove(QStringLiteral("Old"));
			NumerotationContext::saveToSettings(rules, QStringLiteral("Clock"), settings,
							    QStringLiteral("autonum/element"));
		}

		QSettings settings(file, QSettings::IniFormat);
		const auto loaded = NumerotationContext::loadFromSettings(
					settings, QStringLiteral("autonum/element"));
		QCOMPARE(loaded.second, QStringLiteral("Clock"));
		QStringList names = loaded.first.keys();
		names.sort();
		QCOMPARE(names, (QStringList{"Clock", "Motors"}));
		for (const QString &name : names)
			QCOMPARE(parts(loaded.first.value(name)), parts(rules.value(name)));
	}

	void step_data()
	{
		QTest::addColumn<QStringList>("context");
		QTest::addColumn<bool>("forward");
		QTest::addColumn<QStringList>("expected");

		QTest::newRow("unit") << QStringList{"unit|1|1|0|0|"} << true << QStringList{"2"};
		QTest::newRow("unit by its increase") << QStringList{"unit|10|5|0|0|"} << true << QStringList{"15"};
		QTest::newRow("ten past its width") << QStringList{"ten|99|1|0|0|"} << true << QStringList{"100"};
		QTest::newRow("hundredfolio") << QStringList{"hundredfolio|5|10|1|0|"} << true << QStringList{"15"};
		// nothing stops a counter going below zero
		QTest::newRow("unit back below zero") << QStringList{"unit|0|1|0|0|"} << false << QStringList{"-1"};
		QTest::newRow("text parts do not move")
			<< QStringList{"string|K|1|0|0|", "folio||1|0|0|", "unit|1|1|0|0|"} << true
			<< QStringList{"K", "", "2"};

		// The leading counters below step by 0, so only the carry or the
		// borrow of the wrap part moves them.
		QTest::newRow("wrap without overflow")
			<< QStringList{"unit|1|0|0|0|", "wrap|5|1|0|10|"} << true << QStringList{"1", "6"};
		// the carry skips the "." text part to reach the counter before it
		QTest::newRow("wrap carries past text")
			<< QStringList{"unit|1|0|0|0|", "string|.|1|0|0|", "wrap|59|1|0|60|"} << true
			<< QStringList{"2", ".", "0"};
		// seconds into minutes into hours
		QTest::newRow("chained wraps carry")
			<< QStringList{"unit|0|0|0|0|", "wrap|59|0|0|60|", "wrap|59|1|0|60|"} << true
			<< QStringList{"1", "0", "0"};
		// the carry comes on top of the counter's own step
		QTest::newRow("carry and own step")
			<< QStringList{"unit|1|1|0|0|", "wrap|9|1|0|10|"} << true << QStringList{"3", "0"};
		QTest::newRow("wrap with nothing to carry into")
			<< QStringList{"string|A|1|0|0|", "wrap|9|1|0|10|"} << true << QStringList{"A", "0"};
		QTest::newRow("wrap without modulus counts on")
			<< QStringList{"unit|1|0|0|0|", "wrap|99|1|0|0|"} << true << QStringList{"1", "100"};
		QTest::newRow("wrap borrows past text")
			<< QStringList{"unit|2|0|0|0|", "string|.|1|0|0|", "wrap|0|1|0|60|"} << false
			<< QStringList{"1", ".", "59"};
		QTest::newRow("chained wraps borrow")
			<< QStringList{"unit|1|0|0|0|", "wrap|0|0|0|24|", "wrap|0|1|0|60|"} << false
			<< QStringList{"0", "23", "59"};
		QTest::newRow("wrap back without underflow")
			<< QStringList{"unit|1|0|0|0|", "wrap|5|2|0|10|"} << false << QStringList{"1", "3"};
		// A step larger than the modulus overflows it several times; the
		// counter before took only one of them (5 + 25 gave 1|0, not 3|0).
		QTest::newRow("step past several moduli carries each")
			<< QStringList{"unit|0|0|0|0|", "wrap|5|25|0|10|"} << true << QStringList{"3", "0"};
		QTest::newRow("step back past several moduli borrows each")
			<< QStringList{"unit|3|0|0|0|", "wrap|5|25|0|10|"} << false << QStringList{"1", "0"};
		QTest::newRow("step back to a multiple of the modulus")
			<< QStringList{"unit|3|0|0|0|", "wrap|5|15|0|10|"} << false << QStringList{"2", "0"};
		// 0:59:50 + 130 s = 1:02:00
		QTest::newRow("several carries chain")
			<< QStringList{"unit|0|0|0|0|", "wrap|59|0|0|60|", "wrap|50|130|0|60|"} << true
			<< QStringList{"1", "2", "0"};
		QTest::newRow("several borrows chain")
			<< QStringList{"unit|1|0|0|0|", "wrap|2|0|0|60|", "wrap|0|130|0|60|"} << false
			<< QStringList{"0", "59", "50"};
	}

	// NumerotationContextCommands::next() and previous(): one step of a
	// scheme, the carry and borrow of its wrap parts included.
	void step()
	{
		QFETCH(QStringList, context);
		QFETCH(bool, forward);
		QFETCH(QStringList, expected);

		NumerotationContextCommands commands(make(context));
		const NumerotationContext result = forward ? commands.next() : commands.previous();
		QCOMPARE(values(result), expected);
	}

	// A step changes the values only: increase, initial value, modulus
	// and format of every part are what they were.
	void stepKeepsSettings()
	{
		const NumerotationContext before = make({"wrap|59|1|0|60|00",
							 "tenfolio|3|2|7|0|000",
							 "alpha|c|1|0|0|"});
		NumerotationContextCommands commands(before);
		const NumerotationContext after = commands.next();
		QCOMPARE(parts(after), (QStringList{"wrap|0|1|0|60|00",
						    "tenfolio|5|2|7|0|000",
						    "alpha|d|1|0|0|"}));
	}

	// A part of a type the engine does not know (a hand-edited or foreign
	// file, "Unit" for "unit") left no strategy to step or show it with:
	// next(), previous() and the preview used a deleted or null one and
	// crashed. Such a part stays as it is and shows its value as text;
	// the carry of a wrap part passes over it like over text.
	void unknownPartTypeIsKept()
	{
		const NumerotationContext nc = make({"bogus|x|1|0|0|", "unit|1|0|0|0|",
						     "Unit|7|1|0|0|", "wrap|9|1|0|10|"});
		NumerotationContextCommands forward(nc);
		QCOMPARE(parts(forward.next()), (QStringList{"bogus|x|1|0|0|", "unit|2|0|0|0|",
							     "Unit|7|1|0|0|", "wrap|0|1|0|10|"}));
		NumerotationContextCommands back(make({"unit|1|0|0|0|", "Unit|7|1|0|0|",
						       "wrap|0|1|0|10|", "bogus|x|1|0|0|"}));
		QCOMPARE(values(back.previous()), (QStringList{"0", "7", "9", "x"}));
		NumerotationContextCommands shown(nc);
		QCOMPARE(shown.toRepresentedString(), QStringLiteral("x179"));
	}

	void alphaStep_data()
	{
		QTest::addColumn<QString>("value");
		QTest::addColumn<QString>("next");
		QTest::addColumn<QString>("previous");

		QTest::newRow("a")   << "a"   << "b"   << "a";  // nothing before "a"
		QTest::newRow("m")   << "m"   << "n"   << "l";
		QTest::newRow("z")   << "z"   << "aa"  << "y";
		QTest::newRow("aa")  << "aa"  << "ab"  << "z";
		QTest::newRow("az")  << "az"  << "ba"  << "ay";
		QTest::newRow("ba")  << "ba"  << "bb"  << "az";
		QTest::newRow("zz")  << "zz"  << "aaa" << "zy";
		QTest::newRow("aaa") << "aaa" << "aab" << "zz";
		QTest::newRow("upper case kept") << "AZ" << "BA" << "AY";
		QTest::newRow("empty") << "" << "a" << "a";
	}

	// The alpha part counts like a spreadsheet column name.
	void alphaStep()
	{
		QFETCH(QString, value);
		QFETCH(QString, next);
		QFETCH(QString, previous);

		const NumerotationContext nc = make({"alpha|" + value + "|1|0|0|"});
		NumerotationContextCommands forward(nc);
		QCOMPARE(values(forward.next()), QStringList{next});
		NumerotationContextCommands back(nc);
		QCOMPARE(values(back.previous()), QStringList{previous});
	}

	// What a folio numbering shows: number parts at their type's width,
	// the other parts as the variable the folio fills in.
	void representedString()
	{
		NumerotationContextCommands commands(make({
			"string|F-|1|0|0|", "unit|3|1|0|0|", "ten|5|1|0|0|", "ten|12|1|0|0|",
			"hundred|7|1|0|0|", "hundredfolio|42|1|0|0|", "string|/|1|0|0|",
			"folio||1|0|0|", "idfolio||1|0|0|", "plant||1|0|0|", "locmach||1|0|0|",
			"elementline||1|0|0|", "elementcolumn||1|0|0|", "elementprefix||1|0|0|"}));
		QCOMPARE(commands.toRepresentedString(),
			 QStringLiteral("F-30512007042/%F%id%M%LM%l%c%prefix"));

		NumerotationContextCommands empty{NumerotationContext()};
		QCOMPARE(empty.toRepresentedString(), QString());
	}

	void contextToFormula_data()
	{
		QTest::addColumn<QStringList>("context");
		QTest::addColumn<QString>("formula");

		QTest::newRow("empty") << QStringList{} << "";
		QTest::newRow("every type") << QStringList{
			"idfolio||1|0|0|", "folio||1|0|0|", "plant||1|0|0|", "locmach||1|0|0|",
			"elementcolumn||1|0|0|", "elementline||1|0|0|", "elementprefix||1|0|0|",
			"string|-K|1|0|0|", "unit|1|1|0|0|", "wrap|0|1|0|60|", "unitfolio|1|1|0|0|",
			"ten|1|1|0|0|", "tenfolio|1|1|0|0|", "hundred|1|1|0|0|",
			"hundredfolio|1|1|0|0|", "alpha|a|1|0|0|"}
			<< "%id%F%M%LM%c%l%prefix-K%sequ_1%seqw_1%sequf_1%seqt_1%seqtf_1%seqh_1%seqhf_1%seqa_1";
		// each type is counted on its own
		QTest::newRow("counted per type") << QStringList{
			"unit|1|1|0|0|", "string|.|1|0|0|", "unit|1|1|0|0|", "ten|1|1|0|0|",
			"unit|1|1|0|0|", "ten|1|1|0|0|"}
			<< "%sequ_1.%sequ_2%seqt_1%sequ_3%seqt_2";
	}

	// numerotationContextToFormula(): the formula a scheme's elements get,
	// and the one a scheme is recognised by.
	void contextToFormula()
	{
		QFETCH(QStringList, context);
		QFETCH(QString, formula);
		QCOMPARE(autonum::numerotationContextToFormula(make(context)), formula);
	}

	void formulaToLabel_data()
	{
		QTest::addColumn<QString>("formula");
		QTest::addColumn<QString>("label");

		QTest::newRow("folio")       << "%F"        << "B2";
		QTest::newRow("folio index") << "%f"        << "5";   // folio_index + 1
		QTest::newRow("id")          << "%id"       << "5";
		QTest::newRow("total")       << "%f/%total" << "5/12";
		QTest::newRow("plant and location") << "=%M+%LM" << "=PLANT+CAB1";
		QTest::newRow("folio and index") << "%F.%f" << "B2.5";
		QTest::newRow("no element: grid kept")   << "%l%c%prefix" << "%l%c%prefix";
		QTest::newRow("no conductor: wire kept") << "%wf%wc"      << "%wf%wc";
		QTest::newRow("title block, both spellings") << "%{client}/%client" << "ACME/ACME";
		QTest::newRow("project property") << "%{site}" << "Bern";
		// a key in both: the title block's is used
		QTest::newRow("title block before project") << "%{shared}" << "folio";
		QTest::newRow("unknown variable kept") << "%{nope}" << "%{nope}";
		QTest::newRow("sequences") << "%sequ_1-%sequ_2/%seqt_1/%seqh_1/%seqw_1/%seqa_1"
					  << "3-4/07/012/59/c";
		QTest::newRow("folio sequences") << "%sequf_1%seqtf_1%seqhf_1" << "101100";
		QTest::newRow("missing sequence kept") << "%sequ_3" << "%sequ_3";
		QTest::newRow("scheme formula") << "%F-K%sequ_1" << "B2-K3";
	}

	// AssignVariables::formulaToLabel() on a folio given as a
	// FormulaContext, as the project database renders a label.
	void formulaToLabel()
	{
		QFETCH(QString, formula);
		QFETCH(QString, label);

		autonum::FormulaContext context;
		context.folio = QStringLiteral("B2");
		context.folio_index = 4;
		context.folio_total = 12;
		context.plant = QStringLiteral("PLANT");
		context.locmach = QStringLiteral("CAB1");
		context.title_block_fields.addValue(QStringLiteral("client"), QStringLiteral("ACME"));
		context.title_block_fields.addValue(QStringLiteral("shared"), QStringLiteral("folio"));
		context.project_properties.addValue(QStringLiteral("site"), QStringLiteral("Bern"));
		context.project_properties.addValue(QStringLiteral("shared"), QStringLiteral("project"));

		autonum::sequentialNumbers seq;
		seq.unit = QStringList{"3", "4"};
		seq.ten = QStringList{"07"};
		seq.hundred = QStringList{"012"};
		seq.wrap = QStringList{"59"};
		seq.alpha = QStringList{"c"};
		seq.unit_folio = QStringList{"1"};
		seq.ten_folio = QStringList{"01"};
		seq.hundred_folio = QStringList{"100"};
		const autonum::sequentialNumbers before = seq;

		QCOMPARE(autonum::AssignVariables::formulaToLabel(formula, seq, context), label);
		QVERIFY(seq == before);
	}

	void tenAndMoreSequences_data()
	{
		QTest::addColumn<QString>("formula");
		QTest::addColumn<QString>("label");

		QTest::newRow("first and tenth")  << "%sequ_1|%sequ_10" << "A|J";
		QTest::newRow("tenth first")      << "%sequ_10|%sequ_1" << "J|A";
		QTest::newRow("ten and alpha")    << "%seqt_1%seqt_10/%seqa_1%seqa_10" << "0110/ab";
		QTest::newRow("folio")            << "%sequf_10" << "j";
		QTest::newRow("missing twelfth kept") << "%seqt_12" << "%seqt_12";
	}

	// %sequ_1 was replaced inside %sequ_10, so the tenth value of a
	// sequence and beyond showed as the first one followed by a digit
	// ("A0"); a token now only stands for its own number.
	void tenAndMoreSequences()
	{
		QFETCH(QString, formula);
		QFETCH(QString, label);

		autonum::sequentialNumbers seq;
		for (char c = 'A'; c <= 'J'; ++c) {
			seq.unit << QString(QLatin1Char(c));
			seq.unit_folio << QString(QLatin1Char(c)).toLower();
		}
		for (int i = 1; i <= 10; ++i)
			seq.ten << QStringLiteral("%1").arg(i, 2, 10, QLatin1Char('0'));
		seq.alpha = QStringList{"a", "", "", "", "", "", "", "", "", "b"};

		QCOMPARE(autonum::AssignVariables::formulaToLabel(formula, seq, autonum::FormulaContext()),
			 label);
	}

	void elementAndConductorVariables_data()
	{
		QTest::addColumn<bool>("columnsFromZero");
		QTest::addColumn<QString>("label");

		// the "border-columns_0" setting: columns numbered from 0 or 1
		QTest::newRow("columns from 0") << true  << "-Q1 C2 2/mains/BK/2.5/400V";
		QTest::newRow("columns from 1") << false << "-Q1 C3 3/mains/BK/2.5/400V";
	}

	// An element's grid position and prefix, a conductor's properties.
	void elementAndConductorVariables()
	{
		QFETCH(bool, columnsFromZero);
		QFETCH(QString, label);

		QSettings().setValue(QStringLiteral("border-columns_0"), columnsFromZero);

		autonum::FormulaContext context;
		context.has_element = true;
		context.element_position = DiagramPosition(QStringLiteral("C"), 3);
		context.element_prefix = QStringLiteral("-Q");
		context.has_conductor = true;
		context.wire_function = QStringLiteral("mains");
		context.wire_color = QStringLiteral("BK");
		context.wire_section = QStringLiteral("2.5");
		context.wire_tension_protocol = QStringLiteral("400V");

		autonum::sequentialNumbers seq;
		seq.unit = QStringList{"1"};
		QCOMPARE(autonum::AssignVariables::formulaToLabel(
				 QStringLiteral("%prefix%sequ_1 %l%c %c/%wf/%wc/%ws/%wv"), seq, context),
			 label);
	}

	// replaceVariable(): an element's informations in a text, %{void}
	// for nothing; a key the element does not have gives an empty text.
	void replaceVariable()
	{
		DiagramContext dc;
		dc.addValue(QStringLiteral("label"), QStringLiteral("K1"));
		dc.addValue(QStringLiteral("function"), QStringLiteral("Motor"));
		dc.addValue(QStringLiteral("plc_address"), QStringLiteral("%I0.1"));
		QCOMPARE(autonum::AssignVariables::replaceVariable(
				 QStringLiteral("%{label}%{void}: %{function} @%{plc_address} [%{comment}] %{nope}"), dc),
			 QStringLiteral("K1: Motor @%I0.1 [] %{nope}"));
	}

	// The sequence values an element keeps in the project file.
	void sequentialNumbersXmlRoundTrip()
	{
		autonum::sequentialNumbers seq;
		seq.unit = QStringList{"3", "4"};
		seq.wrap = QStringList{"59"};
		seq.unit_folio = QStringList{"1"};
		seq.ten = QStringList{"07", "10"};
		seq.ten_folio = QStringList{"01"};
		seq.hundred = QStringList{"012"};
		seq.hundred_folio = QStringList{"100"};
		seq.alpha = QStringList{"c", "aa"};

		QDomDocument doc;
		const QDomElement xml = seq.toXml(doc);
		QCOMPARE(xml.tagName(), QStringLiteral("sequentialNumbers"));
		QCOMPARE(xml.firstChildElement(QStringLiteral("ten")).text(), QStringLiteral("07;10"));

		autonum::sequentialNumbers read;
		read.fromXml(xml);
		QVERIFY(read == seq);
		QCOMPARE(read.alpha, seq.alpha);
		QCOMPARE(read.wrap, seq.wrap);

		// A kind of sequence the element has no value of is not written,
		// and read back as no value at all, not as one empty value: the
		// struct read compared unequal to the one written, and a label's
		// %seqw_1 was replaced by nothing instead of staying as it is.
		autonum::sequentialNumbers some;
		some.unit = QStringList{"3"};
		autonum::sequentialNumbers someRead;
		someRead.fromXml(some.toXml(doc));
		QCOMPARE(someRead.wrap, QStringList());
		QVERIFY(someRead == some);
		QCOMPARE(autonum::AssignVariables::formulaToLabel(
				 QStringLiteral("%sequ_1[%seqw_1]"), someRead, autonum::FormulaContext()),
			 QStringLiteral("3[%seqw_1]"));

		autonum::sequentialNumbers copy;
		copy = seq;
		QVERIFY(!(copy != seq));
		copy.clear();
		QVERIFY(copy != seq);
		QVERIFY(copy.unit.isEmpty() && copy.alpha.isEmpty());
	}
};

QTEST_GUILESS_MAIN(tst_numerotationcontext)

#include "tst_numerotationcontext.moc"
