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

#ifndef _ZDLPRESETDIALOG_H_
#define _ZDLPRESETDIALOG_H_

#include <QtWidgets>
#include <QObject>
#include "ZDLLaunchFiles.h"
#include "ZDLPwadList.h"

//Edits one preset in place: its name, which files it holds, what order they
//load in, and which of them are excluded. Nothing here touches the launch
//configuration, so a preset can be reworked without disturbing whatever is
//currently set up to run.
class ZDLPresetDialog: public QDialog {
	Q_OBJECT
	public:
		ZDLPresetDialog(QWidget *parent, const QString &name, const QString &port, const QString &iwad, const QList<ZDLFileEntry> &entries);

		QString presetName();
		//Empty when the preset should not touch the source port.
		QString presetPort();
		//Empty when the preset should not touch the IWAD.
		QString presetIwad();
		//The preset's own save folder and config file, passed to the port
		//as -savedir and -config; empty for the port's usual places.
		QString presetSaveDir();
		QString presetConfig();
		void setKeepApart(const QString &savedir, const QString &config);
		QList<ZDLFileEntry> presetEntries();

		//Refuses to close on an empty name, so nothing typed or arranged is
		//lost to a dialog that shut before the mistake could be corrected.
		virtual void accept();
	protected slots:
		void addFiles();
		void addFolder();
		void removeSelected();
		void moveUp();
		void moveDown();
		void toggleExcluded();
		void takeFromLaunch();
		void browseSaveDir();
		void browseConfig();
		void ownFolder();
		//Files picked in the embedded library, by its Add button or a double click.
		void addFromLibrary(const QStringList &files);
	private:
		void appendEntry(const ZDLFileEntry &entry);
		void moveBy(int delta);

		QLineEdit *nameEdit;
		QComboBox *portBox;
		QComboBox *iwadBox;
		QLineEdit *saveDirEdit;
		QLineEdit *configEdit;
		QListWidget *fileList;
		ZDLPwadList *library;
};

#endif
