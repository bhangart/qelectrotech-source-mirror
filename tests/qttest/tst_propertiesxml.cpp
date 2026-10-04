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
#include "NameList/nameslist.h"
#include "borderproperties.h"
#include "conductorproperties.h"
#include "diagramcontext.h"
#include "properties/elementdata.h"
#include "qetapp.h"
#include "titleblockproperties.h"
#include "utils/qetutils.h"

#include <QTemporaryDir>
#include <QtTest>

#include <memory>

	// qet.cpp needs it; the application is not linked
QString QETApp::m_interface_language;

	// NamesList::name() picks the name in the language the user chose;
	// the application reads it from the settings, the test sets it here.
static QString s_language = QStringLiteral("en");
QString QETApp::langFromSetting() { return s_language; }

	// qetxml.cpp, for the element data, also saves fonts through these;
	// their file needs the application, and this test saves no font that
	// way.
QString QETUtils::fontToString(const QFont &) { qFatal("not part of tst_propertiesxml"); }
bool QETUtils::fontFromString(QFont &, const QString &) { qFatal("not part of tst_propertiesxml"); }

/**
	Folio borders, title blocks, wires, names, element and title block
	information, and element definitions are saved to projects, element
	files and the settings, and read back on the next start. What the
	writer writes must be what the reader reads: a field the writer drops
	or the reader misreads is lost on the next save, silently. Each case
	sets every field to a value other than its default, writes it, parses
	it back from text and compares; and reads an element without the
	attributes, as older files have it, to get the documented defaults.
	Settings go to an INI file of their own, never the user's settings.
*/
class tst_propertiesxml : public QObject
{
	Q_OBJECT

	QTemporaryDir m_dir;
	int m_settings_count = 0;

		// Through text, as a saved file is. A project keeps text nodes
		// that are only spaces (QETProject's reader, issue #973); other
		// files are read with Qt's defaults.
	enum Spacing { DropSpacing, KeepSpacingAsProjects };
	static QDomElement reparse(const QDomElement &e, Spacing spacing = DropSpacing)
	{
		QDomDocument doc;
		const QByteArray text = e.ownerDocument().toByteArray();
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
		const bool parsed = bool(doc.setContent(text, spacing == KeepSpacingAsProjects
				? QDomDocument::ParseOption::PreserveSpacingOnlyNodes
				: QDomDocument::ParseOption::Default));
#else
		Q_UNUSED(spacing)
		const bool parsed = doc.setContent(text);
#endif
		if (!parsed) {
			qWarning("cannot parse %s", text.constData());
			return QDomElement();
		}
		return doc.documentElement();
	}

		// A fresh settings file; written to disk and read by another
		// QSettings object, so that values come back as the INI file
		// stores them, as strings.
	QString newSettingsFile()
	{
		return m_dir.filePath(QStringLiteral("settings%1.ini").arg(++m_settings_count));
	}

	static std::unique_ptr<QSettings> settings(const QString &path)
	{
		return std::make_unique<QSettings>(path, QSettings::IniFormat);
	}

	static ConductorProperties allConductorFieldsSet(ConductorProperties::ConductorType type)
	{
		ConductorProperties p;
		p.type = type;
		p.color = QColor(0x12, 0x34, 0x56);
		p.m_color_2 = QColor(0xab, 0xcd, 0xef);
		p.text_color = QColor(0x80, 0x00, 0x20);
		p.text = QStringLiteral("W-12 é");
		p.m_function = QStringLiteral("Power");
		p.m_tension_protocol = QStringLiteral("400V AC");
		p.m_wire_color = QStringLiteral("BK");
		p.m_wire_section = QStringLiteral("2.5");
		p.m_formula = QStringLiteral("%prefix%id");
		p.m_bus = QStringLiteral("Bus A");
		p.m_cable = QStringLiteral("W1");
		p.text_size = 12;
		p.m_dash_size = 4;
		p.cond_size = 3;
		p.verti_rotate_text = 90;
		p.horiz_rotate_text = 180;
		p.m_show_text = false;
		p.m_one_text_per_folio = true;
		p.m_bicolor = true;
		p.m_horizontal_alignment = Qt::AlignTop;
		p.m_vertical_alignment = Qt::AlignLeft;
		p.style = Qt::DashDotLine;
		p.singleLineProperties.hasGround = true;
		p.singleLineProperties.hasNeutral = true;
		p.singleLineProperties.is_pen = true;
		p.singleLineProperties.setPhasesCount(3);
		return p;
	}

