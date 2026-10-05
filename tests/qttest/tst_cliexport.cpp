// SPDX-License-Identifier: GPL-2.0-or-later
#include "qettesthelpers.h"

#include <QtTest>

#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTemporaryDir>

// The command-line exports, run through the real binary:
//  - the text exports (--export-bom, --export-nets, --export-links,
//    --export-wires, --export-wiring) are compared with golden files in
//    fixtures/golden/<option>-<project>.<ext>;
//  - --export-dxf and --export-png are checked for structure, not bytes:
//    text placement in both depends on the fonts of the machine;
//  - --set-titleblock writes the fields asked for into every folio and the
//    project default, and nothing else;
//  - the error paths give their exit code and leave no output behind.
//
// To update the goldens after an intended change of an export, run the test
// with UPDATE_GOLDEN=1 in the environment: it writes the current outputs to
// the source tree (QET_GOLDEN_DIR) instead of comparing, and skips. Review
// the diff before committing it.
//
// What could differ between machines, and how it is kept out:
//  - the language: element names (a link or net names an element by its
//    name when it has no label) follow the "lang" setting, which defaults to
//    the system locale. Every run gets a settings folder of its own
//    (QET_SETTINGS_DIR) saying lang=en.
//  - order: the binary fixes its own hash seed (main.cpp), and every export
//    runs as a separate process, so an order that changes between runs
//    shows as a golden mismatch. An order taken from the scene would:
//    QGraphicsScene::items() follows memory addresses once the scene has
//    sorted its items (see appendInStackingOrder() in diagram.cpp,
//    bugtracker #343). sameOrderEveryRun() runs an export several times to
//    show that up without waiting for a golden mismatch.
//  - line endings: the files are written in text mode, CRLF on Windows.
//    Lines are compared with any CR removed.
// None of the text exports writes a path, a date or a uuid that changes
// between runs, so nothing else is normalised. The UTF-8 byte order mark
// that --export-bom and --export-wiring write is part of the golden.
class tst_cliexport : public QObject
{
	Q_OBJECT

	QTemporaryDir m_dir;
	int m_run = 0;

	struct Result
	{
		bool finished = false;
		int exitCode = -1;
		QString out;
		QString err;
	};

	// Runs the binary with @p args in a sandbox of its own: HOME, TMPDIR
	// and a settings folder pinning the language.
	Result run(const QStringList &args)
	{
		const QString root = m_dir.filePath(QStringLiteral("run%1").arg(m_run++));
		const QString home = root + QStringLiteral("/home");
		const QString tmp = root + QStringLiteral("/tmp");
		const QString settings = root + QStringLiteral("/settings");
		QDir().mkpath(tmp);
		QDir().mkpath(settings + QStringLiteral("/QElectroTech"));
		QFile ini(settings + QStringLiteral("/QElectroTech/QElectroTech.ini"));
		if (ini.open(QIODevice::WriteOnly | QIODevice::Text)) {
			ini.write("[General]\nlang=en\n");
			ini.close();
		}
		QProcessEnvironment env = QET::Test::sandboxEnvironment(home, tmp);
		env.insert(QStringLiteral("QET_SETTINGS_DIR"), settings);

		QProcess proc;
		proc.setProcessEnvironment(env);
		proc.start(QStringLiteral(QET_TEST_BINARY_PATH), args);
		Result r;
		r.finished = proc.waitForFinished(180000);
		if (!r.finished)
			proc.kill();
		r.exitCode = proc.exitStatus() == QProcess::NormalExit ? proc.exitCode() : -1;
		r.out = QString::fromUtf8(proc.readAllStandardOutput());
		r.err = QString::fromUtf8(proc.readAllStandardError());
		return r;
	}

	static QString describe(const Result &r)
	{
		return QStringLiteral("exit %1, stdout: %2, stderr: %3")
				.arg(r.exitCode).arg(r.out.trimmed(), r.err.trimmed());
	}

	static QString example(const QString &name)
	{
		return QStringLiteral(QET_EXAMPLES_DIR "/") + name;
	}

