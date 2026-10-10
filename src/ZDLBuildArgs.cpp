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

#include "ZDLBuildArgs.h"
#include "ZDLBuildGames.h"
#include "ZDLFileInfo.h"
#include "ZDLLaunchFiles.h"
#include "ZDLPaths.h"
#include "ZDLRazeSetup.h"

namespace {

bool Is(const QString &ext, const char *const *list)
{
	for (int i=0; list[i]; i++) {
		if (ext==list[i])
			return true;
	}
	return false;
}

//What these ports load as game data: GRP and the formats they read as one.
const char *const DATA_EXTS[]={"grp", "zip", "pk3", "pk4", "ssi", "dat", NULL};

//A value of the list entry by this name, zdl.iwads' iN or zdl.ports' pN:
//what is f for its file, p for a port's own parameters.
QString ListedValue(ZDLConf *zconf, const char *list, const char *prefix, const QString &name, char what)
{
	ZDLSection *section=zconf?zconf->getSection(list):NULL;
	if (!section||name.isEmpty())
		return QString();

	QVector<ZDLLine*> names;
	section->getRegex(QString("^%1[0-9]+n$").arg(prefix), names);
	foreach (ZDLLine *line, names) {
		if (line->getValue()!=name)
			continue;
		QString var=line->getVariable();
		var[var.size()-1]=QLatin1Char(what);
		QVector<ZDLLine*> values;
		section->getRegex("^"+var+"$", values);
		if (values.size()==1)
			return values[0]->getValue();
	}
	return QString();
}

//The entry of a list by its name, resolved.
QString ListedFile(ZDLConf *zconf, const char *list, const char *prefix, const QString &name)
{
	QString file=ListedValue(zconf, list, prefix, name, 'f');
	return file.isEmpty()?QString():ZDLPaths::resolve(file);
}

//The IWAD selected for the launch, resolved, or an empty string.
QString SelectedIwad(ZDLConf *zconf, const QString &launch)
{
	if (!zconf->hasValue(launch, "iwad"))
		return QString();
	return ListedFile(zconf, "zdl.iwads", "i", zconf->getValue(launch, "iwad"));
}

//Whether two folders are one, in the file system's own sense of case.
bool SameDir(const QString &a, const QString &b)
{
#if defined(Q_OS_WIN)
	return QDir::cleanPath(a).compare(QDir::cleanPath(b), Qt::CaseInsensitive)==0;
#else
	return QDir::cleanPath(a)==QDir::cleanPath(b);
#endif
}

//The enabled files of the launch list, resolved. A disabled one is stored
//under a name ending in d, which this does not match.
QStringList LaunchFiles(ZDLConf *zconf, const QString &launch)
{
	QStringList files;
	ZDLSection *section=zconf->getSection(launch);
	if (!section)
		return files;
	QVector<ZDLLine*> lines;
	section->getRegex("^file[0-9]+$", lines);
	foreach (ZDLLine *line, lines)
		files<<ZDLPaths::resolve(line->getValue());
	return files;
}

//The map field as an episode and a level: E1L2, E1M2, "1 2", or a level
//on its own, which is taken as being in the first episode. Zero when the
//field is something else, such as a map file's name.
bool EpisodeLevel(const QString &text, int &episode, int &level)
{
	QRegularExpressionMatch m=QRegularExpression("^E(\\d+)[LM](\\d+)$", QRegularExpression::CaseInsensitiveOption).match(text);
	if (!m.hasMatch())
		m=QRegularExpression("^(\\d+)\\s+(\\d+)$").match(text);
	if (m.hasMatch()) {
		episode=m.captured(1).toInt();
		level=m.captured(2).toInt();
		return episode>0&&level>0;
	}
	m=QRegularExpression("^(\\d+)$").match(text);
	if (m.hasMatch()) {
		episode=1;
		level=m.captured(1).toInt();
		return level>0;
	}
	return false;
}

//The cache the EDuke32 family keeps tile art and sounds in is 96 MB unless
//told otherwise, 192 MB in Rednukem, and a level that needs more stops the
//port with "CACHE SPACE ALL LOCKED UP!", as Savior of Babes, a 690 MB GRP,
//does on the default. A launch handing the port more game data than
//CACHE_FROM gets a cache of half that, rounded up to CACHE_STEP, from
//CACHE_MIN to CACHE_MAX. The ports keep the size in their config file and
//only ever raise it from there, so a bigger one the user set stays.
const qint64 MB=1024*1024;
const qint64 CACHE_FROM=256*MB;
const qint64 CACHE_MIN=256*MB;
const qint64 CACHE_MAX=512*MB;
const qint64 CACHE_STEP=64*MB;

//The cache for this much game data, in kB as -cachesize takes it; nought
//when the port's own will do.
qint64 CacheKB(qint64 data)
{
	if (data<=CACHE_FROM)
		return 0;
	qint64 size=(data/2+CACHE_STEP-1)/CACHE_STEP*CACHE_STEP;
	return qBound(CACHE_MIN, size, CACHE_MAX)/1024;
}

//Bytes in a folder and the folders in it, counted only until there are
//more than enough.
qint64 FolderBytes(const QString &folder, qint64 enough)
{
	qint64 total=0;
	QDirIterator it(folder, QDir::Files|QDir::Hidden|QDir::NoSymLinks, QDirIterator::Subdirectories);
	while (total<=enough&&it.hasNext()) {
		it.next();
		total+=it.fileInfo().size();
	}
	return total;
}

//Whether the user's own parameters for this launch, the port's, the ones
//always added and the tab's extra ones, already give this switch.
bool UserGives(ZDLConf *zconf, const QString &launch, const QString &option)
{
	QStringList texts;
	if (zconf->hasValue(launch, "port"))
		texts<<ListedValue(zconf, "zdl.ports", "p", zconf->getValue(launch, "port"), 'p');
	if (zconf->hasValue("zdl.general", "alwaysadd"))
		texts<<zconf->getValue("zdl.general", "alwaysadd");
	if (zconf->hasValue(launch, "extra"))
		texts<<zconf->getValue(launch, "extra");

	QRegularExpression given("(^|\\s)[-/]"+QRegularExpression::escape(option)+"(\\s|$)", QRegularExpression::CaseInsensitiveOption);
	foreach (const QString &text, texts) {
		if (given.match(text).hasMatch())
			return true;
	}
	return false;
}

//The archives in a folder, as EDuke32 loads those of its mod folder: GRP,
//zip, SSI, PK3 and PK4, and none from the folders below it.
QStringList ArchivesIn(const QString &folder)
{
	static const char *const kinds[]={"grp", "zip", "ssi", "pk3", "pk4", NULL};
	QStringList archives;
	foreach (const QFileInfo &fi, QDir(folder).entryInfoList(QDir::Files, QDir::Name|QDir::IgnoreCase)) {
		if (Is(fi.suffix().toLower(), kinds))
			archives<<fi.absoluteFilePath();
	}
	return archives;
}

//The file called name in the first of these folders that has one.
QString FileIn(const QStringList &folders, const QString &name)
{
	foreach (const QString &folder, folders) {
		QFileInfo fi(QDir(folder).filePath(name));
		if (fi.isFile())
			return fi.absoluteFilePath();
	}
	return QString();
}

}

