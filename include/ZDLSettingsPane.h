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
#ifndef _ZDLSETTINGSPANE_H_
#define _ZDLSETTINGSPANE_H_
 
#include <QtWidgets>
#include <QObject>
#include <QStyledItemDelegate>
#include "ZDLWidget.h"

class ZDLSettingsPane: public ZDLWidget {
	Q_OBJECT
public:
	//The pane of a tab's launch configuration, kept in section: the Doom
	//Engine tab's zdl.save unless given. The Build Engine tab's lists only
	//the Build engine's ports and games, and leaves out GZDoom's
	//compatibility modes and fast monsters, which those ports do not have;
	//the Doom Engine tab's lists the rest.
	ZDLSettingsPane( QWidget *parent=0, const QString &section=QString(), bool build=false);
	virtual void rebuild();
	virtual void newConfig();
protected slots:
	void currentRowChanged(int);
	void reloadMapList();
	void VerbosePopup();
	void HidePopup();
	void runSourcePort();
	//A port or a game picked here by hand.
	void portPicked(int);
	void iwadPicked(int);
	//The preset's own save folder and settings file no longer used.
	void dropKeptApart();
protected:
	QStringList getFilesMaps();
	//The row that shows a preset's save folder and settings file while
	//they apply to the launch.
	void showKeptApart();
	QWidget *keptRow;
	QLabel *keptLabel;
	QPushButton *keptDrop;
	//The launch configuration's section, and whether the pane is the Build
	//Engine tab's.
	QString launch;
	bool build;
	//Set while the configuration is being read in, when the lists change
	//without anyone picking anything.
	bool loading;
	int lastPortIndex;
	int lastIwadRow;
	QComboBox *diffList;
	QComboBox *compatList;
	QCheckBox *fastCheck;
	QCheckBox *respawnCheck;
	QComboBox *sourceList;
	QPushButton *btnRunPort;
	QListWidget *IWADList;
	QComboBox *warpCombo;
	static bool naturalSortLess(const QString &lm, const QString &rm);
};

class AlwaysFocusedDelegate: public QItemDelegate {
	Q_OBJECT
public:
	AlwaysFocusedDelegate(QObject *parent=NULL): QItemDelegate(parent) {}
	virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
	//virtual void drawFocus(QPainter*, const QStyleOptionViewItem&, const QRect&) const {}
};

class DeselectableListWidget: public QListWidget {
	Q_OBJECT
public:
	DeselectableListWidget(QWidget *parent=NULL): QListWidget(parent) {}
	virtual void mousePressEvent(QMouseEvent *event);
};
#endif
