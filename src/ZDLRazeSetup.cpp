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

#include "ZDLRazeSetup.h"
#include "ZDLFileInfo.h"
#include "ZDLPaths.h"
#include "ZDLMainWindow.h"
#include "ZDLConfigurationManager.h"
#include "zdlcommon.h"
#if defined(Q_OS_WIN)
#include <windows.h>
#endif

extern ZDLMainWindow *mw;

namespace {

const char *const SECTION="[GameSearch.Directories]";

//The line for a grps folder beside Raze's uZDL that needs no depth: uZDL
//sets the variable for every port it starts.
const char *const VARIABLE_LINE="$UZDL_DIR/grps/*";

//Raze's own defaults for the section, which it writes only when the
//section is absent: a section uZDL makes has to carry them itself.
const char *const DEFAULT_PATHS[]={
	".", "./*",
#if defined(Q_OS_WIN)
	"$PROGDIR", "$PROGDIR/*",
#else
	"$HOME/.config/raze", "$HOME/.config/raze/*",
	"/opt/raze",
	"/usr/share/raze", "/usr/share/raze/*",
	"/usr/share/games/raze", "/usr/share/games/raze/*",
	"/usr/local/share/raze", "/usr/local/share/raze/*",
	"/usr/local/share/games/raze", "/usr/local/share/games/raze/*",
	"/usr/share/games/jfduke3d", "/usr/share/games/eduke32", "/usr/share/games/nblood", "/usr/share/games/jfsw", "/usr/share/games/voidsw",
	"/usr/local/share/games/eduke32", "/usr/local/share/games/jfduke3d", "/usr/local/share/games/nblood", "/usr/local/share/games/jfsw", "/usr/local/share/games/voidsw",
#endif
	"$STEAM",
	NULL
};

//The files Raze takes for a game: anything but what the Doom ports take.
bool IsBuildGame(const QString &file)
{
	static const char *const exts[]={"grp", "rff", "dat", "ssi", "zip", "pk4", "7z", "pk7", NULL};
	QString ext=QFileInfo(file).suffix().toLower();
	for (int i=0; exts[i]; i++) {
		if (ext==exts[i])
			return true;
	}
	return false;
}

//A folder the way it is written to and compared in the ini: absolute,
//forward slashes, no slash at the end.
QString Folder(const QString &path)
{
	QString out=QDir::fromNativeSeparators(QDir::cleanPath(path));
	//The root of a drive keeps its slash: "C:" on its own is not a root
	//to Qt but the current folder on that drive.
	while (out.size()>1&&out.endsWith('/')&&!(out.size()==3&&out[1]==':'))
		out.chop(1);
	return out;
}

#if defined(Q_OS_WIN)
const Qt::CaseSensitivity FOLDER_CASE=Qt::CaseInsensitive;
#else
const Qt::CaseSensitivity FOLDER_CASE=Qt::CaseSensitive;
#endif

bool SameFolder(const QString &l, const QString &r)
{
	return l.compare(r, FOLDER_CASE)==0;
}

//Whether a folder is a uZDL folder: the one running, or another one with
//uZDL in it, which is the unit that gets moved or copied whole.
bool IsUzdlFolder(const QString &dir)
{
	if (SameFolder(dir, Folder(ZDLPaths::appDir())))
		return true;
	QDir d(dir);
	if (d.exists("uzdl.exe")||d.exists("uzdl")||d.exists(ZDL_PORTABLE_INI))
		return true;
	return !d.entryList(QStringList()<<"uZDL*.AppImage", QDir::Files).isEmpty();
}

//The deepest folder holding both, or an empty string when nothing below
//the root of the drive does.
QString CommonAncestor(const QString &a, const QString &b)
{
	QStringList pa=a.split('/');
	QStringList pb=b.split('/');
	int n=0;
	while (n<pa.size()&&n<pb.size()&&pa[n].compare(pb[n], FOLDER_CASE)==0)
		n++;
	//The first part is the drive, or nothing for a root on Linux: a match
	//on that alone is the root itself.
	if (n<2)
		return QString();
	return QStringList(pa.mid(0, n)).join('/');
}

//The uZDL folder a folder lies in, itself or the nearest one above it, or
//an empty string when there is none.
QString UzdlFolderAbove(const QString &folder)
{
	QString dir=folder;
	//Bounded as well, so that no odd path can keep this climbing for good.
	for (int depth=0; depth<64&&!dir.isEmpty()&&!QDir(dir).isRoot(); depth++) {
		if (IsUzdlFolder(dir))
			return dir;
		QString up=Folder(QFileInfo(dir).absolutePath());
		if (SameFolder(up, dir)||!up.contains('/'))
			break;
		dir=up;
	}
	return QString();
}

bool InsideUzdlFolder(const QString &folder)
{
	return !UzdlFolderAbove(folder).isEmpty();
}

//Game folders that share a parent, written as that parent with /*: the
//games put there later, and the base game an add-on needs, are reached as
//well. A game folder on its own stays as it is.
QStringList Condense(const QStringList &folders)
{
	QHash<QString, int> perParent;
	foreach (const QString &folder, folders) {
		QString parent=Folder(QFileInfo(folder).absolutePath());
		if (!QDir(parent).isRoot())
			perParent[FOLDER_CASE==Qt::CaseInsensitive?parent.toLower():parent]++;
	}

	QStringList out;
	foreach (const QString &folder, folders) {
		QString parent=Folder(QFileInfo(folder).absolutePath());
		QString key=FOLDER_CASE==Qt::CaseInsensitive?parent.toLower():parent;
		QStringList entries;
		if (perParent.value(key)>=2)
			entries<<parent+"/*";
		else
			entries<<folder;
		foreach (const QString &entry, entries) {
			bool have=false;
			foreach (const QString &known, out)
				have=have||SameFolder(known, entry);
			if (!have)
				out<<entry;
		}
	}
	return out;
}

//A line of the search list as the folder it means, for comparing: $PROGDIR
//and $HOME expanded, a bare relative path taken from Raze's folder, which
//is where uZDL starts Raze. A /* at the end is kept, and anything else
//that starts with $, such as $STEAM, is no folder and stays as it is.
QString Meant(const QString &entry, const QString &razeDir)
{
	QString e=QDir::fromNativeSeparators(entry.trimmed());
	bool star=e.endsWith("/*");
	if (star)
		e.chop(2);
	if (e.startsWith("$PROGDIR", Qt::CaseInsensitive))
		e=razeDir+e.mid(8);
	else if (e.startsWith("$HOME", Qt::CaseInsensitive))
		e=QDir::homePath()+e.mid(5);
	else if (e.startsWith('$'))
		return entry.trimmed();
	if (QFileInfo(e).isRelative())
		e=razeDir+"/"+e;
	return Folder(e)+(star?"/*":"");
}

//A folder the way it goes into the list: relative to Raze's folder, as
//../../grps/*, when it lies inside that folder or the two share a uZDL
//folder; as it is otherwise, since a game elsewhere stays put when uZDL
//is moved. Raze takes a bare relative path from its working directory,
//which is its own folder whenever uZDL starts it.
QString Written(const QString &folder, const QString &razeDir)
{
	bool relative=SameFolder(folder, razeDir)||folder.startsWith(razeDir+"/", FOLDER_CASE);
	if (!relative) {
		QString common=CommonAncestor(folder, razeDir);
		relative=!common.isEmpty()&&InsideUzdlFolder(common);
	}
	return relative?QDir(razeDir).relativeFilePath(folder):folder;
}

//Whether a folder is reached by the paths listed: itself, or its parent
//with /* after it.
bool Covered(const QStringList &listed, const QString &folder)
{
	QString parentStar=Folder(QFileInfo(folder).absolutePath())+"/*";
	foreach (const QString &entry, listed) {
		if (SameFolder(entry, folder)||SameFolder(entry, parentStar))
			return true;
	}
	return false;
}

//The IWAD selected for the launch, resolved.
QString SelectedIwad(ZDLConf *zconf, const QString &launchSection)
{
	const QString launch=launchSection.isEmpty()?QString("zdl.save"):launchSection;
	if (!zconf->hasValue(launch, "iwad"))
		return QString();
	QString name=zconf->getValue(launch, "iwad");
	ZDLSection *section=zconf->getSection("zdl.iwads");
	if (!section)
		return QString();

	QVector<ZDLLine*> names;
	section->getRegex("^i[0-9]+n$", names);
	foreach (ZDLLine *line, names) {
		if (line->getValue()!=name)
			continue;
		QString var=line->getVariable();
		var[var.size()-1]='f';
		QVector<ZDLLine*> files;
		section->getRegex("^"+var+"$", files);
		if (files.size()==1)
			return ZDLPaths::resolve(files[0]->getValue());
	}
	return QString();
}

//What was decided for an executable: "done", "skip", or nothing yet. Kept
//in its own section, since the port list is rewritten whole on every save.
QString Decision(ZDLConf *zconf, const QString &exe, QString *variable=NULL)
{
	ZDLSection *section=zconf->getSection("zdl.raze");
	if (!section)
		return QString();

	QVector<ZDLLine*> files;
	section->getRegex("^r[0-9]+f$", files);
	foreach (ZDLLine *line, files) {
		if (!SameFolder(Folder(ZDLPaths::resolve(line->getValue())), Folder(exe)))
			continue;
		QString var=line->getVariable();
		var[var.size()-1]='s';
		if (variable)
			*variable=var;
		return section->findVariable(var);
	}
	return QString();
}

void Decide(ZDLConf *zconf, const QString &exe, const QString &state)
{
	QString var;
	if (Decision(zconf, exe, &var).isEmpty()) {
		int next=0;
		if (ZDLSection *section=zconf->getSection("zdl.raze")) {
			QVector<ZDLLine*> files;
			section->getRegex("^r[0-9]+f$", files);
			foreach (ZDLLine *line, files) {
				QString v=line->getVariable();
				next=qMax(next, v.mid(1, v.size()-2).toInt()+1);
			}
		}
		zconf->setValue("zdl.raze", QString("r%1f").arg(next), ZDLPaths::preferRelative(exe));
		var=QString("r%1s").arg(next);
	}
	zconf->setValue("zdl.raze", var, state);
}

#if defined(Q_OS_WIN)
//Raze considers a folder under Program Files read-only without looking.
bool UnderProgramFiles(const QString &folder)
{
	static const char *const vars[]={"ProgramFiles", "ProgramFiles(x86)", "ProgramW6432", NULL};
	for (int i=0; vars[i]; i++) {
		QString pf=Folder(qEnvironmentVariable(vars[i]));
		if (!pf.isEmpty()&&(SameFolder(folder, pf)||folder.startsWith(pf+"/", Qt::CaseInsensitive)))
			return true;
	}
	return false;
}

//The per-user file Raze reads when it is not portable: the current place,
//then the older ones, so that whichever it used until now is found.
QStringList PerUserFiles(const QString &exeDir)
{
	QStringList files;
	files<<exeDir+"/raze-"+qEnvironmentVariable("USERNAME")+".ini";
	files<<QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)+"/My Games/Raze/raze.ini";
	QString appData=qEnvironmentVariable("APPDATA");
	if (!appData.isEmpty())
		files<<QDir::fromNativeSeparators(appData)+"/Raze/raze.ini";
	return files;
}
#endif