ZDLBuildArgs::Port ZDLBuildArgs::portOf(const QString &exe)
{
	if (exe.isEmpty())
		return None;

	//The file's name first, which also holds on Linux; then what a renamed
	//executable says of itself, where there is a version block to ask.
	QString name=QFileInfo(exe).baseName().toLower();
#if defined(Q_OS_WIN)
	QString product=ZDLAppInfo(exe).GetProductName().toLower();
#else
	QString product;
#endif

	if (name.startsWith("eduke32")||product=="eduke32")
		return EDuke32;
	if (name.startsWith("rednukem")||product=="rednukem")
		return Rednukem;
	if (name.startsWith("nblood")||product=="nblood")
		return NBlood;
	if (name.startsWith("pcexhumed")||product=="pcexhumed")
		return PCExhumed;
	if (name.startsWith("voidsw")||product=="voidsw")
		return VoidSW;
	if (name.startsWith("buildgdx")||product=="buildgdx")
		return BuildGDX;
	return None;
}

bool ZDLBuildArgs::handles(const QString &exe)
{
	return portOf(exe)!=None;
}

QStringList ZDLBuildArgs::libraryFiles(ZDLConf *zconf)
{
	QStringList files;
	ZDLSection *section=zconf?zconf->getSection("zdl.iwads"):NULL;
	if (!section)
		return files;
	QVector<ZDLLine*> lines;
	section->getRegex("^i[0-9]+f$", lines);
	foreach (ZDLLine *line, lines)
		files<<ZDLPaths::resolve(line->getValue());
	return files;
}

