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

#ifndef _ZDLSHORTCUT_H_
#define _ZDLSHORTCUT_H_

#include <QString>

//Desktop shortcuts that start uZDL with --preset, so a preset can sit on
//the desktop or in Steam as a game of its own: a .lnk on Windows, a
//.desktop entry on Linux.
namespace ZDLShortcut {
	//False where no shortcut format is implemented, in which case the
	//button offering one is not shown at all.
	bool supported();

	//The extension a shortcut file carries here, without the dot.
	QString extension();

	//A file name for the preset that any file system accepts.
	QString safeFileName(const QString &presetName);

	//Writes the shortcut at path. On failure error, when given, says why.
	bool create(const QString &path, const QString &presetName, QString *error=0);
}

#endif