QString Native(const QString &path)
{
	return QDir::toNativeSeparators(path);
}

}

bool ZDLRazeSetup::isRaze(const QString &exe)
{
	if (exe.isEmpty()||!QFileInfo(exe).isFile())
		return false;
#if defined(Q_OS_WIN)
	return ZDLAppInfo(exe).GetProductName().compare("Raze", Qt::CaseInsensitive)==0;
#else
	return QFileInfo(exe).baseName().toLower().startsWith("raze");
#endif
}

QStringList ZDLRazeSetup::gameFolders(ZDLConf *zconf)
{
	QStringList folders;
	ZDLSection *section=zconf->getSection("zdl.iwads");
	if (!section)
		return folders;

	QVector<ZDLLine*> files;
	section->getRegex("^i[0-9]+f$", files);
	foreach (ZDLLine *line, files) {
		QString file=ZDLPaths::resolve(line->getValue());
		if (!IsBuildGame(file)||!QFileInfo(file).isFile())
			continue;
		QString folder=Folder(QFileInfo(file).absolutePath());
		bool have=false;
		foreach (const QString &known, folders)
			have=have||SameFolder(known, folder);
		if (!have)
			folders<<folder;
	}
	return folders;
}

QString ZDLRazeSetup::gamesFolderBeside(const QString &razeDir)
{
	QString uzdl=UzdlFolderAbove(Folder(razeDir));
	if (uzdl.isEmpty())
		return QString();
	QString grps=uzdl+"/grps";
	return QDir(grps).exists()?grps+"/*":QString();
}