	static TitleBlockProperties allTitleBlockFieldsSet()
	{
		TitleBlockProperties p;
		p.title = QStringLiteral("Schaltschrank Küche");
		p.author = QStringLiteral("B. H.");
		p.date = QDate(2024, 2, 29);
		p.filename = QStringLiteral("kitchen.qet");
		p.plant = QStringLiteral("Plant 2");
		p.locmach = QStringLiteral("+A1");
		p.indexrev = QStringLiteral("C");
		p.version = QStringLiteral("1.4");
		p.folio = QStringLiteral("%id/%total");
		p.auto_page_num = QStringLiteral("pages");
		p.template_name = QStringLiteral("A4_company.titleblock");
		p.collection = QET::QetCollection::Company;
		p.display_at = Qt::RightEdge;
		p.context.addValue(QStringLiteral("customer"), QStringLiteral("Müller AG"));
		p.context.addValue(QStringLiteral("order-no"), QStringLiteral("4711"));
		return p;
	}

private slots:
	void initTestCase()
	{
		QVERIFY(m_dir.isValid());
	}

	// ---- BorderProperties ----------------------------------------------

	void borderXmlRoundTrip()
	{
		BorderProperties written;
		written.columns_count = 23;
		written.columns_width = 45;
		written.rows_count = 11;
		written.rows_height = 65;
		written.display_columns = false;
		written.display_rows = false;

		QDomDocument doc;
		QDomElement e = doc.createElement(QStringLiteral("diagram"));
		doc.appendChild(e);
		written.toXml(e);

		QDomElement read_from = reparse(e);
		BorderProperties read;
		read.fromXml(read_from);
		QVERIFY(read == written);
	}

		// fromXml() changes only what the element holds: a border read from
		// an element without the attributes keeps its values.
	void borderXmlMissingAttributes()
	{
		QDomDocument doc;
		QDomElement e = doc.createElement(QStringLiteral("diagram"));
		BorderProperties read;
		read.fromXml(e);
		QVERIFY(read == BorderProperties());
	}

	void borderSettingsRoundTrip()
	{
		BorderProperties written;
		written.columns_count = 4;
		written.columns_width = 120;
		written.rows_count = 3;
		written.rows_height = 150;
		written.display_columns = false;
		written.display_rows = false;

		const QString path = newSettingsFile();
		written.toSettings(*settings(path), QStringLiteral("diagrameditor/default"));

		BorderProperties read;
		read.fromSettings(*settings(path), QStringLiteral("diagrameditor/default"));
		QVERIFY(read == written);

			// another prefix holds nothing: the defaults
		BorderProperties other;
		other.fromSettings(*settings(path), QStringLiteral("other/"));
		QVERIFY(other == BorderProperties());
	}

	// ---- TitleBlockProperties ------------------------------------------

	void titleBlockXmlRoundTrip()
	{
		const TitleBlockProperties written = allTitleBlockFieldsSet();
		QDomDocument doc;
		QDomElement e = doc.createElement(QStringLiteral("diagram"));
		doc.appendChild(e);
		written.toXml(e);

		TitleBlockProperties read;
		read.fromXml(reparse(e));
		QVERIFY(read == written);
		QCOMPARE(read.useDate, TitleBlockProperties::UseDateValue);
		QCOMPARE(read.context.value(QStringLiteral("customer")).toString(),
				 QStringLiteral("Müller AG"));
	}

		// "Use the current date" is saved as "now", not as the date of the
		// save, and no date at all as "null"
	void titleBlockXmlDate_data()
	{
		QTest::addColumn<int>("use_date");
		QTest::addColumn<QDate>("date");
		QTest::addColumn<QString>("stored");
		QTest::newRow("current date") << int(TitleBlockProperties::CurrentDate) << QDate(2020, 1, 1) << "now";
		QTest::newRow("no date")      << int(TitleBlockProperties::UseDateValue) << QDate() << "null";
		QTest::newRow("fixed date")   << int(TitleBlockProperties::UseDateValue) << QDate(1999, 12, 31) << "19991231";
	}

	void titleBlockXmlDate()
	{
		QFETCH(int, use_date);
		QFETCH(QDate, date);
		QFETCH(QString, stored);

		TitleBlockProperties written;
		written.useDate = TitleBlockProperties::DateManagement(use_date);
		written.date = date;
		QDomDocument doc;
		QDomElement e = doc.createElement(QStringLiteral("diagram"));
		doc.appendChild(e);
		written.toXml(e);
		QCOMPARE(e.attribute(QStringLiteral("date")), stored);

		TitleBlockProperties read;
		read.fromXml(reparse(e));
		QCOMPARE(int(read.useDate), use_date);
		if (read.useDate == TitleBlockProperties::CurrentDate)
			QCOMPARE(read.finalDate(), QDate::currentDate());
		else
			QCOMPARE(read.date, date);
	}

