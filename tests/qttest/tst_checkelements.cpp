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
#include "qettesthelpers.h"

#include <QtTest>

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>

// --check-elements, the validator for element definitions (.elmt): what it
// says about each kind of problem it looks for, the exit code that goes with
// it, and what it says about the collection shipped in elements/.
//
// For each file it prints one line, "OK", "WARN" or "FAIL", the path and the
// reason, then a count of each. A FAIL (unreadable XML, a root other than
// <definition type="element">, a missing or zero size, two terminals with
// one name) makes the exit code 1; a WARN (a shape with a nan or inf
// coordinate, a negative size, no terminals, an unnamed terminal on a
// symbol that is not a folio report) does not. A path that does not exist,
// a directory without .elmt files or no path at all give 2.
//
// The fixtures in fixtures/elements/ each carry one problem; four of them
// are definitions QElectroTech refuses to load that the checker still
// passes (rejectedByLoader, an expected failure until it learns the
// loader's rules). Runs the real binary, also over the whole collection,
// which must give exactly the failures listed in knownFailures().
class tst_checkelements : public QObject
{
	Q_OBJECT

	QTemporaryDir m_dir;
	int m_run = 0;

	struct Result
	{
		bool finished = false;
		int exit_code = -1;
		QString out;
		QString err;
	};

	Result check(const QStringList &args, int timeout_ms = 60000)
	{
		const QString home = m_dir.filePath(QStringLiteral("home%1").arg(m_run++));
		QProcess proc;
		proc.setProcessEnvironment(QET::Test::sandboxEnvironment(home));
		proc.start(QStringLiteral(QET_TEST_BINARY_PATH),
				   QStringList{QStringLiteral("--check-elements")} + args);
		Result r;
		r.finished = proc.waitForFinished(timeout_ms)
				&& proc.exitStatus() == QProcess::NormalExit;
		if (!r.finished)
			proc.kill();
		r.exit_code = proc.exitCode();
		r.out = QString::fromUtf8(proc.readAllStandardOutput());
		r.err = QString::fromUtf8(proc.readAllStandardError());
		return r;
	}

	static QString fixturesDir()
	{
		return QFileInfo(QFINDTESTDATA("fixtures/elements/valid.elmt")).absolutePath();
	}

	static QString details(const Result &r)
	{
		return QStringLiteral("exit code %1\nstdout:\n%2stderr:\n%3")
				.arg(r.exit_code).arg(r.out, r.err);
	}

	// The shipped collection: every element but these passes. All of them
	// are devices with bridged terminals drawn twice under one name (a
	// power supply's two -V, a DIN-rail module's two N), which the terminal
	// name rule refuses; they stay listed until they are renamed.
	static QStringList knownFailures()
	{
		const QString loxone = QStringLiteral("10_electric/20_manufacturers_articles/loxone/");
		const QString shelly = QStringLiteral(
				"10_electric/98_graphics/99_assembly_plan/01_thumbnails_mounting_plate/shelly/");
		QStringList list;
		for (const char *device : {"audioserver", "energy_meter_1ph_tree",
								   "energy_meter_3ph_tree", "extension", "froeling_extension",
								   "miniserver_compact", "miniserver_gen_2", "modbus_extension",
								   "multi_extension_air", "power_supply_4_2a",
								   "power_supply_backup", "rs232_extension", "rs485_extension",
								   "schueco_extension", "stereo_extension"}) {
			const QString d = QString::fromLatin1(device);
			list << loxone + d + QLatin1Char('/') + d + QStringLiteral("_komplett_1reihig.elmt")
				 << loxone + d + QLatin1Char('/') + d + QStringLiteral("_komplett_2reihig.elmt");
		}
		for (const char *device : {"lunatone/dali_1ch_dimmer_cv", "lunatone/dali_2ch_dimmer_cv",
								   "mean_well/hdr_100_24", "mean_well/hdr_150_24",
								   "mean_well/hdr_60_24"}) {
			const QString d = QString::fromLatin1(device);
			const QString base = QStringLiteral("10_electric/20_manufacturers_articles/") + d
					+ QLatin1Char('/') + d.section(QLatin1Char('/'), 1);
			list << base + QStringLiteral("_komplett_1reihig.elmt")
				 << base + QStringLiteral("_komplett_2reihig.elmt");
		}
		list << shelly + QStringLiteral("90_shelly_v1/shelly25.elmt")
			 << shelly + QStringLiteral("90_shelly_v1/shelly_dimmer2.elmt")
			 << shelly + QStringLiteral("91_shelly_plus/shelly_plus_1pm.elmt")
			 << shelly + QStringLiteral("91_shelly_plus/shelly_plus_2pm.elmt")
			 << shelly + QStringLiteral("92_shelly_pro/shelly_pro_1pm.elmt")
			 << shelly + QStringLiteral("92_shelly_pro/shelly_pro_2pm.elmt")
			 << shelly + QStringLiteral("92_shelly_pro/shelly_pro_4pm.elmt");
		list.sort();
		return list;
	}


private slots:
	void initTestCase()
	{
		QVERIFY(m_dir.isValid());
		QVERIFY(QFile::exists(QStringLiteral(QET_TEST_BINARY_PATH)));
		QVERIFY2(QFile::exists(fixturesDir() + QStringLiteral("/valid.elmt")),
				 "fixtures/elements/ not found");
	}

