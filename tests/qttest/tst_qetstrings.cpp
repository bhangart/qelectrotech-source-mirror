#include <QtTest>

#include "qet.h"
#include "qetapp.h"

QString QETApp::m_interface_language;

/**
	QET::joinWithSpaces() / QET::splitWithSpaces() are the wire format for the
	SingleApplication message a secondary instance sends to the running one
	(main.cpp: "launched-with-args: " + joinWithSpaces(...), received by
	QETApp::receiveMessage()). If the round trip loses arguments, opening a
	file while QET is already running silently does nothing.

	splitWithSpaces() used to split on QRegularExpression("[^\\]?(?:\\\\)* "),
	which is not a valid pattern: "[^\\]" opens a character class whose "\\]"
	is an escaped bracket, so the class never closes. QRegularExpression
	reported isValid() == false and QString::split() returned an empty list for
	every input -- bugtracker #248.
*/
class tst_qetstrings : public QObject
{
	Q_OBJECT

private slots:
	void roundTrips_data()
	{
		QTest::addColumn<QStringList>("input");

		QTest::newRow("single plain")        << QStringList{"one.qet"};
		QTest::newRow("two plain")           << QStringList{"one.qet", "two.qet"};
		QTest::newRow("space in name")       << QStringList{"my file.qet"};
		QTest::newRow("space then plain")    << QStringList{"my file.qet", "other.qet"};
		QTest::newRow("spaces in path")      << QStringList{"/home/a b/c d.qet", "/tmp/x.qet"};
		QTest::newRow("backslash in name")   << QStringList{"back\\slash.qet"};
		QTest::newRow("trailing backslash")  << QStringList{"trailing\\"};
		QTest::newRow("mixed")               << QStringList{"a b", "c\\d", "e"};
	}

	/// What the IPC actually needs: whatever went in comes back out.
	void roundTrips()
	{
		QFETCH(QStringList, input);
		QCOMPARE(QET::splitWithSpaces(QET::joinWithSpaces(input)), input);
	}

	/// The specific regression: the old implementation returned an empty list
	/// for every input, so this passed nothing on to openFiles().
	void splitIsNotEmptyForPlainArguments()
	{
		QVERIFY(!QET::splitWithSpaces(QStringLiteral("one.qet")).isEmpty());
		QCOMPARE(QET::splitWithSpaces(QStringLiteral("a.qet b.qet")).count(), 2);
	}

	/// QET::stringToFileName(): lower case, characters a file name cannot
	/// hold become dashes, spaces become underscores.
	void stringToFileName_data()
	{
		QTest::addColumn<QString>("name");
		QTest::addColumn<QString>("expected");

		QTest::newRow("plain")      << "test"    << "test";
		QTest::newRow("underscore") << "t_est"   << "t_est";
		QTest::newRow("dash")       << "t-est"   << "t-est";
		QTest::newRow("dot")        << "te.st"   << "te.st";
		QTest::newRow("upper case") << "tesMt"   << "tesmt";
		QTest::newRow("forbidden")  << "te<st"   << "te-st";
		QTest::newRow("accent")     << QStringLiteral("t\u00e9<st") << QStringLiteral("t\u00e9-st");
		QTest::newRow("space")      << "te st"   << "te_st";
	}

	void stringToFileName()
	{
		QFETCH(QString, name);
		QFETCH(QString, expected);
		QCOMPARE(QET::stringToFileName(name), expected);
	}
};

QTEST_APPLESS_MAIN(tst_qetstrings)
#include "tst_qetstrings.moc"
