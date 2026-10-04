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
#include "SearchAndReplace/searchandreplaceworker.h"
#include "bordertitleblock.h"
#include "diagram.h"
#include "diagramcommands.h"
#include "qetapp.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/diagramtextitem.h"
#include "qetgraphicsitem/qetgraphicsitem.h"
#include "qetinformation.h"
#include "undocommand/changeelementinformationcommand.h"
#include "undocommand/changetitleblockcommand.h"

#include <QCheckBox>
#include <QLineEdit>
#include <QtTest>

#include <functional>

	// qet.cpp needs it; the application is not linked
QString QETApp::m_interface_language;

	// The worker's replace*() functions walk a project's folios, elements
	// and wires; this test calls only its static helpers, so what those
	// functions use is never reached. These stand in for it so that the
	// worker links without the whole application, and stop the test if
	// one is called after all.
namespace {
[[noreturn]] void notInThisTest(const char *what)
{
	qFatal("%s is not part of tst_searchandreplace", what);
}
}
QStringList QETInformation::elementInfoKeys() { notInThisTest("elementInfoKeys"); }
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

using ConductorSetter = std::function<void(ConductorProperties &)>;
Q_DECLARE_METATYPE(ConductorSetter)

/**
	SearchAndReplaceWorker::applyChange() merges what the user entered in
	the Search and replace dialog into the folio, element and wire fields
	found. In a text field, empty means "leave as it is" and the erase
	marker, eraseText(), means "clear it". A wire's change starts as
	invalidConductorProperties(), whose out-of-range values mean "leave as
	it is"; a field is applied only once the user gives it a real value. A
	mistake here rewrites, or wipes, that field on every item the search
	found.
*/
class tst_searchandreplace : public QObject
{
	Q_OBJECT

		// A wire whose every field differs from the defaults, from
		// invalidConductorProperties() and from the values the cases set
	static ConductorProperties original()
	{
		ConductorProperties p;
		p.text_size = 11;
		p.text = QStringLiteral("W12");
		p.m_formula = QStringLiteral("%prefix%id");
		p.m_function = QStringLiteral("Power");
		p.m_tension_protocol = QStringLiteral("400V");
		p.m_wire_color = QStringLiteral("BK");
		p.m_wire_section = QStringLiteral("2.5");
		p.m_vertical_alignment = Qt::AlignRight;
		p.m_horizontal_alignment = Qt::AlignBottom;
		p.verti_rotate_text = 270;
		p.horiz_rotate_text = 180;
		p.color = QColor(Qt::darkBlue);
		p.style = Qt::DashDotLine;
		p.cond_size = 2.0;
		p.m_color_2 = QColor(Qt::darkGreen);
		p.m_dash_size = 5;
			// The three fields applyChange() always copies (see
			// alwaysCopied()) hold what invalidConductorProperties()
			// holds, so that only the field under test can differ.
		p.m_show_text = true;
		p.m_bicolor = false;
		p.singleLineProperties = SingleLineProperties();
		return p;
	}

private slots:
	void stringChange_data()
	{
		QTest::addColumn<QString>("original");
		QTest::addColumn<QString>("change");
		QTest::addColumn<QString>("expected");

		const QString erase = SearchAndReplaceWorker::eraseText();
		const QString shorter = erase.left(erase.size() - 1);
		QTest::newRow("empty keeps")          << "K1" << ""    << "K1";
		QTest::newRow("empty on empty")       << ""   << ""    << "";
		QTest::newRow("erase clears")         << "K1" << erase << "";
		QTest::newRow("erase on empty")       << ""   << erase << "";
		QTest::newRow("value replaces")       << "K1" << "K2"  << "K2";
		QTest::newRow("value fills empty")    << ""   << "K2"  << "K2";
			// a space is a value, not "leave as it is"
		QTest::newRow("space replaces")       << "K1" << " "   << " ";
			// only the exact marker erases
		QTest::newRow("marker plus space")    << "K1" << erase + " " << erase + " ";
		QTest::newRow("marker one X short")   << "K1" << shorter << shorter;
		QTest::newRow("marker in lower case") << "K1" << erase.toLower() << erase.toLower();
	}

