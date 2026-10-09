// SPDX-License-Identifier: GPL-2.0-or-later
#include <QtTest>

#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QUndoCommand>

#include "qetmessagebox.h"
#include "qetproject.h"
#include "qetresult.h"

// The autosave timer calls QETProject::autosave(). It writes the project
// only when something changed since the last write(): rewriting an
// unchanged project froze big projects for seconds every interval
// (discussion #800).
//
// To see whether autosave() wrote, each case puts a sentinel in the
// project file and checks whether it is still there afterwards. The test
// links the application's own objects, since QETProject needs most of
// them.
class tst_projectautosave : public QObject
{
	Q_OBJECT

	QTemporaryDir m_dir;

	// An undo step that changes nothing, enough to move the undo stack.
	class NoOpCommand : public QUndoCommand
	{
		public:
			void undo() override {}
			void redo() override {}
	};

	static const QByteArray &sentinel()
	{
		static const QByteArray s("not written by autosave");
		return s;
	}

	static void putSentinel(const QString &path)
	{
		QFile file(path);
		QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
		QCOMPARE(file.write(sentinel()), qint64(sentinel().size()));
	}

	// true if the file holds a project instead of the sentinel
	static bool wasWritten(const QString &path)
	{
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly))
			return false;
		const QByteArray content = file.readAll();
		return content != sentinel()
			&& content.contains("<project");
	}

	// A copy of a small fixture in the temporary folder, opened.
	QString copyFixture(const QString &name)
	{
		const QString source = QFINDTESTDATA("fixtures/free_text_width.qet");
		const QString path = m_dir.filePath(name);
		QFile::remove(path);
		if (!QFile::copy(source, path))
			return QString();
		QFile::setPermissions(path, QFile::permissions(path)
							  | QFileDevice::WriteOwner);
		return path;
	}

	private slots:
		void initTestCase()
		{
			QVERIFY(m_dir.isValid());
			QSettings::setDefaultFormat(QSettings::IniFormat);
			QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
							   m_dir.filePath(QStringLiteral("settings")));
			QCoreApplication::setOrganizationName(
				QStringLiteral("QETAutosaveTest"));
			QETProject::setBackupEnabled(false);
			QET::QetMessageBox::setNonInteractive(true);
		}

		// A project just opened is what its file holds: nothing to write.
		void unchangedProjectIsNotWritten()
		{
			const QString path = copyFixture(QStringLiteral("open.qet"));
			QVERIFY(!path.isEmpty());
			QETProject project(path);
			QCOMPARE(project.state(), QETProject::Ok);

			putSentinel(path);
			QVERIFY(!project.autosave());
			QVERIFY(!project.autosave());
			QVERIFY(!wasWritten(path));
		}

		// A project with no file yet is not written; once it has one, it is.
		void projectWithoutFileIsNotWritten()
		{
			QETProject project;
			QVERIFY(project.filePath().isEmpty());
			QVERIFY(!project.autosave());

			const QString path = m_dir.filePath(QStringLiteral("new.qet"));
			QFile::remove(path);
			project.setFilePath(path);
			QVERIFY(project.autosave());
			QVERIFY(wasWritten(path));

			putSentinel(path);
			QVERIFY(!project.autosave());
			QVERIFY(!wasWritten(path));
		}

		// A project option (setModified()) changed: written once, then not
		// again until the next change.
		void modifiedOptionsAreWrittenOnce()
		{
			const QString path = copyFixture(QStringLiteral("options.qet"));
			QVERIFY(!path.isEmpty());
			QETProject project(path);
			QCOMPARE(project.state(), QETProject::Ok);

			project.setModified(true);
			putSentinel(path);
			QVERIFY(project.autosave());
			QVERIFY(wasWritten(path));
			QVERIFY(!project.projectOptionsWereModified());

			putSentinel(path);
			QVERIFY(!project.autosave());
			QVERIFY(!wasWritten(path));

			project.setModified(true);
			QVERIFY(project.autosave());
			QVERIFY(wasWritten(path));
		}

		// Every move of the undo stack is a change, also an undo back to
		// the state the file was opened in: the file may hold the undone
		// step by then. Undo and redo, several times, ending where it
		// started.
		void undoStackMovesAreWritten()
		{
			const QString path = copyFixture(QStringLiteral("undo.qet"));
			QVERIFY(!path.isEmpty());
			QETProject project(path);
			QCOMPARE(project.state(), QETProject::Ok);
			QUndoStack *stack = project.undoStack();

			stack->push(new NoOpCommand);
			putSentinel(path);
			QVERIFY(project.autosave());
			QVERIFY(wasWritten(path));

			for (int i = 0; i < 2; ++i) {
				stack->undo();
				QVERIFY(stack->isClean());
				putSentinel(path);
				QVERIFY(project.autosave());
				QVERIFY(wasWritten(path));

				putSentinel(path);
				QVERIFY(!project.autosave());
				QVERIFY(!wasWritten(path));

				stack->redo();
				putSentinel(path);
				QVERIFY(project.autosave());
				QVERIFY(wasWritten(path));
			}

			stack->undo();
			QCOMPARE(stack->index(), 0);
			QVERIFY(project.autosave());
			putSentinel(path);
			QVERIFY(!project.autosave());
			QVERIFY(!wasWritten(path));
		}

		// A save (write(), as Ctrl+S does) leaves nothing for autosave. A
		// save still writes an unchanged project: only autosave skips it.
		void nothingToWriteAfterSave()
		{
			const QString path = copyFixture(QStringLiteral("saved.qet"));
			QVERIFY(!path.isEmpty());
			QETProject project(path);
			QCOMPARE(project.state(), QETProject::Ok);

			putSentinel(path);
			QVERIFY(project.write().isOk());
			QVERIFY(wasWritten(path));

			project.undoStack()->push(new NoOpCommand);
			QVERIFY(project.write().isOk());
			project.undoStack()->setClean();

			putSentinel(path);
			QVERIFY(!project.autosave());
			QVERIFY(!wasWritten(path));
		}

		// A write that failed leaves the change to write: the next autosave
		// that can write does.
		void failedWriteIsRetried()
		{
			const QString path = copyFixture(QStringLiteral("retry.qet"));
			QVERIFY(!path.isEmpty());
			QETProject project(path);
			QCOMPARE(project.state(), QETProject::Ok);

			project.undoStack()->push(new NoOpCommand);
			const QString unreachable = m_dir.filePath(
				QStringLiteral("no-such-folder/retry.qet"));
			project.setFilePath(unreachable);
			QVERIFY(!project.autosave());
			QVERIFY(!QFile::exists(unreachable));

			project.setFilePath(path);
			putSentinel(path);
			QVERIFY(project.autosave());
			QVERIFY(wasWritten(path));
		}
};

QTEST_MAIN(tst_projectautosave)
#include "tst_projectautosave.moc"