		// Without a template the collection is not saved, and an element
		// without any attribute reads as an empty title block.
	void titleBlockXmlMissingAttributes()
	{
		TitleBlockProperties written;
		written.collection = QET::QetCollection::Custom;
		QDomDocument doc;
		QDomElement e = doc.createElement(QStringLiteral("diagram"));
		doc.appendChild(e);
		written.toXml(e);
		QVERIFY(!e.hasAttribute(QStringLiteral("titleblocktemplate")));
		QVERIFY(!e.hasAttribute(QStringLiteral("titleblocktemplateCollection")));
		QVERIFY(e.firstChildElement(QStringLiteral("properties")).isNull());

		TitleBlockProperties read;
		read.fromXml(doc.createElement(QStringLiteral("diagram")));
		QVERIFY(read == TitleBlockProperties());
		QCOMPARE(read.display_at, Qt::BottomEdge);
		QCOMPARE(read.collection, QET::QetCollection::Common);
	}

	void titleBlockSettingsRoundTrip()
	{
		const TitleBlockProperties written = allTitleBlockFieldsSet();
		const QString path = newSettingsFile();
		written.toSettings(*settings(path), QStringLiteral("diagrameditor/default"));

		TitleBlockProperties read;
		read.fromSettings(*settings(path), QStringLiteral("diagrameditor/default"));
		QVERIFY(read == written);

			// nothing stored: the folio numbering and bottom edge
		TitleBlockProperties empty;
		empty.fromSettings(*settings(path), QStringLiteral("other/"));
		QCOMPARE(empty.folio, QStringLiteral("%id/%total"));
		QCOMPARE(empty.display_at, Qt::BottomEdge);
		QCOMPARE(empty.title, QString());
		QVERIFY(empty.date.isNull());
		QCOMPARE(empty.context.keys(), QList<QString>());
	}

	// ---- ConductorProperties and SingleLineProperties ------------------

	void conductorXmlRoundTrip_data()
	{
		QTest::addColumn<int>("type");
		QTest::newRow("multi")  << int(ConductorProperties::Multi);
		QTest::newRow("single") << int(ConductorProperties::Single);
	}

	void conductorXmlRoundTrip()
	{
		QFETCH(int, type);
		ConductorProperties written = allConductorFieldsSet(ConductorProperties::ConductorType(type));
		QDomDocument doc;
		QDomElement e = doc.createElement(QStringLiteral("conductor"));
		doc.appendChild(e);
		written.toXml(e);

		QDomElement read_from = reparse(e);
		ConductorProperties read;
		read.fromXml(read_from);
			// a multi-line wire does not save its single-line symbols
		if (type == ConductorProperties::Multi)
			written.singleLineProperties = read.singleLineProperties;
		QVERIFY(read == written);
		QCOMPARE(read.m_cable, written.m_cable);
		QCOMPARE(read.m_bus, written.m_bus);
	}

		// Each line style survives; a solid line is saved as no style
	void conductorXmlStyle_data()
	{
		QTest::addColumn<int>("style");
		QTest::newRow("solid")       << int(Qt::SolidLine);
		QTest::newRow("dashed")      << int(Qt::DashLine);
		QTest::newRow("dash-dotted") << int(Qt::DashDotLine);
	}

	void conductorXmlStyle()
	{
		QFETCH(int, style);
		ConductorProperties written;
		written.style = Qt::PenStyle(style);
		QDomDocument doc;
		QDomElement e = doc.createElement(QStringLiteral("conductor"));
		doc.appendChild(e);
		written.toXml(e);
		QCOMPARE(e.hasAttribute(QStringLiteral("style")), style != Qt::SolidLine);

		QDomElement read_from = reparse(e);
		ConductorProperties read;
		read.style = Qt::DotLine;
		read.fromXml(read_from);
		QCOMPARE(int(read.style), style);
	}

		// A wire as older versions saved it: the attributes added since are
		// missing and read as their defaults. Every field was set to
		// something else first, so a field left as it was shows.
	void conductorXmlMissingAttributes()
	{
		QDomDocument doc;
		QDomElement e = doc.createElement(QStringLiteral("conductor"));
		ConductorProperties read = allConductorFieldsSet(ConductorProperties::Single);
		read.fromXml(e);

		QCOMPARE(read.type, ConductorProperties::Multi);
		QCOMPARE(read.color, QColor(Qt::black));
		QCOMPARE(read.m_bicolor, false);
			// black, unlike a new ConductorProperties' invalid colour
		QCOMPARE(read.m_color_2, QColor(Qt::black));
		QCOMPARE(read.m_dash_size, 1);
		QCOMPARE(read.style, Qt::SolidLine);
		QCOMPARE(read.text_color, QColor(Qt::black));
		QCOMPARE(read.m_formula, QString());
		QCOMPARE(read.m_cable, QString());
		QCOMPARE(read.m_bus, QString());
		QCOMPARE(read.m_function, QString());
		QCOMPARE(read.m_tension_protocol, QString());
		QCOMPARE(read.m_wire_color, QString());
		QCOMPARE(read.m_wire_section, QString());
		QCOMPARE(read.text_size, 9);
		QCOMPARE(read.cond_size, 1.0);
		QCOMPARE(read.m_show_text, true);
		QCOMPARE(read.m_one_text_per_folio, false);
		QCOMPARE(read.horiz_rotate_text, 0.0);
		QCOMPARE(read.m_horizontal_alignment, Qt::Alignment(Qt::AlignBottom));
		QCOMPARE(read.m_vertical_alignment, Qt::Alignment(Qt::AlignRight));
	}

