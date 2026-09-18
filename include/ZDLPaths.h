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

#ifndef _ZDLPATHS_H_
#define _ZDLPATHS_H_

#include <QString>

//Paths kept relative to the folder uZDL runs from, so that a source port or
//IWAD stored inside that folder survives the folder being moved or copied
//as a whole. Only files inside the folder qualify: a path that climbs out
//through ".." would move with the folder in name only.
namespace ZDLPaths {
	//The folder the executable is in, or on Linux the folder the AppImage
	//is in when uZDL runs from one. In portable mode the configuration
	//lives there too.
	QString appDir();

	//The folders meant for source ports and IWADs kept with uZDL; portable
	//mode creates them at startup. Suggestions only: any file under appDir
	//can be stored relative.
	QString sourcePortsDir();
	QString iwadsDir();

	//True for a stored path that is relative to appDir.
	bool isRelative(const QString &stored);

	//The absolute path for a stored one. Relative paths resolve against
	//appDir; absolute ones, and an empty string, come back unchanged.
	QString resolve(const QString &stored);

	//The path as it would be stored relative to appDir, or an empty string
	//when the file is not inside it.
	QString relativeToAppDir(const QString &path);

	//The relative form when the file is inside appDir, the path itself
	//otherwise: the default for anything newly added.
	QString preferRelative(const QString &path);
}

#endif