	static QString fixture(const QString &name)
	{
		return QFINDTESTDATA(QStringLiteral("fixtures/") + name);
	}

	static QByteArray readFile(const QString &path)
	{
		QFile f(path);
		return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
	}

	static QStringList lines(const QByteArray &bytes)
	{
		QString text = QString::fromUtf8(bytes);
		text.remove(QLatin1Char('\r'));
		return text.split(QLatin1Char('\n'));
	}

	// --info's per-folio summary of @p project: the size of each folio in
	// pixels (what --export-png renders) and its conductor count.
	QJsonArray folioInfo(const QString &project)
	{
		const Result r = run({QStringLiteral("--info"), project});
		if (!r.finished || r.exitCode != 0)
			return {};
		return QJsonDocument::fromJson(r.out.toUtf8()).object().value(QStringLiteral("pages")).toArray();
	}

	// The DXF of one folio as group code / value pairs.
	static QList<QPair<int, QString>> dxfPairs(const QByteArray &bytes, QString *error)
	{
		QList<QPair<int, QString>> pairs;
		const QStringList l = lines(bytes);
		if (l.size() % 2) {
			*error = QStringLiteral("odd number of lines (%1)").arg(l.size());
			return {};
		}
		for (int i = 0; i < l.size(); i += 2) {
			bool ok = false;
			const int code = l.at(i).trimmed().toInt(&ok);
			if (!ok) {
				*error = QStringLiteral("line %1: '%2' is not a group code").arg(i + 1).arg(l.at(i));
				return {};
			}
			pairs.append({code, l.at(i + 1)});
		}
		return pairs;
	}

	struct DxfSummary
	{
		QString error;
		QStringList sections;      // in file order
		QStringList layers;        // declared in the LAYER table
		QSet<QString> usedLayers;  // named by an entity
		// entity type counts per layer, text entities left out
		QMap<QString, QMap<QString, int>> shapes;
		int texts = 0;
	};

	// Reads the structure of a DXF: sections in order, each closed, the file
	// ending with EOF, the layer table, and what the entities are.
	static DxfSummary dxfSummary(const QByteArray &bytes)
	{
		DxfSummary s;
		const auto pairs = dxfPairs(bytes, &s.error);
		if (!s.error.isEmpty())
			return s;
		if (pairs.isEmpty() || pairs.last() != qMakePair(0, QStringLiteral("EOF"))) {
			s.error = QStringLiteral("does not end with 0/EOF");
			return s;
		}
		QString section, table, entity, entity_layer;
		auto closeEntity = [&]() {
			if (entity.isEmpty())
				return;
			s.usedLayers.insert(entity_layer);
			if (entity == QLatin1String("TEXT") || entity == QLatin1String("MTEXT"))
				++s.texts;
			else
				++s.shapes[entity_layer][entity];
			entity.clear();
			entity_layer = QStringLiteral("0");
		};
		for (int i = 0; i < pairs.size(); ++i) {
			const int code = pairs.at(i).first;
			const QString &value = pairs.at(i).second;
			if (code == 0 && value == QLatin1String("SECTION")) {
				if (!section.isEmpty()) {
					s.error = QStringLiteral("section %1 not closed").arg(section);
					return s;
				}
				if (i + 1 >= pairs.size() || pairs.at(i + 1).first != 2) {
					s.error = QStringLiteral("SECTION without a name");
					return s;
				}
				section = pairs.at(++i).second;
				s.sections << section;
			} else if (code == 0 && value == QLatin1String("ENDSEC")) {
				closeEntity();
				if (section.isEmpty()) {
					s.error = QStringLiteral("ENDSEC outside a section");
					return s;
				}
				section.clear();
			} else if (code == 0 && value == QLatin1String("EOF")) {
				if (!section.isEmpty() || i != pairs.size() - 1) {
					s.error = QStringLiteral("EOF inside a section or before the end");
					return s;
				}
			} else if (section == QLatin1String("TABLES")) {
				if (code == 0 && value == QLatin1String("TABLE") && i + 1 < pairs.size())
					table = pairs.at(++i).second;
				else if (code == 0 && value == QLatin1String("ENDTAB"))
					table.clear();
				else if (table == QLatin1String("LAYER") && code == 2)
					s.layers << value;
			} else if (section == QLatin1String("ENTITIES")) {
				if (code == 0) {
					closeEntity();
					entity = value;
				} else if (code == 8) {
					entity_layer = value;
				}
			}
		}
		return s;
	}