	void oneElement_data()
	{
		QTest::addColumn<QString>("file");
		QTest::addColumn<QString>("status");
		QTest::addColumn<QString>("reason");
		QTest::addColumn<int>("exit_code");

		QTest::newRow("valid")
				<< "valid.elmt" << "OK" << "(2 terminals)" << 0;
		QTest::newRow("malformed XML")
				<< "malformed_xml.elmt" << "FAIL" << "(XML error line 11" << 1;
		QTest::newRow("root is not an element")
				<< "wrong_root.elmt" << "FAIL"
				<< "(root is not <definition type=\"element\">)" << 1;
		QTest::newRow("no width")
				<< "missing_width.elmt" << "FAIL" << "(missing/zero bounding box x20)" << 1;
		QTest::newRow("zero height")
				<< "zero_height.elmt" << "FAIL" << "(missing/zero bounding box 20x0)" << 1;
		// " L " is "L" once trimmed, as the element editor compares them.
		QTest::newRow("repeated terminal name")
				<< "repeated_terminal_names.elmt" << "FAIL"
				<< QStringLiteral("(repeated terminal names: L ×2)") << 1;
		QTest::newRow("unnamed terminal")
				<< "unnamed_terminal.elmt" << "WARN" << "(1 of 2 terminals have no name)" << 0;
		// A folio report's terminal needs no name: it has only the one.
		QTest::newRow("unnamed terminal on a report")
				<< "report_unnamed_terminal.elmt" << "OK" << "(1 terminals)" << 0;
		QTest::newRow("no terminals")
				<< "no_terminals.elmt" << "WARN" << "(loads, but 0 terminals)" << 0;
		QTest::newRow("nan coordinate")
				<< "nonfinite_coordinate.elmt" << "WARN"
				<< "(<line> with a non-finite coordinate is not drawn)" << 0;
		QTest::newRow("negative width")
				<< "negative_width.elmt" << "WARN"
				<< "(negative bounding box -20x20, 2 terminals)" << 0;
	}

	void oneElement()
	{
		QFETCH(QString, file);
		QFETCH(QString, status);
		QFETCH(QString, reason);
		QFETCH(int, exit_code);

		const QString path = fixturesDir() + QLatin1Char('/') + file;
		const Result r = check({path});
		QVERIFY2(r.finished, qPrintable(QStringLiteral("--check-elements did not finish\n")
										+ details(r)));

		// "WARN  <path>  (reason)": the status padded to six characters.
		const QString line = status.leftJustified(6) + path + QStringLiteral("  ") + reason;
		QVERIFY2(r.out.contains(line),
				 qPrintable(QStringLiteral("expected the line\n%1\n").arg(line) + details(r)));
		const QString summary = QStringLiteral("1 file(s), %1 warning(s), %2 failure(s)")
				.arg(status == QLatin1String("WARN") ? 1 : 0)
				.arg(status == QLatin1String("FAIL") ? 1 : 0);
		QVERIFY2(r.out.contains(summary),
				 qPrintable(QStringLiteral("expected the summary \"%1\"\n").arg(summary)
							+ details(r)));
		QVERIFY2(r.exit_code == exit_code,
				 qPrintable(QStringLiteral("expected exit code %1\n").arg(exit_code)
							+ details(r)));
	}

	// Definitions the checker passes but QElectroTech does not load:
	// Element::buildFromXml() wants width, height, hotspot_x and hotspot_y
	// as integers and at least one child, and leaves the symbol off the
	// folio otherwise (seen through --info: "elements": 0). checkOneElement()
	// reads width and height as reals (so 20.5 and nan pass), never looks
	// at the hotspot, and calls an empty definition one that "loads".
	void rejectedByLoader_data()
	{
		QTest::addColumn<QString>("file");
		QTest::newRow("no hotspot_x") << "missing_hotspot.elmt";
		QTest::newRow("fractional width") << "fractional_width.elmt";
		QTest::newRow("nan width") << "nan_width.elmt";
		QTest::newRow("empty definition") << "empty_definition.elmt";
	}

	void rejectedByLoader()
	{
		QFETCH(QString, file);
		const QString path = fixturesDir() + QLatin1Char('/') + file;
		const Result r = check({path});
		QVERIFY2(r.finished, qPrintable(details(r)));
		QEXPECT_FAIL("", "--check-elements does not apply the loader's rules "
					 "for the size, the hotspot and an empty definition", Abort);
		QVERIFY2(r.out.startsWith(QStringLiteral("FAIL  ")) && r.exit_code == 1,
				 qPrintable(QStringLiteral("QElectroTech does not load this element, "
										   "but the checker passes it\n") + details(r)));
	}

