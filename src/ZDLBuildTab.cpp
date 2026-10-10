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

#include <QtWidgets>
#include "ZDLBuildTab.h"
#include "ZDLConfigurationManager.h"
#include "ZDLAboutDialog.h"
#include "ZDLFileInspector.h"
#include "ZDLFilePane.h"
#include "ZDLLaunchFiles.h"
#include "ZDLMainWindow.h"
#include "ZDLPresetList.h"
#include "ZDLQSplitter.h"
#include "ZDLSettingsPane.h"

extern ZDLMainWindow *mw;

ZDLBuildTab::ZDLBuildTab(QWidget *parent):ZDLWidget(parent){
	LOGDATAO() << "New ZDLBuildTab" << Qt::endl;
	shown = false;

	QVBoxLayout *box = new QVBoxLayout(this);

	//The Doom Engine tab's columns less its PWAD library, which holds Doom's
	//files: presets, mods, and the port, game and launch settings.
	ZDLQSplitter *split = new ZDLQSplitter(this);
	split->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding));
	topSplit = split->getSplit();

	ZDLPresetList *xpane = new ZDLPresetList(topSplit, ZDLLaunchFiles::BUILD_SECTION, true);
	ZDLFilePane *fpane = new ZDLFilePane(topSplit, ZDLLaunchFiles::BUILD_SECTION, true);
	ZDLSettingsPane *spane = new ZDLSettingsPane(topSplit, ZDLLaunchFiles::BUILD_SECTION, true);
	split->addChild(xpane);
	split->addChild(fpane);
	split->addChild(spane);

	//A small floor and an explicit split, for the reason the Doom Engine tab
	//gives: a file list's size hint grows with the longest path in it.
	xpane->setMinimumWidth(90);
	fpane->setMinimumWidth(120);
	spane->setMinimumWidth(180);
	topSplit->setStretchFactor(0, 2);
	topSplit->setStretchFactor(1, 3);
	topSplit->setStretchFactor(2, 3);

	QLabel *ecla = new QLabel("Extra command line arguments", this);
	ecla->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed));
	extraArgs = new QLineEdit(this);
	extraArgs->setToolTip("Added to the end of the command line of every game started from this tab");

	QVBoxLayout *bottom = new QVBoxLayout();
	bottom->addWidget(ecla);
	bottom->addWidget(extraArgs);
	bottom->setSpacing(2);

	//The panes' QSplitter is laid out here; the widget around it only
	//relays the configuration signals, and left on show it would sit over
	//the top left corner, empty, taking the clicks meant for what is there.
	box->addWidget(topSplit);
	split->hide();
	box->addLayout(bottom);
	box->addLayout(getButtonPane());
	box->setSpacing(2);

	setContentsMargins(4,4,4,4);
	layout()->setContentsMargins(0,0,0,0);
}

QLayout *ZDLBuildTab::getButtonPane(){
	QHBoxLayout *box = new QHBoxLayout();

	QPushButton *btnExit = new QPushButton("Exit", this);
	QPushButton *btnZDL = new QPushButton(ZDL_APP_NAME, this);
	QPushButton *btnLaunch = new QPushButton("Launch", this);

	//What of the Doom Engine tab's menu concerns this tab; the configuration
	//file and portable mode are on the uZDL settings tab.
	QMenu *context = new QMenu(btnZDL);
	QMenu *actions = new QMenu("Actions", context);
	QAction *showCommandline = actions->addAction("Show command line");
	QAction *clearModsAction = actions->addAction("Clear mods list");
	QAction *clearAllFieldsAction = actions->addAction("Clear all fields");
	context->addMenu(actions);
	context->addSeparator();
	QAction *inspectAction = context->addAction("File information...");
	inspectAction->setToolTip("What a GRP, archive, script or source port is and holds");
	context->addSeparator();
	QAction *aboutAction = context->addAction("About");
	btnZDL->setMenu(context);

	connect(showCommandline, SIGNAL(triggered()), this, SLOT(showCommandline()));
	connect(clearModsAction, SIGNAL(triggered()), this, SLOT(clearMods()));
	connect(clearAllFieldsAction, SIGNAL(triggered()), this, SLOT(clearAllFields()));
	connect(inspectAction, SIGNAL(triggered()), this, SLOT(inspectFile()));
	connect(aboutAction, SIGNAL(triggered()), this, SLOT(aboutClick()));
	connect(btnExit, SIGNAL(clicked()), this, SLOT(exitzdl()));
	connect(btnLaunch, SIGNAL(clicked()), this, SLOT(launch()));

	int minBtnWidth = 50;
	btnExit->setMinimumWidth(minBtnWidth-15);
	btnZDL->setMinimumWidth(minBtnWidth-15);
	btnLaunch->setMinimumWidth(minBtnWidth);
	btnExit->setMinimumHeight(26);
	btnZDL->setMinimumHeight(26);
	btnLaunch->setMinimumHeight(26);

	box->addWidget(btnExit, 1);
	box->addWidget(btnZDL, 1);
	box->addStretch(2);
	box->addWidget(btnLaunch, 1);
	box->setSpacing(4);
	return box;
}

