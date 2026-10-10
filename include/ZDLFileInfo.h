/*
 * This file is part of uZDL
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
 
#ifndef _ZDLIFILEINFO_H_
#define _ZDLIFILEINFO_H_

#include <QFileInfo>
#include "ZDLBuildGames.h"

class ZDLFileInfo: public QFileInfo {
	public: 
		ZDLFileInfo();
		ZDLFileInfo(const QString &file);
		virtual QString GetFileDescription();
};

//An IWAD, told by its contents: the MD5 of a known release first, then an
//IWADINFO lump the file carries about itself, then the lumps the ZDoom
//ports demand of each game, and only then the file name.
class ZDLIwadInfo: public ZDLFileInfo {
	public:
		ZDLIwadInfo();
		ZDLIwadInfo(const QString &file);
		virtual QString GetFileDescription();
		//The name when the file is a known IWAD, an empty string otherwise.
		QString Identify();
		//Whether the file's contents pass any IWAD rule at all, without
		//reading more than its directory; cheap enough to try on anything.
		static bool LooksLikeIwad(const QString &file);
		//MD5 of the whole file, for telling copies of one IWAD apart.
		QByteArray Digest();
		//Doom, Heretic, Hexen, Strife or Chex, from the name; Other when
		//the file is not a known IWAD.
		QString Family();
		//Which of the stages settled it, in words, for a report.
		QString IdentifiedBy();
		//What is known of it as a game of the Build engine, which is
		//nothing when it is not one.
		const ZDLBuildGames::Match &BuildMatch();
	private:
		void Scan();
		QString scannedPath;
		QString identified;
		QString how;
		QByteArray digest;
		ZDLBuildGames::Match build;
};

//A source port. On Windows the executable says what it is and which
//version, in the block Explorer shows under Properties, Details; elsewhere
//the name comes from a table of known ports and the version from the
//package manager, Flatpak or the file's own name, when any of them knows.
class ZDLAppInfo: public ZDLFileInfo {
	public:
		ZDLAppInfo();
		ZDLAppInfo(const QString &file);
		//Name and version together, "GZDoom 4.14.2", or the name alone.
		virtual QString GetFileDescription();
		//The port's name without a version; the file's base name when no
		//table or version block knows it.
		QString GetProductName();
		//The version as the executable or its package states it, or an
		//empty string.
		QString GetVersion();
		//Whether the name table knows this file as a port at all.
		bool IsKnownPort();
		//One string of the executable's version block by its name, such as
		//LegalCopyright; empty when absent, and always on other platforms.
		QString VersionField(const QString &field);
};

#endif