QString ZDLRazeSetup::uzdlFolderFor(const QString &exe)
{
	QString above=UzdlFolderAbove(Folder(QFileInfo(exe).absolutePath()));
	return above.isEmpty()?Folder(ZDLPaths::appDir()):above;
}

void ZDLRazeSetup::prepareLaunch(ZDLConf *zconf, const QString &exe, const QString &launch)
{
	QString dir=uzdlFolderFor(exe);
#if defined(Q_OS_WIN)
	//Straight into the process environment, which is what a child started
	//with CreateProcess inherits.
	SetEnvironmentVariableW(L"UZDL_DIR", reinterpret_cast<const wchar_t*>(dir.utf16()));
#else
	qputenv("UZDL_DIR", dir.toUtf8());
#endif
	beforeLaunch(zconf, exe, launch);
}

QStringList ZDLRazeSetup::foldersFor(ZDLConf *zconf, const QString &exe)
{
	QStringList folders=gameFolders(zconf);
	QString beside=gamesFolderBeside(QFileInfo(exe).absolutePath());
	if (!beside.isEmpty())
		folders<<beside;
	return folders;
}

QString ZDLRazeSetup::settingsFile(const QString &exe, QString &note)
{
#if defined(Q_OS_WIN)
	QString exeDir=Folder(QFileInfo(exe).absolutePath());
	QString portable=exeDir+"/raze_portable.ini";

	if (QFileInfo(portable).isFile()) {
		note="Raze is portable already: its settings are in "+Native(portable)+".";
		return portable;
	}

	bool refused=UnderProgramFiles(exeDir);
	if (!refused) {
		QString source;
		foreach (const QString &candidate, PerUserFiles(exeDir)) {
			if (QFileInfo(candidate).isFile()) {
				source=candidate;
				break;
			}
		}

		bool made;
		if (source.isEmpty()) {
			QFile f(portable);
			made=f.open(QIODevice::WriteOnly);
			f.close();
		} else {
			made=QFile::copy(source, portable);
			if (made)
				QFile::setPermissions(portable, QFile::permissions(portable)|QFile::WriteOwner|QFile::WriteUser);
		}

		if (made) {
			note="Raze is portable now: its settings and saves stay beside raze.exe, in "+Native(portable)+(source.isEmpty()?
				", a new file Raze fills in with its defaults on its first start.":
				", with the settings copied from "+Native(source)+". Saves made before stay where they were.");
			return portable;
		}
	}

	//Not portable, then: the file Raze reads per user, made if need be.
	foreach (const QString &candidate, PerUserFiles(exeDir)) {
		if (QFileInfo(candidate).isFile()) {
			note=(refused?"Raze refuses to be portable under Program Files, so":"The Raze folder could not be written to, so")+QString(" its per-user settings in ")+Native(candidate)+" were used.";
			return candidate;
		}
	}
	QString perUser=PerUserFiles(exeDir)[1];
	QDir().mkpath(QFileInfo(perUser).absolutePath());
	QFile f(perUser);
	if (f.open(QIODevice::WriteOnly)) {
		f.close();
		note=(refused?"Raze refuses to be portable under Program Files, so":"The Raze folder could not be written to, so")+QString(" a per-user settings file was made at ")+Native(perUser)+".";
		return perUser;
	}
	note="No settings file for Raze could be written.";
	return QString();
#else
	//No portable mode on this platform: Raze reads one file per user.
	QString perUser=QDir::homePath()+"/.config/raze/raze.ini";
	QDir().mkpath(QFileInfo(perUser).absolutePath());
	if (!QFileInfo(perUser).isFile()) {
		QFile f(perUser);
		if (!f.open(QIODevice::WriteOnly)) {
			note="No settings file for Raze could be written at "+perUser+".";
			return QString();
		}
		f.close();
		note="Raze keeps its settings in "+perUser+", made now; Raze fills it in with its defaults on its first start.";
	} else {
		note="Raze keeps its settings in "+perUser+".";
	}
	Q_UNUSED(exe);
	return perUser;
#endif
}

