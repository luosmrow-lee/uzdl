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
#include <QApplication>
#include <QStyleHints>
#include <QDateTime>
#include "ZDLAppSettingsTab.h"
#include "ZDLConfigurationManager.h"
#include "ZDLConfiguration.h"
#include "ZDLMainWindow.h"
#include "ZDLIdgamesTab.h"
#include "ZDLPaths.h"
#include "ZDLRazeSetup.h"
#include "ZDLFinderDialog.h"
#include "ZDLFileInspector.h"
#include "zdlconf.hpp"
#include "zdlcommon.h"

#if defined(Q_OS_WIN)&&!defined(_ZDL_NO_WFA)
#include <windows.h>
#include "ZDLFileAssociations.h"
#endif

extern ZDLMainWindow *mw;

namespace {

QString Value(ZDLConf *zconf, const char *key)
{
	return zconf&&zconf->hasValue("zdl.general", key)?zconf->getValue("zdl.general", key):QString();
}

//A path shown beside its buttons: selectable, and clipped rather than
//pushing the window wider.
QLabel *PathLabel(QWidget *parent)
{
	QLabel *label=new QLabel(parent);
	label->setTextInteractionFlags(Qt::TextSelectableByMouse);
	label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	return label;
}

QPushButton *Button(QWidget *parent, const QString &text, const QString &tip)
{
	QPushButton *button=new QPushButton(text, parent);
	button->setToolTip(tip);
	return button;
}

void OpenFolder(QWidget *parent, const QString &folder)
{
	if (folder.isEmpty()||!QDir(folder).exists()) {
		QMessageBox::information(parent, ZDL_APP_NAME, "That folder does not exist.");
		return;
	}
	QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

}

ZDLAppSettingsTab::ZDLAppSettingsTab(QWidget *parent): ZDLWidget(parent)
{
	loading=false;
	manualCheck=false;

	QVBoxLayout *outer=new QVBoxLayout(this);
	outer->setContentsMargins(0,0,0,0);
	QScrollArea *scroll=new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	//The first Tab goes to the first setting; the area still scrolls to
	//whatever takes the focus.
	scroll->setFocusPolicy(Qt::NoFocus);
	outer->addWidget(scroll);

	QWidget *page=new QWidget(scroll);
	QVBoxLayout *column=new QVBoxLayout(page);
	column->setContentsMargins(4,4,4,4);
	scroll->setWidget(page);

	//How uZDL behaves.
	QGroupBox *behaviour=new QGroupBox("Behaviour", page);
	QVBoxLayout *behaviourBox=new QVBoxLayout(behaviour);
	launchClose=new QCheckBox("Close uZDL when a game starts", behaviour);
	launchClose->setToolTip("Close uZDL completely once the source port has been started");
	savePaths=new QCheckBox("Remember the external file and mods lists between starts", behaviour);
	savePaths->setToolTip("Save the Doom Engine tab's external files and the Build Engine tab's mods on exit and load them at the next start");
	launchZDL=new QCheckBox("Start a .zdl file's game without showing uZDL", behaviour);
	launchZDL->setToolTip("When uZDL is started with a .zdl file, launch it straight away instead of opening the window");
	behaviourBox->addWidget(launchClose);
	behaviourBox->addWidget(savePaths);
	behaviourBox->addWidget(launchZDL);
	column->addWidget(behaviour);

	//How it looks.
	QGroupBox *appearance=new QGroupBox("Appearance", page);
	QVBoxLayout *appearanceBox=new QVBoxLayout(appearance);
	QHBoxLayout *themeRow=new QHBoxLayout();
	themeRow->addWidget(new QLabel("Theme", appearance));
	themeBox=new QComboBox(appearance);
	themeBox->addItem("System");
	themeBox->addItem("Light");
	themeBox->addItem("Dark");
	themeBox->setToolTip("System follows the desktop's own light or dark setting");
	themeRow->addWidget(themeBox);
	themeRow->addStretch(1);
	showPaths=new QCheckBox("Show file paths in the IWAD and source port lists", appearance);
	showPaths->setToolTip("Show each entry's folder in square brackets after its name");
	presetSort=new QCheckBox("Sort presets by name rather than in the order they were made", appearance);
	presetSort->setToolTip("Pinned presets stay at the top either way");
	appearanceBox->addLayout(themeRow);
	appearanceBox->addWidget(showPaths);
	appearanceBox->addWidget(presetSort);
	column->addWidget(appearance);

	//Where it keeps things.
	QGroupBox *files=new QGroupBox("Files and folders", page);
	QGridLayout *filesGrid=new QGridLayout(files);
	filesGrid->setColumnStretch(1, 1);
	int row=0;
	filesGrid->addWidget(new QLabel("Configuration", files), row, 0);
	configPath=PathLabel(files);
	filesGrid->addWidget(configPath, row, 1);
	QPushButton *btnConfigFolder=Button(files, "Open folder", "Open the folder the configuration file is in");
	filesGrid->addWidget(btnConfigFolder, row, 3);
	row++;
	portable=new QCheckBox("Portable mode: keep the configuration beside uZDL, so the folder moves as a whole", files);
	portable->setToolTip("The same switch as Portable mode in the uZDL menu");
	filesGrid->addWidget(portable, row, 0, 1, 4);
	row++;
	filesGrid->addWidget(new QLabel("PWAD library", files), row, 0);
	pwadPath=PathLabel(files);
	filesGrid->addWidget(pwadPath, row, 1);
	QPushButton *btnPwadChoose=Button(files, "Change...", "Pick the folder the PWAD library on the Doom Engine tab shows");
	QPushButton *btnPwadOpen=Button(files, "Open folder", "Open the PWAD library's folder");
	filesGrid->addWidget(btnPwadChoose, row, 2);
	filesGrid->addWidget(btnPwadOpen, row, 3);
	row++;
	filesGrid->addWidget(new QLabel("idgames mirror", files), row, 0);
	mirrorBox=new QComboBox(files);
	mirrorBox->addItems(ZDLIdgamesTab::mirrors());
	mirrorBox->setToolTip("Where the idgames tab gets the archive's listings and files from");
	filesGrid->addWidget(mirrorBox, row, 1, 1, 3);
	row++;
#if defined(Q_OS_WIN)&&!defined(_ZDL_NO_WFA)
	filesGrid->addWidget(new QLabel("File types", files), row, 0);
	QPushButton *btnAssoc=Button(files, "Associations...", "Open WADs, archives and .zdl files with uZDL from Explorer");
	filesGrid->addWidget(btnAssoc, row, 1, 1, 1, Qt::AlignLeft);
	connect(btnAssoc, SIGNAL(clicked()), this, SLOT(fileAssociations()));
	row++;
#endif
	column->addWidget(files);

	//Updates.
	QGroupBox *updates=new QGroupBox("Updates", page);
	QGridLayout *updatesGrid=new QGridLayout(updates);
	updatesGrid->setColumnStretch(0, 1);
	updateStatus=new QLabel(updates);
	updateStatus->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	updateOnStart=new QCheckBox("Check on start", updates);
	updateOnStart->setToolTip("Check at most once a day, and only say anything when there is a newer release");
	btnUpdate=new QPushButton("Check for updates", updates);
	skippedLabel=new QLabel(updates);
	btnForgetSkipped=Button(updates, "Forget the skipped release", "Hear about the release you skipped again at the next check");
	updatesGrid->addWidget(updateStatus, 0, 0);
	updatesGrid->addWidget(updateOnStart, 0, 1);
	updatesGrid->addWidget(btnUpdate, 0, 2);
	updatesGrid->addWidget(skippedLabel, 1, 0, 1, 2);
	updatesGrid->addWidget(btnForgetSkipped, 1, 2);
	column->addWidget(updates);

	//Raze.
	QGroupBox *raze=new QGroupBox("Raze", page);
	QVBoxLayout *razeBox=new QVBoxLayout(raze);
	QLabel *razeText=new QLabel("The Raze executables in the source port list. Setting one up keeps its settings and saves beside it, and adds the folders of the Build games to its search list.", raze);
	razeText->setWordWrap(true);
	razeList=new QTreeWidget(raze);
	razeList->setColumnCount(2);
	razeList->setHeaderLabels(QStringList()<<"Source port"<<"Set up for uZDL");
	razeList->setRootIsDecorated(false);
	razeList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	razeList->setMaximumHeight(120);
	QHBoxLayout *razeButtons=new QHBoxLayout();
	QPushButton *btnRazeSetUp=Button(raze, "Set up selected", "Make the selected Raze portable and add the game folders to its search list");
	QPushButton *btnRazeForget=Button(raze, "Forget the answers", "Ask again about every Raze at the next start");
	razeButtons->addWidget(btnRazeSetUp);
	razeButtons->addWidget(btnRazeForget);
	razeButtons->addStretch(1);
	razeBox->addWidget(razeText);
	razeBox->addWidget(razeList);
	razeBox->addLayout(razeButtons);
	column->addWidget(raze);

	//Fixes for faults in the games' own files.
	QGroupBox *patches=new QGroupBox("Game patches", page);
	QVBoxLayout *patchesBox=new QVBoxLayout(patches);
	QLabel *patchesText=new QLabel("Fixes for known faults in games' own files, the kind today's source ports stop on where the original game carried on. A fix only touches the exact version of a file it was written for, keeps the original beside it, and can be undone. The games in the IWAD list's folders are looked at.", patches);
	patchesText->setWordWrap(true);
	patchList=new QTreeWidget(patches);
	patchList->setColumnCount(4);
	patchList->setHeaderLabels(QStringList()<<"Game"<<"Fix"<<"State"<<"Folder");
	patchList->setRootIsDecorated(false);
	patchList->setMaximumHeight(120);
	QHBoxLayout *patchButtons=new QHBoxLayout();
	QPushButton *btnPatchApply=Button(patches, "Apply", "Fix the selected game's files, keeping the originals");
	QPushButton *btnPatchUndo=Button(patches, "Undo", "Put the selected game's original files back");
	QPushButton *btnPatchDescribe=Button(patches, "What it changes...", "What the selected fix is for and every line it changes");
	QPushButton *btnPatchRescan=Button(patches, "Look again", "Look for games with known fixes in the IWAD list's folders again");
	patchButtons->addWidget(btnPatchApply);
	patchButtons->addWidget(btnPatchUndo);
	patchButtons->addWidget(btnPatchDescribe);
	patchButtons->addStretch(1);
	patchButtons->addWidget(btnPatchRescan);
	patchesBox->addWidget(patchesText);
	patchesBox->addWidget(patchList);
	patchesBox->addLayout(patchButtons);
	column->addWidget(patches);
	connect(btnPatchApply, SIGNAL(clicked()), this, SLOT(applyPatch()));
	connect(btnPatchUndo, SIGNAL(clicked()), this, SLOT(undoPatch()));
	connect(btnPatchDescribe, SIGNAL(clicked()), this, SLOT(describePatch()));
	connect(btnPatchRescan, SIGNAL(clicked()), this, SLOT(fillPatches()));

	//Every feature of the Doom Engine tab's uZDL menu but the clearing of the tab,
	//which stays with the tab it clears.
	QGroupBox *tools=new QGroupBox("Tools", page);
	QGridLayout *toolsGrid=new QGridLayout(tools);
	QPushButton *btnFind=Button(tools, "Find installed games...", "Look where IWADs, Build games and source ports get installed, and add what you tick");
	QPushButton *btnInspect=Button(tools, "File information...", "What a WAD, GRP, archive, patch, demo or source port is and holds");
	QPushButton *btnCommand=Button(tools, "Show command line...", "The command line the Doom Engine tab's settings would start the source port with");
	QPushButton *btnLoadZdl=Button(tools, "Load .zdl...", "Load a launch file into the Doom Engine tab, or into the Build Engine tab when its port or game is the Build engine's");
	QPushButton *btnSaveZdl=Button(tools, "Save .zdl...", "Save the Doom Engine tab's settings as a launch file");
	QPushButton *btnLoadIni=Button(tools, "Load .ini...", "Use another configuration file");
	QPushButton *btnSaveIni=Button(tools, "Save .ini...", "Save the configuration to another file and use that one from now on");
	toolsGrid->addWidget(btnFind, 0, 0);
	toolsGrid->addWidget(btnInspect, 0, 1);
	toolsGrid->addWidget(btnCommand, 0, 2);
	toolsGrid->addWidget(btnLoadZdl, 1, 0);
	toolsGrid->addWidget(btnSaveZdl, 1, 1);
	toolsGrid->addWidget(btnLoadIni, 2, 0);
	toolsGrid->addWidget(btnSaveIni, 2, 1);
#if !defined(NO_IMPORT)
	QPushButton *btnImport=Button(tools, "Import current config...", "Bring in the settings of the ZDL found on this machine");
	toolsGrid->addWidget(btnImport, 1, 2);
	connect(btnImport, &QPushButton::clicked, this, [this]() { callInterface("importCurrentConfig"); });
#endif
	column->addWidget(tools);

	//What this is.
	QGroupBox *about=new QGroupBox("About", page);
	QHBoxLayout *aboutRow=new QHBoxLayout(about);
	QLabel *version=new QLabel(QString("%1 %2, Qt %3").arg(ZDL_APP_NAME, ZDL_VERSION_STRING, qVersion()), about);
	QPushButton *btnAppFolder=Button(about, "Open uZDL folder", "Open the folder uZDL runs from");
	QPushButton *btnAbout=Button(about, "About uZDL", "Version, authors and licence");
	aboutRow->addWidget(version, 1);
	aboutRow->addWidget(btnAppFolder);
	aboutRow->addWidget(btnAbout);
	column->addWidget(about);
	column->addStretch(1);

	connect(themeBox, SIGNAL(currentIndexChanged(int)), this, SLOT(themeChanged(int)));
	connect(showPaths, SIGNAL(toggled(bool)), this, SLOT(pathsToggled(bool)));
	connect(presetSort, SIGNAL(toggled(bool)), this, SLOT(presetSortToggled(bool)));
	//clicked and activated rather than toggled and changed: filling the
	//controls from the configuration must not look like the user acting.
	connect(portable, SIGNAL(clicked(bool)), this, SLOT(portableToggled(bool)));
	connect(mirrorBox, SIGNAL(activated(int)), this, SLOT(mirrorChanged(int)));
	connect(btnConfigFolder, SIGNAL(clicked()), this, SLOT(openConfigFolder()));
	connect(btnPwadChoose, SIGNAL(clicked()), this, SLOT(choosePwadFolder()));
	connect(btnPwadOpen, SIGNAL(clicked()), this, SLOT(openPwadFolder()));
	connect(btnUpdate, SIGNAL(clicked()), this, SLOT(checkForUpdates()));
	connect(btnForgetSkipped, SIGNAL(clicked()), this, SLOT(forgetSkipped()));
	connect(btnRazeSetUp, SIGNAL(clicked()), this, SLOT(setUpRaze()));
	connect(btnRazeForget, SIGNAL(clicked()), this, SLOT(forgetRaze()));
	connect(btnFind, &QPushButton::clicked, this, [this]() { ZDLFinderDialog::run(this, true, true); });
	connect(btnInspect, &QPushButton::clicked, this, [this]() { ZDLFileInspector::open(this); });
	connect(btnCommand, &QPushButton::clicked, this, [this]() { callInterface("showCommandline"); });
	connect(btnLoadZdl, &QPushButton::clicked, this, [this]() { callInterface("loadZdlFile"); });
	connect(btnSaveZdl, &QPushButton::clicked, this, [this]() { callInterface("saveZdlFile"); });
	connect(btnLoadIni, &QPushButton::clicked, this, [this]() { callInterface("loadConfigFile"); });
	connect(btnSaveIni, &QPushButton::clicked, this, [this]() { callInterface("saveConfigFile"); });
	connect(btnAppFolder, SIGNAL(clicked()), this, SLOT(openUzdlFolder()));
	connect(btnAbout, &QPushButton::clicked, this, [this]() { callInterface("aboutClick"); });

	updater=new ZDLUpdateCheck(this);
	selfUpdate=new ZDLSelfUpdate(this);
	connect(updater, &ZDLUpdateCheck::finished, this, &ZDLAppSettingsTab::updateCheckDone);
	connect(selfUpdate, &ZDLSelfUpdate::failed, this, &ZDLAppSettingsTab::selfUpdateFailed);

	//An update that just happened leaves its .old files for this start to
	//clear away, and its version to show.
	QString updated=ZDLSelfUpdate::finishPending(this);
	if (!updated.isEmpty())
		updateStatus->setText("Updated to "+updated);
}

void ZDLAppSettingsTab::callInterface(const char *slot)
{
	ZDLWidget *intr=ZDLConfigurationManager::getInterface();
	if (intr)
		QMetaObject::invokeMethod(intr, slot);
}

void ZDLAppSettingsTab::setShared(const QString &key, const QString &value)
{
	if (!mw)
		return;
	mw->writeConfig();
	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	if (!zconf)
		return;
	if (value.isEmpty())
		zconf->deleteValue("zdl.general", key);
	else
		zconf->setValue("zdl.general", key, value);
	mw->startRead();
}

//Qt follows the desktop's own light or dark setting by default; this only
//overrides that when the user asks for a fixed one.
void ZDLAppSettingsTab::applyTheme(const QString &theme)
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

void ZDLAppSettingsTab::themeChanged(int index)
{
	Q_UNUSED(index);
	if (loading)
		return;

	QString theme=themeBox->currentText().toLower();
	applyTheme(theme);

	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	if (zconf)
		zconf->setValue("zdl.general", "theme", theme);
}

//The lists draw their entries when they read the configuration, so the
//whole of uZDL reads it again.
void ZDLAppSettingsTab::pathsToggled(bool on)
{
	if (!loading)
		setShared("showpaths", on?"1":"0");
}

void ZDLAppSettingsTab::presetSortToggled(bool on)
{
	if (!loading)
		setShared("presetsort", on?"name":QString());
}

void ZDLAppSettingsTab::mirrorChanged(int index)
{
	if (!loading&&index>=0)
		setShared("idgamesmirror", mirrorBox->itemText(index));
}

//The same switch as the uZDL menu's. Everything is written first, so that
//what this tab holds goes with the configuration wherever it moves.
void ZDLAppSettingsTab::portableToggled(bool on)
{
	if (loading||!mw)
		return;
	mw->writeConfig();

	ZDLWidget *intr=ZDLConfigurationManager::getInterface();
	if (intr)
		QMetaObject::invokeMethod(intr, "togglePortable", Q_ARG(bool, on));

	//A refusal or a no leaves things as they were, and the box with them.
	ZDLConfiguration *conf=ZDLConfigurationManager::getConfiguration();
	loading=true;
	portable->setChecked(conf&&conf->isPortable());
	configPath->setText(QDir::toNativeSeparators(QFileInfo(ZDLConfigurationManager::getConfigFileName()).absoluteFilePath()));
	loading=false;
}

void ZDLAppSettingsTab::choosePwadFolder()
{
	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	QString current=ZDLPaths::resolve(Value(zconf, "pwaddir"));
	QString dir=QFileDialog::getExistingDirectory(this, "PWAD library folder", current.isEmpty()?ZDLPaths::appDir():current);
	if (!dir.isEmpty())
		setShared("pwaddir", ZDLPaths::preferRelative(QDir::fromNativeSeparators(dir)));
}

void ZDLAppSettingsTab::openPwadFolder()
{
	QString folder=ZDLPaths::resolve(Value(ZDLConfigurationManager::getActiveConfiguration(), "pwaddir"));
	if (folder.isEmpty()) {
		QMessageBox::information(this, ZDL_APP_NAME, "No PWAD library folder is chosen yet.");
		return;
	}
	OpenFolder(this, folder);
}

void ZDLAppSettingsTab::openConfigFolder()
{
	OpenFolder(this, QFileInfo(ZDLConfigurationManager::getConfigFileName()).absolutePath());
}

void ZDLAppSettingsTab::openUzdlFolder()
{
	OpenFolder(this, ZDLPaths::appDir());
}

void ZDLAppSettingsTab::fileAssociations()
{
#if defined(Q_OS_WIN)&&!defined(_ZDL_NO_WFA)
	ZDLFileAssociations assoc(this);
	assoc.exec();
#endif
}

void ZDLAppSettingsTab::showSkipped()
{
	QString skipped=Value(ZDLConfigurationManager::getActiveConfiguration(), "skipversion");
	skippedLabel->setText(skipped.isEmpty()?QString("No release is being skipped."):QString("Release %1 is being skipped.").arg(skipped));
	btnForgetSkipped->setEnabled(!skipped.isEmpty());
}

void ZDLAppSettingsTab::forgetSkipped()
{
	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	if (zconf)
		zconf->deleteValue("zdl.general", "skipversion");
	showSkipped();
}

void ZDLAppSettingsTab::checkForUpdates()
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

void ZDLAppSettingsTab::updateCheckDone(bool available, const ZDLReleaseInfo &release, const QString &error)
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
			showSkipped();
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
void ZDLAppSettingsTab::selfUpdateFailed(const QString &error)
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

void ZDLAppSettingsTab::fillRaze()
{
	razeList->clear();

	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	ZDLSection *section=zconf?zconf->getSection("zdl.ports"):NULL;
	if (section) {
		QVector<ZDLLine*> files;
		section->getRegex("^p[0-9]+f$", files);
		foreach (ZDLLine *line, files) {
			QString stored=line->getValue();
			QString exe=ZDLPaths::resolve(stored);
			if (!ZDLRazeSetup::isRaze(exe))
				continue;

			QString var=line->getVariable();
			var[var.size()-1]='n';
			QString name=section->findVariable(var);
			QString state=ZDLRazeSetup::decision(zconf, exe);

			QTreeWidgetItem *item=new QTreeWidgetItem(razeList);
			item->setText(0, name.isEmpty()?QFileInfo(exe).fileName():name);
			item->setToolTip(0, QDir::toNativeSeparators(exe));
			item->setText(1, state=="done"?"Yes":state=="skip"?"Declined":"Not asked yet");
			item->setData(0, Qt::UserRole, stored);
		}
	}

	if (!razeList->topLevelItemCount()) {
		QTreeWidgetItem *item=new QTreeWidgetItem(razeList);
		item->setText(0, "No Raze in the source port list");
		item->setFlags(Qt::NoItemFlags);
	}
	razeList->resizeColumnToContents(0);
}

void ZDLAppSettingsTab::setUpRaze()
{
	QList<QTreeWidgetItem*> picked=razeList->selectedItems();
	if (picked.isEmpty()&&razeList->topLevelItemCount()==1)
		picked<<razeList->topLevelItem(0);

	QStringList stored;
	foreach (QTreeWidgetItem *item, picked) {
		QString path=item->data(0, Qt::UserRole).toString();
		if (!path.isEmpty())
			stored<<path;
	}
	if (stored.isEmpty()) {
		QMessageBox::information(this, ZDL_APP_NAME, "Select a Raze in the list first.");
		return;
	}

	//Collected first: each setup reads the configuration back in, which
	//fills the list anew.
	foreach (const QString &path, stored)
		ZDLRazeSetup::setUpNow(this, path);
	fillRaze();
}

void ZDLAppSettingsTab::fillPatches()
{
	patchList->clear();
	patchesShown=ZDLGamePatches::scan(ZDLConfigurationManager::getActiveConfiguration());

	foreach (const ZDLGamePatches::Found &found, patchesShown) {
		QTreeWidgetItem *item=new QTreeWidgetItem(patchList);
		item->setText(0, found.game);
		item->setText(1, found.title);
		item->setText(2, found.stateText);
		item->setText(3, QDir::toNativeSeparators(found.folder));
		item->setToolTip(2, found.stateText);
		item->setToolTip(3, QDir::toNativeSeparators(found.folder));
	}

	if (patchesShown.isEmpty()) {
		QTreeWidgetItem *item=new QTreeWidgetItem(patchList);
		item->setText(0, "No game in the IWAD list has a known fix");
		item->setFlags(Qt::NoItemFlags);
	}
	patchList->resizeColumnToContents(0);
	patchList->resizeColumnToContents(1);
	patchList->resizeColumnToContents(2);
}

const ZDLGamePatches::Found *ZDLAppSettingsTab::pickedPatch()
{
	QList<QTreeWidgetItem*> selected=patchList->selectedItems();
	QTreeWidgetItem *item=selected.isEmpty()?patchList->currentItem():selected.first();
	int row=item?patchList->indexOfTopLevelItem(item):-1;
	if (row<0&&patchesShown.size()==1)
		row=0;
	if (row<0||row>=patchesShown.size()) {
		QMessageBox::information(this, ZDL_APP_NAME, "Select a fix in the list first.");
		return NULL;
	}
	return &patchesShown[row];
}

void ZDLAppSettingsTab::applyPatch()
{
	const ZDLGamePatches::Found *found=pickedPatch();
	if (!found)
		return;
	QString error;
	if (!ZDLGamePatches::apply(*found, &error))
		QMessageBox::warning(this, ZDL_APP_NAME, QString("%1 was not fixed.\n\n%2").arg(found->game, error));
	fillPatches();
}

void ZDLAppSettingsTab::undoPatch()
{
	const ZDLGamePatches::Found *found=pickedPatch();
	if (!found)
		return;
	QString error;
	if (!ZDLGamePatches::undo(*found, &error))
		QMessageBox::warning(this, ZDL_APP_NAME, QString("The fix for %1 was not undone.\n\n%2").arg(found->game, error));
	fillPatches();
}

void ZDLAppSettingsTab::describePatch()
{
	const ZDLGamePatches::Found *found=pickedPatch();
	if (found)
		QMessageBox::information(this, ZDL_APP_NAME, ZDLGamePatches::describe(found->patch));
}

void ZDLAppSettingsTab::forgetRaze()
{
	ZDLRazeSetup::forgetDecisions(ZDLConfigurationManager::getActiveConfiguration());
	fillRaze();
}

void ZDLAppSettingsTab::rebuild()
{
	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	if (!zconf)
		return;

	zconf->setValue("zdl.general", "theme", themeBox->currentText().toLower());
	zconf->setValue("zdl.general", "updateonstart", updateOnStart->isChecked()?"1":"0");
	zconf->setValue("zdl.general", "autoclose", launchClose->isChecked()?"1":"0");
	zconf->setValue("zdl.general", "zdllaunch", launchZDL->isChecked()?"1":"0");
	zconf->setValue("zdl.general", "showpaths", showPaths->isChecked()?"1":"0");
	zconf->setValue("zdl.general", "rememberFilelist", savePaths->isChecked()?"1":"0");
}

void ZDLAppSettingsTab::newConfig()
{
	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	if (!zconf)
		return;

	loading=true;

	QString theme=Value(zconf, "theme");
	if (theme.isEmpty())
		theme="system";
	int themeIndex=themeBox->findText(theme, Qt::MatchFixedString);
	themeBox->setCurrentIndex(themeIndex>=0?themeIndex:0);
	applyTheme(theme);

	launchClose->setChecked(Value(zconf, "autoclose")=="1");
	launchZDL->setChecked(Value(zconf, "zdllaunch")=="1");
	savePaths->setChecked(Value(zconf, "rememberFilelist")!="0");
	showPaths->setChecked(Value(zconf, "showpaths")!="0");
	presetSort->setChecked(Value(zconf, "presetsort")=="name");

	ZDLConfiguration *conf=ZDLConfigurationManager::getConfiguration();
	portable->setChecked(conf&&conf->isPortable());
	configPath->setText(QDir::toNativeSeparators(QFileInfo(ZDLConfigurationManager::getConfigFileName()).absoluteFilePath()));
	QString pwaddir=Value(zconf, "pwaddir");
	pwadPath->setText(pwaddir.isEmpty()?QString("None chosen yet"):QDir::toNativeSeparators(ZDLPaths::resolve(pwaddir)));
	int mirrorIndex=mirrorBox->findText(Value(zconf, "idgamesmirror"));
	mirrorBox->setCurrentIndex(mirrorIndex>=0?mirrorIndex:0);

	bool onStart=Value(zconf, "updateonstart")=="1";
	updateOnStart->setChecked(onStart);
	showSkipped();
	fillRaze();
	fillPatches();

	loading=false;

	//At most once a day, and silent unless there is something to say:
	//updateCheckDone only raises a dialog when a newer release exists.
	if (onStart&&!updater->busy()) {
		qint64 last=Value(zconf, "lastupdatecheck").toLongLong();
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
