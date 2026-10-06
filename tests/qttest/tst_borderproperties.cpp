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
#include "borderproperties.h"

#include <QTest>

// BorderProperties: the folio border a new diagram gets, and comparing two
// sets of border properties.
class tst_borderproperties : public QObject
{
	Q_OBJECT

	static void checkDefaults(const BorderProperties &p)
	{
		QCOMPARE(p.columns_count, 17);
		QCOMPARE(p.columns_width, 60.0);
		QCOMPARE(p.columns_header_height, 20.0);
		QCOMPARE(p.display_columns, true);
		QCOMPARE(p.rows_count, 8);
		QCOMPARE(p.rows_height, 80.0);
		QCOMPARE(p.rows_header_width, 20.0);
		QCOMPARE(p.display_rows, true);
	}

private slots:
	void constructedWithDefaults()
	{
		checkDefaults(BorderProperties());
	}

	void defaultPropertiesEqualConstructed()
	{
		// operator== is not const
		BorderProperties constructed;
		BorderProperties defaults = BorderProperties::defaultProperties();
		QVERIFY(defaults == constructed);
		checkDefaults(defaults);
	}

	void equalityComparesFields()
	{
		BorderProperties a, b;
		QVERIFY(a == b);
		QVERIFY(!(a != b));
		b.rows_count = a.rows_count + 1;
		QVERIFY(!(a == b));
		QVERIFY(a != b);
	}
};

QTEST_APPLESS_MAIN(tst_borderproperties)

#include "tst_borderproperties.moc"