QStringList ZDLRazeSetup::addSearchFolders(const QString &ini, const QString &razeDir, const QStringList &folders, bool *ok)
{
	QStringList added;
	if (ok)
		*ok=false;
	QString base=Folder(razeDir);

	QFile f(ini);
	if (!f.open(QIODevice::ReadOnly))
		return added;
	QString text=QString::fromUtf8(f.readAll());
	f.close();

	//Raze's own line endings are kept, whatever they are.
	QString eol=text.contains("\r\n")?"\r\n":"\n";
	bool endedWithNewline=text.isEmpty()||text.endsWith('\n');
	QStringList lines=text.split('\n');
	for (int i=0; i<lines.size(); i++) {
		if (lines[i].endsWith('\r'))
			lines[i].chop(1);
	}
	if (endedWithNewline&&!lines.isEmpty()&&lines.last().isEmpty())
		lines.removeLast();

	//The section, its Path lines, and where the next one goes.
	int header=-1;
	int end=lines.size();
	int lastPath=-1;
	QStringList listed;
	for (int i=0; i<lines.size(); i++) {
		QString line=lines[i].trimmed();
		if (header<0) {
			if (line.compare(SECTION, Qt::CaseInsensitive)==0)
				header=i;
			continue;
		}
		if (line.startsWith('[')) {
			end=i;
			break;
		}
		if (line.startsWith("Path=", Qt::CaseInsensitive)) {
			listed<<Meant(line.mid(5), base);
			lastPath=i;
		}
	}

	//fresh holds the folders as they are, for the comparisons; written,
	//line for line, how they go into the file.
	QStringList fresh;
	QStringList written;
	foreach (const QString &given, Condense(folders)) {
		QString folder=Folder(given);
		if (folder.isEmpty()||Covered(listed, folder)||Covered(fresh, folder))
			continue;
		fresh<<folder;
		written<<Written(folder, base);
	}

	//The variable line goes by its wording alone: nothing else reaches the
	//folder the way it does, so nothing else stands in for it.
	if (!gamesFolderBeside(base).isEmpty()) {
		bool present=false;
		for (int i=header<0?lines.size():header+1; i<end; i++) {
			QString line=lines[i].trimmed();
			if (line.startsWith("Path=", Qt::CaseInsensitive)&&QDir::fromNativeSeparators(line.mid(5).trimmed()).compare(VARIABLE_LINE, Qt::CaseInsensitive)==0)
				present=true;
		}
		if (!present)
			written<<VARIABLE_LINE;
	}

	if (header<0) {
		//A new section carries Raze's defaults, since Raze adds them only to
		//a config that has no section at all.
		if (!lines.isEmpty()&&!lines.last().trimmed().isEmpty())
			lines<<"";
		lines<<SECTION;
		for (int i=0; DEFAULT_PATHS[i]; i++)
			lines<<QString("Path=")+DEFAULT_PATHS[i];
		foreach (const QString &folder, written)
			lines<<"Path="+folder;
		lines<<"";
		added=written;
	} else if (!written.isEmpty()) {
		int at=lastPath>=0?lastPath+1:header+1;
		//Behind the last Path line, or the header, but never past the next
		//section, and ahead of the blank line that ends this one.
		at=qMin(at, end);
		foreach (const QString &folder, written) {
			lines.insert(at, "Path="+folder);
			at++;
		}
		added=written;
	}

	if (!added.isEmpty()) {
		QString out=lines.join(eol);
		if (endedWithNewline||header<0)
			out+=eol;
		QFile w(ini);
		if (!w.open(QIODevice::WriteOnly|QIODevice::Truncate))
			return QStringList();
		w.write(out.toUtf8());
		w.close();
	}

	if (ok)
		*ok=true;
	return added;
}

