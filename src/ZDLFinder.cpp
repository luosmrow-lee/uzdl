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

#include <QtCore>
#include <algorithm>
#include "ZDLFinder.h"
#include "ZDLFileInfo.h"
#include "ZDLPaths.h"
#include "ZDLConfigurationManager.h"

namespace {

//A folder to look through, and how far down.
struct Place {
	QString dir;
	QString where;
	int depth;	//1 is the folder's own files only
};

//A file the walk turned up, with the place that led to it.
struct Hit {
	QString path;
	QString where;
};

//Where the Steam releases keep their WADs under steamapps/common, as the
//ZDoom ports list them: the DOS games, the 2024 and 2025 KEX re-releases,
//the Unity ports and Doom 3 BFG's copies.
const char *const STEAM_GAME_FOLDERS[]={
	"Ultimate Doom/base",
	"Ultimate Doom/base/doom2",
	"Ultimate Doom/base/tnt",
	"Ultimate Doom/base/plutonia",
	"Ultimate Doom/rerelease",
	"Ultimate Doom/rerelease/DOOM_Data/StreamingAssets",
	"Doom 2/base",
	"Doom 2/finaldoombase",
	"Doom 2/rerelease/DOOM II_Data/StreamingAssets",
	"Final Doom/base",
	"Master Levels of Doom/doom2",
	"Heretic Shadow of the Serpent Riders/base",
	"Hexen/base",
	"Hexen Deathkings of the Dark Citadel/base",
	"Heretic + Hexen/dos/base/heretic",
	"Heretic + Hexen/dos/base/hexen",
	"Heretic + Hexen/dos/base/hexendk",
	"DOOM 3 BFG Edition/base/wads",
	"Strife",
	NULL
};

//Where the Steam releases of the Build games keep their files under
//steamapps/common, as Raze, EDuke32 and NBlood look for them.
const char *const STEAM_BUILD_FOLDERS[]={
	"Duke Nukem 3D/gameroot",
	"Duke Nukem 3D/gameroot/addons/dc",
	"Duke Nukem 3D/gameroot/addons/nw",
	"Duke Nukem 3D/gameroot/addons/vacation",
	"Nam/NAM",
	"World War II GI/WW2GI",
	"Blood",
	"Blood/addons/Cryptic Passage",
	"One Unit Whole Blood",
	"Shadow Warrior Classic/gameroot",
	"Shadow Warrior Classic/gameroot/addons",
	"Shadow Warrior Original/gameroot",
	"Redneck Rampage/Redneck",
	"Redneck Rampage Rides Again/AGAIN",
	"Redneck Deer Huntin'/HUNTIN",
	"PowerslaveCE/PWRSLAVE",
	NULL
};

#if defined(Q_OS_WIN)
//The launcher's games, by the folders GZDoom lists under its install path.
const char *const BETHESDA_GAME_FOLDERS[]={
	"DOOM_Classic_2019/base",
	"DOOM_Classic_2019/rerelease/DOOM_Data/StreamingAssets",
	"DOOM_II_Classic_2019/base",
	"DOOM_II_Classic_2019/rerelease/DOOM II_Data/StreamingAssets",
	"DOOM 3 BFG Edition/base/wads",
	"Heretic Shadow of the Serpent Riders/base",
	"Hexen/base",
	"Hexen Deathkings of the Dark Citadel/base",
	NULL
};

//A registry value by name, ignoring case: the stores are not consistent
//about it and QSettings takes the name as given.
QString RegistryValue(const QString &key, const QString &name)
{
	QSettings reg(key, QSettings::NativeFormat);
	foreach (const QString &child, reg.childKeys()) {
		if (!child.compare(name, Qt::CaseInsensitive))
			return reg.value(child).toString();
	}
	return QString();
}
#endif

//Adds a folder that exists, once. Symbolic links are resolved first so the
//same folder under two names is still one place.
void AddPlace(QList<Place> &places, const QString &dir, const QString &where, int depth)
{
	if (dir.trimmed().isEmpty())
		return;

	QString clean=QFileInfo(QDir::cleanPath(QString(dir).replace('\\', '/'))).canonicalFilePath();
	if (clean.isEmpty()||!QFileInfo(clean).isDir())
		return;

	foreach (const Place &place, places) {
		if (place.dir==clean)
			return;
	}

	Place place;
	place.dir=clean;
	place.where=where;
	place.depth=depth;
	places<<place;
}

//uZDL's own folders, then beside everything already configured: ports and
//IWADs tend to share a folder, and a portable install keeps its own copies.
void AddOwnPlaces(QList<Place> &places)
{
	AddPlace(places, ZDLPaths::iwadsDir(), "uZDL folder", 2);
	AddPlace(places, ZDLPaths::sourcePortsDir(), "uZDL folder", 3);
	AddPlace(places, ZDLPaths::appDir(), "uZDL folder", 1);
}

void AddBesideConfigured(QList<Place> &places, const char *sectionName, const char *pattern, const QString &where)
{
	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	if (!zconf)
		return;

	ZDLSection *section=zconf->getSection(sectionName);
	if (!section)
		return;

	QVector<ZDLLine*> lines;
	section->getRegex(pattern, lines);
	foreach (ZDLLine *line, lines)
		AddPlace(places, QFileInfo(ZDLPaths::resolve(line->getValue())).absolutePath(), where, 1);
}

//DOOMWADDIR and DOOMWADPATH, the same variables every port honours.
void AddEnvPlaces(QList<Place> &places)
{
	AddPlace(places, qEnvironmentVariable("DOOMWADDIR"), "DOOMWADDIR", 1);

#if defined(Q_OS_WIN)
	const QChar separator=';';
#else
	const QChar separator=':';
#endif
	foreach (const QString &dir, qEnvironmentVariable("DOOMWADPATH").split(separator, Qt::SkipEmptyParts))
		AddPlace(places, dir, "DOOMWADPATH", 1);
}

//Where Steam itself is. Windows says in the registry; on Linux it is one
//of a few folders, the Flatpak and Snap installs included.
QStringList SteamRoots()
{
	QStringList roots;

#if defined(Q_OS_WIN)
	roots<<RegistryValue("HKEY_CURRENT_USER\\Software\\Valve\\Steam", "SteamPath");
	roots<<RegistryValue("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Valve\\Steam", "InstallPath");
	roots<<RegistryValue("HKEY_LOCAL_MACHINE\\SOFTWARE\\Valve\\Steam", "InstallPath");
#else
	QString home=QDir::homePath();
	roots<<home+"/.local/share/Steam"
		<<home+"/.steam/steam"
		<<home+"/.steam/root"
		<<home+"/.var/app/com.valvesoftware.Steam/.local/share/Steam"
		<<home+"/snap/steam/common/.local/share/Steam";
#endif

	QStringList existing;
	foreach (const QString &root, roots) {
		if (root.trimmed().isEmpty())
			continue;
		QString real=QFileInfo(QDir::cleanPath(QString(root).replace('\\', '/'))).canonicalFilePath();
		if (!real.isEmpty()&&QFileInfo(real).isDir()&&!existing.contains(real))
			existing<<real;
	}

	return existing;
}

//Every library Steam installs games into: the root itself and each path in
//config/libraryfolders.vdf, in both layouts that file has had - a "path"
//key per library today, a numbered key with the path as its value before.
QStringList SteamLibraries(const QString &root)
{
	QStringList libraries;

	QFile vdf(root+"/config/libraryfolders.vdf");
	if (vdf.open(QIODevice::ReadOnly|QIODevice::Text)) {
		QString text=QString::fromUtf8(vdf.readAll());
		QRegularExpression entry("^\\s*\"(?:path|\\d+)\"\\s+\"([^\"]+)\"", QRegularExpression::MultilineOption);
		QRegularExpressionMatchIterator it=entry.globalMatch(text);
		while (it.hasNext()) {
			//Paths in the file are written with doubled backslashes.
			QString library=it.next().captured(1);
			library.replace("\\\\", "/");
			library.replace('\\', '/');
			libraries<<library;
		}
	}

	libraries<<root;
	return libraries;
}

void AddSteamPlaces(QList<Place> &places)
{
	foreach (const QString &root, SteamRoots()) {
		foreach (const QString &library, SteamLibraries(root)) {
			for (int i=0; STEAM_GAME_FOLDERS[i]; i++)
				AddPlace(places, library+"/steamapps/common/"+STEAM_GAME_FOLDERS[i], "Steam", 1);
		}
	}
}

//GOG registers each game under its own id, and keeps the WADs one or two
//folders down; a few levels cover every layout it has used. On Linux
//there is no registry, only the folders the usual installers make.
void AddGogPlaces(QList<Place> &places)
{
#if defined(Q_OS_WIN)
	struct GogGame {
		const char *id;
		const char *sub;
	};
	static const GogGame games[]={
		{"1435827232", ""},								//The Ultimate Doom
		{"2015545325", "DOOM_Data/StreamingAssets"},		//Doom I Enhanced
		{"1435848814", "doom2"},							//Doom II
		{"1426071866", "DOOM II_Data/StreamingAssets"},	//Doom II Enhanced
		{"1413291984", ""},								//DOOM + DOOM II
		{"1435848742", "TNT"},								//Final Doom
		{"1435848742", "Plutonia"},
		{"1135892318", "base/wads"},						//Doom 3: BFG Edition
		{"1432899949", ""},								//Strife: Veteran Edition
		{"1290366318", ""},								//Heretic: Shadow of the Serpent Riders
		{"1247951670", ""},								//Hexen: Beyond Heretic
		{"1983497091", ""},								//Hexen: Deathkings of the Dark Citadel
		{NULL, NULL}
	};

	for (int i=0; games[i].id; i++) {
		QString path=RegistryValue(QString("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\GOG.com\\Games\\")+games[i].id, "path");
		if (path.isEmpty())
			path=RegistryValue(QString("HKEY_LOCAL_MACHINE\\SOFTWARE\\GOG.com\\Games\\")+games[i].id, "path");
		if (!path.isEmpty())
			AddPlace(places, path+"/"+games[i].sub, "GOG", 3);
	}
#else
	QString home=QDir::homePath();
	AddPlace(places, home+"/GOG Games", "GOG", 3);
	AddPlace(places, home+"/Games/Heroic", "GOG", 3);
#endif
}

//The Build games: Steam's folders, and on Windows the registry entries
//Raze, EDuke32 and NBlood read, which also find a release whose folder
//is named otherwise; GOG under its ids and the older keys, and ZOOM.
void AddBuildPlaces(QList<Place> &places)
{
	foreach (const QString &root, SteamRoots()) {
		foreach (const QString &library, SteamLibraries(root)) {
			for (int i=0; STEAM_BUILD_FOLDERS[i]; i++)
				AddPlace(places, library+"/steamapps/common/"+STEAM_BUILD_FOLDERS[i], "Steam", 1);
		}
	}

#if defined(Q_OS_WIN)
	struct Install {
		const char *key;
		const char *value;
		const char *sub;
	};
	static const Install installs[]={
		{"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 434050", "InstallLocation", ""},	//Duke Nukem 3D: 20th Anniversary World Tour
		{"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 225140", "InstallLocation", "gameroot"},	//Megaton Edition
		{"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 225140", "InstallLocation", "gameroot/addons/dc"},
		{"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 225140", "InstallLocation", "gameroot/addons/nw"},
		{"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 225140", "InstallLocation", "gameroot/addons/vacation"},
		{"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 359850", "InstallLocation", "Duke Nukem 3D"},	//3D Realms Anthology
		{"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 329650", "InstallLocation", "NAM"},
		{"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 376750", "InstallLocation", "WW2GI"},
		{"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 299030", "InstallLocation", ""},	//Blood: One Unit Whole Blood
		{"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 1010750", "InstallLocation", ""},	//Blood: Fresh Supply
		{"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 1010750", "InstallLocation", "addons/Cryptic Passage"},
		{"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 225160", "InstallLocation", "gameroot"},	//Shadow Warrior Classic Redux
		{"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 225160", "InstallLocation", "gameroot/addons"},
		{"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 238070", "InstallLocation", "gameroot"},	//Shadow Warrior Classic
		{"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 358400", "InstallLocation", "Shadow Warrior"},
		{"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 1260020", "InstallLocation", "PWRSLAVE"},	//PowerSlave Exhumed
		{"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 565550", "InstallLocation", "Redneck"},
		{"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 580940", "InstallLocation", "AGAIN"},
		{"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 580930", "InstallLocation", "HUNTIN"},
		{"HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\ZOOM PLATFORM\\Duke Nukem 3D - Atomic Edition", "InstallPath", ""},
		{"HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\ZOOM PLATFORM\\Duke Nukem 3D - Atomic Edition", "InstallPath", "AddOns"},
		{NULL, NULL, NULL}
	};
	for (int i=0; installs[i].key; i++) {
		QString path=RegistryValue(installs[i].key, installs[i].value);
		if (!path.isEmpty())
			AddPlace(places, path+"/"+installs[i].sub, QString(installs[i].key).contains("ZOOM")?"ZOOM":"Steam", 1);
	}

	static const char *const gog[]={
		"Games\\1207658730",	//Duke Nukem 3D: Atomic Edition
		"Games\\1207658856",	//Blood: One Unit Whole Blood
		"Games\\1374469660",	//Blood: Fresh Supply
		"Games\\1207659142",	//Shadow Warrior Classic Complete
		"Games\\1618073558",	//Shadow Warrior Classic Redux
		"Games\\1575726518",	//NAM
		"Games\\1625778777",	//WWII GI
		"Games\\2132611980",	//PowerSlave
		"Games\\1207658674",	//Redneck Rampage Collection
		"GOGDUKE3D",			//The older installers' own keys
		"GOGONEUNITONEBLOOD",
		"GOGSHADOWARRIOR",
		"GOGREDNECKRAMPAGE",
		"GOGCREDNECKRIDESAGAIN",
		NULL
	};
	for (int i=0; gog[i]; i++) {
		QString value=QString(gog[i]).startsWith("Games")?"path":"PATH";
		QString path=RegistryValue(QString("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\GOG.com\\")+gog[i], value);
		if (path.isEmpty())
			path=RegistryValue(QString("HKEY_LOCAL_MACHINE\\SOFTWARE\\GOG.com\\")+gog[i], value);
		if (!path.isEmpty())
			AddPlace(places, path, "GOG", 3);
	}
#endif
}

void AddBethesdaPlaces(QList<Place> &places)
{
#if defined(Q_OS_WIN)
	QString root=RegistryValue("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Bethesda Softworks\\Bethesda.net", "installLocation");
	if (root.isEmpty())
		root=RegistryValue("HKEY_LOCAL_MACHINE\\SOFTWARE\\Bethesda Softworks\\Bethesda.net", "installLocation");
	if (root.isEmpty())
		return;

	for (int i=0; BETHESDA_GAME_FOLDERS[i]; i++)
		AddPlace(places, root+"/games/"+BETHESDA_GAME_FOLDERS[i], "Bethesda.net", 1);
#else
	Q_UNUSED(places);
#endif
}

//The folders the ports themselves search by default.
void AddIwadDefaultPlaces(QList<Place> &places)
{
#if defined(Q_OS_WIN)
	AddPlace(places, QDir::homePath(), "home folder", 1);
#else
	QString home=QDir::homePath();
	AddPlace(places, home+"/.config/gzdoom", "GZDoom's folder", 1);
	AddPlace(places, home+"/.config/uzdoom", "UZDoom's folder", 1);
	AddPlace(places, home+"/.var/app/org.zdoom.GZDoom/.config/gzdoom", "GZDoom Flatpak", 1);
	AddPlace(places, home+"/.local/share/games/doom", "share folder", 1);
	AddPlace(places, "/usr/share/games/doom", "share folder", 1);
	AddPlace(places, "/usr/share/doom", "share folder", 1);
	AddPlace(places, "/usr/local/share/games/doom", "share folder", 1);
	AddPlace(places, "/usr/local/share/doom", "share folder", 1);
#endif
}

//Where ports get installed or unpacked.
void AddPortPlaces(QList<Place> &places)
{
#if defined(Q_OS_WIN)
	AddPlace(places, qEnvironmentVariable("ProgramFiles"), "Program Files", 2);
	AddPlace(places, qEnvironmentVariable("ProgramFiles(x86)"), "Program Files", 2);
	AddPlace(places, qEnvironmentVariable("LOCALAPPDATA")+"/Programs", "installed programs", 2);
#else
	QString home=QDir::homePath();
	AddPlace(places, "/usr/games", "system", 1);
	AddPlace(places, "/usr/bin", "system", 1);
	AddPlace(places, "/usr/local/games", "system", 1);
	AddPlace(places, "/usr/local/bin", "system", 1);
	AddPlace(places, "/opt", "system", 2);
	AddPlace(places, "/var/lib/flatpak/exports/bin", "Flatpak", 1);
	AddPlace(places, home+"/.local/share/flatpak/exports/bin", "Flatpak", 1);
	AddPlace(places, home+"/.local/bin", "home folder", 1);
	AddPlace(places, home+"/Applications", "home folder", 2);
	AddPlace(places, home+"/Games", "home folder", 3);
#endif
	AddPlace(places, QStandardPaths::writableLocation(QStandardPaths::DesktopLocation), "Desktop", 2);
	AddPlace(places, QStandardPaths::writableLocation(QStandardPaths::DownloadLocation), "Downloads", 2);
	AddPlace(places, QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation), "Documents", 2);
}

