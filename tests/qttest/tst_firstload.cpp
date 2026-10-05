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
#include "qettesthelpers.h"

#include <QtTest>

#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QUuid>

// Opening a project written by the current version and saving it again
// loses nothing. tst_resaveunchanged compares a second save with the first,
// so whatever the first load drops is gone from both and goes unnoticed;
// this test compares the original file with the first save instead.
//
// The two files are compared as XML: elements by tag (and by the value of
// a name attribute, which is what tells properties apart), in order;
// attributes as sets, since their order is not kept; text with the spaces
// around it trimmed. A save legitimately rewrites a few things -- the
// version and date of the save, uuids for items that had none, values it
// works out again from the rest of the file -- and each of those is listed
// in allowList() below with the reason. Anything else that differs is
// reported, the first differences first, with the path to them.
//
// Runs the real binary's --resave on every fixture and example whose
// <project> carries the current version (0.200.x). Older files are left
// out: loading migrates them (terminals numbered rather than named by
// uuid, symbols written in another order), so they differ everywhere.
class tst_firstload : public QObject
{
	Q_OBJECT

	enum Kind { AttributeLost, AttributeChanged, AttributeAdded,
				ElementLost, ElementAdded, TextChanged };

	struct Difference
	{
		Kind kind;
		QString path;      // "project/diagram[2]/elements/element[5]"
		QString attribute; // the attribute, or the tag of the element
		QDomElement element; // in the original (in the save for ElementAdded)
		QString before;    // the attribute's or the text's value in each
		QString after;
		QString text;      // readable description
	};

	// A difference a save may make. @c path is a regular expression matched
	// against the whole path of the element concerned; @c when, if given,
	// must hold as well.
	struct Allowed
	{
		Kind kind;
		const char *path;
		const char *attribute; // nullptr: any
		bool (*when)(const Difference &);
		const char *why;
	};

	static bool isUuid(const QString &s)
	{
		return !QUuid::fromString(s).isNull();
	}

	static const QList<Allowed> &allowList()
	{
		static const QList<Allowed> list {
			{AttributeChanged, "^project$", "version", nullptr,
			 "the project is stamped with the version that saved it"},
			{AttributeChanged, "^project/diagram(\\[\\d+\\])?$", "version", nullptr,
			 "each folio is stamped with the version that saved it"},

			{TextChanged, "^project/properties/property\\(name=saved[a-z-]*\\)$", nullptr, nullptr,
			 "the date, time and file name of the last save, filled in by every save"},
			{ElementAdded, "^project/properties/property\\(name=savedfilepath\\)$", nullptr, nullptr,
			 "the path of the last save, filled in by every save"},

			// Conductor::toXml() copies, beside the uuids that join a wire to
			// its terminals, the label and name of each symbol and the name
			// of each terminal, for readers of the file (the wiring list
			// export). Loading reads none of them back; the name is the
			// symbol's in the language QElectroTech runs in.
			{AttributeAdded, "/conductor(\\[\\d+\\])?$",
			 nullptr, [](const Difference &d) {
				 return QRegularExpression(QStringLiteral(
						 "^(element[12]_(name|label|linked)|terminalname[12])$"))
						 .match(d.attribute).hasMatch(); },
			 "a copy of the symbol's label and name and the terminal's name"},
			{AttributeChanged, "/conductor(\\[\\d+\\])?$",
			 nullptr, [](const Difference &d) {
				 return QRegularExpression(QStringLiteral("^element[12]_name$"))
						 .match(d.attribute).hasMatch(); },
			 "the symbol's name, in the language QElectroTech runs in"},

			// A file can still name a wire's terminals by their number in the
			// file (terminal1="5"), the way before terminals had uuids. The
			// load joins the wire to the terminals and the save names them by
			// uuid, with the symbol each belongs to.
			{AttributeChanged, "/conductor(\\[\\d+\\])?$",
			 nullptr, [](const Difference &d) {
				 return (d.attribute == QLatin1String("terminal1")
						 || d.attribute == QLatin1String("terminal2"))
						 && QRegularExpression(QStringLiteral("^\\d+$")).match(d.before).hasMatch()
						 && isUuid(d.after); },
			 "a terminal number from before terminals had uuids, now named by its uuid"},
			{AttributeAdded, "/conductor(\\[\\d+\\])?$",
			 nullptr, [](const Difference &d) {
				 return (d.attribute == QLatin1String("element1")
						 || d.attribute == QLatin1String("element2"))
						 && isUuid(d.after); },
			 "the symbol a wire's terminal belongs to, saved with the terminal's uuid"},

			// An item saved without a uuid gets one when it is loaded
			// (derived from what it is, see tst_derivedwireuuid), and keeps it.
			{AttributeAdded, "/(conductor|terminal)(\\[\\d+\\])?$", "uuid",
			 [](const Difference &d) { return isUuid(d.after); },
			 "a uuid given to a wire or terminal that had none"},

			// The <text> of a symbol's text that shows an information or a
			// formula is what it displayed when saved; it is worked out again
			// on loading. Only a text the user typed (UserText) is kept as is.
			{TextChanged, "/dynamic_elmt_text(\\[\\d+\\])?/text$", nullptr,
			 [](const Difference &d) {
				 const QString from = d.element.parentNode().toElement()
						 .attribute(QStringLiteral("text_from"));
				 return !from.isEmpty() && from != QLatin1String("UserText"); },
			 "the displayed value of a text worked out from the symbol's data"},

			// An empty cross-reference position, as older versions wrote it,
			// reads as the default (issue #1238, see tst_xrefpos).
			{AttributeChanged, "/xref(\\[\\d+\\])?$", "xrefpos",
			 [](const Difference &d) {
				 return d.before.isEmpty() && d.after == QLatin1String("AlignBottom"); },
			 "an empty cross-reference position read as the default"},

			// Cross references at the folio bottom stack only when their
			// texts overlap (#1287 follow-up); a file written before that
			// option has no attribute, reads as true and is saved with it.
			{AttributeAdded, "/xref(\\[\\d+\\])?$", "stackoverlapping",
			 [](const Difference &d) { return d.after == QLatin1String("true"); },
			 "the cross-reference stacking option, added with its default"},
		};
		return list;
	}