QString ZDLRazeSetup::setUp(ZDLConf *zconf, const QString &exe, const QStringList &extraFolders)
{
	QString note;
	QString ini=settingsFile(exe, note);
	if (ini.isEmpty())
		return note;

	QStringList folders=foldersFor(zconf, exe);
	foreach (const QString &extra, extraFolders)
		folders<<Folder(extra);

	bool ok;
	QStringList added=addSearchFolders(ini, QFileInfo(exe).absolutePath(), folders, &ok);
	QString report=note+"\n\n";
	if (!ok) {
		report+="Its search list could not be changed: "+Native(ini)+" could not be written.";
	} else if (!added.isEmpty()) {
		report+="Added to the folders Raze searches for games, as written to its ini (a relative path counts from Raze's own folder):";
		foreach (const QString &folder, added)
			report+="\n  "+folder;
	} else if (folders.isEmpty()) {
		report+="No Build game is in the IWAD list yet, and the uZDL folder Raze is in has no grps folder. The folder of each Build game is added to the folders Raze searches as it is launched.";
	} else {
		report+="The folders of the Build games in the IWAD list were in its search list already.";
	}
	return report;
}

void ZDLRazeSetup::offer(QWidget *parent, const QString &storedExe)
{
	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	QString exe=ZDLPaths::resolve(storedExe);
	if (!zconf||!isRaze(exe)||!Decision(zconf, exe).isEmpty())
		return;

	QString text="Raze was added as a source port:\n"+Native(exe)+"\n\nuZDL can set it up: ";
#if defined(Q_OS_WIN)
	text+="keep its settings and saves beside raze.exe rather than per user, and ";
#endif
	text+="add the folders of the Build games in the IWAD list to the folders Raze searches, so that an add-on finds its base game and the game asked for starts without Raze's own list. The search list is kept up to date at every launch.\n\nSet Raze up now?";

	if (QMessageBox::question(parent, ZDL_APP_NAME, text, QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes)!=QMessageBox::Yes) {
		Decide(zconf, exe, "skip");
		return;
	}

	//What the panes hold goes into the configuration first, so that an
	//IWAD added a moment ago counts.
	if (mw)
		mw->writeConfig();
	QString report=setUp(zconf, exe);
	Decide(zconf, exe, "done");
	QMessageBox::information(parent, ZDL_APP_NAME, report);
}

