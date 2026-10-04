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
#include "terminalclass.h"

/**
	@brief TerminalClass::toAttribute
	@param c
	@return the attribute value for @p c; empty for Unspecified and Unknown,
	which have no value of their own
*/
QString TerminalClass::toAttribute(Class c)
{
	switch (c) {
		case Electrical:  return QStringLiteral("electrical");
		case Signal:      return QStringLiteral("signal");
		case Data:        return QStringLiteral("data");
		case Hydraulic:   return QStringLiteral("hydraulic");
		case Gas:         return QStringLiteral("gas");
		case Refrigerant: return QStringLiteral("refrigerant");
		case Air:         return QStringLiteral("air");
		case Unspecified:
		case Unknown:     break;
	}
	return QString();
}

/**
	@brief TerminalClass::fromAttribute
	@param value : the `class` attribute as written in the file
	@return Unspecified for an empty value, Unknown for a value this build
	does not know. Matching is exact: values are lower case.
*/
TerminalClass::Class TerminalClass::fromAttribute(const QString &value)
{
	if (value.isEmpty())
		return Unspecified;
	for (Class c : {Electrical, Signal, Data, Hydraulic, Gas, Refrigerant, Air})
		if (value == toAttribute(c))
			return c;
	return Unknown;
}
