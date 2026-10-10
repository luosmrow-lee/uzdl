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

#ifndef _ZDLFINDER_H_
#define _ZDLFINDER_H_

#include <QString>
#include <QStringList>
#include <QList>
#include <QByteArray>

//Finds the IWADs and source ports on this machine the way the ZDoom ports
//look for IWADs at start: the folders they and uZDL know about, then
//whatever Steam, GOG and the Bethesda launcher registered, every file told
//by its contents rather than trusted on its name.
namespace ZDLFinder {
	struct Found {
		QString path;		//absolute
		QString name;		//as identified, "Doom II: Hell on Earth v1.9" or "GZDoom 4.14.2"
		QString where;		//what led here: Steam, GOG, the uZDL folder, a folder of the user's
		QString family;		//IWADs: Doom, Heretic, Hexen, Strife, Chex or Other
		QByteArray digest;	//IWADs: MD5 of the file, what tells copies of one IWAD apart
		QString version;	//ports: as the executable or its package states it
		QStringList also;	//other places holding the same IWAD
	};

	//The built-in places are searched one level deep, as the ports do, or
	//a little deeper where noted in the source; extra folders all the way
	//down. Copies of one IWAD come back as one entry.
	QList<Found> findIwads(const QStringList &extraFolders=QStringList());
	QList<Found> findPorts(const QStringList &extraFolders=QStringList());
}

#endif
