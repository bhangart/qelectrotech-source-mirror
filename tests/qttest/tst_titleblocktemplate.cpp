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
#include "titleblocktemplate.h"
#include "diagram.h"
#include "dxfexport.h"
#include "qetapp.h"

#include <QDir>
#include <QDomDocument>
#include <QLoggingCategory>
#include <QPaintEngine>
#include <QPainter>
#include <QTemporaryDir>
#include <QTest>

/**
	titleblocktemplate.cpp, nameslist.cpp and qet.cpp need these few
	symbols from the application; standing in for them here is what lets a
	title block template be loaded, saved and rendered without linking (or
	starting) the whole of QElectroTech. The DXF export (renderDxf(), via
	createdxf.cpp) is not run here.
*/
QString QETApp::m_interface_language;
QColor Diagram::background_color = Qt::white;

QETApp *QETApp::instance()
{
	return nullptr;
}

QString QETApp::langFromSetting()
{
	return QStringLiteral("en");
}

QFont QETApp::diagramTextsFont(qreal)
{
	return QFont();
}

QETDiagramEditor *QETApp::diagramEditorAncestorOf(const QWidget *)
{
	return nullptr;
}

QPointF DxfExport::rotation_transformed(qreal, qreal, qreal, qreal, qreal)
{
	qFatal("%s: not reached by this test", Q_FUNC_INFO);
}

namespace {
	/// A paint device which keeps the texts drawn on it, in order.
	class TextRecorder : public QPaintDevice
	{
		class Engine : public QPaintEngine
		{
			public:
				Engine() : QPaintEngine(QPaintEngine::AllFeatures) {}
				bool begin(QPaintDevice *) override { return true; }
				bool end() override { return true; }
				void updateState(const QPaintEngineState &) override {}
				void drawPixmap(const QRectF &, const QPixmap &, const QRectF &) override {}
				void drawImage(const QRectF &, const QImage &, const QRectF &,
					       Qt::ImageConversionFlags) override {}
				void drawRects(const QRectF *, int) override {}
				void drawRects(const QRect *, int) override {}
				void drawLines(const QLineF *, int) override {}
				void drawLines(const QLine *, int) override {}
				void drawPath(const QPainterPath &) override {}
				void drawPolygon(const QPointF *, int, PolygonDrawMode) override {}
				void drawPolygon(const QPoint *, int, PolygonDrawMode) override {}
				void drawTextItem(const QPointF &, const QTextItem &item) override
				{
					texts << item.text();
				}
				Type type() const override { return QPaintEngine::User; }

				QStringList texts;
		};

		public:
			QPaintEngine *paintEngine() const override { return &m_engine; }
			QStringList texts() const { return m_engine.texts; }

		protected:
			int metric(PaintDeviceMetric metric) const override
			{
				switch (metric) {
					case PdmWidth: case PdmHeight: return 4000;
					case PdmWidthMM: case PdmHeightMM: return 1000;
					case PdmDpiX: case PdmDpiY:
					case PdmPhysicalDpiX: case PdmPhysicalDpiY: return 96;
					case PdmDepth: return 32;
					case PdmNumColors: return INT_MAX;
					case PdmDevicePixelRatio: return 1;
					case PdmDevicePixelRatioScaled: return int(devicePixelRatioFScale());
					default: return 0;
				}
			}

		private:
			mutable Engine m_engine;
	};

	/// The texts @p tbt draws at @p width for @p context
	QStringList renderedTexts(const TitleBlockTemplate &tbt,
				  const DiagramContext &context,
				  int width = 800)
	{
		TextRecorder recorder;
		QPainter painter(&recorder);
		tbt.render(painter, context, width);
		painter.end();
		return recorder.texts();
	}

	QStringList trimmed(const QStringList &texts)
	{
		QStringList list;
		for (const QString &text : texts)
			list << text.trimmed();
		return list;
	}

	/// A template named "test" with @p rows, @p cols and @p cells inside <grid>
	QString templateXml(const QString &rows, const QString &cols, const QString &cells = QString())
	{
		return QStringLiteral("<titleblocktemplate name=\"test\"><information>info</information>"
				      "<logos/><grid rows=\"%1\" cols=\"%2\">%3</grid></titleblocktemplate>")
				.arg(rows, cols, cells);
	}