	// Compares @p actual with the golden file @p golden, or writes it there
	// with UPDATE_GOLDEN set (then skips).
	void compareWithGolden(const QByteArray &actual, const QString &golden)
	{
		const QString path = QStringLiteral(QET_GOLDEN_DIR "/") + golden;
		if (qEnvironmentVariableIsSet("UPDATE_GOLDEN")) {
			QDir().mkpath(QStringLiteral(QET_GOLDEN_DIR));
			QFile f(path);
			QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(f.errorString()));
			f.write(actual);
			f.close();
			QSKIP(qPrintable(QStringLiteral("UPDATE_GOLDEN: wrote %1 (%2 bytes)")
							 .arg(path).arg(actual.size())));
		}
		QVERIFY2(QFile::exists(path),
				 qPrintable(QStringLiteral("no golden %1; run with UPDATE_GOLDEN=1 to write it")
							.arg(path)));
		const QStringList expected = lines(readFile(path));
		const QStringList got = lines(actual);
		const int n = qMax(expected.size(), got.size());
		for (int i = 0; i < n; ++i) {
			const QString e = expected.value(i, QStringLiteral("<end of file>"));
			const QString g = got.value(i, QStringLiteral("<end of file>"));
			if (e != g || i >= expected.size() || i >= got.size()) {
				QFAIL(qPrintable(QStringLiteral(
						"output differs from %1 at line %2\n   expected: %3\n     actual: %4\n"
						"(%5 lines expected, %6 written; UPDATE_GOLDEN=1 rewrites the golden)")
						.arg(golden).arg(i + 1).arg(e, g)
						.arg(expected.size()).arg(got.size())));
			}
		}
	}

	// The layers DxfExport declares (sources/dxfexport.h), besides "0".
	static QStringList qetLayers()
	{
		return {QStringLiteral("QET_BORDER"), QStringLiteral("QET_TITLEBLOCK"),
				QStringLiteral("QET_SYMBOLS"), QStringLiteral("QET_SYMBOL_TEXTS"),
				QStringLiteral("QET_TERMINALS"), QStringLiteral("QET_WIRES"),
				QStringLiteral("QET_WIRE_NUMBERS"), QStringLiteral("QET_JUNCTIONS"),
				QStringLiteral("QET_TEXTS"), QStringLiteral("QET_XREFS"),
				QStringLiteral("QET_SHAPES"), QStringLiteral("QET_TABLES"),
				QStringLiteral("QET_IMAGES")};
	}

	// The files an image/DXF export wrote to @p dir, in name order, after
	// checking they are named <NN>_<title>.<ext>, NN = 01, 02...
	static QStringList folioFiles(const QString &dir, const QString &ext, QString *error)
	{
		const QStringList files = QDir(dir).entryList(QDir::Files, QDir::Name);
		static const QRegularExpression name(QStringLiteral("^(\\d\\d)_.+\\.(\\w+)$"));
		for (int i = 0; i < files.size(); ++i) {
			const auto m = name.match(files.at(i));
			if (!m.hasMatch() || m.captured(1).toInt() != i + 1 || m.captured(2) != ext) {
				*error = QStringLiteral("unexpected file %1 at position %2").arg(files.at(i)).arg(i + 1);
				return {};
			}
		}
		return files;
	}

