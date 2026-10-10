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
 
#include <QtWidgets>
#include <QApplication>
#include <QListWidget>

#include "ZDLListWidget.h"
#include "ZDLFilePane.h"
#include "ZDLPresetDialog.h"
#include "ZDLLaunchFiles.h"
#include "ZDLConfigurationManager.h"
#include "ZDLMainWindow.h"

extern ZDLMainWindow *mw;


ZDLFilePane::ZDLFilePane(QWidget *parent, const QString &section, bool buildEngine):ZDLWidget(parent){
	launch = section.isEmpty() ? QString(ZDLLaunchFiles::LAUNCH_SECTION) : section;
	build = buildEngine;
	QVBoxLayout *box = new QVBoxLayout(this);
	box->setSpacing(2);

	box->addWidget(new QLabel(build ? "Mods" : "External files",this));

	fList = new ZDLFileList(this, launch, build);
	fList->doDragDrop(true);
	box->addWidget(fList);

	//A preset is built out of whatever is in this list, so the button that
	//turns one into a preset belongs under the list itself.
	QPushButton *btnSave = new QPushButton("Save as preset", this);
	btnSave->setToolTip(build ? "Store the port, the game and the mods as a named preset" : "Store the current external file list as a named preset");
	box->addWidget(btnSave);

	QObject::connect(btnSave, SIGNAL(clicked()), this, SLOT(saveAsPreset()));
	setContentsMargins(0,0,0,0);
	layout()->setContentsMargins(0,0,0,0);
	
}

void ZDLFilePane::saveAsPreset()
{
	//Take what the panes currently show, not what was last written out.
	mw->writeConfig();

	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	if (!zconf)
		return;

	//Opened with the current list already in it, so it can be trimmed or
	//reordered before being stored. An empty preset is allowed too.
	QString port=zconf->hasValue(launch, "port")?zconf->getValue(launch, "port"):QString();
	QString iwad=zconf->hasValue(launch, "iwad")?zconf->getValue(launch, "iwad"):QString();

	ZDLPresetDialog dialog(this, QString(), port, iwad, ZDLLaunchFiles::read(zconf, launch), launch, build);
	//The dialog will not close without a name, so there is nothing to check.
	if (dialog.exec()!=QDialog::Accepted)
		return;

	QString section=ZDLLaunchFiles::newPresetSection(zconf);
	zconf->setValue(section, "name", dialog.presetName());
	ZDLLaunchFiles::storeChoices(zconf, section, dialog.presetPort(), dialog.presetIwad(), dialog.presetSaveDir(), dialog.presetConfig());
	ZDLLaunchFiles::markBuild(zconf, section, build);
	ZDLLaunchFiles::write(zconf, section, dialog.presetEntries());

	//Written out now, for a shortcut to it started while uZDL is open.
	mw->saveNow();
	//Refreshes every pane, which is how the preset list picks the new one up.
	mw->startRead();
}

void ZDLFilePane::rebuild(){
	//std::cout << "Rebuilding config" << std::endl;
}