//The files under a folder, to the given depth. Folders reached through a
//link are not entered, and a budget keeps a huge tree from taking the
//session with it.
void Walk(const QString &dir, int depth, const QString &where, QList<Hit> &hits, int &budget)
{
	if (depth<1||budget<=0)
		return;

	foreach (const QFileInfo &entry, QDir(dir).entryInfoList(QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot)) {
		if (--budget<=0)
			return;

		if (entry.isDir()) {
			if (depth>1&&!entry.isSymLink())
				Walk(entry.filePath(), depth-1, where, hits, budget);
		} else {
			Hit hit;
			hit.path=entry.filePath();
			hit.where=where;
			hits<<hit;
		}
	}
}

int FamilyOrder(const QString &family)
{
	static const char *const order[]={"Doom", "Heretic", "Hexen", "Strife", "Chex", NULL};
	for (int i=0; order[i]; i++) {
		if (family==order[i])
			return i;
	}
	//The Build games after the Doom ones, in their own order.
	return ZDLBuildGames::familyOrder(family);
}

}

QList<ZDLFinder::Found> ZDLFinder::findIwads(const QStringList &extraFolders)
{
	QList<Place> places;

	//In order of preference, since the first copy of an IWAD found is the
	//one offered: what the user pointed at, then uZDL's own, then the rest.
	foreach (const QString &dir, extraFolders)
		AddPlace(places, dir, QDir::toNativeSeparators(dir), 5);
	AddOwnPlaces(places);
	AddBesideConfigured(places, "zdl.iwads", "^i[0-9]+f$", "beside an IWAD");
	AddBesideConfigured(places, "zdl.ports", "^p[0-9]+f$", "beside a source port");
	AddEnvPlaces(places);
	AddSteamPlaces(places);
	AddGogPlaces(places);
	AddBethesdaPlaces(places);
	AddBuildPlaces(places);
	AddIwadDefaultPlaces(places);

	QList<Found> found;
	int budget=20000;
	//The games that are a folder of loose files: by folder, the file that
	//stands for each, so that a folder is offered once.
	QHash<QString, QString> standIns;
	QSet<QString> offeredFolders;

	foreach (const Place &place, places) {
		QList<Hit> hits;
		Walk(place.dir, place.depth, place.where, hits, budget);

		foreach (const Hit &hit, hits) {
			QString ext=QFileInfo(hit.path).suffix().toLower();
			if (ext!="wad"&&ext!="iwad"&&ext!="pk3"&&ext!="ipk3"&&ext!="pk7"&&ext!="ipk7"&&ext!="zip"&&ext!="con"&&ext!="ini"&&!ZDLBuildGames::isCandidate(ext))
				continue;

			QString path=hit.path;
			bool folderGame=false;
			if (ZDLBuildGames::isCandidate(ext)) {
				QString folder=QFileInfo(path).absolutePath();
				if (!standIns.contains(folder))
					standIns.insert(folder, ZDLBuildGames::folderGameFile(path));
				QString standIn=standIns.value(folder);
				if (!standIn.isEmpty()) {
					if (offeredFolders.contains(folder))
						continue;
					offeredFolders.insert(folder);
					path=standIn;
					folderGame=true;
				}
			}

			//The lump check reads only the file's directory; the hash that
			//gives the exact release, and tells copies apart, reads all of
			//it, so only files that pass the first get the second.
			if (!ZDLIwadInfo::LooksLikeIwad(path))
				continue;

			ZDLIwadInfo info(path);
			QString name=info.Identify();
			if (name.isEmpty())
				continue;

			//Copies of one file are one IWAD; the games told by their folder
			//are not, since such games share files like TABLES.DAT.
			QByteArray digest=folderGame?QCryptographicHash::hash(QFileInfo(path).absolutePath().toUtf8(), QCryptographicHash::Md5):info.Digest();
			bool duplicate=false;
			for (int i=0; i<found.size()&&!digest.isEmpty(); i++) {
				if (found[i].digest==digest) {
					found[i].also<<path;
					duplicate=true;
					break;
				}
			}
			if (duplicate)
				continue;

			Found entry;
			entry.path=path;
			entry.name=name;
			entry.where=hit.where;
			entry.family=info.Family();
			entry.digest=digest;
			found<<entry;
		}
	}

	std::stable_sort(found.begin(), found.end(), [](const Found &l, const Found &r) {
		int lo=FamilyOrder(l.family), ro=FamilyOrder(r.family);
		if (lo!=ro)
			return lo<ro;
		return l.name.compare(r.name, Qt::CaseInsensitive)<0;
	});

	return found;
}