private slots:
	void initTestCase()
	{
		QVERIFY(m_dir.isValid());
		QVERIFY(QFile::exists(QStringLiteral(QET_TEST_BINARY_PATH)));
	}

	// industrial.qet: 50 folios with cross references and folio reports.
	// Its nets (190 kB) and wiring list (90 kB) are too long to review as
	// goldens, so those two come from the smaller projects:
	// tableau_domestique.qet, masters and slaves on several folios and
	// reports between them; wiring_list_arrows.qet, reports joining wires
	// across folios, and cables.
	void textExport_data()
	{
		QTest::addColumn<QString>("option");
		QTest::addColumn<QString>("project");
		QTest::addColumn<QString>("golden");

		const QMap<QString, QString> extension {
			{QStringLiteral("bom"), QStringLiteral("csv")},
			{QStringLiteral("nets"), QStringLiteral("json")},
			{QStringLiteral("links"), QStringLiteral("csv")},
			{QStringLiteral("wires"), QStringLiteral("csv")},
			{QStringLiteral("wiring"), QStringLiteral("csv")},
		};
		const QStringList all = extension.keys();
		struct Case { QString name; QString path; QStringList options; };
		const QList<Case> cases {
			{QStringLiteral("industrial"), example(QStringLiteral("industrial.qet")),
			 {QStringLiteral("bom"), QStringLiteral("links"), QStringLiteral("wires")}},
			{QStringLiteral("tableau_domestique"), example(QStringLiteral("tableau_domestique.qet")), all},
			{QStringLiteral("wiring_list_arrows"), fixture(QStringLiteral("wiring_list_arrows.qet")), all},
		};
		for (const Case &c : cases) {
			for (const QString &option : c.options) {
				const QString golden = option + QLatin1Char('-') + c.name
						+ QLatin1Char('.') + extension.value(option);
				QTest::newRow(qPrintable(golden))
						<< QStringLiteral("--export-") + option << c.path << golden;
			}
		}
	}

	void textExport()
	{
		QFETCH(QString, option);
		QFETCH(QString, project);
		QFETCH(QString, golden);
		QVERIFY2(QFile::exists(project), qPrintable(project));

		const QString output = m_dir.filePath(golden);
		const Result r = run({option, project, output});
		QVERIFY2(r.finished && r.exitCode == 0, qPrintable(describe(r)));
		QVERIFY2(QFile::exists(output), "the export wrote no file");
		compareWithGolden(readFile(output), golden);
	}

	// An export that writes a folio's items in QGraphicsScene::items() order
	// writes them in a different order on each run, as that order follows
	// memory addresses. The goldens would catch it too, but only by chance in
	// a single run; here the same export runs several times, each in a
	// process of its own, and must write the same bytes every time.
	// --export-nets starts each net from the conductors in items() order too;
	// it has never been seen to vary, as conductors are all allocated alike,
	// but nothing guarantees that.
	void sameOrderEveryRun_data()
	{
		QTest::addColumn<QString>("option");
		QTest::newRow("links") << QStringLiteral("--export-links");
		QTest::newRow("nets") << QStringLiteral("--export-nets");
	}

	void sameOrderEveryRun()
	{
		QFETCH(QString, option);
		const QString project = example(QStringLiteral("tableau_domestique.qet"));
		QVERIFY2(QFile::exists(project), qPrintable(project));

		QByteArray first;
		for (int i = 0; i < 5; ++i) {
			const QString output = m_dir.filePath(
					QStringLiteral("same-order-%1-%2").arg(option.mid(2)).arg(i));
			const Result r = run({option, project, output});
			QVERIFY2(r.finished && r.exitCode == 0, qPrintable(describe(r)));
			const QByteArray bytes = readFile(output);
			QVERIFY2(!bytes.isEmpty(), "the export wrote nothing");
			if (i == 0) {
				first = bytes;
				continue;
			}
			const QStringList expected = lines(first);
			const QStringList got = lines(bytes);
			for (int l = 0; l < qMax(expected.size(), got.size()); ++l) {
				if (expected.value(l) != got.value(l) || l >= expected.size() || l >= got.size())
					QFAIL(qPrintable(QStringLiteral(
							"run %1 differs from run 1 at line %2\n   run 1: %3\n   run %1: %4")
							.arg(i + 1).arg(l + 1)
							.arg(expected.value(l, QStringLiteral("<end of file>")),
								 got.value(l, QStringLiteral("<end of file>")))));
			}
		}
	}

	// One DXF per folio, each a whole DXF file: HEADER, TABLES, BLOCKS and
	// ENTITIES sections, each closed, then EOF; the QET layers declared and
	// every entity on a declared layer. Text positions and the splitting of
	// long texts depend on the fonts, so texts are only counted as present;
	// the other entities, which come from the saved geometry, are counted
	// per layer and compared with a golden.
	void dxf_data()
	{
		QTest::addColumn<QString>("project");
		QTest::addColumn<QString>("golden");
		QTest::newRow("industrial") << example(QStringLiteral("industrial.qet"))
									<< QStringLiteral("dxf-industrial.txt");
		QTest::newRow("tableau_domestique") << example(QStringLiteral("tableau_domestique.qet"))
											<< QStringLiteral("dxf-tableau_domestique.txt");
	}

	void dxf()
	{
		QFETCH(QString, project);
		QFETCH(QString, golden);

		const QJsonArray folios = folioInfo(project);
		QVERIFY2(!folios.isEmpty(), "--info failed");

		const QString dir = m_dir.filePath(QStringLiteral("dxf-") + golden);
		const Result r = run({QStringLiteral("--export-dxf"), project, dir});
		QVERIFY2(r.finished && r.exitCode == 0, qPrintable(describe(r)));

		QString error;
		const QStringList files = folioFiles(dir, QStringLiteral("dxf"), &error);
		QVERIFY2(error.isEmpty(), qPrintable(error));
		QCOMPARE(files.size(), folios.size());

		const QStringList expected_layers = QStringList{QStringLiteral("0")} + qetLayers();
		const QStringList expected_sections {QStringLiteral("HEADER"), QStringLiteral("TABLES"),
											 QStringLiteral("BLOCKS"), QStringLiteral("ENTITIES")};
		QByteArray counts;
		for (int i = 0; i < files.size(); ++i) {
			const QString file = files.at(i);
			const QByteArray bytes = readFile(QDir(dir).filePath(file));
			QVERIFY2(bytes.startsWith("999\r\nQET\r\n0\r\nSECTION\r\n"), qPrintable(file));
			const DxfSummary s = dxfSummary(bytes);
			QVERIFY2(s.error.isEmpty(), qPrintable(file + QStringLiteral(": ") + s.error));
			QCOMPARE(s.sections, expected_sections);
			QCOMPARE(s.layers, expected_layers);
			for (const QString &used : s.usedLayers)
				QVERIFY2(s.layers.contains(used), qPrintable(file + QStringLiteral(": undeclared layer ") + used));

			// Every folio has its border and title block; one with wires
			// has them on the wire layer; no terminal is drawn unless asked.
			QVERIFY2(s.shapes.contains(QStringLiteral("QET_BORDER")), qPrintable(file));
			QVERIFY2(s.texts > 0, qPrintable(file));
			const int conductors = folios.at(i).toObject().value(QStringLiteral("conductors")).toInt();
			QCOMPARE(s.shapes.contains(QStringLiteral("QET_WIRES")), conductors > 0);
			QVERIFY2(!s.shapes.contains(QStringLiteral("QET_TERMINALS")), qPrintable(file));

			counts += QStringLiteral("%1").arg(i + 1, 2, 10, QLatin1Char('0')).toUtf8();
			for (auto layer = s.shapes.cbegin(); layer != s.shapes.cend(); ++layer)
				for (auto type = layer->cbegin(); type != layer->cend(); ++type)
					counts += QStringLiteral(" %1:%2=%3").arg(layer.key(), type.key()).arg(type.value()).toUtf8();
			counts += '\n';
		}
		compareWithGolden(counts, golden);
	}

	// --show-terminals draws the terminals, on their own layer.
	void dxfShowTerminals()
	{
		const QString project = fixture(QStringLiteral("wiring_list_arrows.qet"));
		const QString dir = m_dir.filePath(QStringLiteral("dxf-terminals"));
		const Result r = run({QStringLiteral("--export-dxf"), project, dir, QStringLiteral("--show-terminals")});
		QVERIFY2(r.finished && r.exitCode == 0, qPrintable(describe(r)));
		QString error;
		const QStringList files = folioFiles(dir, QStringLiteral("dxf"), &error);
		QVERIFY2(error.isEmpty(), qPrintable(error));
		QVERIFY(!files.isEmpty());
		for (const QString &file : files) {
			const DxfSummary s = dxfSummary(readFile(QDir(dir).filePath(file)));
			QVERIFY2(s.error.isEmpty(), qPrintable(s.error));
			QVERIFY2(s.shapes.contains(QStringLiteral("QET_TERMINALS")), qPrintable(file));
		}
	}

	// One PNG per folio, of the folio's size in pixels (as --info reports
	// it), and not blank. The pixels themselves depend on the fonts and the
	// rasteriser, so they are not compared.
	void png()
	{
		const QString project = example(QStringLiteral("tableau_domestique.qet"));
		const QJsonArray folios = folioInfo(project);
		QVERIFY2(!folios.isEmpty(), "--info failed");

		const QString dir = m_dir.filePath(QStringLiteral("png"));
		const Result r = run({QStringLiteral("--export-png"), project, dir});
		QVERIFY2(r.finished && r.exitCode == 0, qPrintable(describe(r)));

		QString error;
		const QStringList files = folioFiles(dir, QStringLiteral("png"), &error);
		QVERIFY2(error.isEmpty(), qPrintable(error));
		QCOMPARE(files.size(), folios.size());
		for (int i = 0; i < files.size(); ++i) {
			const QString path = QDir(dir).filePath(files.at(i));
			QImageReader reader(path);
			QCOMPARE(reader.format(), QByteArray("png"));
			const QImage image = reader.read();
			QVERIFY2(!image.isNull(), qPrintable(path + QStringLiteral(": ") + reader.errorString()));
			const QJsonObject folio = folios.at(i).toObject();
			QCOMPARE(image.size(), QSize(folio.value(QStringLiteral("width_px")).toInt(),
										 folio.value(QStringLiteral("height_px")).toInt()));
			// Something is drawn: the corner is the white background, the
			// border line is not.
			int dark = 0;
			const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
			for (int y = 0; y < gray.height(); y += 4) {
				const uchar *row = gray.constScanLine(y);
				for (int x = 0; x < gray.width(); x += 4)
					if (row[x] < 128)
						++dark;
			}
			QVERIFY2(dark > 100, qPrintable(QStringLiteral("%1 looks blank").arg(path)));
		}
	}

	// --set-titleblock writes the fields asked for on every folio and on the
	// project default, a custom key as a title block property, and changes
	// nothing else: with those fields taken out, the file is the one --resave
	// writes.
	void setTitleBlock()
	{
		const QString project = example(QStringLiteral("industrial.qet"));
		const QString base = m_dir.filePath(QStringLiteral("settb-base.qet"));
		const QString stamped = m_dir.filePath(QStringLiteral("settb-stamped.qet"));

		Result r = run({QStringLiteral("--resave"), project, base});
		QVERIFY2(r.finished && r.exitCode == 0, qPrintable(describe(r)));
		r = run({QStringLiteral("--set-titleblock"), project, stamped,
				 QStringLiteral("title=CLI golden test"), QStringLiteral("revision=C"),
				 QStringLiteral("date=2021-03-04"), QStringLiteral("cli_customer=ACME")});
		QVERIFY2(r.finished && r.exitCode == 0, qPrintable(describe(r)));

		QDomDocument base_doc, stamped_doc;
		QVERIFY(base_doc.setContent(readFile(base)));
		QVERIFY(stamped_doc.setContent(readFile(stamped)));

		// The title blocks: each folio's <diagram> and the project default,
		// <newdiagrams><inset>.
		auto titleBlocks = [](const QDomDocument &doc) {
			QList<QDomElement> list;
			const QDomElement root = doc.documentElement();
			for (QDomElement d = root.firstChildElement(QStringLiteral("diagram"));
				 !d.isNull(); d = d.nextSiblingElement(QStringLiteral("diagram")))
				list << d;
			list << root.firstChildElement(QStringLiteral("newdiagrams"))
							.firstChildElement(QStringLiteral("inset"));
			return list;
		};
		const QList<QDomElement> stamped_blocks = titleBlocks(stamped_doc);
		const QList<QDomElement> base_blocks = titleBlocks(base_doc);
		QCOMPARE(stamped_blocks.size(), base_blocks.size());
		QVERIFY2(stamped_blocks.size() == 51, qPrintable(QString::number(stamped_blocks.size())));

		for (QDomElement block : stamped_blocks) {
			QVERIFY(!block.isNull());
			QCOMPARE(block.attribute(QStringLiteral("title")), QStringLiteral("CLI golden test"));
			QCOMPARE(block.attribute(QStringLiteral("indexrev")), QStringLiteral("C"));
			QCOMPARE(block.attribute(QStringLiteral("date")), QStringLiteral("20210304"));
			QDomElement found;
			const QDomElement properties = block.firstChildElement(QStringLiteral("properties"));
			for (QDomElement p = properties.firstChildElement(QStringLiteral("property"));
				 !p.isNull(); p = p.nextSiblingElement(QStringLiteral("property")))
				if (p.attribute(QStringLiteral("name")) == QLatin1String("cli_customer"))
					found = p;
			QVERIFY2(!found.isNull(), "custom field not written");
			QCOMPARE(found.text(), QStringLiteral("ACME"));

			// Take the stamped fields out, for the comparison below.
			found.parentNode().removeChild(found);
			if (!properties.hasChildNodes())
				block.removeChild(properties);
		}
		for (const QList<QDomElement> &blocks : {stamped_blocks, base_blocks}) {
			for (QDomElement block : blocks) {
				block.removeAttribute(QStringLiteral("title"));
				block.removeAttribute(QStringLiteral("indexrev"));
				block.removeAttribute(QStringLiteral("date"));
			}
		}
		const QStringList expected = base_doc.toString(1).split(QLatin1Char('\n'));
		const QStringList got = stamped_doc.toString(1).split(QLatin1Char('\n'));
		for (int i = 0; i < qMax(expected.size(), got.size()); ++i) {
			if (expected.value(i) != got.value(i))
				QFAIL(qPrintable(QStringLiteral("--set-titleblock changed more than the fields "
												"asked for, at line %1:\n   --resave: %2\n    stamped: %3")
								 .arg(i + 1).arg(expected.value(i).left(300), got.value(i).left(300))));
		}
	}

	// The documented exit codes: 2 for a missing input or bad arguments, 1
	// for a project that cannot be opened or an output that cannot be
	// written. No output file is left behind.
	void errors_data()
	{
		QTest::addColumn<QStringList>("args");
		QTest::addColumn<int>("exitCode");
		QTest::addColumn<QString>("message");

		// Placeholders, replaced in errors(): @OUT the output path that must
		// not appear, @MISSING a file that does not exist, @GARBAGE a file
		// that is not a project, @EMPTY an empty file, @PROJECT a good one.
		const QString out = QStringLiteral("@OUT");
		const QString project = QStringLiteral("@PROJECT");

		for (const QString &option : {QStringLiteral("--export-bom"), QStringLiteral("--export-nets"),
									  QStringLiteral("--export-links"), QStringLiteral("--export-wires"),
									  QStringLiteral("--export-wiring"), QStringLiteral("--export-dxf"),
									  QStringLiteral("--export-png")}) {
			const QByteArray o = option.toUtf8();
			QTest::newRow(o + " missing project") << QStringList{option, QStringLiteral("@MISSING"), out}
												  << 2 << QStringLiteral("Project not found");
			QTest::newRow(o + " garbage project") << QStringList{option, QStringLiteral("@GARBAGE"), out}
												  << 1 << QStringLiteral("Failed to open project");
			QTest::newRow(o + " no output") << QStringList{option, project}
											<< 2 << QStringLiteral("Usage:");
			QTest::newRow(o + " no project") << QStringList{option}
											 << 2 << QStringLiteral("Usage:");
		}
		QTest::newRow("empty project") << QStringList{QStringLiteral("--export-bom"), QStringLiteral("@EMPTY"), out}
									   << 1 << QStringLiteral("Failed to open project");

		for (const QString &option : {QStringLiteral("--export-bom"), QStringLiteral("--export-nets"),
									  QStringLiteral("--export-links"), QStringLiteral("--export-wires"),
									  QStringLiteral("--export-wiring")}) {
			QTest::newRow((option + QStringLiteral(" unwritable output")).toUtf8())
					<< QStringList{option, project, QStringLiteral("@NODIR/out.txt")}
					<< 1 << QString();
		}

		const QString settb = QStringLiteral("--set-titleblock");
		QTest::newRow("settb no assignment") << QStringList{settb, project, out}
											 << 2 << QStringLiteral("No field assignments");
		QTest::newRow("settb bad assignment") << QStringList{settb, project, out, QStringLiteral("revision")}
											  << 2 << QStringLiteral("Bad assignment");
		QTest::newRow("settb empty key") << QStringList{settb, project, out, QStringLiteral("=B")}
										 << 2 << QStringLiteral("Bad assignment");
		QTest::newRow("settb bad date") << QStringList{settb, project, out, QStringLiteral("date=04.03.2021")}
										<< 2 << QStringLiteral("Bad date");
		QTest::newRow("settb good then bad") << QStringList{settb, project, out, QStringLiteral("revision=B"),
															QStringLiteral("date=tomorrow")}
											 << 2 << QStringLiteral("Bad date");
		QTest::newRow("settb missing project") << QStringList{settb, QStringLiteral("@MISSING"), out,
															  QStringLiteral("revision=B")}
											   << 2 << QStringLiteral("Project not found");
		QTest::newRow("check-elements missing") << QStringList{QStringLiteral("--check-elements"),
															   QStringLiteral("@MISSING")}
												<< 2 << QStringLiteral("Not found");
	}

	void errors()
	{
		QFETCH(QStringList, args);
		QFETCH(int, exitCode);
		QFETCH(QString, message);

		const QString dir = m_dir.filePath(QStringLiteral("errors%1").arg(m_run));
		QVERIFY(QDir().mkpath(dir));
		const QString out = dir + QStringLiteral("/out");
		const QString garbage = dir + QStringLiteral("/garbage.qet");
		const QString empty = dir + QStringLiteral("/empty.qet");
		{
			QFile g(garbage);
			QVERIFY(g.open(QIODevice::WriteOnly));
			g.write("this is not a QElectroTech project\n<project><diagram>");
			QFile e(empty);
			QVERIFY(e.open(QIODevice::WriteOnly));
		}
		for (QString &a : args) {
			a.replace(QStringLiteral("@OUT"), out);
			a.replace(QStringLiteral("@MISSING"), dir + QStringLiteral("/missing.qet"));
			a.replace(QStringLiteral("@GARBAGE"), garbage);
			a.replace(QStringLiteral("@EMPTY"), empty);
			a.replace(QStringLiteral("@NODIR"), dir + QStringLiteral("/no/such/dir"));
			a.replace(QStringLiteral("@PROJECT"), fixture(QStringLiteral("wiring_list_arrows.qet")));
		}

		const Result r = run(args);
		QVERIFY2(r.finished, "the binary did not finish");
		QVERIFY2(r.exitCode == exitCode, qPrintable(describe(r)));
		if (!message.isEmpty())
			QVERIFY2(r.err.contains(message), qPrintable(describe(r)));
		QVERIFY2(!QFileInfo::exists(out), "an output was left behind");
		QVERIFY2(!QFileInfo::exists(dir + QStringLiteral("/no")), "an output directory was created");
		// Only the inputs made above are left in the folder.
		QCOMPARE(QDir(dir).entryList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name),
				 (QStringList{QStringLiteral("empty.qet"), QStringLiteral("garbage.qet")}));
	}
};

QTEST_APPLESS_MAIN(tst_cliexport)

#include "tst_cliexport.moc"
