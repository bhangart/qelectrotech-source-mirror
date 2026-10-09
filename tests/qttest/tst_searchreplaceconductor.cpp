// SPDX-License-Identifier: GPL-2.0-or-later
#include <QtTest>

#include "SearchAndReplace/searchandreplaceworker.h"
#include "bordertitleblock.h"
#include "diagram.h"
#include "diagramcommands.h"
#include "qetapp.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/diagramtextitem.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/qetgraphicsitem.h"
#include "qetinformation.h"
#include "undocommand/changeelementinformationcommand.h"
#include "undocommand/changetitleblockcommand.h"

Q_DECLARE_METATYPE(advancedReplaceStruct)

	// qet.cpp needs it; the application is not linked
QString QETApp::m_interface_language;

	// The worker's other functions walk the folios, elements and wires of
	// a project. This test calls only the static
	// SearchAndReplaceWorker::replaceAdvanced(properties, advanced), so
	// what those functions use is never reached: these stand-ins let the
	// worker link without the whole application, and stop the test if one
	// is called after all.
namespace {
[[noreturn]] void notInThisTest(const char *what)
{
	qFatal("%s is not part of tst_searchreplaceconductor", what);
}
}
Diagram *QetGraphicsItem::diagram() const { notInThisTest("QetGraphicsItem::diagram"); }
Diagram *DiagramTextItem::diagram() const { notInThisTest("DiagramTextItem::diagram"); }
void DiagramTextItem::setPlainText(const QString &) { notInThisTest("DiagramTextItem::setPlainText"); }
QETProject *Diagram::project() const { notInThisTest("Diagram::project"); }
TitleBlockProperties BorderTitleBlock::exportTitleBlock() { notInThisTest("exportTitleBlock"); }
Diagram *Conductor::diagram() const { notInThisTest("Conductor::diagram"); }
ConductorProperties Conductor::properties() const { notInThisTest("Conductor::properties"); }
QSet<Conductor *> Conductor::relatedPotentialConductors(const bool, QList<Terminal *> *)
{ notInThisTest("relatedPotentialConductors"); }
ChangeTitleBlockCommand::ChangeTitleBlockCommand(
		Diagram *, const TitleBlockProperties &, const TitleBlockProperties &, QUndoCommand *)
{ notInThisTest("ChangeTitleBlockCommand"); }
ChangeTitleBlockCommand::~ChangeTitleBlockCommand() {}
void ChangeTitleBlockCommand::undo() {}
void ChangeTitleBlockCommand::redo() {}
ChangeDiagramTextCommand::ChangeDiagramTextCommand(
		DiagramTextItem *, const QString &, const QString &, QUndoCommand *)
{ notInThisTest("ChangeDiagramTextCommand"); }
ChangeDiagramTextCommand::~ChangeDiagramTextCommand() {}
void ChangeDiagramTextCommand::undo() {}
void ChangeDiagramTextCommand::redo() {}
ChangeElementInformationCommand::ChangeElementInformationCommand(
		Element *, const DiagramContext &, const DiagramContext &, QUndoCommand *)
{ notInThisTest("ChangeElementInformationCommand"); }
bool ChangeElementInformationCommand::mergeWith(const QUndoCommand *) { return false; }
void ChangeElementInformationCommand::undo() {}
void ChangeElementInformationCommand::redo() {}

/**
	SearchAndReplaceWorker::replaceAdvanced() for a wire: the "Advanced"
	dialog of Search and replace offers the fields of
	QETInformation::conductorInfoKeys(), and a regular expression is
	replaced in the field picked there. A key the worker does not know
	changes nothing, so the user's replace is silently lost.
*/
class tst_searchreplaceconductor : public QObject
{
	Q_OBJECT

		// The text field of a wire that a "what" key of the dialog names
	static QString *field(ConductorProperties &p, const QString &key)
	{
		using namespace QETInformation;
		if (key == COND_FORMULA)          return &p.m_formula;
		if (key == COND_TEXT)             return &p.text;
		if (key == COND_FUNCTION)         return &p.m_function;
		if (key == COND_TENSION_PROTOCOL) return &p.m_tension_protocol;
		if (key == COND_COLOR)            return &p.m_wire_color;
		if (key == COND_SECTION)          return &p.m_wire_section;
		return nullptr;
	}

		// A wire whose six text fields all hold "A1"
	static ConductorProperties wire()
	{
		ConductorProperties p;
		for (const QString &key : QETInformation::conductorInfoKeys())
			*field(p, key) = QStringLiteral("A1");
		return p;
	}

	static advancedReplaceStruct change(const QString &what,
					    const QString &search,
					    const QString &replace,
					    int who = 2)
	{
		advancedReplaceStruct a;
		a.who = who;
		a.what = what;
		a.search = search;
		a.replace = replace;
		return a;
	}

private slots:
		// Every field the dialog offers is replaced, and only that one
	void everyOfferedFieldIsReplaced_data()
	{
		QTest::addColumn<QString>("key");
		const QStringList keys = QETInformation::conductorInfoKeys();
		QVERIFY(!keys.isEmpty());
		for (const QString &key : keys)
			QTest::newRow(qPrintable(key)) << key;
	}

