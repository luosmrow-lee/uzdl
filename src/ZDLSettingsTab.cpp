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

#include <QStyleHints>
#include <QDateTime>
#include "ZDLConfigurationManager.h"
#include "ZDLListWidget.h"
#include "ZDLSettingsTab.h"
#include "ZDLQSplitter.h"

#if defined(Q_OS_WIN)&&!defined(_ZDL_NO_WFA)
#include <windows.h>
#include "ZDLFileAssociations.h"
#endif

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
	
	launchClose = new QCheckBox("Close on launch",this);
	launchClose->setToolTip("Close ZDL completely when launching a new game");

	showPaths = new QCheckBox("Show file paths in lists",this);
	showPaths->setToolTip("Show the directory path in square brackets in list widgets");
	connect(showPaths,SIGNAL(stateChanged(int)),this,SLOT(pathToggled(int)));
	sections->addWidget(alwaysArgs);

	QHBoxLayout *fileassoc = new QHBoxLayout();
	launchZDL = new QCheckBox("Launch *.ZDL files transparently", this);
	launchZDL->setToolTip("If a .ZDL file is specified on the command line path, launch the configuration without showing the interface");
	fileassoc->addWidget(launchZDL);
	
#if defined(Q_OS_WIN)&&!defined(_ZDL_NO_WFA)
	QPushButton *assoc = new QPushButton("Associations", this);
	assoc->setToolTip("Associate various file types with ZDL");
	fileassoc->addWidget(assoc);
	connect(assoc, SIGNAL(clicked()), this, SLOT(fileAssociations()));
#endif
	
	savePaths = new QCheckBox("Remember external file list", this);
	savePaths->setToolTip("Save external file list on exit and load it on next program launch");

	sections->addLayout(fileassoc);
	QHBoxLayout *themeRow = new QHBoxLayout();
	themeRow->addWidget(new QLabel("Theme", this));
	themeBox = new QComboBox(this);
	themeBox->addItem("System");
	themeBox->addItem("Light");
	themeBox->addItem("Dark");
	themeBox->setToolTip("System follows the desktop's own light or dark setting");
	themeRow->addWidget(themeBox);

	updateStatus = new QLabel(this);
	updateStatus->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	updateOnStart = new QCheckBox("Check on start", this);
	updateOnStart->setToolTip("Check at most once a day, and only say anything when there is a newer release");
	btnUpdate = new QPushButton("Check for updates", this);

	themeRow->addWidget(updateStatus, 1);
	themeRow->addWidget(updateOnStart);
	themeRow->addWidget(btnUpdate);

	updater = new ZDLUpdateCheck(this);

	connect(themeBox, SIGNAL(currentIndexChanged(int)), this, SLOT(themeChanged(int)));
	connect(btnUpdate, SIGNAL(clicked()), this, SLOT(checkForUpdates()));
	selfUpdate = new ZDLSelfUpdate(this);
	manualCheck = false;
	connect(updater, &ZDLUpdateCheck::finished, this, &ZDLSettingsTab::updateCheckDone);
	connect(selfUpdate, &ZDLSelfUpdate::failed, this, &ZDLSettingsTab::selfUpdateFailed);

	//An update that just happened leaves its .old files for this start to
	//clear away, and its version to show.
	QString updated=ZDLSelfUpdate::finishPending(this);
	if (!updated.isEmpty())
		updateStatus->setText("Updated to "+updated);

	sections->addWidget(split);
	sections->addLayout(themeRow);
	sections->addWidget(launchClose);
	sections->addWidget(showPaths);
	sections->addWidget(savePaths);
	setContentsMargins(4,4,4,4);
	layout()->setContentsMargins(0,0,0,0);
}

void ZDLSettingsTab::pathToggled(int state)
{
	Q_UNUSED(state);

	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	if(showPaths->checkState()==Qt::Checked)
		zconf->setValue("zdl.general", "showpaths", "1");
	else
		zconf->setValue("zdl.general", "showpaths", "0");
	iwadList->newConfig();
	sourceList->newConfig();
}

void ZDLSettingsTab::fileAssociations()
{
#if defined(Q_OS_WIN)&&!defined(_ZDL_NO_WFA)
	ZDLFileAssociations assoc(this);
	assoc.exec();
#endif
}

//Qt follows the desktop's own light or dark setting by default; this only
//overrides that when the user asks for a fixed one.
void ZDLSettingsTab::applyTheme(const QString &theme)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
	if (!theme.compare("light", Qt::CaseInsensitive))
		qApp->styleHints()->setColorScheme(Qt::ColorScheme::Light);
	else if (!theme.compare("dark", Qt::CaseInsensitive))
		qApp->styleHints()->setColorScheme(Qt::ColorScheme::Dark);
	else
		qApp->styleHints()->unsetColorScheme();