	void stringChange()
	{
		QFETCH(QString, original);
		QFETCH(QString, change);
		QFETCH(QString, expected);
		QCOMPARE(SearchAndReplaceWorker::applyChange(original, change), expected);
	}

		// replaceDiagram() erases a folio's date only when the change's
		// date is valid and equals eraseDate(): an invalid marker could
		// never erase it.
	void eraseMarkersAreUsable()
	{
		QVERIFY(SearchAndReplaceWorker::eraseDate().isValid());
		QVERIFY(!SearchAndReplaceWorker::eraseText().isEmpty());
	}

		// The change the dialog starts from leaves a wire as it is
	void invalidPropertiesChangeNothing()
	{
		const ConductorProperties before = original();
		const ConductorProperties after = SearchAndReplaceWorker::applyChange(
					before, SearchAndReplaceWorker::invalidConductorProperties());
		QCOMPARE(after.text_size, before.text_size);
		QCOMPARE(after.text, before.text);
		QCOMPARE(after.m_vertical_alignment, before.m_vertical_alignment);
		QCOMPARE(after.m_horizontal_alignment, before.m_horizontal_alignment);
		QCOMPARE(after.verti_rotate_text, before.verti_rotate_text);
		QCOMPARE(after.horiz_rotate_text, before.horiz_rotate_text);
		QCOMPARE(after.color, before.color);
		QCOMPARE(after.style, before.style);
		QCOMPARE(after.cond_size, before.cond_size);
		QCOMPARE(after.m_color_2, before.m_color_2);
		QCOMPARE(after.m_dash_size, before.m_dash_size);
		QVERIFY(after == before);
	}

		// Each text field of a wire follows the rules of stringChange(),
		// and changing it leaves every other field alone
	void conductorTextField_data()
	{
		QTest::addColumn<int>("field");
		QTest::newRow("text")             << 0;
		QTest::newRow("formula")          << 1;
		QTest::newRow("function")         << 2;
		QTest::newRow("tension_protocol") << 3;
		QTest::newRow("wire_color")       << 4;
		QTest::newRow("wire_section")     << 5;
	}

	void conductorTextField()
	{
		QFETCH(int, field);
		const auto member = [field](ConductorProperties &p) -> QString & {
			switch (field) {
				case 0:  return p.text;
				case 1:  return p.m_formula;
				case 2:  return p.m_function;
				case 3:  return p.m_tension_protocol;
				case 4:  return p.m_wire_color;
				default: return p.m_wire_section;
			}
		};

		const ConductorProperties before = original();
		ConductorProperties change = SearchAndReplaceWorker::invalidConductorProperties();
		ConductorProperties expected = before;

		member(change) = QStringLiteral("new value");
		member(expected) = QStringLiteral("new value");
		ConductorProperties after = SearchAndReplaceWorker::applyChange(before, change);
		QCOMPARE(member(after), QStringLiteral("new value"));
		QVERIFY(after == expected);

		member(change) = SearchAndReplaceWorker::eraseText();
		member(expected) = QString();
		after = SearchAndReplaceWorker::applyChange(before, change);
		QCOMPARE(member(after), QString());
		QVERIFY(after == expected);
	}

