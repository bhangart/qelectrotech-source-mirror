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
#include "kautosavefile.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <algorithm>
#include <memory>
#include <vector>

#ifdef Q_OS_UNIX
#include <ctime>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

// The Qt-only KAutoSaveFile used when building without KDE Frameworks:
// after a crash (a child process killed while holding its backups), the
// backups it left are found stale and can be recovered, every generation
// of them. Unix-only, since the crash is simulated with fork() and kill().
class tst_kautosavefile : public QObject
{
	Q_OBJECT

	// Start from no backups at all: a run that failed half way can leave
	// some behind, and the counts below would be off.
	static void clearBackups()
	{
		QDir(QStandardPaths::writableLocation(
			QStandardPaths::AppDataLocation)).removeRecursively();
	}

private slots:
	// Keep the backups out of the user's own data directory: macOS, for
	// one, ignores XDG_DATA_HOME.
	void initTestCase()
	{
		QStandardPaths::setTestModeEnabled(true);
	}
	void recoversStaleFiles()
	{
#ifndef Q_OS_UNIX
		QSKIP("crash-style stale lock test is Unix-only");
#else
		QTemporaryDir data_home;
		QVERIFY(data_home.isValid());

		qputenv("XDG_DATA_HOME", QFile::encodeName(data_home.path()));
		QCoreApplication::setOrganizationName(QStringLiteral("QElectroTech"));
		QCoreApplication::setApplicationName(QStringLiteral("KAutoSaveFileTest"));
		clearBackups();

		const auto managed_path = data_home.filePath(QStringLiteral("project.qet"));
		QFile managed_file(managed_path);
		QVERIFY(managed_file.open(QIODevice::WriteOnly | QIODevice::Text));
		QVERIFY(managed_file.write("<project/>\n") > 0);
		managed_file.close();

		int ready_pipe[2] = {-1, -1};
		QVERIFY(pipe(ready_pipe) == 0);

		const QByteArray payload("<project><diagram /></project>\n");
		const auto child_pid = fork();
		QVERIFY(child_pid >= 0);

		if (child_pid == 0) {
			close(ready_pipe[0]);

			KAutoSaveFile backup(QUrl::fromLocalFile(managed_path));
			if (!backup.open(QIODevice::WriteOnly
						  | QIODevice::Truncate
						  | QIODevice::Text)) {
				_exit(2);
			}
			if (backup.write(payload) != payload.size()) {
				_exit(3);
			}
			if (!backup.flush()) {
				_exit(4);
			}

			const char ready = '1';
			if (write(ready_pipe[1], &ready, 1) != 1) {
				_exit(5);
			}
			close(ready_pipe[1]);

			for (;;) {
				pause();
			}
		}

		close(ready_pipe[1]);
		char ready = 0;
		QVERIFY(read(ready_pipe[0], &ready, 1) == 1);
		close(ready_pipe[0]);
		QCOMPARE(ready, '1');

		auto active_files = KAutoSaveFile::allStaleFiles();
		QVERIFY(active_files.isEmpty());
		for (auto *file : active_files) {
			delete file;
		}

		QVERIFY(kill(child_pid, SIGKILL) == 0);
		int status = 0;
		QVERIFY(waitpid(child_pid, &status, 0) == child_pid);
		QVERIFY(WIFSIGNALED(status));
		QCOMPARE(WTERMSIG(status), SIGKILL);

		auto stale_files = KAutoSaveFile::allStaleFiles();
		QCOMPARE(stale_files.size(), qsizetype(1));

		std::unique_ptr<KAutoSaveFile> stale_file(stale_files.takeFirst());
		QCOMPARE(stale_file->managedFile().path(),
			 QFileInfo(managed_path).absoluteFilePath());
		QVERIFY(stale_file->open(QIODevice::ReadOnly | QIODevice::Text));
		QCOMPARE(stale_file->readAll(), payload);

		const auto autosave_file_name = stale_file->fileName();
		const auto metadata_file_name = autosave_file_name + QStringLiteral(".path");
		const auto lock_file_name = autosave_file_name + QStringLiteral(".lock");
		stale_file.reset();

		QVERIFY(!QFile::exists(autosave_file_name));
		QVERIFY(!QFile::exists(metadata_file_name));
		QVERIFY(!QFile::exists(lock_file_name));
#endif
	}