	bool load(TitleBlockTemplate &tbt, const QString &xml)
	{
		QDomDocument doc;
		if (!doc.setContent(xml))
			return false;
		return tbt.loadFromXmlElement(doc.documentElement());
	}

	/// What @p tbt writes for its grid: dimensions and every cell
	QString savedGrid(const TitleBlockTemplate &tbt)
	{
		QDomDocument doc;
		QDomElement root = doc.createElement(QStringLiteral("root"));
		doc.appendChild(root);
		if (!tbt.saveToXmlElement(root))
			return QStringLiteral("<not saved>");
		QString grid;
		QTextStream stream(&grid);
		root.firstChildElement(QStringLiteral("grid")).save(stream, 0);
		return grid;
	}

	/// Write @p from to a file with saveToXmlFile() and read it into @p to,
	/// the way a template is stored
	bool reloadThroughFile(TitleBlockTemplate &from, TitleBlockTemplate &to)
	{
		QTemporaryDir dir;
		const QString path = dir.filePath(QStringLiteral("saved.titleblock"));
		return dir.isValid() && from.saveToXmlFile(path) && to.loadFromXmlFile(path);
	}

	/// Every cell of @p tbt as it was read, one line each
	QStringList cells(const TitleBlockTemplate &tbt)
	{
		QStringList list;
		for (int row = 0 ; row < tbt.rowsCount() ; ++row) {
			for (int col = 0 ; col < tbt.columnsCount() ; ++col) {
				const TitleBlockCell *c = tbt.cell(row, col);
				list << QStringLiteral("%1,%2 type=%3 span=%4x%5 spanned=%6 name=%7 value=%8 "
						       "label=%9 shown=%10 align=%11 size=%12 hadjust=%13 logo=%14")
					.arg(row).arg(col).arg(int(c->type()))
					.arg(c->row_span).arg(c->col_span).arg(c->spanner_cell != nullptr)
					.arg(c->value_name, c->value.name().trimmed(), c->label.name().trimmed())
					.arg(c->display_label).arg(c->alignment).arg(c->font_size)
					.arg(c->hadjust).arg(c->logo_reference);
			}
		}
		return list;
	}

	QStringList columns(TitleBlockTemplate &tbt)
	{
		QStringList list;
		for (int i = 0 ; i < tbt.columnsCount() ; ++i)
			list << tbt.columnDimension(i).toShortString();
		return list;
	}

	QStringList shippedTemplateFiles()
	{
		return QDir(QStringLiteral(QET_TITLEBLOCKS_DIR))
				.entryList({QStringLiteral("*.titleblock")}, QDir::Files, QDir::Name);
	}
}

// TitleBlockTemplate: the title block templates shipped in titleblocks/
// load, are written back by saveToXmlFile() the way they were read, and
// render every variable they use; the rows and cols descriptions of a grid
// are read as documented (absolute heights; absolute, t% and r% widths);
// the text a cell draws is its value, with its label if shown, variables
// substituted and unset ones left blank; and a file which is not a
// template is refused. A template is part of every project and every
// exported folio, so a slip here shows on every drawing.
class tst_titleblocktemplate : public QObject
{
	Q_OBJECT

private slots:
	// Some logos of the shipped templates use SVG features Qt SVG does
	// not render; what it says about them is not what is tested here.
	void initTestCase()
	{
		QLoggingCategory::setFilterRules(QStringLiteral("qt.svg*.warning=false"));
	}

	void shippedTemplates_data()
	{
		QTest::addColumn<QString>("file");
		const QStringList files = shippedTemplateFiles();
		QVERIFY2(files.size() >= 5, qPrintable(QStringLiteral(QET_TITLEBLOCKS_DIR)));
		for (const QString &file : files)
			QTest::newRow(qPrintable(file)) << file;
	}

