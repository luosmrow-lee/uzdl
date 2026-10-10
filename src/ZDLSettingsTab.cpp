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

#include <QtWidgets>
#include <QApplication>

#include "ZDLConfigurationManager.h"
#include "ZDLListWidget.h"
#include "ZDLSettingsTab.h"
#include "ZDLQSplitter.h"

ZDLSettingsTab::ZDLSettingsTab(QWidget *parent): ZDLWidget(parent){
	LOGDATAO() << "New ZDLSettingsTab" << Qt::endl;
	QVBoxLayout *sections = new QVBoxLayout(this);

	alwaysArgs = new QLineEdit(this);

	ZDLQSplitter *split = new ZDLQSplitter(this);
	split->setSizePolicy( QSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding ));
	QSplitter *rsplit = split->getSplit();
	listSplit = rsplit;

	//IWAD
	QWidget *rwidget = new QWidget(rsplit);
	QVBoxLayout *rpane = new QVBoxLayout();
	iwadList = new ZDLIWadList(this);
	iwadList->doDragDrop(true);
	rpane->addWidget(new QLabel("IWADs", this));
	rpane->addWidget(iwadList);
	rwidget->setLayout(rpane);
	rpane->setContentsMargins(0,0,0,0);

	//Source Port
	QWidget *lwidget = new QWidget(rsplit);
	QVBoxLayout *lpane = new QVBoxLayout();
	sourceList = new ZDLSourcePortList(this);
	sourceList->doDragDrop(true);
	lpane->addWidget(new QLabel("Source ports", this));
	lpane->addWidget(sourceList);
	lwidget->setLayout(lpane);
	lpane->setContentsMargins(0,0,0,0);

	split->addChild(lwidget);
	split->addChild(rwidget);

	//Add all the sections together
	sections->addWidget(new QLabel("Always add these parameters", this));
	sections->addWidget(alwaysArgs);
	sections->addWidget(split);

	QLabel *elsewhere = new QLabel("uZDL's own options, updates and tools are on the uZDL settings tab.", this);
	elsewhere->setEnabled(false);
	sections->addWidget(elsewhere);

	setContentsMargins(4,4,4,4);
	layout()->setContentsMargins(0,0,0,0);
}

void ZDLSettingsTab::rebuild(){
	saveSplitterSizes("splitsettings", listSplit);

	ZDLConf *zconf = ZDLConfigurationManager::getActiveConfiguration();

	if(alwaysArgs->text().isEmpty()){
		zconf->deleteValue("zdl.general", "alwaysadd");
	}else{
		zconf->setValue("zdl.general", "alwaysadd", alwaysArgs->text());
	}
}

void ZDLSettingsTab::newConfig(){
	restoreSplitterSizes("splitsettings", listSplit);

	ZDLConf *zconf = ZDLConfigurationManager::getActiveConfiguration();

	if(zconf->hasValue("zdl.general","alwaysadd")){
		int ok;
		QString rc = zconf->getValue("zdl.general","alwaysadd", &ok);
		if(!rc.isNull()){
			alwaysArgs->setText(rc);
			LOGDATAO() << "Set alwaysadd as " << rc << Qt::endl;
		}else{
			LOGDATAO() << "alwaysadd was null" << Qt::endl;
		}
	}else{
		LOGDATAO() << "No alwaysadd" << Qt::endl;
	}
}

void ZDLSettingsTab::reloadConfig(){
	LOGDATAO() << "Reloading config" << Qt::endl;
	writeConfig();
	startRead();
	LOGDATAO() << "Reload complete" << Qt::endl;
}

void ZDLSettingsTab::startRead(){
	LOGDATAO() << "Reading new configuration" << Qt::endl;
	emit readChildren(this);
	newConfig();
}

void ZDLSettingsTab::writeConfig(){
	LOGDATAO() << "Writing configuration" << Qt::endl;
	emit buildChildren(this);
	rebuild();
}
