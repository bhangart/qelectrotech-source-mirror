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
#include "properties/terminalclass.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

// The optional `class` attribute of <terminal>: what a terminal carries.
// TerminalClass on its own, then the real binary's --resave on a project
// whose symbols carry the attribute, one value known and one not.
class tst_terminalclass : public QObject
{
	Q_OBJECT

	QTemporaryDir m_dir;
	int m_run = 0;

	// --resave @p in to a new file, in a sandbox of its own (so a running
	// QElectroTech cannot answer instead); returns the new file's path.
	QString resave(const QString &in)
	{
		const QString out = m_dir.filePath(QStringLiteral("out%1.qet").arg(m_run));
		const QString home = m_dir.filePath(QStringLiteral("home%1").arg(m_run++));
		QDir().mkpath(home);
		QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
		env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
		env.insert(QStringLiteral("HOME"), home);
		env.insert(QStringLiteral("XDG_CONFIG_HOME"), home + QStringLiteral("/config"));
		env.insert(QStringLiteral("XDG_DATA_HOME"), home + QStringLiteral("/data"));
		QProcess proc;
		proc.setProcessEnvironment(env);
		proc.start(QStringLiteral(QET_TEST_BINARY_PATH), {QStringLiteral("--resave"), in, out});
		if (!proc.waitForFinished(120000) || proc.exitCode() != 0) return {};
		return out;
	}

	static QByteArray read(const QString &path)
	{
		QFile f(path);
		return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
	}

private slots:
	void initTestCase()
	{
		QVERIFY(m_dir.isValid());
		QVERIFY(QFile::exists(QStringLiteral(QET_TEST_BINARY_PATH)));
	}

	void knownValuesRoundTrip_data()
	{
		QTest::addColumn<int>("cls");
		QTest::addColumn<QString>("value");
		QTest::newRow("electrical")  << int(TerminalClass::Electrical)  << "electrical";
		QTest::newRow("signal")      << int(TerminalClass::Signal)      << "signal";
		QTest::newRow("data")        << int(TerminalClass::Data)        << "data";
		QTest::newRow("hydraulic")   << int(TerminalClass::Hydraulic)   << "hydraulic";
		QTest::newRow("gas")         << int(TerminalClass::Gas)         << "gas";
		QTest::newRow("refrigerant") << int(TerminalClass::Refrigerant) << "refrigerant";
		QTest::newRow("air")         << int(TerminalClass::Air)         << "air";
	}

	void knownValuesRoundTrip()
	{
		QFETCH(int, cls);
		QFETCH(QString, value);
		const auto c = TerminalClass::Class(cls);
		QCOMPARE(TerminalClass::toAttribute(c), value);
		QCOMPARE(TerminalClass::fromAttribute(value), c);
	}

	// No attribute is every terminal written before it existed; a value
	// from a newer release is Unknown, never mistaken for a known one.
	void absentAndUnknownValues()
	{
		QCOMPARE(TerminalClass::fromAttribute(QString()), TerminalClass::Unspecified);
		QCOMPARE(TerminalClass::fromAttribute(QStringLiteral("steam")), TerminalClass::Unknown);
		QCOMPARE(TerminalClass::fromAttribute(QStringLiteral("Electrical")), TerminalClass::Unknown);
		QCOMPARE(TerminalClass::fromAttribute(QStringLiteral(" electrical")), TerminalClass::Unknown);
		QVERIFY(TerminalClass::toAttribute(TerminalClass::Unspecified).isEmpty());
		QVERIFY(TerminalClass::toAttribute(TerminalClass::Unknown).isEmpty());
	}

	// A project whose symbol terminals carry `class` opens, and saving it
	// keeps both values as written, the unknown one included, and adds the
	// attribute nowhere else. Run against a build without the feature, the
	// same steps show older releases open such a file.
	void projectKeepsClassThroughResave()
	{
		const QString fixture = QFINDTESTDATA("fixtures/unlinked_contact_label.qet");
		QVERIFY(!fixture.isEmpty());
		QByteArray xml = read(fixture);
		const QByteArray generic("type=\"Generic\"");
		const int first = xml.indexOf(generic);
		QVERIFY(first >= 0);
		const int second = xml.indexOf(generic, first + generic.size());
		QVERIFY(second > first);
		xml.insert(second + generic.size(), " class=\"steam\"");
		xml.insert(first + generic.size(), " class=\"hydraulic\"");
		const QString in = m_dir.filePath(QStringLiteral("classes.qet"));
		QFile f(in);
		QVERIFY(f.open(QIODevice::WriteOnly));
		f.write(xml);
		f.close();

		const QString once = resave(in);
		QVERIFY2(!once.isEmpty(), "--resave failed on a project with terminal classes");
		const QString twice = resave(once);
		QVERIFY2(!twice.isEmpty(), "second --resave failed");
		const QByteArray a = read(once), b = read(twice);
		QVERIFY2(a == b, "the second save changed the file");

		const QString saved = QString::fromUtf8(b);
		const QRegularExpression cls(QStringLiteral("<terminal [^>]*\\bclass=\"([^\"]*)\""));
		QStringList found;
		for (auto it = cls.globalMatch(saved); it.hasNext();)
			found << it.next().captured(1);
		found.sort();
		QCOMPARE(found, QStringList({QStringLiteral("hydraulic"), QStringLiteral("steam")}));
	}
};

QTEST_APPLESS_MAIN(tst_terminalclass)

#include "tst_terminalclass.moc"