	void everyOfferedFieldIsReplaced()
	{
		QFETCH(QString, key);
		ConductorProperties before = wire();
		QVERIFY2(field(before, key),
			 "a key of the dialog that this test does not know");

		const ConductorProperties after = SearchAndReplaceWorker::replaceAdvanced(
					before, change(key, QStringLiteral("A"), QStringLiteral("B")));

		ConductorProperties expected = before;
		*field(expected, key) = QStringLiteral("B1");
		ConductorProperties copy = after;
		QCOMPARE(*field(copy, key), QStringLiteral("B1"));
		QVERIFY(after == expected);
	}

		// The case that was lost: "Tension / protocol" in the dialog
	void tensionProtocol()
	{
		ConductorProperties before;
		before.m_tension_protocol = QStringLiteral("400V AC");
		const ConductorProperties after = SearchAndReplaceWorker::replaceAdvanced(
					before, change(QETInformation::COND_TENSION_PROTOCOL,
						       QStringLiteral("^400V"), QStringLiteral("230V")));
		QCOMPARE(after.m_tension_protocol, QStringLiteral("230V AC"));
	}

		// The regular expression and its replacement, on one field
	void regularExpression_data()
	{
		QTest::addColumn<QString>("original");
		QTest::addColumn<QString>("search");
		QTest::addColumn<QString>("replace");
		QTest::addColumn<QString>("expected");

		QTest::newRow("every match")      << "24V/24V" << "24"       << "48"    << "48V/48V";
		QTest::newRow("capture group")    << "400V"    << "(\\d+)V"  << "\\1 V" << "400 V";
		QTest::newRow("empty replace")    << "400V AC" << " AC"      << ""      << "400V";
		QTest::newRow("erase everything") << "400V"    << ".*"       << ""      << "";
		QTest::newRow("no match")         << "400V"    << "230"      << "110"   << "400V";
		QTest::newRow("empty field")      << ""        << "400"      << "230"   << "";
			// "^$" matches the empty field: it is filled
		QTest::newRow("fill empty field") << ""        << "^$"       << "230V"  << "230V";
		QTest::newRow("case sensitive")   << "400v"    << "V"        << "W"     << "400v";
	}

	void regularExpression()
	{
		QFETCH(QString, original);
		QFETCH(QString, search);
		QFETCH(QString, replace);
		QFETCH(QString, expected);

		ConductorProperties before;
		before.m_tension_protocol = original;
		const ConductorProperties after = SearchAndReplaceWorker::replaceAdvanced(
					before, change(QETInformation::COND_TENSION_PROTOCOL,
						       search, replace));
		QCOMPARE(after.m_tension_protocol, expected);
	}

		// No change: the properties come back equal, so the worker
		// pushes no undo step for that wire (it compares old and new)
	void nothingChanges_data()
	{
		QTest::addColumn<advancedReplaceStruct>("advanced");

		using namespace QETInformation;
		QTest::newRow("no match")
			<< change(COND_TENSION_PROTOCOL, "Z", "B");
		QTest::newRow("replaced by itself")
			<< change(COND_TENSION_PROTOCOL, "A", "A");
		QTest::newRow("folio target")
			<< change(COND_TENSION_PROTOCOL, "A", "B", 0);
		QTest::newRow("element target")
			<< change(COND_TENSION_PROTOCOL, "A", "B", 1);
		QTest::newRow("text target")
			<< change(COND_TENSION_PROTOCOL, "A", "B", 3);
		QTest::newRow("no target")
			<< change(COND_TENSION_PROTOCOL, "A", "B", -1);
		QTest::newRow("unknown field")
			<< change("cable", "A", "B");
		QTest::newRow("empty field name")
			<< change("", "A", "B");
			// the key the worker used to expect; the dialog never sends it
		QTest::newRow("old key tension/protocol")
			<< change("tension/protocol", "A", "B");
	}

	void nothingChanges()
	{
		QFETCH(advancedReplaceStruct, advanced);
		const ConductorProperties before = wire();
		const ConductorProperties after =
				SearchAndReplaceWorker::replaceAdvanced(before, advanced);
		QVERIFY(after == before);
	}

		// The same change applied to several wires one after the other,
		// as the worker does, gives each its own result and is stable:
		// applying it again to a result changes nothing more.
	void severalWires()
	{
		const advancedReplaceStruct advanced = change(
					QETInformation::COND_TENSION_PROTOCOL,
					QStringLiteral("^400V"), QStringLiteral("230V"));
		const QStringList originals {"400V", "400V AC", "24V", ""};
		const QStringList expected  {"230V", "230V AC", "24V", ""};
		for (int i = 0 ; i < originals.size() ; ++i)
		{
			ConductorProperties before;
			before.m_tension_protocol = originals.at(i);
			const ConductorProperties after =
					SearchAndReplaceWorker::replaceAdvanced(before, advanced);
			QCOMPARE(after.m_tension_protocol, expected.at(i));
			QVERIFY(SearchAndReplaceWorker::replaceAdvanced(after, advanced)
				== after);
		}
	}
};

QTEST_MAIN(tst_searchreplaceconductor)

#include "tst_searchreplaceconductor.moc"