	// Each shipped template loads, has a grid, and reads back from the file
	// saveToXmlFile() writes as the same template, drawn the same.
	void shippedTemplates()
	{
		QFETCH(QString, file);

		TitleBlockTemplate original;
		QVERIFY(original.loadFromXmlFile(QDir(QStringLiteral(QET_TITLEBLOCKS_DIR)).filePath(file)));
		QVERIFY(!original.name().isEmpty());
		QVERIFY(original.rowsCount() > 0);
		QVERIFY(original.columnsCount() > 0);

		TitleBlockTemplate reloaded;
		QVERIFY(reloadThroughFile(original, reloaded));
		QCOMPARE(reloaded.name(), original.name());
		QCOMPARE(reloaded.information(), original.information());
		QCOMPARE(reloaded.rowsHeights(), original.rowsHeights());
		QCOMPARE(columns(reloaded), columns(original));
		QCOMPARE(cells(reloaded), cells(original));
		QCOMPARE(savedGrid(reloaded), savedGrid(original));
		QCOMPARE(reloaded.listOfVariables(), original.listOfVariables());

		DiagramContext context;
		for (const QString &variable : original.listOfVariables())
			context.addValue(variable, variable.toUpper());
		// NamesList::toXml() writes a value or a label trimmed, so the
		// spaces "page_de garde" pads one label with are not written back:
		// the texts are compared trimmed.
		QCOMPARE(trimmed(renderedTexts(reloaded, context)),
			 trimmed(renderedTexts(original, context)));

		QStringList logos = original.logos();
		QStringList reloaded_logos = reloaded.logos();
		logos.sort();
		reloaded_logos.sort();
		QCOMPARE(reloaded_logos, logos);
		for (const QString &logo : logos)
			QCOMPARE(reloaded.logoType(logo), original.logoType(logo));
	}

	void shippedTemplatesRender_data() { shippedTemplates_data(); }

	// Every variable a shipped template uses is substituted when it is
	// drawn: given a value for each, the drawn texts show them all.
	void shippedTemplatesRender()
	{
		QFETCH(QString, file);

		TitleBlockTemplate tbt;
		QVERIFY(tbt.loadFromXmlFile(QDir(QStringLiteral(QET_TITLEBLOCKS_DIR)).filePath(file)));

		DiagramContext context;
		const QStringList variables = tbt.listOfVariables();
		for (const QString &variable : variables)
			context.addValue(variable, QStringLiteral("[%1]").arg(variable));

		const QString drawn = renderedTexts(tbt, context).join(QLatin1Char('\n'));
		for (const QString &variable : variables)
			QVERIFY2(drawn.contains(QStringLiteral("[%1]").arg(variable)), qPrintable(variable));
		QVERIFY2(!drawn.contains(QLatin1Char('%')), qPrintable(drawn));
	}

	void parseRows_data()
	{
		QTest::addColumn<QString>("rows");
		QTest::addColumn<QList<int>>("heights");

		QTest::newRow("plain")          << "25;25;50"        << QList<int>{25, 25, 50};
		QTest::newRow("px, any case")   << "25px;10PX"       << QList<int>{25, 10};
		QTest::newRow("no trailing ;")  << "30"              << QList<int>{30};
		QTest::newRow("empty entries")  << ";25;;50;"        << QList<int>{25, 50};
		// what is not a whole, positive number of pixels is left out
		QTest::newRow("invalid entries") << "25;abc;-5;10.5;t20%;50" << QList<int>{25, 50};
		QTest::newRow("spaces")          << "25; 50"         << QList<int>{25};
	}

	// The rows attribute of a grid: absolute heights.
	void parseRows()
	{
		QFETCH(QString, rows);
		QFETCH(QList<int>, heights);

		TitleBlockTemplate tbt;
		QVERIFY(load(tbt, templateXml(rows, QStringLiteral("100"))));
		QCOMPARE(tbt.rowsHeights(), heights);
		QCOMPARE(tbt.rowsCount(), heights.size());
	}

	void parseColumns_data()
	{
		QTest::addColumn<QString>("cols");
		QTest::addColumn<QStringList>("widths");

		QTest::newRow("absolute")  << "100;50px;20PX" << QStringList{"100px;", "50px;", "20px;"};
		QTest::newRow("relative to total") << "t22%;t100%" << QStringList{"t22%;", "t100%;"};
		QTest::newRow("relative to remaining") << "r100%;R30%" << QStringList{"r100%;", "r30%;"};
		QTest::newRow("mixed") << "t22%;r100%;80;t22%;" << QStringList{"t22%;", "r100%;", "80px;", "t22%;"};
		// a percentage needs t or r, and r or t a percentage
		QTest::newRow("invalid entries") << "20%;r20;x10%;t-5%;;abc;40" << QStringList{"40px;"};
	}