void ZDLBuildTab::showEvent(QShowEvent *event){
	ZDLWidget::showEvent(event);
	if (!shown){
		shown = true;
		QTimer::singleShot(0, this, SLOT(balanceTopPane()));
	}
}

void ZDLBuildTab::balanceTopPane(){
	//Whatever the panes were dragged to last time wins over the default.
	if (restoreSplitterSizes("splitbuild", topSplit))
		return;

	int width = topSplit->width();
	if (width < 200)
		return;

	//Presets, mods, settings.
	QList<int> sizes;
	sizes << width*28/100 << width*34/100 << width*38/100;
	topSplit->setSizes(sizes);
}

void ZDLBuildTab::launch(){
	mw->launchFrom(ZDLLaunchFiles::BUILD_SECTION);
}

void ZDLBuildTab::showCommandline(){
	mw->writeConfig();

	QString previous = mw->getLaunchSection();
	mw->setLaunchSection(ZDLLaunchFiles::BUILD_SECTION);
	QString exec = mw->getExecutable();
	QString args = mw->getArgumentsString();
	mw->setLaunchSection(previous);

	if (exec.isEmpty()){
		QMessageBox::critical(this, ZDL_APP_NAME, "Please select a source port");
		return;
	}

	QFileInfo exec_fi(exec);
	QMessageBox msgBox(this);
	msgBox.setWindowTitle("Command line and environment");
	if (args.length())
		args = "\n\nArguments: "+args;
	msgBox.setText("Executable: "+exec_fi.fileName()+args+"\n\nWorking directory: "+exec_fi.canonicalPath());
	msgBox.setStandardButtons(QMessageBox::Cancel);
	QPushButton *launch_btn = msgBox.addButton("Execute", QMessageBox::AcceptRole);
	msgBox.setDefaultButton(launch_btn);
	msgBox.setEscapeButton(QMessageBox::Cancel);
	msgBox.exec();

	if (msgBox.clickedButton() == launch_btn)
		mw->launchFrom(ZDLLaunchFiles::BUILD_SECTION);
}

void ZDLBuildTab::clearMods(){
	mw->writeConfig();
	ZDLLaunchFiles::write(ZDLConfigurationManager::getActiveConfiguration(), ZDLLaunchFiles::BUILD_SECTION, QList<ZDLFileEntry>());
	mw->startRead();
}

void ZDLBuildTab::clearAllFields(){
	mw->writeConfig();
	ZDLConfigurationManager::getActiveConfiguration()->deleteSectionByName(ZDLLaunchFiles::BUILD_SECTION);
	mw->startRead();
}

void ZDLBuildTab::inspectFile(){
	ZDLFileInspector::open(this);
}

void ZDLBuildTab::aboutClick(){
	ZDLAboutDialog zad(this);
	zad.exec();
}

void ZDLBuildTab::exitzdl(){
	mw->close();
}

void ZDLBuildTab::rebuild(){
	if (shown)
		saveSplitterSizes("splitbuild", topSplit);

	ZDLConf *zconf = ZDLConfigurationManager::getActiveConfiguration();
	if (extraArgs->text().length() > 0)
		zconf->setValue(ZDLLaunchFiles::BUILD_SECTION, "extra", extraArgs->text());
	else
		zconf->deleteValue(ZDLLaunchFiles::BUILD_SECTION, "extra");
}

void ZDLBuildTab::newConfig(){
	ZDLConf *zconf = ZDLConfigurationManager::getActiveConfiguration();
	extraArgs->setText(zconf->hasValue(ZDLLaunchFiles::BUILD_SECTION, "extra") ? zconf->getValue(ZDLLaunchFiles::BUILD_SECTION, "extra") : QString());
}

void ZDLBuildTab::startRead(){
	emit readChildren(this);
	newConfig();
}

void ZDLBuildTab::writeConfig(){
	rebuild();
	emit buildChildren(this);
}