	void conductorSettingsRoundTrip_data()
	{
		conductorXmlRoundTrip_data();
	}

	void conductorSettingsRoundTrip()
	{
		QFETCH(int, type);
		const ConductorProperties written = allConductorFieldsSet(ConductorProperties::ConductorType(type));
		const QString path = newSettingsFile();
		written.toSettings(*settings(path), QStringLiteral("diagrameditor/defaultconductor"));

		ConductorProperties read;
		read.fromSettings(*settings(path), QStringLiteral("diagrameditor/defaultconductor"));
		QVERIFY(read == written);
		QCOMPARE(read.m_cable, written.m_cable);
		QCOMPARE(read.m_bus, written.m_bus);
	}

		// The editor sets the default wire size in steps of 0.2 from 0.4;
		// the reader used to read it as an integer, and "1.5" came back
		// as 0.
	void conductorSettingsFractionalSize_data()
	{
		QTest::addColumn<double>("size");
		QTest::newRow("1.5")  << 1.5;
		QTest::newRow("0.4")  << 0.4;
		QTest::newRow("2")    << 2.0;
	}

	void conductorSettingsFractionalSize()
	{
		QFETCH(double, size);
		ConductorProperties written;
		written.cond_size = size;
		const QString path = newSettingsFile();
		written.toSettings(*settings(path), QStringLiteral("diagrameditor/defaultconductor"));

		ConductorProperties read;
		read.fromSettings(*settings(path), QStringLiteral("diagrameditor/defaultconductor"));
		QCOMPARE(read.cond_size, size);
	}

		// A new installation: nothing stored
	void conductorSettingsMissing()
	{
		ConductorProperties read = allConductorFieldsSet(ConductorProperties::Single);
		read.fromSettings(*settings(newSettingsFile()), QStringLiteral("diagrameditor/defaultconductor"));

		QCOMPARE(read.type, ConductorProperties::Multi);
		QCOMPARE(read.color, QColor(Qt::black));
		QCOMPARE(read.m_color_2, QColor(Qt::black));
		QCOMPARE(read.m_bicolor, false);
		QCOMPARE(read.m_dash_size, 1);
		QCOMPARE(read.style, Qt::SolidLine);
		QCOMPARE(read.text, QStringLiteral("_"));
		QCOMPARE(read.text_color, QColor(Qt::black));
		QCOMPARE(read.m_formula, QString());
		QCOMPARE(read.m_wire_section, QString());
		QCOMPARE(read.cond_size, 1.0);
		QCOMPARE(read.m_show_text, true);
		QCOMPARE(read.m_one_text_per_folio, false);
		QCOMPARE(read.verti_rotate_text, 270.0);
		QCOMPARE(read.horiz_rotate_text, 0.0);
		QCOMPARE(read.m_horizontal_alignment, Qt::Alignment(Qt::AlignBottom));
		QCOMPARE(read.m_vertical_alignment, Qt::Alignment(Qt::AlignRight));
		QVERIFY(read.singleLineProperties == SingleLineProperties());
	}

		// The PEN symbol needs both ground and neutral: without either it
		// is not saved, and not read back even when the file says so.
	void singleLineXml_data()
	{
		QTest::addColumn<bool>("ground");
		QTest::addColumn<bool>("neutral");
		QTest::addColumn<bool>("pen");
		QTest::addColumn<int>("phases");
		QTest::addColumn<bool>("pen_read");

		QTest::newRow("nothing, no phase")    << false << false << false << 0 << false;
		QTest::newRow("ground, 2 phases")     << true  << false << false << 2 << false;
		QTest::newRow("neutral, 3 phases")    << false << true  << false << 3 << false;
		QTest::newRow("PEN")                  << true  << true  << true  << 1 << true;
		QTest::newRow("PEN without ground")   << false << true  << true  << 1 << false;
		QTest::newRow("PEN without neutral")  << true  << false << true  << 1 << false;
	}