	// The cols attribute of a grid: absolute widths, and widths relative
	// to the whole width (t) or to what the other columns leave (r).
	void parseColumns()
	{
		QFETCH(QString, cols);
		QFETCH(QStringList, widths);

		TitleBlockTemplate tbt;
		QVERIFY(load(tbt, templateXml(QStringLiteral("10"), cols)));
		QCOMPARE(columns(tbt), widths);
	}

	void columnsWidth_data()
	{
		QTest::addColumn<QString>("cols");
		QTest::addColumn<int>("total");
		QTest::addColumn<QList<int>>("widths");

		QTest::newRow("mixed") << "100;t20%;r50%;r50%" << 500 << QList<int>{100, 100, 150, 150};
		QTest::newRow("absolute only") << "100;50" << 500 << QList<int>{100, 50};
		// 3 x 33 = 99: the pixel lost to rounding goes to the first relative column
		QTest::newRow("rounding compensated") << "r33%;r33%;r33%" << 100 << QList<int>{34, 33, 33};
		// a difference larger than half a pixel per relative column is left as is
		QTest::newRow("shortfall kept") << "r25%;r25%" << 100 << QList<int>{25, 25};
		QTest::newRow("negative width") << "100;r100%" << -1 << QList<int>{};
	}

	// The columns' widths for a title block of a given width.
	void columnsWidth()
	{
		QFETCH(QString, cols);
		QFETCH(int, total);
		QFETCH(QList<int>, widths);

		TitleBlockTemplate tbt;
		QVERIFY(load(tbt, templateXml(QStringLiteral("10"), cols)));
		QCOMPARE(tbt.columnsWidth(total), widths);
		int sum = 0;
		for (int w : widths)
			sum += w;
		QCOMPARE(tbt.width(total), sum);
	}

	void widthLimits_data()
	{
		using Case = TitleBlockTemplate::WidthConstraintCase;
		QTest::addColumn<QString>("cols");
		QTest::addColumn<int>("minimum");
		QTest::addColumn<int>("maximum");

		QTest::newRow("absolute only") << "100;50" << 150 << 150;
		// 100 px must fit in the half the t50% column leaves
		QTest::newRow("absolute and t%") << "100;t50%" << 200 << int(Case::Unconstrained);
		QTest::newRow("r% only") << "r100%" << 0 << int(Case::Unconstrained);
		QTest::newRow("t% filling the width") << "t50%;t50%" << int(Case::Unconstrained) << int(Case::Unconstrained);
		QTest::newRow("t% over 100") << "t60%;t50%" << int(Case::RelativeWidthExceeds100Percent)
						 << int(Case::RelativeWidthExceeds100Percent);
		QTest::newRow("t% leaving no room") << "t100%;10" << int(Case::AbsoluteColumnsExceedRemainingWidth)
						    << int(Case::AbsoluteColumnsExceedRemainingWidth);
	}

	// minimumWidth() and maximumWidth(), and the cases where no width fits.
	void widthLimits()
	{
		QFETCH(QString, cols);
		QFETCH(int, minimum);
		QFETCH(int, maximum);

		TitleBlockTemplate tbt;
		QVERIFY(load(tbt, templateXml(QStringLiteral("10"), cols)));
		QCOMPARE(tbt.minimumWidth(), minimum);
		QCOMPARE(tbt.maximumWidth(), maximum);
	}

