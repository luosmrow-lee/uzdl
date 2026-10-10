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

#ifndef _ZDLGAMEPATCHES_H_
#define _ZDLGAMEPATCHES_H_

#include <QString>
#include <QStringList>
#include <QList>
#include "zdlconf.hpp"

//Fixes for known faults in games' own files: the kind today's source ports
//stop on where the original game carried on. Each fix is tied to the exact
//files it was written for, told by their MD5, and changes only the lines
//it names; a file of any other version is left alone. The original is kept
//beside a fixed file, so a fix can always be undone. Where the ports read
//the file out of the game's GRP, a fixed copy is written beside the GRP
//instead, which they read in preference, and undoing removes it.
namespace ZDLGamePatches {
	enum State { NotApplied, Applied, OtherVersion };

	struct Found {
		int patch;
		QString game;
		QString title;
		QString folder;
		State state;
		//How it stands, in words, for the list.
		QString stateText;
	};

	//The fixes for the games in the IWAD list's folders.
	QList<Found> scan(ZDLConf *zconf);

	//The fixes for the game in one folder.
	QList<Found> scanFolder(const QString &folder);

	//Each tells, in error, why nothing was changed when it fails.
	bool apply(const Found &found, QString *error);
	bool undo(const Found &found, QString *error);

	//What a fix is for and every line it changes, for a dialog.
	QString describe(int patch);

	//For File information: the fixes that concern this file.
	QStringList notesFor(const QString &file);
}

#endif