	void singleLineXml()
	{
		QFETCH(bool, ground);
		QFETCH(bool, neutral);
		QFETCH(bool, pen);
		QFETCH(int, phases);
		QFETCH(bool, pen_read);

		SingleLineProperties written;
		written.hasGround = ground;
		written.hasNeutral = neutral;
		written.is_pen = pen;
		written.setPhasesCount(phases);

		QDomDocument doc;
		QDomElement e = doc.createElement(QStringLiteral("conductor"));
		doc.appendChild(e);
		written.toXml(e);

		QDomElement read_from = reparse(e);
			// a file that claims PEN anyway
		read_from.setAttribute(QStringLiteral("pen"), pen ? "true" : "false");
		SingleLineProperties read;
		read.fromXml(read_from);
		QCOMPARE(read.hasGround, ground);
		QCOMPARE(read.hasNeutral, neutral);
		QCOMPARE(read.is_pen, pen_read);
		QCOMPARE(int(read.phasesCount()), phases);
		QCOMPARE(read.isPen(), pen_read);
	}

		// Phases are 0 to 3; a file saying more or less is clamped
	void singleLinePhasesClamped_data()
	{
		QTest::addColumn<QString>("stored");
		QTest::addColumn<int>("expected");
		QTest::newRow("7")  << "7"  << 3;
		QTest::newRow("-2") << "-2" << 0;
		QTest::newRow("not a number") << "three" << 0;
	}

	void singleLinePhasesClamped()
	{
		QFETCH(QString, stored);
		QFETCH(int, expected);
		QDomDocument doc;
		QDomElement e = doc.createElement(QStringLiteral("conductor"));
		e.setAttribute(QStringLiteral("phase"), stored);
		SingleLineProperties read;
		read.fromXml(e);
		QCOMPARE(int(read.phasesCount()), expected);
	}

	// ---- NamesList -------------------------------------------------------

		// The QDom reader (projects, categories) and the pugixml reader
		// (the element collection) give the same names.
	void namesReadersAgree_data()
	{
		QTest::addColumn<QString>("names");
		QTest::addColumn<QStringList>("expected"); // lang=name, sorted by lang

		QTest::newRow("two languages")
			<< "<name lang=\"fr\">Moteur</name><name lang=\"en\">Motor</name>"
			<< QStringList{"en=Motor", "fr=Moteur"};
		QTest::newRow("accents and non-Latin")
			<< "<name lang=\"de\">Schütz</name><name lang=\"ru\">Двигатель</name>"
			<< QStringList{"de=Schütz", "ru=Двигатель"};
		QTest::newRow("entities")
			<< "<name lang=\"en\">Motor &amp; brake &lt;3&gt;</name>"
			<< QStringList{"en=Motor & brake <3>"};
		QTest::newRow("five-letter language")
			<< "<name lang=\"pt_BR\">Motor elétrico</name>"
			<< QStringList{"pt_BR=Motor elétrico"};
			// a language must be two letters, or five with "_" in the middle
		QTest::newRow("bad languages ignored")
			<< "<name lang=\"e\">x</name><name lang=\"eng\">x</name>"
			   "<name lang=\"pt-BR\">x</name><name>x</name><name lang=\"it\">ok</name>"
			<< QStringList{"it=ok"};
			// the reader keeps what the file holds; toXml() trims
		QTest::newRow("spaces kept")
			<< "<name lang=\"en\"> Motor </name>"
			<< QStringList{"en= Motor "};
		QTest::newRow("other tags ignored")
			<< "<label lang=\"en\">x</label><name lang=\"en\">Motor</name>"
			<< QStringList{"en=Motor"};
		QTest::newRow("later name wins")
			<< "<name lang=\"en\">first</name><name lang=\"en\">second</name>"
			<< QStringList{"en=second"};
	}

	void namesReadersAgree()
	{
		QFETCH(QString, names);
		QFETCH(QStringList, expected);

		const QByteArray xml = QStringLiteral("<definition><names>%1</names></definition>")
								   .arg(names).toUtf8();
		const auto flatten = [](const NamesList &list) {
			QStringList out;
			for (const QString &lang : list.langs())
				out << lang + QLatin1Char('=') + list[lang];
			return out;
		};

		QDomDocument dom;
		QVERIFY(dom.setContent(xml));
		NamesList from_dom;
		from_dom.fromXml(dom.documentElement());

		pugi::xml_document pugi_doc;
		QVERIFY(pugi_doc.load_buffer(xml.constData(), size_t(xml.size())));
		NamesList from_pugi;
		from_pugi.fromXml(pugi_doc.document_element());

		QCOMPARE(flatten(from_dom), expected);
		QCOMPARE(flatten(from_pugi), expected);
	}

	void namesXmlRoundTrip()
	{
		NamesList written;
		written.addName(QStringLiteral("en"), QStringLiteral("Contactor"));
		written.addName(QStringLiteral("de"), QStringLiteral("Schütz"));
		written.addName(QStringLiteral("pt_BR"), QStringLiteral("Contator"));

			// custom tag names, as the title block templates use them
		const QHash<QString, QString> options{
			{QStringLiteral("ParentTagName"), QStringLiteral("labels")},
			{QStringLiteral("TagName"), QStringLiteral("label")},
			{QStringLiteral("LanguageAttribute"), QStringLiteral("language")}};

		for (const auto &opts : {QHash<QString, QString>(), options}) {
			QDomDocument doc;
			QDomElement root = doc.createElement(QStringLiteral("definition"));
			doc.appendChild(root);
			root.appendChild(written.toXml(doc, opts));

			NamesList read;
			read.fromXml(reparse(root), opts);
			QVERIFY(read == written);
		}
	}

