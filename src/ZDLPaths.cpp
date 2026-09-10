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
	return QDir::cleanPath(QCoreApplication::applicationDirPath());
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
