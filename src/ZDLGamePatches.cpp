/*
 * This file is part of uZDL
 * Copyright (C) 2026  luosmrow-lee
 *
 * uZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cstring>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QCryptographicHash>
#include "ZDLGamePatches.h"
#include "ZDLPaths.h"

namespace {

//One line changed: the line, counted from one, must hold the text it
//replaces, and only that text changes.
struct Change {
	int line;
	const char *from;
	const char *to;
	const char *why;
};

//A version of the file a fix was written for, whether it sits loose beside
//the GRP or inside it, and its MD5 once fixed, which is checked before
//anything is written.
struct Source {
	const char *md5;
	const char *fixedMd5;
};

struct Patch {
	const char *game;
	const char *title;
	const char *about;
	//The game's own archive, by which its folder is known. Games can share
	//a script, so a script on its own does not say which game it is for.
	const char *anchor;
	//The file the ports read, and the fix writes: the loose one beside the
	//GRP, or when there is none, the copy inside it.
	const char *file;
	Source sources[4];
	Change changes[8];
};

const Patch PATCHES[]={
	{
		"Quest for Hussein",
		"Script fixes for EDuke32",
		"EDuke32 aborts while compiling the game's scripts: in state femcode an 'else' follows no 'if', which the DOS game passed over. "
		"The same file misspells the blimp's waiting action, so in respawn mode a destroyed blimp never came back. "
		"Both copies of the script are covered, the loose one beside the GRP and the one inside it, which differ only in a comment.",
		"QUEST2.GRP",
		"GAME.CON",
		{
			{"582b620d278f1d6a33bdbacdc61b459e", "81eaa63bfaeb22678347b4d3ad1db958"},
			{"7a38d82e570d5831df4545177a26fe6c", "cf15792a5fb1ad45574093e7e92fec74"},
			{NULL, NULL}
		},
		{
			{1234, "else ifactor FEM8 break", "ifactor FEM8 break", "an 'else' with no 'if' before it, on which EDuke32 aborts"},
			{157, "action BLIMPRESPAWNTIME", "action BLIMPWAITTORESPAWN", "a number used as an action, where the blimp's own waiting action belongs"},
			{0, NULL, NULL, NULL}
		}
	},
	{
		"Quest for Al-Qa'eda",
		"Script fixes for EDuke32",
		"The game's scripts are the ones Quest for Hussein reused, with the same two faults. "
		"EDuke32 aborts while compiling them: in state femcode an 'else' follows no 'if', which the DOS game passed over. "
		"The same file misspells the blimp's waiting action, so in respawn mode a destroyed blimp never came back. "
		"The loose script beside the GRP and the one inside it are the same file, and both are covered.",
		"QUEST1.GRP",
		"GAME.CON",
		{
			{"7a38d82e570d5831df4545177a26fe6c", "cf15792a5fb1ad45574093e7e92fec74"},
			{NULL, NULL}
		},
		{
			{1234, "else ifactor FEM8 break", "ifactor FEM8 break", "an 'else' with no 'if' before it, on which EDuke32 aborts"},
			{157, "action BLIMPRESPAWNTIME", "action BLIMPWAITTORESPAWN", "a number used as an action, where the blimp's own waiting action belongs"},
			{0, NULL, NULL, NULL}
		}
	},
	{
		"Plunder & Pillage",
		"Script fixes for EDuke32",
		"The scripts both Quest games were later built on, with the faults they inherited. "
		"EDuke32 aborts while compiling them: in state femcode an 'else' follows no 'if', which the DOS game passed over. "
		"The same file misspells the blimp's waiting action, so in respawn mode a destroyed blimp never came back. "
		"The loose script beside the GRP and the one inside it are the same file, and both are covered.",
		"PIRATE.GRP",
		"GAME.CON",
		{
			{"5b0e08bdea6e8bf63de4dabd20b5ddf2", "14ac0b9c1a3b1bf467c9af852cce50d1"},
			{NULL, NULL}
		},
		{
			{1234, "else ifactor FEM8 break", "ifactor FEM8 break", "an 'else' with no 'if' before it, on which EDuke32 aborts"},
			{157, "action BLIMPRESPAWNTIME", "action BLIMPWAITTORESPAWN", "a number used as an action, where the blimp's own waiting action belongs"},
			{0, NULL, NULL, NULL}
		}
	},
	{NULL, NULL, NULL, NULL, NULL, {{NULL, NULL}}, {{0, NULL, NULL, NULL}}}
};

QString Md5(const QByteArray &bytes)
{
	return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Md5).toHex());
}

QByteArray ReadAll(const QString &path, bool *ok=NULL)
{
	QFile file(path);
	bool opened=file.open(QIODevice::ReadOnly);
	if (ok)
		*ok=opened;
	return opened?file.readAll():QByteArray();
}

//A file in a folder by name, whatever its case.
QString FindFile(const QString &folder, const char *name)
{
	QDir dir(folder);
	foreach (const QString &entry, dir.entryList(QDir::Files)) {
		if (entry.compare(QString::fromLatin1(name), Qt::CaseInsensitive)==0)
			return dir.filePath(entry);
	}
	return QString();
}

//One file out of a GRP.
QByteArray GrpEntry(const QString &grp, const char *name, bool *ok)
{
	*ok=false;
	QFile file(grp);
	char magic[12];
	quint32 count=0;
	if (!file.open(QIODevice::ReadOnly)||file.read(magic, 12)!=12||memcmp(magic, "KenSilverman", 12)||file.read(reinterpret_cast<char*>(&count), 4)!=4||count>200000)
		return QByteArray();

	QByteArray directory=file.read(qint64(count)*16);
	qint64 offset=16+qint64(count)*16;
	for (quint32 i=0; i<count&&qint64(i)*16+16<=directory.size(); i++) {
		const char *entry=directory.constData()+i*16;
		quint32 size;
		memcpy(&size, entry+12, 4);
		if (QString::fromLatin1(entry, int(qstrnlen(entry, 12))).compare(QString::fromLatin1(name), Qt::CaseInsensitive)==0) {
			if (!file.seek(offset))
				return QByteArray();
			QByteArray bytes=file.read(size);
			*ok=bytes.size()==qint64(size);
			return bytes;
		}
		offset+=size;
	}
	return QByteArray();
}

//The changes made to a file's bytes. The rest of every line, its line
//ending included, stays as it was.
bool ApplyChanges(const Patch &patch, QByteArray &bytes, QString *error)
{
	QList<QByteArray> lines=bytes.split('\n');
	for (int i=0; patch.changes[i].from; i++) {
		const Change &change=patch.changes[i];
		if (change.line<1||change.line>lines.size()) {
			*error=QString("%1 has no line %2.").arg(patch.file).arg(change.line);
			return false;
		}
		QByteArray &line=lines[change.line-1];
		int at=line.indexOf(change.from);
		if (at<0) {
			*error=QString("Line %1 of %2 does not hold \"%3\".").arg(change.line).arg(patch.file).arg(change.from);
			return false;
		}
		line.replace(at, int(qstrlen(change.from)), change.to);
	}
	bytes=lines.join('\n');
	return true;
}

const Source *SourceFor(const Patch &patch, const QString &md5)
{
	for (int i=0; patch.sources[i].md5; i++) {
		if (md5==QString::fromLatin1(patch.sources[i].md5))
			return &patch.sources[i];
	}
	return NULL;
}

const Source *SourceFixedAs(const Patch &patch, const QString &md5)
{
	for (int i=0; patch.sources[i].md5; i++) {
		if (md5==QString::fromLatin1(patch.sources[i].fixedMd5))
			return &patch.sources[i];
	}
	return NULL;
}

QString BackupOf(const QString &target)
{
	return target+".uzdl-original";
}

//The version of the file inside the game's GRP, when it is one the fix was
//written for.
const Source *SourceInGrp(const Patch &patch, const QString &folder)
{
	QString anchor=FindFile(folder, patch.anchor);
	bool ok=false;
	QByteArray inner;
	if (!anchor.isEmpty())
		inner=GrpEntry(anchor, patch.file, &ok);
	return ok?SourceFor(patch, Md5(inner)):NULL;
}

//A loose file that is the fixed copy of the GRP's own, which is what the
//fix writes beside a GRP when there is no loose file to fix.
bool IsCopyBesideGrp(const Patch &patch, const QString &folder, const QString &looseMd5)
{
	const Source *source=SourceInGrp(patch, folder);
	return source&&looseMd5==QString::fromLatin1(source->fixedMd5);
}

bool Write(const QString &path, const QByteArray &bytes, QString *error)
{
	QSaveFile out(path);
	if (!out.open(QIODevice::WriteOnly)||out.write(bytes)!=bytes.size()||!out.commit()) {
		*error=QString("%1 could not be written.").arg(QDir::toNativeSeparators(path));
		return false;
	}
	return true;
}

}

QList<ZDLGamePatches::Found> ZDLGamePatches::scanFolder(const QString &folder)
{
	QList<Found> out;

	for (int i=0; PATCHES[i].game; i++) {
		const Patch &patch=PATCHES[i];
		QString anchor=FindFile(folder, patch.anchor);
		if (anchor.isEmpty())
			continue;
		QString loose=FindFile(folder, patch.file);

		Found found;
		found.patch=i;
		found.game=patch.game;
		found.title=patch.title;
		found.folder=folder;

		if (!loose.isEmpty()) {
			QString md5=Md5(ReadAll(loose));
			if (SourceFor(patch, md5)) {
				found.state=NotApplied;
				found.stateText="Not applied";
			} else if (SourceFixedAs(patch, md5)) {
				found.state=Applied;
				found.stateText=!QFile::exists(BackupOf(loose))&&IsCopyBesideGrp(patch, folder, md5)?QString("Applied, as a fixed copy beside %1").arg(QFileInfo(anchor).fileName()):QString("Applied");
			} else {
				found.state=OtherVersion;
				found.stateText=QString("A version of %1 this fix was not written for: left alone").arg(QFileInfo(loose).fileName());
			}
		} else if (SourceInGrp(patch, folder)) {
			found.state=NotApplied;
			found.stateText=QString("Not applied; a fixed copy will be written beside %1").arg(QFileInfo(anchor).fileName());
		} else {
			found.state=OtherVersion;
			found.stateText=QString("A version of %1 this fix was not written for: left alone").arg(QFileInfo(anchor).fileName());
		}
		out<<found;
	}
	return out;
}

QList<ZDLGamePatches::Found> ZDLGamePatches::scan(ZDLConf *zconf)
{
	QList<Found> out;
	ZDLSection *section=zconf?zconf->getSection("zdl.iwads"):NULL;
	if (!section)
		return out;

	QStringList folders;
	QVector<ZDLLine*> files;
	section->getRegex("^i[0-9]+f$", files);
	foreach (ZDLLine *line, files) {
		QFileInfo fi(ZDLPaths::resolve(line->getValue()));
		QString folder=QDir::cleanPath(fi.isDir()?fi.absoluteFilePath():fi.absolutePath());
		bool seen=false;
		foreach (const QString &known, folders)
			seen=seen||known.compare(folder, Qt::CaseInsensitive)==0;
		if (!seen&&QDir(folder).exists())
			folders<<folder;
	}

	foreach (const QString &folder, folders)
		out<<scanFolder(folder);
	return out;
}

bool ZDLGamePatches::apply(const Found &found, QString *error)
{
	const Patch &patch=PATCHES[found.patch];
	QString loose=FindFile(found.folder, patch.file);
	QByteArray bytes;
	const Source *source=NULL;

	if (!loose.isEmpty()) {
		bytes=ReadAll(loose);
		source=SourceFor(patch, Md5(bytes));
		if (!source) {
			*error=SourceFixedAs(patch, Md5(bytes))?QString("This fix is applied already."):QString("%1 is not a version this fix was written for, so it was left alone.").arg(QFileInfo(loose).fileName());
			return false;
		}
	} else {
		QString anchor=FindFile(found.folder, patch.anchor);
		bool ok=false;
		if (!anchor.isEmpty())
			bytes=GrpEntry(anchor, patch.file, &ok);
		source=ok?SourceFor(patch, Md5(bytes)):NULL;
		if (!source) {
			*error=QString("The %1 in %2 is not a version this fix was written for, so nothing was written.").arg(patch.file).arg(patch.anchor);
			return false;
		}
	}

	if (!ApplyChanges(patch, bytes, error))
		return false;
	if (Md5(bytes)!=QString::fromLatin1(source->fixedMd5)) {
		*error="The fixed file did not come out as it should, so nothing was changed.";
		return false;
	}

	if (!loose.isEmpty()) {
		QString backup=BackupOf(loose);
		if (!QFile::exists(backup)&&!QFile::copy(loose, backup)) {
			*error=QString("The original could not be kept as %1, so nothing was changed.").arg(QDir::toNativeSeparators(backup));
			return false;
		}
		return Write(loose, bytes, error);
	}
	return Write(QDir(found.folder).filePath(patch.file), bytes, error);
}

bool ZDLGamePatches::undo(const Found &found, QString *error)
{
	const Patch &patch=PATCHES[found.patch];
	QString loose=FindFile(found.folder, patch.file);
	if (loose.isEmpty()) {
		*error="There is no fixed file to undo.";
		return false;
	}

	QString backup=BackupOf(loose);
	if (QFile::exists(backup)) {
		QByteArray original=ReadAll(backup);
		if (!SourceFor(patch, Md5(original))) {
			*error=QString("%1 is not the original this fix kept, so it was left alone.").arg(QDir::toNativeSeparators(backup));
			return false;
		}
		if (!Write(loose, original, error))
			return false;
		QFile::remove(backup);
		return true;
	}

	//Written beside the GRP: removed, and only when it is the fixed copy of
	//the GRP's own. The original is still in the GRP, which the ports go
	//back to reading, so the copy is simply deleted.
	if (!IsCopyBesideGrp(patch, found.folder, Md5(ReadAll(loose)))) {
		*error=QString("%1 is not a copy this fix wrote, so it was left alone.").arg(QFileInfo(loose).fileName());
		return false;
	}
	if (!QFile::remove(loose)) {
		*error=QString("%1 could not be removed.").arg(QDir::toNativeSeparators(loose));
		return false;
	}
	return true;
}

QString ZDLGamePatches::describe(int index)
{
	const Patch &patch=PATCHES[index];
	QString text=QString("%1: %2\n\n%3\n\nChanges to %4:").arg(patch.game, patch.title, patch.about, patch.file);
	for (int i=0; patch.changes[i].from; i++) {
		const Change &change=patch.changes[i];
		text+=QString("\n\nLine %1, %2:\n    %3\nbecomes\n    %4").arg(change.line).arg(change.why).arg(change.from).arg(change.to);
	}
	text+=QString("\n\nThe original is kept beside the fixed file, and Undo puts it back. "
		"Where there is no loose %1, a fixed copy of the one inside %2 is written beside it, and Undo removes it.").arg(patch.file, patch.anchor);
	return text;
}

QStringList ZDLGamePatches::notesFor(const QString &file)
{
	QStringList notes;
	QFileInfo fi(file);
	if (!fi.isFile())
		return notes;

	//What the Game patches list says of the file's folder, for the fixes
	//whose script or GRP this is.
	QList<Found> inFolder;
	bool scanned=false;
	for (int i=0; PATCHES[i].game; i++) {
		const Patch &patch=PATCHES[i];
		bool script=fi.fileName().compare(QString::fromLatin1(patch.file), Qt::CaseInsensitive)==0;
		bool anchor=fi.fileName().compare(QString::fromLatin1(patch.anchor), Qt::CaseInsensitive)==0;
		if (!script&&!anchor)
			continue;
		if (!scanned) {
			inFolder=scanFolder(fi.absolutePath());
			scanned=true;
		}
		foreach (const Found &found, inFolder) {
			if (found.patch!=i)
				continue;
			if (anchor) {
				QString state=found.stateText;
				state[0]=state[0].toLower();
				notes<<QString("uZDL has a fix for this game, %1: %2. See Game patches on the uZDL settings tab.").arg(found.title, state);
			} else if (found.state==NotApplied) {
				notes<<QString("uZDL can fix this file: %1, %2. See Game patches on the uZDL settings tab.").arg(found.game, found.title);
			} else if (found.state==Applied) {
				notes<<QString("Fixed by uZDL: %1, %2.").arg(found.game, found.title);
			}
		}
	}
	return notes;
}
