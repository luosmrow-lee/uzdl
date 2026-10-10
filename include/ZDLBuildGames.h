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

#ifndef _ZDLBUILDGAMES_H_
#define _ZDLBUILDGAMES_H_

#include <QString>
#include <QStringList>
#include <QList>
#include <QPair>

//The games of the Build engine: Duke Nukem 3D, Blood, Shadow Warrior,
//Redneck Rampage, PowerSlave and the rest, with their add-ons. They are
//told apart the way Raze, EDuke32 and the other ports tell them apart: by
//a file's exact size and CRC32, from the ports' own tables, and by any
//.grpinfo file beside it, as those ports read them too. A few add-ons come
//loose or in a zip and are told by the files they hold; a few total
//conversions no port's table names are told by their file names.
namespace ZDLBuildGames {
	struct Match {
		QString name;
		QString family;
		//Which rule settled it, in words, for a report.
		QString how;
		//The base game's name when this is an add-on that needs one.
		QString dependsOn;
		//The ports that run it.
		QString runsOn;
		bool valid() const { return !name.isEmpty(); }
	};

	//Whether files of this extension can be a Build game by size and
	//CRC32 or by name: GRP, RFF, DAT and SSI. The ones worth looking at
	//when searching for games.
	bool isCandidate(const QString &ext);

	//Whether a file of this size, in this folder, could be one by
	//checksum, which is the only reason to read it all through.
	bool sizeKnown(qint64 size, const QString &folder);

	//By exact size and CRC32: the ports' tables, then any .grpinfo in the
	//folder.
	Match bySizeCrc(qint64 size, quint32 crc, const QString &folder);

	//By what a zip archive holds, by Raze's rules for add-ons packed so.
	Match byContents(const QString &file);

	//By file name, and size where a name is shared: the conversions no
	//port's table has, and the scripts and INI files that pick a game.
	Match byName(const QString &file);

	//By the files in its folder, for the games that come as a folder of
	//loose files rather than one archive, told the way BuildGDX tells them:
	//Witchaven, Witchaven II, TekWar and the Seven Paladins. A folder is
	//told by what it holds, and a file by its folder when it is one of the
	//game's own files.
	Match byFolder(const QString &path);

	//An add-on of loose files in its base game's folder, as the GOG and
	//Steam releases ship Route 66 and Cryptic Passage, told by its script
	//with the add-on's files beside it: GAME66.CON with TILESA66.ART and
	//TILESB66.ART, CRYPTIC.INI with CPART07.AR_ and CPART15.AR_.
	Match byLooseFiles(const QString &file);

	//The base game of such an add-on: beside it, in the two folders above
	//it, where Fresh Supply keeps Cryptic Passage, or elsewhere in the
	//library; the one it was made for first, then any of its family.
	//Empty when there is none, or the file is no such add-on.
	QString looseAddonBase(const QString &file, const QStringList &library);

	//An add-on of Blood's told by its INI, which defines episodes of its
	//own, as NBlood tells the INIs it offers: "Blood add-on: Death Wish",
	//after its folder, or after its first episode in Blood's own folder.
	//Not Blood's own INI.
	Match byBloodIni(const QString &file);

	//The INI of a Blood add-on that comes as a folder: the one in it that
	//defines episodes; empty when there is none, or more than one.
	QString bloodModIni(const QString &folder);

	//Blood's game file for such an add-on: beside it, in the two folders
	//above it, or elsewhere in the library. Empty when there is none, or
	//the file is no such add-on.
	QString bloodAddonBase(const QString &file, const QStringList &library);

	//For a game that is a folder of loose files, the one file that stands
	//for it in the IWAD list: its TABLES.DAT. Empty for any other file.
	QString folderGameFile(const QString &path);

	//By the episodes its own scripts name, for a Duke Nukem 3D add-on in a
	//GRP or zip no table knows: "Duke Nukem 3D add-on: Plug and Pray", or
	//by its file's name when it keeps Duke's episodes first and adds its
	//own after them. Needs maps in it and an episode of its own.
	Match byScripts(const QString &file);

	//The same for a mod that comes as a folder of loose files over Duke
	//Nukem 3D, told by its main script, EDUKE.CON or else GAME.CON, in a
	//folder with maps and episodes of its own: "Duke Nukem 3D mod: Horror
	//Castle", or the folder's name when it keeps Duke's episodes first.
	Match byModScripts(const QString &file);

	//The main script of such a mod, for its folder; empty for a folder
	//that is no mod.
	QString modFolderScript(const QString &folder);

	//The base game for a script picked as the game, a mod's or an
	//add-on's, for EDuke32 or Rednukem: beside it, in the two folders
	//above it, or elsewhere in the library; Atomic first, or Redneck
	//Rampage for Rednukem. Empty when there is none.
	QString scriptBase(const QString &file, const QStringList &library, bool redneck);

	//BuildGDX's name for the game a file or folder belongs to, for its
	//-game switch; empty when BuildGDX has none for it.
	QString buildGdxGame(const QString &path);

	//How EDuke32 and Rednukem are best given a game file, as they pick
	//their game: one they know by size and CRC32, whose base game they
	//find, by its own name; any other loaded with -g over a base game of
	//theirs picked by name, since they start a game they know instead of
	//one they do not, with the add-on's main script where that is not
	//GAME.CON. The library is the files of the IWAD list, where a base game
	//kept in another folder is found.
	struct DukeLaunch {
		//Folders to search besides the file's own.
		QStringList folders;
		//For -gamegrp: a file name, which the port matches among the games
		//it found.
		QString gameGrp;
		//For -g: a file loaded over the game, with its full path.
		QString overlay;
		//For -x: the main script.
		QString script;
	};
	DukeLaunch forDukePorts(const QString &file, const QStringList &library, bool redneck);

	//How Raze is best given a game file. Raze takes its game with -iwad and
	//picks the games of its table by size and CRC32, and the add-on zips of
	//its own rules by what they hold. Anything else it needs loaded with
	//-file over a game it knows: a total conversion, Liquidator's folder,
	//an add-on repacked in a form no table has, a mod's folder with its
	//main script given by -con. For those a Duke game is
	//found, beside the file or elsewhere in the library, and the add-on's
	//main script given with -con where it is not GAME.CON.
	struct RazeLaunch {
		//For -iwad: the file itself, or the base game for the rest; empty
		//when Raze is to find the base game itself.
		QString iwad;
		//For -file, with the base game: the file or folder loaded over it.
		QString file;
		//For -con: the main script.
		QString script;
		//Raze's own switches for an add-on it sets up itself: -route66,
		//-cryptic.
		QStringList switches;
	};
	RazeLaunch forRaze(const QString &file, const QStringList &library);

	//The names of the files in a GRP or a zip, in capitals; none for any
	//other file.
	QStringList entryNames(const QString &file);

	//Whether an entry of the IWAD list is a Build engine game rather than a
	//Doom engine one, which decides the tab that lists it: by its kind of
	//file, the Doom engine's WADs and PK3s against GRPs, RFFs, scripts and
	//the rest; a zip by what it holds. Quick, and remembered per file.
	bool isBuildGameFile(const QString &path);

	//Whether a family is one of these games', and where it sorts after
	//the Doom ones.
	bool isFamily(const QString &family);
	int familyOrder(const QString &family);

	//The CRC32 of a whole file, as the ports take it; ok tells whether it
	//could be read through.
	quint32 crcOf(const QString &file, bool *ok=NULL);

	//The directory of a GRP archive: each file's name and size. Empty when
	//the file is no GRP.
	QList<QPair<QString, qint64> > grpEntries(const QString &file);
}

#endif
