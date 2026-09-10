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

#ifndef _ZDLPWADLIST_H_
#define _ZDLPWADLIST_H_

#include <QtWidgets>
#include <QObject>
#include <QDateTime>
#include <QVector>
#include "ZDLWidget.h"

class QFileSystemWatcher;

//A browsable library of the PWADs found under one folder. Nothing here is
//part of the launch configuration: the pane only holds a folder to scan and
//hands selected files over to the external file list.
class ZDLPwadList: public ZDLWidget {
	Q_OBJECT
	public:
		ZDLPwadList(QWidget *parent);
		virtual void rebuild();
		virtual void newConfig();

		//On the launch tab the pane puts chosen files straight into the
		//external file list. Embedded in a dialog there is no launch list to
		//add to, so it only announces the choice and the dialog decides.
		void setForwardToLaunch(bool forward);
	signals:
		void filesChosen(const QStringList &files);
	protected slots:
		void chooseFolder();
		void rescan();
		void addSelected();
		void filterChanged(const QString &text);
		void viewChanged(int index);
		void itemActivated(QTreeWidgetItem *item, int column);
		void treeChanged(const QString &path);
		void rescanTimeout();
	private:
		//One scanned file. The relative path is what the flat views show and
		//what the filter matches, so it is worked out once here.
		struct Found {
			QString path;
			QString rel;
			QDateTime modified;
		};

		void scan();
		void build();
		void watchTree();
		void applyFilter();
		bool filterItem(QTreeWidgetItem *item, const QString &needle);
		void collectFiles(QTreeWidgetItem *item, QStringList &out);
		QStringList selectedFiles();

		QLabel *status;
		QLineEdit *filter;
		QComboBox *viewBox;
		QTreeWidget *tree;

		QString root;
		QVector<Found> found;

		bool forwardToLaunch;
		QFileSystemWatcher *watcher;
		//Unpacking an archive fires a burst of directory changes; this
		//collapses them into one rescan once things have settled.
		QTimer *rescanTimer;
};

#endif