	static bool allowed(const Difference &d)
	{
		for (const Allowed &a : allowList()) {
			if (a.kind != d.kind)
				continue;
			if (a.attribute && d.attribute != QLatin1String(a.attribute))
				continue;
			if (!QRegularExpression(QString::fromLatin1(a.path)).match(d.path).hasMatch())
				continue;
			if (a.when && !a.when(d))
				continue;
			return true;
		}
		return false;
	}

	// The text an element holds itself (not its children's), trimmed.
	static QString ownText(const QDomElement &e)
	{
		QString text;
		for (QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling())
			if (n.isText()) // QDomCDATASection is a QDomText too
				text += n.toText().data();
		return text.trimmed();
	}

	// What pairs a child of the original with one of the save: its tag,
	// and the value of a name attribute, which is what tells properties
	// apart.
	static QString key(const QDomElement &e)
	{
		if (!e.hasAttribute(QStringLiteral("name")))
			return e.tagName();
		return QStringLiteral("%1(name=%2)").arg(e.tagName(), e.attribute(QStringLiteral("name")));
	}

	// "tag", or "tag[3]" when it has siblings of the same key.
	static QString segment(const QDomElement &e, int index, int count)
	{
		QString s = key(e);
		if (count > 1)
			s += QStringLiteral("[%1]").arg(index);
		return s;
	}

	static QList<QDomElement> children(const QDomElement &e)
	{
		QList<QDomElement> list;
		for (QDomElement c = e.firstChildElement(); !c.isNull(); c = c.nextSiblingElement())
			list << c;
		return list;
	}

	static QString quoted(const QString &s)
	{
		const QString shown = s.size() > 60 ? s.left(57) + QStringLiteral("...") : s;
		return QLatin1Char('"') + shown + QLatin1Char('"');
	}

	// Every difference between @p a (the original) and @p b (the save).
	static void compare(const QDomElement &a, const QDomElement &b,
						const QString &path, QList<Difference> &out)
	{
		const QDomNamedNodeMap aa = a.attributes(), ba = b.attributes();
		for (int i = 0; i < aa.count(); ++i) {
			const QDomAttr attr = aa.item(i).toAttr();
			if (!b.hasAttribute(attr.name()))
				out << Difference{AttributeLost, path, attr.name(), a, attr.value(), QString(),
								  QStringLiteral("%1: attribute %2=%3 is missing from the save")
								  .arg(path, attr.name(), quoted(attr.value()))};
			else if (b.attribute(attr.name()) != attr.value())
				out << Difference{AttributeChanged, path, attr.name(), a, attr.value(),
								  b.attribute(attr.name()),
								  QStringLiteral("%1: attribute %2 changed from %3 to %4")
								  .arg(path, attr.name(), quoted(attr.value()),
									   quoted(b.attribute(attr.name())))};
		}
		for (int i = 0; i < ba.count(); ++i) {
			const QDomAttr attr = ba.item(i).toAttr();
			if (!a.hasAttribute(attr.name()))
				out << Difference{AttributeAdded, path, attr.name(), a, QString(), attr.value(),
								  QStringLiteral("%1: attribute %2=%3 was added by the save")
								  .arg(path, attr.name(), quoted(attr.value()))};
		}

		const QString at = ownText(a), bt = ownText(b);
		if (at != bt)
			out << Difference{TextChanged, path, QString(), a, at, bt,
							  QStringLiteral("%1: text changed from %2 to %3")
							  .arg(path, quoted(at), quoted(bt))};

		// Children in order, an element with a name attribute matched by
		// its name as well as its tag. Where the two part, an element of the
		// original that turns up further on in the save means the save
		// added the ones in between; one that does not was lost.
		const QList<QDomElement> ac = children(a), bc = children(b);
		QHash<QString, int> a_count, b_count, a_seen, b_seen;
		for (const QDomElement &c : ac) ++a_count[key(c)];
		for (const QDomElement &c : bc) ++b_count[key(c)];
		auto childPath = [&](const QDomElement &c, QHash<QString, int> &seen,
							 const QHash<QString, int> &count) {
			return path + QLatin1Char('/')
					+ segment(c, ++seen[key(c)], count.value(key(c)));
		};
		int i = 0, j = 0;
		while (i < ac.size() || j < bc.size()) {
			if (i < ac.size() && j < bc.size() && key(ac[i]) == key(bc[j])) {
				childPath(bc[j], b_seen, b_count);
				compare(ac[i], bc[j], childPath(ac[i], a_seen, a_count), out);
				++i; ++j;
				continue;
			}
			int found = -1;
			if (i < ac.size())
				for (int k = j; k < bc.size() && found < 0; ++k)
					if (key(bc[k]) == key(ac[i]))
						found = k;
			if (found < 0 && i < ac.size()) {
				const QString p = childPath(ac[i], a_seen, a_count);
				out << Difference{ElementLost, p, ac[i].tagName(), ac[i], QString(), QString(),
								  QStringLiteral("%1: element is missing from the save").arg(p)};
				++i;
			} else {
				const QString p = childPath(bc[j], b_seen, b_count);
				out << Difference{ElementAdded, p, bc[j].tagName(), bc[j], QString(), QString(),
								  QStringLiteral("%1: element was added by the save").arg(p)};
				++j;
			}
		}
	}

