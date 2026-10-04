// SPDX-License-Identifier: GPL-2.0-or-later
#include "qettesthelpers.h"

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QPageSize>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTemporaryDir>

// --export-pdf sizes each page to its folio, and QPageSize rounds a size
// within 3 pt of a standard sheet to the sheet. It knows the sheets upright
// only, so a wide folio used to miss: the fixture's A3 landscape folio
// (frame 1190.25 x 840.75 pt) came out 1190 x 841 pt while its A3 portrait
// folio came out 842 x 1191, the sheet. Both must land on A3.
class tst_pdfpagesize : public QObject
{
	Q_OBJECT

	QTemporaryDir m_dir;

	// The MediaBox of each page, in page order. Qt writes one uncompressed
	// "/MediaBox [0 0 w h]" per page object.
	QList<QSizeF> exportPages(const QString &project)
	{
		const QString home = m_dir.filePath(QStringLiteral("home"));
		const QString tmp = m_dir.filePath(QStringLiteral("tmp"));
		const QString out = m_dir.filePath(QStringLiteral("out.pdf"));
		QDir().mkpath(home);
		QDir().mkpath(tmp);
		QProcessEnvironment env = QET::Test::sandboxEnvironment(home, tmp);

		QProcess proc;
		proc.setProcessEnvironment(env);
		proc.start(QStringLiteral(QET_TEST_BINARY_PATH),
				   {QStringLiteral("--export-pdf"), project, out});
		if (!proc.waitForFinished(60000) || proc.exitCode() != 0)
			return {};

		QFile file(out);
		if (!file.open(QIODevice::ReadOnly))
			return {};
		const QString pdf = QString::fromLatin1(file.readAll());
		static const QRegularExpression box(
			QStringLiteral(R"(/MediaBox \[0 0 ([0-9.]+) ([0-9.]+)\])"));
		QList<QSizeF> pages;
		auto it = box.globalMatch(pdf);
		while (it.hasNext()) {
			const auto m = it.next();
			pages << QSizeF(m.captured(1).toDouble(), m.captured(2).toDouble());
		}
		return pages;
	}

private slots:
	void initTestCase()
	{
		QVERIFY(m_dir.isValid());
		QVERIFY(QFile::exists(QStringLiteral(QET_TEST_BINARY_PATH)));
	}

	void a3BothWays()
	{
		const QString fixture = QFINDTESTDATA("fixtures/pdf_page_a3.qet");
		QVERIFY2(!fixture.isEmpty(), "fixture project not found");

		const QList<QSizeF> pages = exportPages(fixture);
		QCOMPARE(pages.size(), 2);

		// Qt writes the sheet's whole points: 842 x 1191 for A3.
		const QSize a3 = QPageSize(QPageSize::A3).sizePoints();
		QCOMPARE(pages.at(0), QSizeF(a3.height(), a3.width())); // landscape
		QCOMPARE(pages.at(1), QSizeF(a3));                      // portrait
	}
};

QTEST_MAIN(tst_pdfpagesize)
#include "tst_pdfpagesize.moc"