QList<ZDLFinder::Found> ZDLFinder::findPorts(const QStringList &extraFolders)
{
	QList<Place> places;

	foreach (const QString &dir, extraFolders)
		AddPlace(places, dir, QDir::toNativeSeparators(dir), 5);
	AddOwnPlaces(places);
	AddBesideConfigured(places, "zdl.ports", "^p[0-9]+f$", "beside a source port");
	AddBesideConfigured(places, "zdl.iwads", "^i[0-9]+f$", "beside an IWAD");
	AddEnvPlaces(places);
	AddPortPlaces(places);

	QList<Found> found;
	QStringList seen;
	int budget=20000;

	foreach (const Place &place, places) {
		QList<Hit> hits;
		Walk(place.dir, place.depth, place.where, hits, budget);

		foreach (const Hit &hit, hits) {
			QFileInfo fi(hit.path);
#if defined(Q_OS_WIN)
			if (fi.suffix().compare("exe", Qt::CaseInsensitive))
				continue;
#else
			if (!fi.isExecutable())
				continue;
#endif
			//Installers, servers and the like sit beside the game and carry
			//its name; none of them is what a launch wants.
			QString base=fi.fileName().toLower();
			if (base.contains("setup")||base.contains("unins")||base.contains("serv")||base.contains("dedicated")||base.contains("updater")||base.contains("crash"))
				continue;

			ZDLAppInfo app(hit.path);
			if (!app.IsKnownPort()||app.GetProductName().contains("Server", Qt::CaseInsensitive))
				continue;

			QString real=fi.canonicalFilePath();
			if (real.isEmpty()||seen.contains(real))
				continue;
			seen<<real;

			Found entry;
			entry.path=hit.path;
			entry.name=app.GetFileDescription();
			entry.version=app.GetVersion();
			entry.where=hit.where;
			found<<entry;
		}
	}

	std::stable_sort(found.begin(), found.end(), [](const Found &l, const Found &r) {
		return l.name.compare(r.name, Qt::CaseInsensitive)<0;
	});

	return found;
}
