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

#ifndef _ZDLFINDERDIALOG_H_
#define _ZDLFINDERDIALOG_H_

#include <QtWidgets>
#include <QObject>
#include "ZDLFinder.h"

//Shows what ZDLFinder turned up and adds what the user ticks. Nothing is
//added without a tick, and what the lists already hold is shown greyed.
class ZDLFinderDialog: public QDialog {
	Q_OBJECT
	public:
		//Searches, shows, adds. True when something was added.
		static bool run(QWidget *parent, bool ports, bool iwads);

		//On a start with no ports and no IWADs configured, asks once whether
		//to look for them, and runs when told to.
		static void offerOnFirstStart(QWidget *parent);
	private slots:
		void lookElsewhere();
		void addSelected();
		void showDetails();
	private:
		ZDLFinderDialog(QWidget *parent, bool ports, bool iwads);
		void search();
		void fill();
		QTreeWidgetItem *addRow(QTreeWidgetItem *group, const ZDLFinder::Found &found, bool port, int index);

		bool wantPorts;
		bool wantIwads;
		QStringList extraFolders;
		QList<ZDLFinder::Found> ports;
		QList<ZDLFinder::Found> iwads;
		//Resolved paths of what is configured already.
		QStringList havePorts;
		QStringList haveIwads;
		bool added;

		QTreeWidget *tree;
		QCheckBox *copyIwads;
		QLabel *status;
};

#endif