	void cellText_data()
	{
		QTest::addColumn<QString>("field");
		QTest::addColumn<QString>("text");

		const QString label = QStringLiteral("<label><translation lang=\"en\">%1</translation></label>");
		const QString value = QStringLiteral("<value><translation lang=\"en\">%1</translation></value>");

		QTest::newRow("variable") << value.arg("%title") << " Motor control";
		QTest::newRow("braced variable") << value.arg("%{author}") << " N.V.";
		QTest::newRow("label") << label.arg("Author") + value.arg("%author") << " Author : N.V.";
		QTest::newRow("variable in label") << label.arg("%author") + value.arg("%title")
						   << " N.V. : Motor control";
		// the longest key first: %folio-total is not %folio followed by "-total"
		QTest::newRow("longest key first") << value.arg("%folio/%folio-total") << " 3/12";
		QTest::newRow("braced, both") << value.arg("%{folio}/%{folio-total}") << " 3/12";
		// a variable nobody has set is drawn blank, not as its name (#1113, #973)
		QTest::newRow("unset variable") << value.arg("Client: %client.") << " Client: .";
		QTest::newRow("unset braced variable") << value.arg("[%{client}]") << " []";
		// %key is replaced wherever it appears, so a bare name a key starts
		// is not unset
		QTest::newRow("key as a prefix") << value.arg("%folios") << " 3s";
		// a value is not itself searched for variables to blank
		QTest::newRow("value with a %") << value.arg("%note") << " 50%client";
		QTest::newRow("text only") << value.arg("Sheet") << " Sheet";
	}

	// The text a cell draws: " value", or " label : value" when it has a
	// label to show, with the folio's variables substituted.
	void cellText()
	{
		QFETCH(QString, field);
		QFETCH(QString, text);

		TitleBlockTemplate tbt;
		QVERIFY(load(tbt, templateXml(QStringLiteral("20"), QStringLiteral("t100%"),
			QStringLiteral("<field row=\"0\" col=\"0\" name=\"f\" displaylabel=\"true\">%1</field>")
				.arg(field))));
		QCOMPARE(tbt.cell(0, 0)->type(), TitleBlockCell::TextCell);

		DiagramContext context;
		context.addValue(QStringLiteral("title"), QStringLiteral("Motor control"));
		context.addValue(QStringLiteral("author"), QStringLiteral("N.V."));
		context.addValue(QStringLiteral("folio"), QStringLiteral("3"));
		context.addValue(QStringLiteral("folio-total"), QStringLiteral("12"));
		context.addValue(QStringLiteral("note"), QStringLiteral("50%client"));
		QCOMPARE(renderedTexts(tbt, context), QStringList{text});
	}

	// A label is drawn only when the cell says so.
	void hiddenLabel()
	{
		TitleBlockTemplate tbt;
		QVERIFY(load(tbt, templateXml(QStringLiteral("20"), QStringLiteral("t100%"),
			QStringLiteral("<field row=\"0\" col=\"0\" displaylabel=\"FALSE\">"
				       "<label><translation lang=\"en\">Author</translation></label>"
				       "<value><translation lang=\"en\">%author</translation></value>"
				       "</field>"))));
		DiagramContext context;
		context.addValue(QStringLiteral("author"), QStringLiteral("N.V."));
		QCOMPARE(renderedTexts(tbt, context), QStringList{" N.V."});
	}

	void malformedRejected_data()
	{
		QTest::addColumn<QByteArray>("content");

		QTest::newRow("not XML") << QByteArray("<titleblocktemplate name=\"x\"><grid");
		QTest::newRow("another root") << QByteArray("<template name=\"x\"><grid rows=\"10\" cols=\"10\"/></template>");
		QTest::newRow("no name") << QByteArray("<titleblocktemplate><grid rows=\"10\" cols=\"10\"/></titleblocktemplate>");
		QTest::newRow("empty file") << QByteArray();
	}

	// A file which is not a title block template is refused.
	void malformedRejected()
	{
		QFETCH(QByteArray, content);

		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QString path = dir.filePath(QStringLiteral("bad.titleblock"));
		QFile file(path);
		QVERIFY(file.open(QIODevice::WriteOnly));
		file.write(content);
		file.close();

		TitleBlockTemplate tbt;
		QVERIFY(!tbt.loadFromXmlFile(path));
	}

	void missingFileRejected()
	{
		TitleBlockTemplate tbt;
		QVERIFY(!tbt.loadFromXmlFile(QStringLiteral(QET_TITLEBLOCKS_DIR "/does-not-exist.titleblock")));
		QDomElement unnamed;
		QVERIFY(!tbt.saveToXmlElement(unnamed));
	}

