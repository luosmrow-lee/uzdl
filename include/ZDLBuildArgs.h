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

#ifndef _ZDLBUILDARGS_H_
#define _ZDLBUILDARGS_H_

#include <QtWidgets>
#include <QObject>
#include "zdlconf.hpp"

//The command line of the Build engine ports that are not Raze: the
//EDuke32 family, EDuke32, Rednukem, NBlood, PCExhumed and VoidSW, and
//BuildGDX. None of them takes -iwad. To the EDuke32 family a game is the
//folder it is in, given with -j, and for the first three the file in it,
//given with -gamegrp, -g or -ini; the rest of the launch settings have
//their own words in these ports, and some have none. BuildGDX takes the
//game's folder and its name for the game. When the selected port is one
//of these, this list replaces the ZDoom one entirely; the port's own
//parameters and the extra ones still follow it as always.
class ZDLBuildArgs {
	public:
		enum Port { None, EDuke32, Rednukem, NBlood, PCExhumed, VoidSW, BuildGDX };

		//Which of the family an executable is, by its name, or by what it
		//says of itself on Windows; None for anything else.
		static Port portOf(const QString &exe);
		static bool handles(const QString &exe);

		//The structured part of the command line for the launch
		//configuration as it stands in section, the Doom Engine tab's unless
		//named: game, files, skill, monsters, map, save folder, config, and
		//a bigger cache for a big game or mod. Without the user's own
		//parameters.
		static QStringList arguments(ZDLConf *zconf, const QString &exe, const QString &section=QString());

		//Every file of the IWAD list, resolved: where a base game kept apart
		//from its add-ons is found.
		static QStringList libraryFiles(ZDLConf *zconf);

		//Whether a port plays the Build engine's games: the EDuke32 family,
		//BuildGDX, or Raze; what the Build Engine tab lists.
		static bool isBuildPort(const QString &exe);

		//The executable of the port listed by this name, and the file of the
		//IWAD listed by this name, resolved; empty for a name not listed.
		static QString portExecutable(ZDLConf *zconf, const QString &name);
		static QString iwadFile(ZDLConf *zconf, const QString &name);

		//Whether a port and a game, by their list names, are the Build
		//engine's: by the port when it is listed, by the game when only that
		//is; with neither listed, the answer is fallback.
		static bool isBuildChoice(ZDLConf *zconf, const QString &port, const QString &iwad, bool fallback);

		//Whether the preset or launch configuration in section belongs on
		//the Build Engine tab: by its port and game as above, and when it
		//names neither, by the mark a preset made on that tab carries.
		static bool isBuildLaunch(ZDLConf *zconf, const QString &section);
};

#endif
