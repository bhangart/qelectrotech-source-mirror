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
#include <QtTest>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>

/// Run the qelectrotech binary with arguments, without a display.
static bool runQet(const QStringList &arguments)
{
	QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
	QProcess proc;
	proc.setProcessEnvironment(env);
	proc.start(QStringLiteral(QET_TEST_BINARY_PATH), arguments);
	return proc.waitForFinished(60000)
			&& proc.exitStatus() == QProcess::NormalExit
			&& proc.exitCode() == 0;
}

/// The number of lines the words starting with prefix are drawn on.
static int drawnLines(const QString &svg, const QString &prefix)
{
	const QRegularExpression text_re(
				QStringLiteral("<text[^>]*\\by=\"([^\"]+)\"[^>]*>\\s*%1").arg(prefix));
	QSet<QString> lines;
	auto it = text_re.globalMatch(svg);
	while (it.hasNext())
		lines.insert(it.next().captured(1));
	return lines.size();
}

/**
	A static text of a symbol with a width (text_width on its <text>) is
	drawn wrapped to it on the folio; one without is drawn on one line, as
	before. The width stays in the symbol definition the project carries.
*/
class tst_statictextwidth : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase()
	{
		QVERIFY2(QFile::exists(QStringLiteral(QET_TEST_BINARY_PATH)), "qelectrotech binary not found");
		m_fixture = QFINDTESTDATA("fixtures/static_text_width.qet");
		QVERIFY2(!m_fixture.isEmpty(), "fixture project not found");
		QVERIFY(m_dir.isValid());
	}

	void textWrapsToItsWidth()
	{
		QVERIFY2(runQet({QStringLiteral("--export-svg"), m_fixture, m_dir.path()}), "--export-svg failed");
		QFile file(m_dir.filePath(QStringLiteral("01_diagram.svg")));
		QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
		const QString svg = QString::fromUtf8(file.readAll());

		QVERIFY(drawnLines(svg, QStringLiteral("Stat")) > 1);
		QCOMPARE(drawnLines(svg, QStringLiteral("Sing")), 1);
	}

	void widthStaysInTheProject()
	{
		const QString out = m_dir.filePath(QStringLiteral("resaved.qet"));
		QVERIFY2(runQet({QStringLiteral("--resave"), m_fixture, out}), "--resave failed");
		QFile file(out);
		QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
		const QString saved = QString::fromUtf8(file.readAll());

		static const QRegularExpression wrapped(QStringLiteral("<text [^>]*text=\"StatAlpha[^\"]*\"[^>]*text_width=\"70\""));
		static const QRegularExpression single(QStringLiteral("<text [^>]*text=\"SingAlpha[^>]*>"));
		QVERIFY(wrapped.match(saved).hasMatch());
		const QRegularExpressionMatch match = single.match(saved);
		QVERIFY(match.hasMatch());
		QVERIFY(!match.captured(0).contains(QStringLiteral("text_width")));
	}

private:
	QString m_fixture;
	QTemporaryDir m_dir;
};

QTEST_APPLESS_MAIN(tst_statictextwidth)
#include "tst_statictextwidth.moc"