#else
	Q_UNUSED(theme);
#endif
}

void ZDLSettingsTab::themeChanged(int index)
{
	Q_UNUSED(index);

	QString theme=themeBox->currentText().toLower();
	applyTheme(theme);

	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	if (zconf)
		zconf->setValue("zdl.general", "theme", theme);
}

void ZDLSettingsTab::checkForUpdates()
{
	if (updater->busy())
		return;

	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	QString repo=zconf&&zconf->hasValue("zdl.general", "updaterepo")?zconf->getValue("zdl.general", "updaterepo"):QString(ZDL_UPDATE_REPO);

	if (repo.trimmed().isEmpty()) {
		QMessageBox::information(this, ZDL_APP_NAME,
			QString("No update repository is set. ")+
			"Put the GitHub project this build should watch into the configuration as "+
			"updaterepo under [zdl.general], as owner/repo, and this button will ask it "+
			"for its latest release.");
		return;
	}

	btnUpdate->setEnabled(false);
	manualCheck=true;
	updateStatus->setText("Checking...");
	updater->check(repo);

	if (zconf)
		zconf->setValue("zdl.general", "lastupdatecheck", QString::number(QDateTime::currentSecsSinceEpoch()));
}

void ZDLSettingsTab::updateCheckDone(bool available, const ZDLReleaseInfo &release, const QString &error)
{
	btnUpdate->setEnabled(true);

	if (!error.isEmpty()) {
		updateStatus->setText(error);
		updateStatus->setToolTip(error);
		return;
	}

	lastRelease=release;
	QString version=release.version();

	if (!available) {
		updateStatus->setText(QString("Up to date (")+ZDL_VERSION_STRING+")");
		updateStatus->setToolTip(QString("Latest release is ")+version);
		return;
	}

	updateStatus->setText(QString("Update available: ")+version);
	updateStatus->setToolTip(release.url);

	//A release the user asked to hear no more about stays quiet on the daily
	//check. Pressing the button is asking, so that always gets the dialog.
	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	QString skipped=zconf&&zconf->hasValue("zdl.general", "skipversion")?zconf->getValue("zdl.general", "skipversion"):QString();
	if (!manualCheck&&skipped==release.tag) {
		updateStatus->setText(QString("Update available: ")+version+" (skipped)");
		return;
	}

	QString whyNot;
	bool canInstall=ZDLSelfUpdate::canUpdate(release, &whyNot);

	ZDLUpdateDialog dialog(this, release, canInstall, whyNot);
	dialog.exec();

	switch (dialog.choice()) {
		case ZDLUpdateDialog::Install:
			updateStatus->setText(QString("Updating to ")+version+"...");
			btnUpdate->setEnabled(false);
			selfUpdate->run(release);
			break;
		case ZDLUpdateDialog::Skip:
			if (zconf)
				zconf->setValue("zdl.general", "skipversion", release.tag);
			updateStatus->setText(QString("Update available: ")+version+" (skipped)");
			break;
		case ZDLUpdateDialog::OpenPage:
			if (!release.url.isEmpty())
				QDesktopServices::openUrl(QUrl(release.url));
			break;
		default:
			break;
	}
}

//An empty error is the user cancelling, which needs no dialog of its own.
void ZDLSettingsTab::selfUpdateFailed(const QString &error)
{
	btnUpdate->setEnabled(true);

	if (error.isEmpty()) {
		updateStatus->setText("Update cancelled");
		return;
	}

	updateStatus->setText("Update failed");
	updateStatus->setToolTip(error);

	if (QMessageBox::question(this, ZDL_APP_NAME,
			"The update could not be installed:\n\n"+error+"\n\nOpen the release page to get it by hand?",
			QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes)==QMessageBox::Yes&&!lastRelease.url.isEmpty())
		QDesktopServices::openUrl(QUrl(lastRelease.url));
}