		// toXml() trims; an empty list is saved as one blank English name
		// so the file stays valid, and reads back as no name.
	void namesXmlTrimmedAndEmpty()
	{
		NamesList padded;
		padded.addName(QStringLiteral("en"), QStringLiteral("  Motor \n"));
		QDomDocument doc;
		QDomElement root = doc.createElement(QStringLiteral("definition"));
		doc.appendChild(root);
		root.appendChild(padded.toXml(doc));
		NamesList read;
		read.fromXml(reparse(root));
		QCOMPARE(read[QStringLiteral("en")], QStringLiteral("Motor"));

		QDomDocument empty_doc;
		QDomElement empty_root = empty_doc.createElement(QStringLiteral("definition"));
		empty_doc.appendChild(empty_root);
		empty_root.appendChild(NamesList().toXml(empty_doc));
		const QDomElement name = empty_root.firstChildElement(QStringLiteral("names"))
									 .firstChildElement(QStringLiteral("name"));
		QCOMPARE(name.attribute(QStringLiteral("lang")), QStringLiteral("en"));
		NamesList read_empty;
		read_empty.fromXml(reparse(empty_root));
		QCOMPARE(read_empty.name(), QString());
	}

		// name(): the user's language, then its base language (de for
		// de_CH), then English, then the fallback given, then any name.
	void namesLanguageFallback_data()
	{
		QTest::addColumn<QString>("language");
		QTest::addColumn<QStringList>("names"); // lang=name
		QTest::addColumn<QString>("fallback");
		QTest::addColumn<QString>("expected");

		QTest::newRow("own language")
			<< "fr" << QStringList{"en=Motor", "fr=Moteur"} << "" << "Moteur";
		QTest::newRow("full locale")
			<< "de_CH" << QStringList{"de=Motor DE", "de_CH=Motor CH", "en=Motor"} << "" << "Motor CH";
		QTest::newRow("base language of locale")
			<< "de_CH" << QStringList{"de=Motor DE", "en=Motor"} << "" << "Motor DE";
		QTest::newRow("English")
			<< "fr" << QStringList{"de=Motor DE", "en=Motor"} << "" << "Motor";
		QTest::newRow("empty name skipped")
			<< "fr" << QStringList{"fr=", "en=Motor"} << "" << "Motor";
		QTest::newRow("fallback before other languages")
			<< "fr" << QStringList{"de=Motor DE"} << "unnamed" << "unnamed";
		QTest::newRow("first language without fallback")
			<< "fr" << QStringList{"it=Motore", "de=Motor DE"} << "" << "Motor DE";
		QTest::newRow("no name at all")
			<< "fr" << QStringList{} << "" << "";
	}

	void namesLanguageFallback()
	{
		QFETCH(QString, language);
		QFETCH(QStringList, names);
		QFETCH(QString, fallback);
		QFETCH(QString, expected);

		NamesList list;
		for (const QString &pair : names)
			list.addName(pair.section(QLatin1Char('='), 0, 0), pair.section(QLatin1Char('='), 1));
		s_language = language;
		const QString name = list.name(fallback);
		s_language = QStringLiteral("en");
		QCOMPARE(name, expected);
	}

	// ---- DiagramContext --------------------------------------------------

		// Values and show flags survive; stray spaces around a value are
		// trimmed, a value that is only a space is kept.
	void contextXmlRoundTrip()
	{
		DiagramContext written;
		written.addValue(QStringLiteral("label"), QStringLiteral("-K1"), true);
		written.addValue(QStringLiteral("comment"), QStringLiteral("hidden note"), false);
		written.addValue(QStringLiteral("blank"), QStringLiteral(" "), true);
		written.addValue(QStringLiteral("empty"), QString(), false);

		QDomDocument doc;
		QDomElement root = doc.createElement(QStringLiteral("properties"));
		doc.appendChild(root);
		written.toXml(root);

		DiagramContext read;
		read.fromXml(reparse(root, KeepSpacingAsProjects));
		QCOMPARE(read.keyMustShow(QStringLiteral("comment")), false);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
		QVERIFY(read == written);
		QCOMPARE(read.value(QStringLiteral("blank")).toString(), QStringLiteral(" "));
#else
			// older Qt always drops a text node of spaces only
		QCOMPARE(read.value(QStringLiteral("blank")).toString(), QString());
#endif

		DiagramContext padded;
		padded.addValue(QStringLiteral("label"), QStringLiteral("  -K1 "));
		QDomDocument padded_doc;
		QDomElement padded_root = padded_doc.createElement(QStringLiteral("properties"));
		padded_doc.appendChild(padded_root);
		padded.toXml(padded_root);
		DiagramContext padded_read;
		padded_read.fromXml(reparse(padded_root));
		QCOMPARE(padded_read.value(QStringLiteral("label")).toString(), QStringLiteral("-K1"));
	}