bool ZDLBuildArgs::isBuildPort(const QString &exe)
{
	//Asked for every port each time a tab reads the configuration, and on
	//Windows the answer can mean reading the executable's version block.
	static QHash<QString, bool> known;
	QString key=QDir::cleanPath(exe);
	if (key.isEmpty())
		return false;
	QHash<QString, bool>::const_iterator found=known.constFind(key);
	if (found!=known.constEnd())
		return found.value();
	bool build=handles(exe)||ZDLRazeSetup::isRaze(exe);
	known.insert(key, build);
	return build;
}

QString ZDLBuildArgs::portExecutable(ZDLConf *zconf, const QString &name)
{
	return ListedFile(zconf, "zdl.ports", "p", name);
}

QString ZDLBuildArgs::iwadFile(ZDLConf *zconf, const QString &name)
{
	return ListedFile(zconf, "zdl.iwads", "i", name);
}

bool ZDLBuildArgs::isBuildChoice(ZDLConf *zconf, const QString &port, const QString &iwad, bool fallback)
{
	QString exe=portExecutable(zconf, port);
	if (!exe.isEmpty())
		return isBuildPort(exe);
	QString game=iwadFile(zconf, iwad);
	if (!game.isEmpty())
		return ZDLBuildGames::isBuildGameFile(game);
	return fallback;
}

bool ZDLBuildArgs::isBuildLaunch(ZDLConf *zconf, const QString &section)
{
	if (!zconf||section.isEmpty())
		return false;
	return isBuildChoice(zconf,
		zconf->hasValue(section, "port")?zconf->getValue(section, "port"):QString(),
		zconf->hasValue(section, "iwad")?zconf->getValue(section, "iwad"):QString(),
		ZDLLaunchFiles::markedBuild(zconf, section));
}

