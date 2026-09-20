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

#include <QtCore>
#include "ZDLShortcut.h"
#include "ZDLPaths.h"

#if defined(Q_OS_WIN)
#include <windows.h>
#include <shobjidl.h>
#include <shlguid.h>

//Defined in ZDLMainWindow.cpp: quotes one argument the way CommandLineToArgvW
//expects, so a preset name with spaces or quotes in it arrives whole.
QString QuoteParam(const QString &param);

static inline const wchar_t *WStr(const QString &str)
{
	return reinterpret_cast<const wchar_t*>(str.utf16());
}
#endif

bool ZDLShortcut::supported()
{
#if defined(Q_OS_WIN)||defined(Q_OS_LINUX)
	return true;
#else
	return false;
#endif
}

QString ZDLShortcut::extension()
{
#if defined(Q_OS_WIN)
	return "lnk";
#else
	return "desktop";
#endif
}

QString ZDLShortcut::safeFileName(const QString &presetName)
{
	QString name=presetName;
	//What Windows refuses, plus the slash Linux does; the rest is fine.
	name.remove(QRegularExpression("[\\\\/:*?\"<>|\\x00-\\x1f]"));
	name=name.trimmed();
	while (name.endsWith('.'))
		name.chop(1);
	return name.isEmpty()?QString("preset"):name;
}

#if defined(Q_OS_WIN)

//A shell link, through the same COM interface Explorer uses. The link
//points at this executable with the preset on its command line and takes
//its icon from the executable, so it looks like uZDL in the menu.
static bool CreateLnk(const QString &path, const QString &presetName, QString *error)
{
	QString exe=QDir::toNativeSeparators(ZDLPaths::launcher());
	QString args="--preset "+QuoteParam(presetName);
	QString cwd=QDir::toNativeSeparators(ZDLPaths::appDir());
	QString description="uZDL preset "+presetName;
	QString target=QDir::toNativeSeparators(path);

	bool ok=false;
	HRESULT init=CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	IShellLinkW *link=NULL;
	if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link)))) {
		link->SetPath(WStr(exe));
		link->SetArguments(WStr(args));
		link->SetWorkingDirectory(WStr(cwd));
		link->SetIconLocation(WStr(exe), 0);
		link->SetDescription(WStr(description));

		IPersistFile *file=NULL;
		if (SUCCEEDED(link->QueryInterface(IID_PPV_ARGS(&file)))) {
			ok=SUCCEEDED(file->Save(WStr(target), TRUE));
			file->Release();
		}
		link->Release();
	}

	if (SUCCEEDED(init))
		CoUninitialize();

	if (!ok&&error)
		*error="Windows would not write the shortcut file.";
	return ok;
}

#elif defined(Q_OS_LINUX)

//One argument of a desktop entry's Exec line. The entry's own string
//escaping is applied first and the Exec quoting on top of it, which is why
//a literal backslash ends up as four.
static QString ExecQuote(const QString &arg)
{
	QString out=arg;
	out.replace("\\", "\\\\\\\\");
	out.replace("\"", "\\\"");
	out.replace("`", "\\`");
	out.replace("$", "\\$");
	out.replace("%", "%%");
	return "\""+out+"\"";
}

//A plain string value of a desktop entry, where only the backslash needs
//escaping.
static QString EntryString(const QString &value)
{
	QString out=value;
	out.replace("\\", "\\\\");
	return out;
}

//The window icon, written out once where the entry can point at it: an
//AppImage's icon is not in any icon theme, and an absolute path works for
//an installed uZDL just the same.
static QString IconFile()
{
	QDir data(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
	QString icon=data.filePath("uzdl.png");

	if (!QFile::exists(icon)) {
		data.mkpath(".");
		QFile::copy(":/icon/uzdl.png", icon);
	}

	return QFile::exists(icon)?icon:QString("uzdl");
}

static bool CreateDesktopEntry(const QString &path, const QString &presetName, QString *error)
{
	QString entry=
		"[Desktop Entry]\n"
		"Type=Application\n"
		"Name="+EntryString(presetName)+"\n"
		"Comment=uZDL preset\n"
		"Exec="+ExecQuote(ZDLPaths::launcher())+" --preset "+ExecQuote(presetName)+"\n"
		"Path="+EntryString(ZDLPaths::appDir())+"\n"
		"Icon="+EntryString(IconFile())+"\n"
		"Terminal=false\n"
		"Categories=Game;\n"
		"StartupWMClass=uzdl\n";

	QFile file(path);
	if (!file.open(QIODevice::WriteOnly|QIODevice::Truncate|QIODevice::Text)) {
		if (error)
			*error="Cannot write "+path;
		return false;
	}
	file.write(entry.toUtf8());
	file.close();

	//Desktops refuse to launch an entry that is not marked executable.
	file.setPermissions(file.permissions()|QFileDevice::ExeOwner|QFileDevice::ExeGroup|QFileDevice::ExeOther);
	return true;
}

#endif

bool ZDLShortcut::create(const QString &path, const QString &presetName, QString *error)
{
#if defined(Q_OS_WIN)
	return CreateLnk(path, presetName, error);
#elif defined(Q_OS_LINUX)
	return CreateDesktopEntry(path, presetName, error);
#else
	Q_UNUSED(path);
	Q_UNUSED(presetName);
	if (error)
		*error="Shortcuts are not made on this platform.";
	return false;
#endif
}
