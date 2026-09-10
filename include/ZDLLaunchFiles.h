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

#ifndef _ZDLLAUNCHFILES_H_
#define _ZDLLAUNCHFILES_H_

#include <QString>
#include <QList>
#include "zdlcommon.h"

//One entry of an external file list, in the fileN / fileNd form [zdl.save]
//has always used - the trailing d marks an entry excluded from the launch.
//Presets are stored in the identical layout, which is what lets a preset be
//written straight back out as a .zdl file with no conversion.
struct ZDLFileEntry {
	QString file;
	bool disabled;

	ZDLFileEntry(): disabled(false) {}
	ZDLFileEntry(const QString &file, bool disabled): file(file), disabled(disabled) {}
};

namespace ZDLLaunchFiles {
	//Section holding the launch configuration's own external file list.
	extern const char *LAUNCH_SECTION;
	//Prefix of the per-preset sections. Still zdl.mix for the sake of any
	//preset saved before they were renamed; nothing user facing shows it.
	extern const char *PRESET_PREFIX;

	QList<ZDLFileEntry> read(ZDLConf *zconf, const QString &section);
	//Replaces the section's file entries wholesale, leaving its other
	//variables - a mix's name, for one - untouched.
	void write(ZDLConf *zconf, const QString &section, const QList<ZDLFileEntry> &entries);

	//Hands entries to the launch configuration and refreshes the interface.
	//Replacing swaps the list out; otherwise entries are appended, skipping
	//files already listed so loading a preset twice cannot duplicate it.
	//A port name, when given, is applied too. That has to happen inside this
	//call, after the interface is flushed and before it re-reads, or the
	//combo's own value would win.
	void sendToLaunch(const QList<ZDLFileEntry> &entries, bool replace, const QString &port=QString(), const QString &iwad=QString());

	//Preset sections, in ascending order.
	QStringList presetSections(ZDLConf *zconf);
	//An unused preset section name.
	QString newPresetSection(ZDLConf *zconf);
}

#endif
