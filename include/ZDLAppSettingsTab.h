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

#ifndef _ZDLAPPSETTINGSTAB_H_
#define _ZDLAPPSETTINGSTAB_H_

#include <QtWidgets>
#include <QObject>
#include "ZDLWidget.h"
#include "ZDLUpdateCheck.h"
#include "ZDLSelfUpdate.h"
#include "ZDLGamePatches.h"

//uZDL's own settings and tools, apart from the games it launches: how it
//behaves and looks, where it keeps things, updates, file types, portable
//mode, the Raze setup, and every feature of the uZDL menu but the clearing
//of the launch tab. Each setting has one owner. The ones this tab owns it
//writes with the rest of the configuration; one another part of uZDL
//owns, such as the PWAD folder or the idgames mirror, it changes by having
//everything write what it holds, setting the value, and having everything
//read it back, so that the two never disagree.
class ZDLAppSettingsTab: public ZDLWidget{
Q_OBJECT
public:
	ZDLAppSettingsTab(QWidget *parent);
	virtual void rebuild();
	virtual void newConfig();
protected slots:
	void themeChanged(int index);
	void pathsToggled(bool on);
	void presetSortToggled(bool on);
	void portableToggled(bool on);
	void mirrorChanged(int index);
	void choosePwadFolder();
	void openPwadFolder();
	void openConfigFolder();
	void openUzdlFolder();
	void fileAssociations();
	void checkForUpdates();
	void forgetSkipped();
	void updateCheckDone(bool available, const ZDLReleaseInfo &release, const QString &error);
	void selfUpdateFailed(const QString &error);
	void setUpRaze();
	void forgetRaze();
	void fillPatches();
	void applyPatch();
	void undoPatch();
	void describePatch();
private:
	//The fix of the row picked in the list, or NULL with a word to the user.
	const ZDLGamePatches::Found *pickedPatch();
	//Applies the stored choice to the running application. Kept apart from
	//the combo so start up can apply it without the combo emitting.
	void applyTheme(const QString &theme);
	//A value of zdl.general another part of uZDL owns, set the safe way.
	//An empty value removes it.
	void setShared(const QString &key, const QString &value);
	//One of the launch tab's own actions, the ones the uZDL menu offers.
	void callInterface(const char *slot);
	void fillRaze();
	void showSkipped();

	QCheckBox *launchClose;
	QCheckBox *launchZDL;
	QCheckBox *savePaths;
	QComboBox *themeBox;
	QCheckBox *showPaths;
	QCheckBox *presetSort;
	QLabel *configPath;
	QCheckBox *portable;
	QLabel *pwadPath;
	QComboBox *mirrorBox;
	QLabel *updateStatus;
	QCheckBox *updateOnStart;
	QPushButton *btnUpdate;
	QLabel *skippedLabel;
	QPushButton *btnForgetSkipped;
	QTreeWidget *razeList;
	QTreeWidget *patchList;
	//What the list shows, row for row.
	QList<ZDLGamePatches::Found> patchesShown;

	ZDLUpdateCheck *updater;
	ZDLSelfUpdate *selfUpdate;
	//The release the last check found, for the page link after a failure.
	ZDLReleaseInfo lastRelease;
	//Whether the running check was asked for rather than the daily one: a
	//skipped version is still shown when the user asks.
	bool manualCheck;
	//Set while newConfig fills the controls, so that filling them does not
	//look like the user changing them.
	bool loading;
};
#endif
