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

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include "ZDLPaths.h"

QString ZDLPaths::appDir()
{
	QString dir=QDir::cleanPath(QCoreApplication::applicationDirPath());

#if defined(Q_OS_LINUX)
	//Inside an AppImage the executable runs from a read-only mount that is
	//gone once it exits, so the folder uZDL is "in" is the one holding the
	//AppImage file. The runtime names both: APPDIR for the mount, APPIMAGE
	//for the file. Only an APPDIR this executable actually lives under
	//counts, so that a pair of variables inherited from some other AppImage
	//that started uZDL is ignored.
	QString mount=QDir::cleanPath(qEnvironmentVariable("APPDIR"));
	QString image=qEnvironmentVariable("APPIMAGE");
	if (!mount.isEmpty()&&!image.isEmpty()&&(dir==mount||dir.startsWith(mount+"/"))) {
		QFileInfo fi(image);
		QString real=fi.canonicalFilePath();
		dir=QDir::cleanPath(QFileInfo(real.isEmpty()?image:real).absolutePath());
	}
#endif

	return dir;
}

QString ZDLPaths::sourcePortsDir()
{
	return QDir(appDir()).filePath("sourceports");
}

QString ZDLPaths::iwadsDir()
{
	return QDir(appDir()).filePath("iwads");
}

bool ZDLPaths::isRelative(const QString &stored)
{
	return !stored.isEmpty()&&QFileInfo(stored).isRelative();
}

QString ZDLPaths::resolve(const QString &stored)
{
	if (!isRelative(stored))
		return stored;
	return QDir::cleanPath(QDir(appDir()).filePath(stored));
}

QString ZDLPaths::relativeToAppDir(const QString &path)
{
	if (path.isEmpty())
		return QString();

	//A stored relative path is relative to appDir already, never to the
	//working directory, so it is resolved before being made relative again.
	QString rel=QDir(appDir()).relativeFilePath(QDir::cleanPath(QFileInfo(resolve(path)).absoluteFilePath()));

	//relativeFilePath answers with the absolute path for another drive,
	//with "." for the folder itself, and climbs with ".." for anything
	//outside it. None of those is a file inside.
	if (rel.isEmpty()||!QFileInfo(rel).isRelative()||rel=="."||rel==".."||rel.startsWith("../"))
		return QString();

	return rel;
}

QString ZDLPaths::preferRelative(const QString &path)
{
	QString rel=relativeToAppDir(path);
	return rel.isEmpty()?path:rel;
}
