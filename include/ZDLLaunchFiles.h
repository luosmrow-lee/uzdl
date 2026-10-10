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
	//Section holding the launch configuration's own external file list: the
	//Doom Engine tab's.
	extern const char *LAUNCH_SECTION;
	//The Build Engine tab's launch configuration, in the same layout.
	extern const char *BUILD_SECTION;
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
	//combo's own value would win. The save folder and config file are
	//different: when replacing they are set or cleared outright, since a
	//folder left over from the previous preset would quietly catch the
	//next one's saves.
	//The target is the launch configuration's section: the Doom Engine tab's
	//unless another is named.
	void sendToLaunch(const QList<ZDLFileEntry> &entries, bool replace, const QString &port=QString(), const QString &iwad=QString(), const QString &savedir=QString(), const QString &config=QString(), const QString &target=QString());

	//The configuration side of sendToLaunch on its own: no pane is flushed
	//first or told afterwards. For callers that have nothing on screen to
	//keep in step, or must not depend on it.
	void apply(ZDLConf *zconf, const QList<ZDLFileEntry> &entries, bool replace, const QString &port=QString(), const QString &iwad=QString(), const QString &savedir=QString(), const QString &config=QString(), const QString &target=QString());

	//Loads the preset in section: its files, and its port, IWAD, save
	//folder and config file where it names them; into the launch
	//configuration of target, the Doom Engine tab's unless given.
	void loadPreset(ZDLConf *zconf, const QString &section, const QString &target=QString());

	//The same into the configuration only, for a launch straight from the
	//command line.
	void applyPreset(ZDLConf *zconf, const QString &section, const QString &target=QString());

	//Puts the launch configuration of a .zdl file, its zdl.save section, in
	//place of a tab's: the Build Engine tab's when the port or the game it
	//names is the Build engine's, the Doom Engine tab's otherwise. Returns
	//the section it went to, or an empty string for a file without one.
	QString loadZdl(ZDLConf *zconf, ZDLConf *zdl);

	//Marks a preset as made on the Build Engine tab, or clears the mark. It
	//only counts for a preset that names no port or game; see
	//ZDLBuildArgs::isBuildLaunch.
	void markBuild(ZDLConf *zconf, const QString &section, bool build);
	bool markedBuild(ZDLConf *zconf, const QString &section);

	//The section of the preset called name, compared without regard to
	//case, or an empty string when there is none.
	QString presetSection(ZDLConf *zconf, const QString &name);

	//Writes a preset's port, IWAD, save folder and config file: each set
	//when given and dropped when blank, so blank means "leave the launch
	//tab's own alone" for the first two and "the port's usual place" for
	//the other two.
	void storeChoices(ZDLConf *zconf, const QString &section, const QString &port, const QString &iwad, const QString &savedir, const QString &config);

	//Preset sections, in ascending order.
	QStringList presetSections(ZDLConf *zconf);
	//An unused preset section name.
	QString newPresetSection(ZDLConf *zconf);
}

#endif