	void findsEveryGenerationStale()
	{
#ifndef Q_OS_UNIX
		QSKIP("crash-style stale lock test is Unix-only");
#else
			//Simulates QETProject's rotating crash-recovery generations
			//(BackupGenerations snapshots written round-robin): several
			//KAutoSaveFile instances sharing one managed file, alive at once.
		QTemporaryDir data_home;
		QVERIFY(data_home.isValid());

		qputenv("XDG_DATA_HOME", QFile::encodeName(data_home.path()));
		QCoreApplication::setOrganizationName(QStringLiteral("QElectroTech"));
		QCoreApplication::setApplicationName(
			QStringLiteral("KAutoSaveFileGenerationsTest"));
		clearBackups();

		const auto managed_path = data_home.filePath(QStringLiteral("project.qet"));
		QFile managed_file(managed_path);
		QVERIFY(managed_file.open(QIODevice::WriteOnly | QIODevice::Text));
		QVERIFY(managed_file.write("<project/>\n") > 0);
		managed_file.close();

		constexpr int generations = 3;

		int ready_pipe[2] = {-1, -1};
		QVERIFY(pipe(ready_pipe) == 0);

		const auto child_pid = fork();
		QVERIFY(child_pid >= 0);

		if (child_pid == 0) {
			close(ready_pipe[0]);

			std::vector<std::unique_ptr<KAutoSaveFile>> backups;
			for (int i = 0; i < generations; ++i) {
				auto backup = std::make_unique<KAutoSaveFile>(
					QUrl::fromLocalFile(managed_path));
				if (!backup->open(QIODevice::WriteOnly
							  | QIODevice::Truncate
							  | QIODevice::Text)) {
					_exit(2);
				}
				const QByteArray payload =
					"<project><generation n=\"" + QByteArray::number(i)
					+ "\" /></project>\n";
				if (backup->write(payload) != payload.size()) {
					_exit(3);
				}
				if (!backup->flush()) {
					_exit(4);
				}
					//Give each generation a distinct, increasing mtime.
				struct timespec pause{0, 20 * 1000 * 1000};
				nanosleep(&pause, nullptr);
				backups.push_back(std::move(backup));
			}

			const char ready = '1';
			if (write(ready_pipe[1], &ready, 1) != 1) {
				_exit(5);
			}
			close(ready_pipe[1]);

			for (;;) {
				pause();
			}
		}

		close(ready_pipe[1]);
		char ready = 0;
		QVERIFY(read(ready_pipe[0], &ready, 1) == 1);
		close(ready_pipe[0]);
		QCOMPARE(ready, '1');

		QVERIFY(kill(child_pid, SIGKILL) == 0);
		int status = 0;
		QVERIFY(waitpid(child_pid, &status, 0) == child_pid);
		QVERIFY(WIFSIGNALED(status));
		QCOMPARE(WTERMSIG(status), SIGKILL);

		auto stale_files = KAutoSaveFile::allStaleFiles();
		QCOMPARE(stale_files.size(), qsizetype(generations));

		std::sort(stale_files.begin(), stale_files.end(),
			[](KAutoSaveFile *a, KAutoSaveFile *b) {
				return QFileInfo(*a).lastModified() < QFileInfo(*b).lastModified();
			});

		for (int i = 0; i < generations; ++i) {
			std::unique_ptr<KAutoSaveFile> stale_file(stale_files.at(i));
			QCOMPARE(stale_file->managedFile().path(),
				 QFileInfo(managed_path).absoluteFilePath());
			QVERIFY(stale_file->open(QIODevice::ReadOnly | QIODevice::Text));
			const QByteArray content = stale_file->readAll();
			QVERIFY(content.contains(
				"generation n=\"" + QByteArray::number(i) + "\""));
		}
#endif
	}
};

QTEST_GUILESS_MAIN(tst_kautosavefile)

#include "tst_kautosavefile.moc"
