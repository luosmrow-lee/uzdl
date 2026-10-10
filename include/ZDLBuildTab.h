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

#ifndef _ZDLBUILDTAB_H_
#define _ZDLBUILDTAB_H_

#include <QtWidgets>
#include <QObject>
#include "ZDLWidget.h"

//The Build engine's games on a tab of their own, laid out as the Doom
//Engine tab is: the presets, the mods loaded over the game, and the port,
//the game and the launch settings. Its launch configuration is a section of
//its own, zdl.build, so that the two tabs never disturb each other.
class ZDLBuildTab: public ZDLWidget{
Q_OBJECT
public:
	ZDLBuildTab(QWidget *parent=0);
	void startRead();
	void writeConfig();
	virtual void newConfig();
	virtual void rebuild();
protected:
	virtual void showEvent(QShowEvent *event);
private slots:
	void launch();
	void showCommandline();
	void clearMods();
	void clearAllFields();
	void inspectFile();
	void aboutClick();
	void exitzdl();
	//The panes' split can only be set once the splitter has a real width,
	//which the tab does not have until it is first shown.
	void balanceTopPane();
private:
	QLayout *getButtonPane();
	QSplitter *topSplit;
	QLineEdit *extraArgs;
	//Whether the tab has been on show yet; until then its splitter has no
	//sizes worth storing.
	bool shown;
};
#endif