		// Element information drops empty and blank values instead of
		// saving them
	void contextXmlElementInformation()
	{
		DiagramContext written;
		written.addValue(QStringLiteral("label"), QStringLiteral("-K1"));
		written.addValue(QStringLiteral("comment"), QString());
		written.addValue(QStringLiteral("function"), QStringLiteral("  "));

		QDomDocument doc;
		QDomElement root = doc.createElement(QStringLiteral("elementInformations"));
		doc.appendChild(root);
		written.toXml(root, QStringLiteral("elementInformation"));

		DiagramContext read;
		read.fromXml(reparse(root), QStringLiteral("elementInformation"));
		QCOMPARE(read.keys(), QList<QString>{QStringLiteral("label")});
		QCOMPARE(read.value(QStringLiteral("label")).toString(), QStringLiteral("-K1"));
	}

	void contextSettingsRoundTrip()
	{
		DiagramContext written;
		written.addValue(QStringLiteral("customer"), QStringLiteral("Müller AG"));
		written.addValue(QStringLiteral("order-no"), QStringLiteral("4711"));
		written.addValue(QStringLiteral("note_1"), QStringLiteral("a, b; c = d"));

		const QString path = newSettingsFile();
		written.toSettings(*settings(path), QStringLiteral("diagrameditor/defaultproperties"));

		DiagramContext read;
		read.fromSettings(*settings(path), QStringLiteral("diagrameditor/defaultproperties"));
		QVERIFY(read == written);

		DiagramContext none;
		none.fromSettings(*settings(path), QStringLiteral("other"));
		QCOMPARE(none.keys(), QList<QString>());
	}

	// ---- ElementData -----------------------------------------------------

		// An element definition as the element editor writes it: the kind
		// information, the element information, the names and the drawing
		// information, read back by fromXml().
	void elementDataRoundTrip_data()
	{
		QTest::addColumn<int>("kind");
		QTest::newRow("master with contact groups") << 0;
		QTest::newRow("slave")                      << 1;
		QTest::newRow("terminal")                   << 2;
		QTest::newRow("PLC master")                 << 3;
	}

	void elementDataRoundTrip()
	{
		QFETCH(int, kind);

		ElementData written;
		switch (kind) {
			case 0: {
				written.m_type = ElementData::Master;
				written.m_master_type = ElementData::Protection;
				written.m_max_slaves = 3;
				written.m_slave_contact_groups_enabled = true;
				ElementData::SlaveContactGroup power;
				power.type = ElementData::NO;
				power.subtype = ElementData::Power;
				power.contactCount = 3;
				power.terminalCount = 6;
				power.labels = QStringList{"1", "2", "3", "4", "5", "6"};
				ElementData::SlaveContactGroup aux;
				aux.type = ElementData::SW;
				aux.subtype = ElementData::DelayOff;
				aux.labels = QStringList{"11", "12", "14"};
				aux.terminalCount = 3;
				written.m_slave_contact_groups = {power, aux};
				break;
			}
			case 1:
				written.m_type = ElementData::Slave;
				written.m_slave_type = ElementData::delayOnOff;
				written.m_slave_state = ElementData::NC;
				written.m_contact_count = 2;
				break;
			case 2:
				written.m_type = ElementData::Terminal;
				written.m_terminal_type = ElementData::TTFuse;
				written.m_terminal_function = ElementData::TFNeutral;
				break;
			default: {
				written.m_type = ElementData::Master;
				written.m_master_type = ElementData::PLC;
				ElementData::PlcMasterData plc;
				plc.rowHeight = 6.5;
				plc.breakPositions = {8, 16};
				plc.colWidths = {{0, 12.5}, {3, 40.0}};
				plc.colVisible = {{1, false}, {2, true}};
				plc.headerFont.setFamily(QStringLiteral("DejaVu Sans"));
				plc.headerFont.setPointSize(9);
				plc.headerFont.setBold(true);
				plc.cellFont.setFamily(QStringLiteral("DejaVu Sans Mono"));
				plc.cellFont.setPointSize(7);
				plc.columnNames = QStringList{"Addr", "", "Function"};
				plc.columnOrder = {2, 0, 1, 4, 3};
				plc.showHeaders = false;
				ElementData::PlcIO in;
				in.type = ElementData::EntreeAnalogique;
				in.address = QStringLiteral("I1.0");
				in.functionText = QStringLiteral("Level tank");
				in.comment = QStringLiteral("4-20 mA");
				in.crossRef = QStringLiteral("3-B4");
				in.terminalCount = 2;
				in.terminals = QStringList{"T1+", "T1-"};
				ElementData::PlcIO out;
				out.type = ElementData::SortieDigitale;
				out.address = QStringLiteral("Q0.1");
				plc.ios = {in, out};
				written.m_plc_master_data = plc;
				break;
			}
		}
		written.m_informations.addValue(QStringLiteral("manufacturer"), QStringLiteral("ABB"));
		written.m_informations.addValue(QStringLiteral("label"), QStringLiteral("-Q1"));
		written.m_names_list.addName(QStringLiteral("en"), QStringLiteral("Circuit breaker"));
		written.m_names_list.addName(QStringLiteral("de"), QStringLiteral("Leitungsschutzschalter"));
		written.m_drawing_information = QStringLiteral("Drawn after the datasheet, rev. 3");

		QDomDocument doc;
		QDomElement definition = doc.createElement(QStringLiteral("definition"));
		definition.setAttribute(QStringLiteral("type"), QStringLiteral("element"));
		definition.setAttribute(QStringLiteral("link_type"), written.typeToString());
		doc.appendChild(definition);
		definition.appendChild(written.kindInfoToXml(doc));
		QDomElement infos = doc.createElement(QStringLiteral("elementInformations"));
		written.m_informations.toXml(infos, QStringLiteral("elementInformation"));
		definition.appendChild(infos);
		definition.appendChild(written.m_names_list.toXml(doc));
		QDomElement drawing = doc.createElement(QStringLiteral("informations"));
		drawing.appendChild(doc.createTextNode(written.m_drawing_information));
		definition.appendChild(drawing);

		ElementData read;
		QVERIFY(read.fromXml(reparse(definition)));
		QVERIFY(read == written);
			// operator==() does not compare these for every kind of
			// element (nor the drawing information at all); compare them
		QCOMPARE(read.m_slave_contact_groups.size(), written.m_slave_contact_groups.size());
		QVERIFY(read.m_plc_master_data == written.m_plc_master_data);
		QCOMPARE(read.m_drawing_information, written.m_drawing_information);
	}

