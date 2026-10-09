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
#ifndef STATICTEXTWIDTH_H
#define STATICTEXTWIDTH_H

#include <QDomElement>
#include <QString>
#include <QtGlobal>

/**
	The width a static text of a symbol (<text> in a .elmt) wraps to,
	saved as its text_width attribute. -1 is the automatic width: the text
	is not wrapped, as before the attribute existed.
	Shared by the element editor (PartText) and the folio
	(ElementPictureFactory::parseText()), so both wrap the same way.
*/
namespace StaticTextWidth
{
	/**
		The document margin of a static text in the element editor. The
		saved width includes it on both sides; the folio draws the text
		without a margin.
	*/
	constexpr qreal editorMargin = 1.0;

	/**
		@return width, or -1 (automatic) for 0, a negative value, nan or inf
	*/
	inline qreal normalized(qreal width) {
		return (qIsFinite(width) && width > 0) ? width : -1;
	}

	/**
		@return the width saved on element, -1 when it has none
	*/
	inline qreal fromXml(const QDomElement &element) {
		return normalized(element.attribute(QStringLiteral("text_width"),
											QStringLiteral("-1")).toDouble());
	}

	/**
		Save width on element, only when it is not automatic, so a symbol
		without a width is saved as before.
	*/
	inline void toXml(QDomElement &element, qreal width)
	{
		width = normalized(width);
		if (width > 0)
			element.setAttribute(QStringLiteral("text_width"), QString::number(width));
	}

	/**
		@return the width of the lines of a text of width width, without
		the editor's margins: the text width of a document without margin
		that wraps as the editor does.
	*/
	inline qreal lineWidth(qreal width) {
		return qMax(width - 2 * editorMargin, qreal(0));
	}
}

#endif // STATICTEXTWIDTH_H