		// Any other field is applied from a threshold on, or only for the
		// values the dialog offers; below it, or for a value it does not
		// offer, the wire keeps its own.
	void conductorField_data()
	{
		QTest::addColumn<ConductorSetter>("set");
		QTest::addColumn<bool>("applied");

		using P = ConductorProperties;
		QTest::newRow("text size 2 kept")      << ConductorSetter([](P &p) { p.text_size = 2; }) << false;
		QTest::newRow("text size 3 applied")   << ConductorSetter([](P &p) { p.text_size = 3; }) << true;
		QTest::newRow("wire size 0.3 kept")    << ConductorSetter([](P &p) { p.cond_size = 0.3; }) << false;
		QTest::newRow("wire size 0.4 applied") << ConductorSetter([](P &p) { p.cond_size = 0.4; }) << true;
			// 0 degrees is a real angle; only a negative one means "keep"
		QTest::newRow("vertical rotation 0")   << ConductorSetter([](P &p) { p.verti_rotate_text = 0; }) << true;
		QTest::newRow("horizontal rotation 0") << ConductorSetter([](P &p) { p.horiz_rotate_text = 0; }) << true;
		QTest::newRow("dash size 1 kept")      << ConductorSetter([](P &p) { p.m_dash_size = 1; }) << false;
		QTest::newRow("dash size 2 applied")   << ConductorSetter([](P &p) { p.m_dash_size = 2; }) << true;
		QTest::newRow("colour")                << ConductorSetter([](P &p) { p.color = Qt::red; }) << true;
		QTest::newRow("second colour")         << ConductorSetter([](P &p) { p.m_color_2 = Qt::yellow; }) << true;
		QTest::newRow("dashed style")          << ConductorSetter([](P &p) { p.style = Qt::DashLine; }) << true;
		QTest::newRow("vertical text left")    << ConductorSetter([](P &p) { p.m_vertical_alignment = Qt::AlignLeft; }) << true;
		QTest::newRow("vertical text centred kept")
			<< ConductorSetter([](P &p) { p.m_vertical_alignment = Qt::AlignHCenter; }) << false;
		QTest::newRow("horizontal text top")   << ConductorSetter([](P &p) { p.m_horizontal_alignment = Qt::AlignTop; }) << true;
		QTest::newRow("horizontal text centred kept")
			<< ConductorSetter([](P &p) { p.m_horizontal_alignment = Qt::AlignVCenter; }) << false;
	}

	void conductorField()
	{
		QFETCH(ConductorSetter, set);
		QFETCH(bool, applied);

		const ConductorProperties before = original();
		ConductorProperties change = SearchAndReplaceWorker::invalidConductorProperties();
		set(change);

		ConductorProperties expected = before;
		if (applied) {
			set(expected);
				// otherwise the case could not tell applied from kept
			QVERIFY(expected != before);
		}

		const ConductorProperties after = SearchAndReplaceWorker::applyChange(before, change);
		QVERIFY(after == expected);
	}

		// The check boxes and the single-line symbols have no "leave as it
		// is" value: the change's state is always applied.
	void alwaysCopied()
	{
		ConductorProperties before = original();
		before.m_show_text = false;
		before.m_bicolor = true;
		before.singleLineProperties.hasGround = false;
		before.singleLineProperties.setPhasesCount(3);

		const ConductorProperties after = SearchAndReplaceWorker::applyChange(
					before, SearchAndReplaceWorker::invalidConductorProperties());
		QCOMPARE(after.m_show_text, true);
		QCOMPARE(after.m_bicolor, false);
		QVERIFY(after.singleLineProperties == SingleLineProperties());
	}

		// The dialog's text editor: the erase marker shows as a checked
		// "erase" box and a disabled line, any other text as editable.
	void setupLineEdit_data()
	{
		QTest::addColumn<QString>("text");
		QTest::addColumn<bool>("erase");
		QTest::newRow("erase marker") << SearchAndReplaceWorker::eraseText() << true;
		QTest::newRow("text")         << "K1" << false;
		QTest::newRow("empty")        << ""   << false;
	}

	void setupLineEdit()
	{
		QFETCH(QString, text);
		QFETCH(bool, erase);

		QLineEdit line;
		QCheckBox box;
			// start in the opposite state, so that a widget left alone shows
		box.setChecked(!erase);
		line.setDisabled(!erase);

		SearchAndReplaceWorker::setupLineEdit(&line, &box, text);
		QCOMPARE(line.text(), text);
		QCOMPARE(box.isChecked(), erase);
		QCOMPARE(line.isEnabled(), !erase);
	}
};

QTEST_MAIN(tst_searchandreplace)

#include "tst_searchandreplace.moc"
