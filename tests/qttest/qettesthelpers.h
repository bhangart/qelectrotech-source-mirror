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
#ifndef QETTESTHELPERS_H
#define QETTESTHELPERS_H

#include <QDir>
#include <QProcessEnvironment>
#include <QString>

namespace QET {
namespace Test {

/**
	@brief sandboxEnvironment
	The environment for one run of the qelectrotech binary from a test:
	- Qt's offscreen platform, so no display is needed;
	- HOME, XDG_CONFIG_HOME and XDG_DATA_HOME inside @p home, created here.
	  On Linux that moves the settings (~/.config) and the data folder
	  (~/.local/share), so the run neither reads nor changes the user's.
	  macOS and Windows keep both elsewhere (the preferences and
	  ~/Library; the registry and AppData) and ignore these variables:
	  there the run reads the user's settings. A test that needs settings
	  of its own sets QET_SETTINGS_DIR, which works on every system;
	- TMPDIR set to @p tmp, when given (read on Linux and macOS only);
	- no QET_SETTINGS_DIR inherited from the shell.

	Add what a test needs on top (QET_ENABLE_SCRIPTING, LC_ALL...) to the
	environment returned.
	@param home : a directory for this run alone
	@param tmp : the temporary directory for this run, or empty to keep
	the one inherited
*/
inline QProcessEnvironment sandboxEnvironment(const QString &home,
					      const QString &tmp = QString())
{
	QDir().mkpath(home);
	QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
	env.insert(QStringLiteral("HOME"), home);
	env.insert(QStringLiteral("XDG_CONFIG_HOME"), home + QStringLiteral("/.config"));
	env.insert(QStringLiteral("XDG_DATA_HOME"), home + QStringLiteral("/.local/share"));
	if (!tmp.isEmpty())
		env.insert(QStringLiteral("TMPDIR"), tmp);
	env.remove(QStringLiteral("QET_SETTINGS_DIR"));
	return env;
}

} // namespace Test
} // namespace QET

#endif // QETTESTHELPERS_H