void ZDLSettingsTab::rebuild(){
	saveSplitterSizes("splitsettings", listSplit);

	{
		ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
		if (zconf) {
			zconf->setValue("zdl.general", "theme", themeBox->currentText().toLower());
			zconf->setValue("zdl.general", "updateonstart", updateOnStart->isChecked()?"1":"0");
		}
	}
	ZDLConf *zconf = ZDLConfigurationManager::getActiveConfiguration();
	
	if(launchClose->checkState() == Qt::Checked){
		zconf->setValue("zdl.general","autoclose", "1");
	}else{
		zconf->setValue("zdl.general","autoclose", "0");
	}
	if(launchZDL->checkState() == Qt::Checked){
		zconf->setValue("zdl.general","zdllaunch", "1");
	}else{
		zconf->setValue("zdl.general","zdllaunch", "0");
	}
	if(alwaysArgs->text().isEmpty()){
		zconf->deleteValue("zdl.general", "alwaysadd");
	}else{
		zconf->setValue("zdl.general", "alwaysadd", alwaysArgs->text());
	}
	if(showPaths->checkState() == Qt::Checked){
		zconf->setValue("zdl.general", "showpaths", "1");
	}else{
		zconf->setValue("zdl.general", "showpaths", "0");
	}
	if(savePaths->checkState() == Qt::Checked){
		zconf->setValue("zdl.general", "rememberFilelist", "1");
	}else{
		zconf->setValue("zdl.general", "rememberFilelist", "0");
	}
}

void ZDLSettingsTab::newConfig(){
	restoreSplitterSizes("splitsettings", listSplit);

	{
		ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
		if (zconf) {
			QString theme=zconf->hasValue("zdl.general", "theme")?zconf->getValue("zdl.general", "theme"):QString("system");
			int idx=themeBox->findText(theme, Qt::MatchFixedString);

			//Applied directly rather than through the combo, so loading a
			//configuration does not look like the user picking a theme.
			themeBox->blockSignals(true);
			themeBox->setCurrentIndex(idx>=0?idx:0);
			themeBox->blockSignals(false);
			applyTheme(theme);

			bool on_start=zconf->hasValue("zdl.general", "updateonstart")&&zconf->getValue("zdl.general", "updateonstart")=="1";
			updateOnStart->setChecked(on_start);

			//At most once a day, and silent unless there is something to say:
			//updateCheckDone only raises a dialog when a newer release exists.
			if (on_start&&!updater->busy()) {
				qint64 last=zconf->hasValue("zdl.general", "lastupdatecheck")?zconf->getValue("zdl.general", "lastupdatecheck").toLongLong():0;
				qint64 now=QDateTime::currentSecsSinceEpoch();
				QString repo=zconf->hasValue("zdl.general", "updaterepo")?zconf->getValue("zdl.general", "updaterepo"):QString(ZDL_UPDATE_REPO);

				if (!repo.trimmed().isEmpty()&&now-last>24*60*60) {
					zconf->setValue("zdl.general", "lastupdatecheck", QString::number(now));
					manualCheck=false;
					updateStatus->setText("Checking...");
					updater->check(repo);
				}
			}
		}
	}
	ZDLConf *zconf = ZDLConfigurationManager::getActiveConfiguration();

	if(zconf->hasValue("zdl.general","showpaths")){
		int ok = 0;
		QString setting = zconf->getValue("zdl.general","showpaths", &ok);
		if(!setting.isNull()){
			if(setting == "0"){
				showPaths->setCheckState(Qt::Unchecked);
			}else{
				showPaths->setCheckState(Qt::Checked);
			}
		}else{
			showPaths->setCheckState(Qt::Checked);
		}
	}else{
		showPaths->setCheckState(Qt::Checked);
	}

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
	
	if(zconf->hasValue("zdl.general","autoclose")){
		int ok;
		QString closeSetting = zconf->getValue("zdl.general","autoclose",&ok);
		if(closeSetting == "1"){
			launchClose->setCheckState(Qt::Checked);
		}else{
			launchClose->setCheckState(Qt::Unchecked);
		}
	}else{
		launchClose->setCheckState(Qt::Unchecked);
	}

	if(zconf->hasValue("zdl.general","zdllaunch")){
		int ok;
		QString closeSetting = zconf->getValue("zdl.general","zdllaunch",&ok);
		if(closeSetting == "1"){
			launchZDL->setCheckState(Qt::Checked);
		}else{
			launchZDL->setCheckState(Qt::Unchecked);
		}
	}else{
		launchZDL->setCheckState(Qt::Unchecked);
	}
	bool rememberFilelist = true;
	if(zconf->hasValue("zdl.general", "rememberFilelist")){
		int ok;
		QString val = zconf->getValue("zdl.general", "rememberFilelist", &ok);
		if(val == "0"){
			rememberFilelist = false;
		}
	}
	if (rememberFilelist){
		savePaths->setCheckState(Qt::Checked);
	}else{
		savePaths->setCheckState(Qt::Unchecked);
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
