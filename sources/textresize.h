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
#ifndef TEXTRESIZE_H
#define TEXTRESIZE_H

#include "textanchor.h"

#include <QGraphicsTextItem>
#include <QTextDocument>

/**
	Changing the width of a text box: the text wraps to the new width
	and the text stays where it is drawn.
*/
namespace TextResize
{
	/**
		Give the document of item the width width (-1 = automatic width,
		no wrapping).
		@param alignment : the anchor point of this alignment (see
		TextAnchor) stays in place, in parent coordinates. Without it a
		right-aligned or centred text would grow to the right only.
		@param keep_anchor : false while a text is being loaded, its
		saved position is set afterwards.
		@param centre_pivot : the text is rotated around the centre of its
		box. The centre moves with the width, so the transform origin is
		moved too: undo/redo or a later rotation would otherwise make the
		text jump.
	*/
	inline void applyWidth(QGraphicsTextItem *item,
						   qreal width,
						   Qt::Alignment alignment,
						   bool keep_anchor,
						   bool centre_pivot)
	{
		const QPointF anchor = TextAnchor::pos(item, alignment);

		item->document()->setTextWidth(width);
			//Lay the document out now, so boundingRect() below is the new one
		item->document()->size();

		if (centre_pivot)
			item->setTransformOriginPoint(item->boundingRect().center());
		if (keep_anchor)
			item->setPos(TextAnchor::itemPosFor(item, alignment, anchor));
	}
}

#endif // TEXTRESIZE_H
