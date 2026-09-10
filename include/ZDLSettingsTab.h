/*
 * This file is part of uZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
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
#ifndef _ZDLSETTINGSTAB_H_
#define _ZDLSETTINGSTAB_H_
 
#include <QtWidgets>
#include <QObject>
#include "ZDLWidget.h"
#include "ZDLSourcePortList.h"
#include "ZDLIWadList.h"
#include "ZDLUpdateCheck.h"
#include "ZDLSelfUpdate.h"

class ZDLSettingsTab: public ZDLWidget{
Q_OBJECT
public: 
	ZDLSettingsTab(QWidget *parent);
	virtual void rebuild();
	virtual void newConfig();
	void startRead();
	void writeConfig();
protected slots:
	void fileAssociations();
	void reloadConfig();
	void pathToggled(int state);
	void themeChanged(int index);
	void checkForUpdates();
	void updateCheckDone(bool available, const ZDLReleaseInfo &release, const QString &error);
	void selfUpdateFailed(const QString &error);
private:
	//Applies the stored choice to the running application. Kept apart from
	//the combo so start up can apply it without the combo emitting.
	void applyTheme(const QString &theme);

	QComboBox *themeBox;
	QPushButton *btnUpdate;
	QLabel *updateStatus;
	QCheckBox *updateOnStart;
	ZDLUpdateCheck *updater;
	ZDLSelfUpdate *selfUpdate;
	//The release the last check found, for the page link after a failure.
	ZDLReleaseInfo lastRelease;
	//Whether the running check was asked for rather than the daily one: a
	//skipped version is still shown when the user asks.
	bool manualCheck;

	QLineEdit *alwaysArgs;
	QCheckBox *launchClose;
	ZDLIWadList *iwadList;
	ZDLSourcePortList *sourceList;
	QCheckBox *showPaths;
	QCheckBox *launchZDL;
	QCheckBox *savePaths;
	QSplitter *listSplit;
};
#endif