void ZDLRazeSetup::setUpNow(QWidget *parent, const QString &storedExe)
{
	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	QString exe=ZDLPaths::resolve(storedExe);
	if (!zconf||!isRaze(exe))
		return;

	if (mw)
		mw->writeConfig();
	QString report=setUp(zconf, exe);
	Decide(zconf, exe, "done");
	QMessageBox::information(parent, ZDL_APP_NAME, report);
}

QString ZDLRazeSetup::decision(ZDLConf *zconf, const QString &exe)
{
	return zconf?Decision(zconf, exe):QString();
}

void ZDLRazeSetup::forgetDecisions(ZDLConf *zconf)
{
	if (zconf)
		zconf->deleteSection("zdl.raze");
}

void ZDLRazeSetup::offerForNewPorts(QWidget *parent)
{
	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	if (!zconf)
		return;
	ZDLSection *section=zconf->getSection("zdl.ports");
	if (!section)
		return;

	QVector<ZDLLine*> files;
	section->getRegex("^p[0-9]+f$", files);
	QStringList stored;
	foreach (ZDLLine *line, files)
		stored<<line->getValue();
	//Collected first: a yes writes to the configuration the lines came from.
	foreach (const QString &file, stored)
		offer(parent, file);
}

void ZDLRazeSetup::beforeLaunch(ZDLConf *zconf, const QString &exe, const QString &launch)
{
	if (!zconf||!isRaze(exe)||Decision(zconf, exe)!="done")
		return;

	QString note;
	QString ini=settingsFile(exe, note);
	if (ini.isEmpty())
		return;

	QStringList folders=foldersFor(zconf, exe);
	QString iwad=SelectedIwad(zconf, launch);
	if (!iwad.isEmpty()&&QFileInfo(iwad).isFile())
		folders<<Folder(QFileInfo(iwad).absolutePath());
	addSearchFolders(ini, QFileInfo(exe).absolutePath(), folders);
}
