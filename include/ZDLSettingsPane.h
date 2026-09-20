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
	ZDLSettingsPane( QWidget *parent=0);
	virtual void rebuild();
	virtual void newConfig();
protected slots:
	void currentRowChanged(int);
	void reloadMapList();
	void VerbosePopup();
	void HidePopup();
	void runSourcePort();
protected:
	QStringList getFilesMaps();
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