		// The PLC table of an element placed on a folio, written and read
		// by plcMasterDataToXml() / plcMasterDataFromXml(); reading
		// replaces the table instead of adding to it.
	void elementDataPlcMasterData()
	{
		ElementData written;
		ElementData::PlcMasterData plc;
		plc.rowHeight = 10.25;
		plc.breakPositions = {4};
		plc.colWidths = {{1, 22.75}};
		plc.colVisible = {{0, false}};
		plc.cellFont.setFamily(QStringLiteral("DejaVu Sans"));
		plc.cellFont.setPointSize(8);
		plc.columnNames = QStringList{"A", "B"};
		plc.columnOrder = {1, 0};
		plc.showHeaders = false;
		ElementData::PlcIO io;
		io.type = ElementData::SortieUniverselle;
		io.address = QStringLiteral("Q2.7");
		io.functionText = QStringLiteral("Pump K3");
		io.terminalCount = 3;
		io.terminals = QStringList{"1", "2", "3"};
		plc.ios = {io};
		written.setPlcMasterData(plc);

		QDomDocument doc;
		QDomElement root = written.plcMasterDataToXml(doc);
		doc.appendChild(root);

		ElementData read;
		ElementData::PlcMasterData stale;
		stale.ios = {ElementData::PlcIO(), ElementData::PlcIO()};
		stale.breakPositions = {1, 2, 3};
		read.setPlcMasterData(stale);
		read.plcMasterDataFromXml(reparse(root));
		QVERIFY(read.plcMasterData() == plc);
	}

		// Not an element definition: nothing is read. A definition without
		// a link type is a simple element.
	void elementDataNotADefinition()
	{
		QDomDocument doc;
		QDomElement wrong_tag = doc.createElement(QStringLiteral("element"));
		wrong_tag.setAttribute(QStringLiteral("type"), QStringLiteral("element"));
		wrong_tag.setAttribute(QStringLiteral("link_type"), QStringLiteral("master"));
		ElementData read;
		QVERIFY(!read.fromXml(wrong_tag));
		QCOMPARE(read.m_type, ElementData::Simple);

		QDomElement wrong_type = doc.createElement(QStringLiteral("definition"));
		wrong_type.setAttribute(QStringLiteral("type"), QStringLiteral("category"));
		QVERIFY(!read.fromXml(wrong_type));

		QDomElement plain = doc.createElement(QStringLiteral("definition"));
		plain.setAttribute(QStringLiteral("type"), QStringLiteral("element"));
		read.m_type = ElementData::Slave;
		QVERIFY(read.fromXml(plain));
		QCOMPARE(read.m_type, ElementData::Simple);
	}
};

QTEST_MAIN(tst_propertiesxml)

#include "tst_propertiesxml.moc"
