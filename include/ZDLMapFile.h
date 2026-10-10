/*
 * This file is part of uZDL
 * Copyright (C) 2007-2012  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
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

#ifndef _ZDLMAPFILE_HPP_
#define _ZDLMAPFILE_HPP_

#include <QtCore>

class ZDLMapFile{
	public:
		static ZDLMapFile *getMapFile(QString file);
		//The Name a file gives itself in its IWADINFO lump, or an empty
		//string. An engine's own resource archive carries that lump as the
		//list of every game it knows, one IWad block per game, and that
		//describes nothing; only a lump with a single block counts.
		static QString nameFromIwadinfo(const QString &text);
		virtual QString getIwadinfoName() = 0;
		virtual QStringList getMapNames() = 0;
		//Every lump name in the file, upper case, the way the IWAD rules
		//look at them: the WAD directory as it is, or an archive's file
		//names without their extensions, cut to eight characters.
		virtual QStringList getLumpNames() = 0;
		virtual bool isMAPXX() = 0;
		virtual ~ZDLMapFile() = 0;
};


#endif



