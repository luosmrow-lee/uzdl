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

#ifndef _ZDLPRESETLIST_H_
#define _ZDLPRESETLIST_H_

#include <QtWidgets>
#include <QObject>
#include "ZDLWidget.h"

//Named external file lists - presets - kept alongside the launch
//configuration rather than inside it, so switching between them does not
//disturb the source port, IWAD or anything else that has been set up.
class ZDLPresetList: public ZDLWidget {
	Q_OBJECT
	public:
		ZDLPresetList(QWidget *parent);
		virtual void rebuild();
		virtual void newConfig();
	protected slots:
		void editPreset();
		void loadPreset();
		void launchPreset();
		void appendPreset();
		void deletePreset();
		void exportPreset();
		void importPreset();
		void itemActivated(QListWidgetItem *item);
		void showMenu(const QPoint &pos);
	private:
		//Section name of the highlighted preset, empty when nothing is selected.
		QString currentSection();
		QString currentName();
		//Port the preset asks for, empty when it should not change it.
		QString currentPort();
		//IWAD the preset asks for, empty when it should not change it.
		QString currentIwad();
		void send(bool replace);

		QListWidget *pList;
};

#endif