	// Cells outside the grid, without a usable position, or on a place
	// already taken are left out; the template itself still loads.
	void invalidCellsIgnored()
	{
		const QString cell = QStringLiteral(
			"<field row=\"%1\" col=\"%2\"><value><translation lang=\"en\">%3</translation></value></field>");
		TitleBlockTemplate tbt;
		QVERIFY(load(tbt, templateXml(QStringLiteral("10;10"), QStringLiteral("50;50"),
			cell.arg("0", "0", "first")
			+ cell.arg("0", "0", "same place")
			+ cell.arg("2", "0", "past the last row")
			+ cell.arg("0", "-1", "negative column")
			+ cell.arg("x", "1", "no row")
			+ QStringLiteral("<field col=\"1\"><value><translation lang=\"en\">no row at all</translation></value></field>"))));

		QCOMPARE(tbt.cell(0, 0)->value.name(), QStringLiteral("first"));
		QCOMPARE(tbt.cell(0, 1)->type(), TitleBlockCell::EmptyCell);
		QCOMPARE(tbt.cell(1, 0)->type(), TitleBlockCell::EmptyCell);
		QCOMPARE(tbt.cell(1, 1)->type(), TitleBlockCell::EmptyCell);
		QCOMPARE(tbt.cell(2, 0), nullptr);
	}

	// Spans are applied as far as the grid goes and kept when saved; a
	// span over a cell which has content of its own is not applied.
	void spans()
	{
		const QString cell = QStringLiteral(
			"<field row=\"%1\" col=\"%2\" %3><value><translation lang=\"en\">%4</translation></value></field>");
		TitleBlockTemplate tbt;
		QVERIFY(load(tbt, templateXml(QStringLiteral("10;10;10"), QStringLiteral("50;50;50"),
			cell.arg("0", "0", "rowspan=\"1\" colspan=\"1\"", "block")
			+ cell.arg("2", "1", "colspan=\"5\"", "too wide")
			+ cell.arg("0", "2", "rowspan=\"1\"", "blocked")
			+ cell.arg("1", "2", "", "in the way"))));

		TitleBlockCell *block = tbt.cell(0, 0);
		QCOMPARE(tbt.cell(0, 1)->spanner_cell, block);
		QCOMPARE(tbt.cell(1, 0)->spanner_cell, block);
		QCOMPARE(tbt.cell(1, 1)->spanner_cell, block);
		QCOMPARE(block->span_state, int(TitleBlockCell::Enabled));

		TitleBlockCell *wide = tbt.cell(2, 1);
		QCOMPARE(wide->span_state, int(TitleBlockCell::Restricted));
		QCOMPARE(wide->applied_col_span, 1);
		QCOMPARE(tbt.cell(2, 2)->spanner_cell, wide);

		QCOMPARE(tbt.cell(0, 2)->span_state, int(TitleBlockCell::Disabled));
		QCOMPARE(tbt.cell(1, 2)->spanner_cell, nullptr);

		// what was asked for is what is written back
		TitleBlockTemplate reloaded;
		QVERIFY(reloadThroughFile(tbt, reloaded));
		QCOMPARE(reloaded.cell(2, 1)->col_span, 5);
		QCOMPARE(reloaded.cell(1, 1)->spanner_cell, reloaded.cell(0, 0));
		QCOMPARE(savedGrid(reloaded), savedGrid(tbt));
	}

	// A clone is the same template, with its spans pointing at its own cells.
	void cloneIsDeep()
	{
		TitleBlockTemplate tbt;
		QVERIFY(load(tbt, templateXml(QStringLiteral("10;10"), QStringLiteral("50;r100%"),
			QStringLiteral("<field row=\"0\" col=\"0\" colspan=\"1\">"
				       "<value><translation lang=\"en\">%title</translation></value></field>"))));
		QScopedPointer<TitleBlockTemplate> copy(tbt.clone());
		QCOMPARE(copy->name(), tbt.name());
		QCOMPARE(savedGrid(*copy), savedGrid(tbt));
		QVERIFY(copy->cell(0, 0) != tbt.cell(0, 0));
		QCOMPARE(copy->cell(0, 1)->spanner_cell, copy->cell(0, 0));
	}
};

QTEST_MAIN(tst_titleblocktemplate)

#include "tst_titleblocktemplate.moc"