	// A directory is searched for .elmt files, in subdirectories too, and
	// the failures of one file do not stop the others being checked.
	void directory()
	{
		const QString dir = m_dir.filePath(QStringLiteral("collection"));
		QVERIFY(QDir().mkpath(dir + QStringLiteral("/sub")));
		QVERIFY(QFile::copy(fixturesDir() + QStringLiteral("/valid.elmt"),
							dir + QStringLiteral("/valid.elmt")));
		QVERIFY(QFile::copy(fixturesDir() + QStringLiteral("/no_terminals.elmt"),
							dir + QStringLiteral("/sub/no_terminals.elmt")));
		QVERIFY(QFile::copy(fixturesDir() + QStringLiteral("/zero_height.elmt"),
							dir + QStringLiteral("/sub/zero_height.elmt")));
		QFile other(dir + QStringLiteral("/notes.txt"));
		QVERIFY(other.open(QIODevice::WriteOnly));
		other.write("not an element");
		other.close();

		Result r = check({dir});
		QVERIFY2(r.finished, qPrintable(details(r)));
		QVERIFY2(r.out.contains(QStringLiteral("3 file(s), 1 warning(s), 1 failure(s)")),
				 qPrintable(details(r)));
		QVERIFY2(r.exit_code == 1, qPrintable(details(r)));

		// Warnings alone leave the exit code at 0.
		QVERIFY(QFile::remove(dir + QStringLiteral("/sub/zero_height.elmt")));
		r = check({dir});
		QVERIFY2(r.out.contains(QStringLiteral("2 file(s), 1 warning(s), 0 failure(s)")),
				 qPrintable(details(r)));
		QVERIFY2(r.exit_code == 0, qPrintable(details(r)));
	}

	void usageErrors_data()
	{
		QTest::addColumn<QStringList>("args");
		QTest::addColumn<QString>("message");

		QTest::newRow("no path")
				<< QStringList() << "Usage: qelectrotech --check-elements";
		QTest::newRow("no such path")
				<< QStringList{m_dir.filePath(QStringLiteral("absent.elmt"))} << "Not found: ";
		const QString empty = m_dir.filePath(QStringLiteral("empty"));
		QDir().mkpath(empty);
		QTest::newRow("no .elmt files")
				<< QStringList{empty} << "No .elmt files found under: ";
	}

	void usageErrors()
	{
		QFETCH(QStringList, args);
		QFETCH(QString, message);
		const Result r = check(args);
		QVERIFY2(r.finished, qPrintable(details(r)));
		QVERIFY2(r.err.contains(message),
				 qPrintable(QStringLiteral("expected \"%1\" on stderr\n").arg(message)
							+ details(r)));
		QVERIFY2(r.exit_code == 2, qPrintable(details(r)));
	}

	// Takes a few seconds for the 8 800 files (hence the "slow" label).
	void wholeCollection()
	{
		const QString elements = QDir(QStringLiteral(QET_ELEMENTS_DIR)).absolutePath();
		const Result r = check({elements}, 300000);
		QVERIFY2(r.finished, qPrintable(QStringLiteral("--check-elements did not finish\n")
										+ r.err));

		static const QRegularExpression summary(
					QStringLiteral("(\\d+) file\\(s\\), \\d+ warning\\(s\\), (\\d+) failure\\(s\\)"));
		const QRegularExpressionMatch m = summary.match(r.out);
		QVERIFY2(m.hasMatch(), qPrintable(QStringLiteral("no summary line\n") + r.err));
		QVERIFY2(m.captured(1).toInt() > 8000,
				 qPrintable(QStringLiteral("only %1 elements checked").arg(m.captured(1))));

		QStringList failures;
		const QStringList lines = r.out.split(QLatin1Char('\n'));
		for (const QString &line : lines) {
			if (!line.startsWith(QStringLiteral("FAIL  ")))
				continue;
			const QString path = line.mid(6).section(QStringLiteral("  ("), 0, 0);
			failures << QDir(elements).relativeFilePath(path);
		}
		failures.sort();
		QCOMPARE(m.captured(2).toInt(), failures.size());

		const QStringList known = knownFailures();
		QStringList unexpected, gone;
		for (const QString &f : std::as_const(failures))
			if (!known.contains(f))
				unexpected << lines.filter(f).value(0);
		for (const QString &f : known)
			if (!failures.contains(f))
				gone << f;
		const int shown = 20;
		QString message = QStringLiteral("%1 element(s) fail the check:\n  ").arg(unexpected.size())
				+ unexpected.mid(0, shown).join(QStringLiteral("\n  "));
		if (unexpected.size() > shown)
			message += QStringLiteral("\n  ... and %1 more").arg(unexpected.size() - shown);
		QVERIFY2(unexpected.isEmpty(), qPrintable(message));
		QVERIFY2(gone.isEmpty(),
				 qPrintable(QStringLiteral("known failures that now pass or are gone; "
										   "take them out of knownFailures():\n  ")
							+ gone.join(QStringLiteral("\n  "))));
		QVERIFY2(r.exit_code == 1, qPrintable(QStringLiteral("exit code %1").arg(r.exit_code)));
	}
};

QTEST_APPLESS_MAIN(tst_checkelements)

#include "tst_checkelements.moc"
