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
#ifndef TERMINALCLASS_H
#define TERMINALCLASS_H

#include <QString>

/**
	@brief The TerminalClass namespace
	What a terminal carries: the optional `class` attribute of <terminal>.

	The attribute is independent of the terminal type (Generic, Inner...):
	a Generic terminal can carry 24 V supply, a bus signal or water.
	It is optional; a terminal without it is Unspecified, which is every
	terminal written before the attribute existed.

	The value is kept in the file as written. A value this build does not
	know (from a newer release, say) reads as Unknown and is written back
	unchanged, so opening and saving a file never loses it.
*/
namespace TerminalClass
{
	enum Class {
		Unspecified, ///< no `class` attribute
		Unknown,     ///< a value this build does not know
		Electrical,
		Signal,
		Data,
		Hydraulic,
		Gas,
		Refrigerant,
		Air
	};

	QString toAttribute(Class c);
	Class fromAttribute(const QString &value);
}

#endif // TERMINALCLASS_H