QStringList ZDLBuildArgs::arguments(ZDLConf *zconf, const QString &exe, const QString &launchSection)
{
	QStringList args;
	Port port=portOf(exe);
	if (port==None||!zconf)
		return args;

	const QString launch=launchSection.isEmpty()?QString(ZDLLaunchFiles::LAUNCH_SECTION):launchSection;
	bool duke=port==EDuke32||port==Rednukem;
	QString iwad=SelectedIwad(zconf, launch);

	//BuildGDX: the game's folder, and the game in it by BuildGDX's own name
	//for it, which starts it without the launcher window. Without a name,
	//the launcher opens on that folder. It has no switches for the rest;
	//add-ons and maps are picked in its menus.
	if (port==BuildGDX) {
		if (!iwad.isEmpty()) {
			//An add-on of loose files is played from its base game's folder,
			//where BuildGDX finds it and offers it in its menu; a mod of
			//loose files from Duke's, with the mod picked from User Content.
			QString game=iwad;
			QString base;
			if (ZDLBuildGames::byLooseFiles(iwad).valid())
				base=ZDLBuildGames::looseAddonBase(iwad, libraryFiles(zconf));
			else if (ZDLBuildGames::byModScripts(iwad).valid())
				base=ZDLBuildGames::scriptBase(iwad, libraryFiles(zconf), false);
			if (!base.isEmpty())
				game=base;
			QFileInfo fi(game);
			QString folder=QDir::toNativeSeparators(fi.isDir()?fi.absoluteFilePath():fi.absolutePath());
#if defined(Q_OS_WIN)
			//BuildGDX.exe passes its arguments on to Java joined by spaces with
			//their quotes dropped, so a folder with a space in its name reached
			//BuildGDX cut short at the space. Quotes that are part of the
			//argument itself survive the trip; a final backslash is doubled so
			//that it does not escape the closing one.
			if (exe.endsWith(".exe", Qt::CaseInsensitive)&&(folder.contains(' ')||folder.contains('\t'))) {
				if (folder.endsWith('\\'))
					folder+='\\';
				folder='"'+folder+'"';
			}
#endif
			args<<"-path"<<folder;
			QString name=ZDLBuildGames::buildGdxGame(game);
			if (!name.isEmpty())
				args<<"-game"<<name;
		}
		return args;
	}

	//The port's own startup window would ask what this command line
	//already says.
	args<<"-nosetup";

	//Whether a main script has been given, after which any other is a
	//module added to it.
	bool mainScript=false;

	//The game data handed to the port, which the size of its cache goes
	//by: files, and folders of loose files that are the game or a mod.
	QStringList dataFiles, dataFolders;

	//The game: its folder is searched, and the file in it named the way the
	//port wants, bare, since these ports match by name among what they
	//found. A folder on its own is just searched.
	if (!iwad.isEmpty()) {
		QFileInfo fi(iwad);
		//The base game under an add-on of loose files, or under a script
		//picked as the game, a mod's main script above all, since a script on
		//its own is no game: when it is kept elsewhere, Cryptic Passage in
		//Fresh Supply's addons folder or a mod unpacked anywhere, its folder
		//is searched too. It is named first: these ports search the folder
		//named last first, and the add-on's own files have to win over any
		//of the same name beside the base game. The Duke ports also get it
		//named as the game.
		QString base;
		if (!fi.isDir()&&!ZDLBuildGames::byFolder(iwad).valid()) {
			if (ZDLBuildGames::byLooseFiles(iwad).valid())
				base=ZDLBuildGames::looseAddonBase(iwad, libraryFiles(zconf));
			else if (duke&&fi.suffix().compare("con", Qt::CaseInsensitive)==0)
				base=ZDLBuildGames::scriptBase(iwad, libraryFiles(zconf), port==Rednukem);
			else if (port==NBlood&&fi.suffix().compare("ini", Qt::CaseInsensitive)==0)
				base=ZDLBuildGames::bloodAddonBase(iwad, libraryFiles(zconf));
		}
		if (!base.isEmpty()&&!SameDir(QFileInfo(base).absolutePath(), fi.absolutePath()))
			args<<"-j"<<QFileInfo(base).absolutePath();
		args<<"-j"<<(fi.isDir()?fi.absoluteFilePath():fi.absolutePath());
		if (fi.isDir())
			dataFolders<<fi.absoluteFilePath();
		if (!base.isEmpty())
			dataFiles<<base;
		if (!fi.isDir()) {
			QString file=fi.fileName();
			QString ext=fi.suffix().toLower();
			switch (port) {
			case EDuke32:
			case Rednukem:
				if (ZDLBuildGames::byFolder(iwad).valid()) {
					//A game that is a folder of loose files, Liquidator: the
					//folder is all, and the port finds its scripts in it.
					dataFolders<<fi.absolutePath();
				} else if (Is(ext, DATA_EXTS)) {
					//A game the port knows by name; any other over a base game
					//of its own, with its main script.
					ZDLBuildGames::DukeLaunch launch=ZDLBuildGames::forDukePorts(iwad, libraryFiles(zconf), port==Rednukem);
					foreach (const QString &folder, launch.folders)
						args<<"-j"<<folder;
					if (!launch.gameGrp.isEmpty()) {
						args<<"-gamegrp"<<launch.gameGrp;
						dataFiles<<FileIn(QStringList()<<fi.absolutePath()<<launch.folders, launch.gameGrp);
					}
					if (!launch.overlay.isEmpty()) {
						args<<"-g"<<launch.overlay;
						dataFiles<<launch.overlay;
					}
					if (!launch.script.isEmpty()) {
						args<<"-x"<<launch.script;
						mainScript=true;
					}
				} else if (ext=="con") {
					//A script over the base game: an add-on's, as Route 66's, or
					//a mod's main script, whose folder of loose files is the mod.
					if (!base.isEmpty())
						args<<"-gamegrp"<<QFileInfo(base).fileName();
					args<<"-x"<<file;
					mainScript=true;
					if (ZDLBuildGames::byModScripts(iwad).valid())
						dataFolders<<fi.absolutePath();
				}
				break;
			case NBlood:
				if (ext=="ini")
					args<<"-ini"<<file;
				else if (ext=="rff"&&file.compare("blood.rff", Qt::CaseInsensitive)!=0)
					args<<"-rff"<<file;
				dataFiles<<iwad;
				break;
			case PCExhumed:
				dataFiles<<iwad;
				break;
			case VoidSW:
				//The two add-ons are picked by number; their files sit beside SW.GRP.
				if (file.compare("wt.grp", Qt::CaseInsensitive)==0||file.compare("wantdest.grp", Qt::CaseInsensitive)==0)
					args<<"-addon1";
				else if (file.compare("td.grp", Qt::CaseInsensitive)==0||file.contains("twin", Qt::CaseInsensitive))
					args<<"-addon2";
				break;
			default:
				break;
			}
		}
	}

	//With no game picked, the first mod in the launch list that needs a
	//base game gets one named as the game, as it would picked as the game
	//itself, rather than whatever game the port would start by itself: a
	//mod that comes as a folder of loose files, by its main script, and an
	//add-on GRP or zip the port does not know, on its own or in a folder,
	//over the base game beside it or in the library, with its main script.
	QStringList launchFiles=LaunchFiles(zconf, launch);
	if (iwad.isEmpty()&&duke) {
		foreach (const QString &file, launchFiles) {
			QFileInfo entry(file);
			QStringList archives;
			if (entry.isDir()) {
				QString script=ZDLBuildGames::modFolderScript(file);
				QString base=script.isEmpty()?QString():ZDLBuildGames::scriptBase(script, libraryFiles(zconf), port==Rednukem);
				if (!base.isEmpty()) {
					args<<"-j"<<QFileInfo(base).absolutePath()<<"-gamegrp"<<QFileInfo(base).fileName();
					dataFiles<<base;
					break;
				}
				archives=ArchivesIn(file);
			} else if (Is(entry.suffix().toLower(), DATA_EXTS)) {
				archives<<entry.absoluteFilePath();
			}

			bool pinned=false;
			foreach (const QString &archive, archives) {
				ZDLBuildGames::DukeLaunch addon=ZDLBuildGames::forDukePorts(archive, libraryFiles(zconf), port==Rednukem);
				QString folder=QFileInfo(archive).absolutePath();
				if (addon.overlay.isEmpty()||addon.gameGrp.compare(QFileInfo(archive).fileName(), Qt::CaseInsensitive)==0)
					continue;
				//The base beside the add-on is found in the add-on's folder,
				//which a folder in the list has searched anyway.
				if (addon.folders.isEmpty()&&!entry.isDir())
					args<<"-j"<<folder;
				foreach (const QString &baseFolder, addon.folders)
					args<<"-j"<<baseFolder;
				args<<"-gamegrp"<<addon.gameGrp;
				dataFiles<<FileIn(QStringList()<<folder<<addon.folders, addon.gameGrp);
				if (!addon.script.isEmpty()) {
					args<<"-x"<<addon.script;
					mainScript=true;
				}
				pinned=true;
				break;
			}
			if (pinned)
				break;
		}
	}
	//For NBlood, an add-on of Blood's, its INI or a folder that holds one,
	//gets Blood's folder searched.
	if (iwad.isEmpty()&&port==NBlood) {
		foreach (const QString &file, launchFiles) {
			QString ini=QFileInfo(file).isDir()?ZDLBuildGames::bloodModIni(file):file;
			QString base=ini.isEmpty()?QString():ZDLBuildGames::bloodAddonBase(ini, libraryFiles(zconf));
			if (base.isEmpty())
				continue;
			args<<"-j"<<QFileInfo(base).absolutePath();
			dataFiles<<base;
			break;
		}
	}

	//The launch list, each file by what it is. What a port has no word for
	//is left out rather than sent to be refused.
	foreach (const QString &file, launchFiles) {
		QFileInfo entry(file);
		//A mod that comes as a folder of loose files, Zombie Nukem: searched
		//with -j, named after the game so that the port, which searches the
		//folder named last first, takes the mod's files over the game's,
		//with the GRPs and zips in it loaded with -g. Not as the port's mod
		//folder, -game_dir: given a full path, these ports do not load the
		//archives in it, and look every file up under that path first
		//through every archive loaded, which took Savior of Babes' scripts
		//from a fifth of a second to compile to nearly a minute.
		if (entry.isDir()) {
			QString folder=entry.absoluteFilePath();
			dataFolders<<folder;
			args<<"-j"<<folder;
			foreach (const QString &archive, ArchivesIn(folder)) {
				if (!dataFiles.contains(archive, Qt::CaseInsensitive))
					args<<"-g"<<archive;
			}
			//An add-on of Blood's in a folder of its own, Death Wish: its
			//episodes are in its INI, which NBlood plays only when told to.
			if (port==NBlood) {
				QString ini=ZDLBuildGames::bloodModIni(folder);
				if (!ini.isEmpty())
					args<<"-ini"<<QFileInfo(ini).fileName();
			}
			continue;
		}
		QString ext=entry.suffix().toLower();
		//The same for the INI itself: NBlood finds it only by its name, in
		//the folders it searches, so its own folder is searched too.
		if (ext=="ini"&&port==NBlood) {
			args<<"-j"<<entry.absolutePath()<<"-ini"<<entry.fileName();
			continue;
		}
		if (Is(ext, DATA_EXTS)) {
			args<<"-g"<<file;
			dataFiles<<file;
		} else if (ext=="con"&&duke) {
			//One main script; the others are modules compiled after it.
			args<<(mainScript?"-mx":"-x")<<file;
			mainScript=true;
		} else if (ext=="def")
			args<<"-mh"<<file;
		else if (ext=="map")
			args<<"-map"<<file;
		else if (ext=="rts"&&duke)
			args<<"-rts"<<file;
	}

	//Skill: four levels in Duke and Shadow Warrior, five in Blood, counted
	//from nought there; none to choose in Exhumed. The sixth entry of
	//uZDL's own list is no monsters.
	if (zconf->hasValue(launch, "skill")) {
		int skill=zconf->getValue(launch, "skill").toInt();
		if (skill>=1&&skill<=5) {
			switch (port) {
			case EDuke32:
			case Rednukem:
			case VoidSW:
				args<<"-s"+QString::number(qMin(skill, 4));
				break;
			case NBlood:
				args<<"-s"<<QString::number(skill-1);
				break;
			default:
				break;
			}
		} else if (skill==6) {
			switch (port) {
			case EDuke32:
			case Rednukem:
				args<<"-m";
				break;
			case NBlood:
				args<<"-nodudes";
				break;
			case PCExhumed:
				args<<"-nocreatures";
				break;
			case VoidSW:
				args<<"-monst";
				break;
			default:
				break;
			}
		}
	}

	if (duke&&zconf->hasValue(launch, "respawn")&&zconf->getValue(launch, "respawn")=="1")
		args<<"-t1";

	//The map field: an episode and level for the ports that count that
	//way, a level for those that count straight through, a map file for
	//any of them.
	if (zconf->hasValue(launch, "warp")) {
		QString warp=zconf->getValue(launch, "warp").trimmed();
		int episode, level;
		if (warp.endsWith(".map", Qt::CaseInsensitive)) {
			args<<"-map"<<warp;
		} else if (EpisodeLevel(warp, episode, level)) {
			switch (port) {
			case EDuke32:
			case Rednukem:
				args<<"-v"+QString::number(episode)<<"-l"+QString::number(level);
				break;
			case VoidSW:
				args<<"-level"+QString::number(level);
				break;
			case PCExhumed:
				args<<"-"+QString::number(level);
				break;
			default:
				break;
			}
		}
	}

	//A preset's own save folder is a mod folder to these ports, which keep
	//saves and settings in it; its config file goes by -cfg.
	if (zconf->hasValue(launch, "savedir")&&(duke||port==NBlood))
		args<<"-game_dir"<<ZDLPaths::resolve(zconf->getValue(launch, "savedir"));
	if (zconf->hasValue(launch, "config")&&(duke||port==NBlood))
		args<<"-cfg"<<ZDLPaths::resolve(zconf->getValue(launch, "config"));

	//A bigger cache for a big game or mod, unless the user's own parameters
	//set one; VoidSW has no switch for it. See CACHE_FROM.
	if (port!=VoidSW&&!UserGives(zconf, launch, "cachesize")) {
		const qint64 enough=2*CACHE_MAX;
		qint64 data=0;
		dataFiles.removeAll(QString());
		dataFiles.removeDuplicates();
		foreach (const QString &file, dataFiles)
			data+=QFileInfo(file).size();
		foreach (const QString &folder, dataFolders) {
			if (data>enough)
				break;
			data+=FolderBytes(folder, enough-data);
		}
		qint64 kb=CacheKB(data);
		if (kb)
			args<<"-cachesize"<<QString::number(kb);
	}

	return args;
}