	static QDomDocument load(const QString &path)
	{
		QFile f(path);
		QDomDocument doc;
		if (!f.open(QIODevice::ReadOnly) || !doc.setContent(&f))
			return {};
		return doc;
	}

	QTemporaryDir m_dir;
	int m_run = 0;

	// --resave @p in to a new file, in a sandbox of its own; returns the
	// new file's path, or an empty string when the run failed.
	QString resave(const QString &in)
	{
		const QString out = m_dir.filePath(QStringLiteral("out%1.qet").arg(m_run));
		const QString home = m_dir.filePath(QStringLiteral("home%1").arg(m_run++));
		QProcess proc;
		proc.setProcessEnvironment(QET::Test::sandboxEnvironment(home));
		proc.start(QStringLiteral(QET_TEST_BINARY_PATH), {QStringLiteral("--resave"), in, out});
		if (!proc.waitForFinished(120000) || proc.exitStatus() != QProcess::NormalExit
			|| proc.exitCode() != 0)
			return {};
		return out;
	}

	static bool isCurrentFormat(const QString &path)
	{
		const QString version =
				load(path).documentElement().attribute(QStringLiteral("version"));
		return version.startsWith(QLatin1String("0.200."));
	}

private slots:
	void initTestCase()
	{
		QVERIFY(m_dir.isValid());
		QVERIFY(QFile::exists(QStringLiteral(QET_TEST_BINARY_PATH)));
	}

	void firstSaveLosesNothing_data()
	{
		QTest::addColumn<QString>("project");
		const QDir fixtures(QFileInfo(QFINDTESTDATA("fixtures/pdf_page_a3.qet")).absolutePath());
		const QDir examples(QStringLiteral(QET_EXAMPLES_DIR));
		int rows = 0;
		for (const QDir &dir : {fixtures, examples}) {
			const QStringList files =
					dir.entryList({QStringLiteral("*.qet")}, QDir::Files, QDir::Name);
			for (const QString &file : files) {
				const QString path = dir.filePath(file);
				if (!isCurrentFormat(path))
					continue;
				QTest::newRow(file.toUtf8().constData()) << path;
				++rows;
			}
		}
		QVERIFY2(rows >= 5, "fewer current-format projects found than expected");
	}

	void firstSaveLosesNothing()
	{
		QFETCH(QString, project);
		const QDomDocument original_doc = load(project);
		const QDomElement original = original_doc.documentElement();
		QVERIFY2(!original.isNull(), "the project is not well-formed XML");
		const QString saved_path = resave(project);
		QVERIFY2(!saved_path.isEmpty(), "--resave failed");
		const QDomDocument saved_doc = load(saved_path);
		const QDomElement saved = saved_doc.documentElement();
		QVERIFY2(!saved.isNull(), "the save is not well-formed XML");

		QList<Difference> all;
		compare(original, saved, original.tagName(), all);
		QStringList unexpected;
		for (const Difference &d : std::as_const(all))
			if (!allowed(d))
				unexpected << d.text;

		const int shown = 20;
		QString message = QStringLiteral("%1 unexpected difference(s) between the original "
										 "and its first save:\n  ").arg(unexpected.size());
		message += unexpected.mid(0, shown).join(QStringLiteral("\n  "));
		if (unexpected.size() > shown)
			message += QStringLiteral("\n  ... and %1 more").arg(unexpected.size() - shown);
		QVERIFY2(unexpected.isEmpty(), qPrintable(message));
	}
};

QTEST_APPLESS_MAIN(tst_firstload)

#include "tst_firstload.moc"
