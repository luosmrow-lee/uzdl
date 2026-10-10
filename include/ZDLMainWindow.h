/*
 * This file is part of uZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2019  Lcferrum
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
 
#ifndef _MAINWINDOW_H_
#define _MAINWINDOW_H_

#include <QtWidgets>
#include <QApplication>
#include <QMainWindow>
#include "ZDLWidget.h"
#include "ZDLInterface.h"
#include "ZDLSettingsTab.h"
#include "ZDLAppSettingsTab.h"
#include "ZDLIdgamesTab.h"

class ZDLBuildTab;

class ZDLMainWindow: public QMainWindow{
	Q_OBJECT
public:
	ZDLMainWindow( QWidget *parent=0);
	~ZDLMainWindow();
	void startRead();
	void writeConfig();
	//The panes flushed and the configuration written to its file at once,
	//rather than at exit: after a preset changes, so that a shortcut to it
	//finds it while uZDL is open. False when the file cannot be written.
	bool saveNow();
	//The launch configuration the launch, the command line and the port
	//are taken from: the Doom Engine tab's zdl.save, or for a while the
	//Build Engine tab's own.
	void setLaunchSection(const QString &section);
	QString getLaunchSection() const { return launchSection; }
	QString getArgumentsString(bool native_sep=false);
	QStringList getArgumentsList();
	QString getExecutable();
	//The same from the launch configuration of section.
	QString getExecutable(const QString &section);
	//A launch from the launch configuration of section, the panes flushed
	//first.
	void launchFrom(const QString &section);
	//Brings up the tab whose launch configuration is in section.
	void showLaunchTab(const QString &section);
	//Parameters stored against the selected source port; empty when none.
	QString getPortParams();
	//The port's own parameters, the always-added ones and the launch's
	//extra ones, in that order, behind whatever was built so far.
	void appendUserParams(QString &args);
	void appendUserParams(QStringList &args);
	void handleImport();
	QString getWindowTitle();
	//The launch without the pane flush launch does first: for a launch set
	//up in the configuration directly, as the command line's --preset is.
	void launchNow();
protected:
	ZDLInterface* intr;
	ZDLBuildTab* buildTab;
	QString launchSection;
	ZDLSettingsTab* settings;
	ZDLAppSettingsTab* appSettings;
	ZDLIdgamesTab* idgames;
	QAction *qact2;
public slots:
	void launch();
	void quit();
	void tabChange(int index);
};
#endif
