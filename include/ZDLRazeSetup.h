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

#ifndef _ZDLRAZESETUP_H_
#define _ZDLRAZESETUP_H_

#include <QtWidgets>
#include <QObject>
#include "zdlconf.hpp"

//Raze, the Build engine port of the ZDoom family, only plays an add-on
//when it also finds the base game in one of the folders it searches, and
//on Windows keeps its settings and saves per user unless a file named
//raze_portable.ini sits beside it. This sets a Raze up for uZDL: portable
//where that is possible, and the folders of the Build games in the IWAD
//list added to the search list in its ini, which is kept current at every
//launch. Nothing else in Raze's ini is touched.
class ZDLRazeSetup {
	public:
		//Whether this executable is Raze: by what it says of itself on
		//Windows, by its name elsewhere.
		static bool isRaze(const QString &exe);

		//Offers the setup for one port just added, unless that executable
		//was asked about before. A yes does it and reports; a no is kept.
		static void offer(QWidget *parent, const QString &storedExe);

		//The same for every Raze in the port list not asked about yet: the
		//ports added by other means, and those from before this existed.
		static void offerForNewPorts(QWidget *parent);

		//The setup done now, asked for by hand, whatever was decided before;
		//from then on it counts as done.
		static void setUpNow(QWidget *parent, const QString &storedExe);

		//What was answered for this executable: "done", "skip", or an empty
		//string when it was never asked.
		static QString decision(ZDLConf *zconf, const QString &exe);

		//Forgets every answer, so that each Raze is offered the setup again.
		static void forgetDecisions(ZDLConf *zconf);

		//Before a launch of a Raze that was set up: the search list brought
		//up to date, the game selected in the launch section's folder
		//included, the Doom Engine tab's unless named. Silent, and never in
		//the way of the launch.
		static void beforeLaunch(ZDLConf *zconf, const QString &exe, const QString &launch=QString());

		//Before any port is started: UZDL_DIR put in the environment the
		//port inherits, as the uZDL folder the port lives in, or the one
		//running when it lives in none, and then the above. Raze expands
		//$UZDL_DIR in its search list, which is what makes the line
		//$UZDL_DIR/grps/* hold at any depth and after any move.
		static void prepareLaunch(ZDLConf *zconf, const QString &exe, const QString &launch=QString());

		//The uZDL folder for a port: the nearest one above it, or the one
		//running.
		static QString uzdlFolderFor(const QString &exe);

		//The folders of the Build games in the IWAD list.
		static QStringList gameFolders(ZDLConf *zconf);

		//The grps folder of the uZDL folder Raze lives in, with /* after it,
		//when there is one: where Build games are kept beside uZDL, listed
		//or not yet. Found from Raze's own place, so it does not depend on
		//which configuration uZDL runs with. Empty when there is none.
		static QString gamesFolderBeside(const QString &razeDir);

		//Everything the search list should hold for this executable: the
		//two of the above together.
		static QStringList foldersFor(ZDLConf *zconf, const QString &exe);

		//The ini uZDL writes to for this executable: on Windows the portable
		//one beside it, made when absent with the settings Raze used until
		//now, else the per-user file. Empty when none can be had; note says
		//what was done or why not.
		static QString settingsFile(const QString &exe, QString &note);

		//Adds folders to [GameSearch.Directories] in the ini, creating the
		//section with Raze's own defaults when it is absent. A folder already
		//listed, or covered by a parent listed with /*, is left alone. Game
		//folders that share a parent go in as that parent with /*, which
		//reaches the games put there later as well. A folder in the same
		//uZDL folder as Raze, this one or another, or inside Raze's own
		//folder, is written relative to Raze's folder, as ../../grps/*, so
		//that the folder still moves as a whole; any other as it is. A grps
		//folder beside Raze's uZDL gets a second line, $UZDL_DIR/grps/*,
		//which holds at any depth and after any move whenever uZDL does the
		//starting, written word for word when absent. Returns the lines
		//newly written, as written; ok tells whether the file could be
		//written at all.
		static QStringList addSearchFolders(const QString &ini, const QString &razeDir, const QStringList &folders, bool *ok=NULL);

		//The whole setup for one executable, as text for a dialog or a report.
		static QString setUp(ZDLConf *zconf, const QString &exe, const QStringList &extraFolders=QStringList());
};

#endif
