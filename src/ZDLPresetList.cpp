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

#include <algorithm>
#include "ZDLPresetList.h"
#include "ZDLPresetDialog.h"
#include "ZDLBuildArgs.h"
#include "ZDLLaunchFiles.h"
#include "ZDLShortcut.h"
#include "ZDLConfigurationManager.h"
#include "ZDLMainWindow.h"

extern ZDLMainWindow *mw;

//Section the highlighted item stands for, e.g. "zdl.mix2".
#define PRESET_SECTION_ROLE (Qt::UserRole+1)

static QString Stored(ZDLConf *zconf, const QString &section, const char *key)
{
	return zconf->hasValue(section, key)?zconf->getValue(section, key):QString();
}

//Port, IWAD, save folder and config file as the editor left them.
static void StoreChoices(ZDLConf *zconf, const QString &section, ZDLPresetDialog &dialog)
{
	ZDLLaunchFiles::storeChoices(zconf, section, dialog.presetPort(), dialog.presetIwad(), dialog.presetSaveDir(), dialog.presetConfig());
}

ZDLPresetList::ZDLPresetList(QWidget *parent, const QString &launch, bool buildEngine): ZDLWidget(parent)
{
	LOGDATAO() << "New ZDLPresetList" << Qt::endl;
	target=launch.isEmpty()?QString(ZDLLaunchFiles::LAUNCH_SECTION):launch;
	build=buildEngine;

	QVBoxLayout *column=new QVBoxLayout(this);
	column->setSpacing(2);

	column->addWidget(new QLabel("Presets", this));

	pList=new QListWidget(this);
	pList->setSelectionMode(QAbstractItemView::SingleSelection);
	pList->setContextMenuPolicy(Qt::CustomContextMenu);
	pList->setToolTip(build
		?"Named ports, games and mods. Double click to load one;\nright click to add its mods to the list, pin it, sort, export and more."
		:"Named external file lists. Double click to load one;\nright click to add it to the list, pin it, sort, export and more.");
	//Keep the pane from demanding the width of its longest preset name.
	pList->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
	pList->setMinimumHeight(80);
	column->addWidget(pList);

	QHBoxLayout *buttonRow=new QHBoxLayout();

	QPushButton *btnEdit=new QPushButton("Edit", this);
	btnEdit->setToolTip("Rename this preset and change which files it holds");

	QPushButton *btnLaunch=new QPushButton(build?"Launch preset":"Launch", this);
	btnLaunch->setToolTip("Load this preset and start the game with it");

	QPushButton *btnDelete=new QPushButton("Delete", this);
	btnDelete->setToolTip("Delete this preset. The files themselves are not touched.");

	QPushButton *btnImport=new QPushButton("Import", this);
	btnImport->setToolTip("Make a preset from a .zdl file");

	//A desktop shortcut, or a Steam entry, that starts straight into the
	//preset. Only where a shortcut format is implemented.
	QPushButton *btnShortcut=ZDLShortcut::supported()?new QPushButton("Shortcut", this):NULL;
	if (btnShortcut)
		btnShortcut->setToolTip("Make a desktop shortcut that starts the game with this preset");

	//Load and Add live in the context menu and on a double click: as buttons
	//they crowded the row past what the pane's width can hold.
	buttonRow->addWidget(btnEdit);
	buttonRow->addWidget(btnDelete);
	buttonRow->addWidget(btnImport);
	if (btnShortcut)
		buttonRow->addWidget(btnShortcut);
	buttonRow->setSpacing(0);
	if (build) {
		//The Build Engine tab's presets have a column to themselves, with
		//height to spare and less width than five buttons need: Launch goes
		//under the rest, across the column, as Save as preset does under the
		//mods beside it.
		column->addLayout(buttonRow);
		column->addWidget(btnLaunch);
	} else {
		buttonRow->addStretch();
		buttonRow->addWidget(btnLaunch);
		column->addLayout(buttonRow);
	}

	column->setContentsMargins(0,0,0,0);
	setContentsMargins(0,0,0,0);

	QObject::connect(btnEdit, SIGNAL(clicked()), this, SLOT(editPreset()));
	QObject::connect(btnDelete, SIGNAL(clicked()), this, SLOT(deletePreset()));
	QObject::connect(btnImport, SIGNAL(clicked()), this, SLOT(importPreset()));
	if (btnShortcut)
		QObject::connect(btnShortcut, SIGNAL(clicked()), this, SLOT(shortcutPreset()));
	QObject::connect(btnLaunch, SIGNAL(clicked()), this, SLOT(launchPreset()));
	QObject::connect(pList, SIGNAL(itemActivated(QListWidgetItem*)), this, SLOT(itemActivated(QListWidgetItem*)));
	QObject::connect(pList, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showMenu(QPoint)));
}

void ZDLPresetList::rebuild()
{
	//Presets are written the moment they are created, edited or deleted, so
	//there is nothing to flush here.
}

void ZDLPresetList::newConfig()
{
	QString previous=currentSection();

	pList->clear();

	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	if (!zconf)
		return;

	struct Row {
		QString section;
		QString name;
		int files;
		bool pinned;
	};
	QList<Row> rows;

	foreach (const QString &section, ZDLLaunchFiles::presetSections(zconf)) {
		if (ZDLBuildArgs::isBuildLaunch(zconf, section)!=build)
			continue;
		Row row;
		row.section=section;
		row.name=zconf->hasValue(section, "name")?zconf->getValue(section, "name"):section;
		row.files=ZDLLaunchFiles::read(zconf, section).size();
		row.pinned=zconf->hasValue(section, "pinned")&&zconf->getValue(section, "pinned")=="1";
		rows<<row;
	}

	//Pinned first. After that either by name or, when two rows tie, in the
	//order they came, which is the order the presets were made in; the
	//sort is stable, so that order survives.
	bool byName=sortByName();
	std::stable_sort(rows.begin(), rows.end(), [byName](const Row &l, const Row &r) {
		if (l.pinned!=r.pinned)
			return l.pinned;
		if (!byName)
			return false;
		return l.name.compare(r.name, Qt::CaseInsensitive)<0;
	});

	foreach (const Row &row, rows) {
		QString text=QString("%1 (%2)").arg(row.name).arg(row.files);
		if (row.pinned)
			text.prepend("\342\230\205 ");

		QListWidgetItem *item=new QListWidgetItem(text, pList);
		item->setData(PRESET_SECTION_ROLE, row.section);

		if (row.section==previous)
			pList->setCurrentItem(item);
	}
}

bool ZDLPresetList::sortByName()
{
	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	return zconf&&zconf->hasValue("zdl.general", "presetsort")&&zconf->getValue("zdl.general", "presetsort")=="name";
}

void ZDLPresetList::setSortByName(bool byName)
{
	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	if (!zconf)
		return;

	if (byName)
		zconf->setValue("zdl.general", "presetsort", "name");
	else
		zconf->deleteValue("zdl.general", "presetsort");
}

bool ZDLPresetList::currentPinned()
{
	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	QString section=currentSection();

	return zconf&&!section.isEmpty()&&zconf->hasValue(section, "pinned")&&zconf->getValue(section, "pinned")=="1";
}

void ZDLPresetList::togglePin()
{
	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	QString section=currentSection();

	if (!zconf||section.isEmpty())
		return;

	if (currentPinned())
		zconf->deleteValue(section, "pinned");
	else
		zconf->setValue(section, "pinned", "1");

	mw->saveNow();
	newConfig();
}

QString ZDLPresetList::currentSection()
{
	QListWidgetItem *item=pList->currentItem();
	return item?item->data(PRESET_SECTION_ROLE).toString():QString();
}

QString ZDLPresetList::currentPort()
{
	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	QString section=currentSection();

	if (!zconf||section.isEmpty()||!zconf->hasValue(section, "port"))
		return QString();

	return zconf->getValue(section, "port");
}

QString ZDLPresetList::currentIwad()
{
	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	QString section=currentSection();

	if (!zconf||section.isEmpty()||!zconf->hasValue(section, "iwad"))
		return QString();

	return zconf->getValue(section, "iwad");
}

QString ZDLPresetList::currentName()
{
	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	QString section=currentSection();

	if (!zconf||section.isEmpty())
		return QString();

	return zconf->hasValue(section, "name")?zconf->getValue(section, "name"):section;
}

void ZDLPresetList::editPreset()
{
	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	QString section=currentSection();

	if (!zconf||section.isEmpty()) {
		QMessageBox::information(this, ZDL_APP_NAME, "Select a preset to edit.");
		return;
	}

	ZDLPresetDialog dialog(this, currentName(), currentPort(), currentIwad(), ZDLLaunchFiles::read(zconf, section), target, build);
	dialog.setKeepApart(Stored(zconf, section, "savedir"), Stored(zconf, section, "config"));
	//The dialog will not close without a name, so there is nothing to check.
	if (dialog.exec()!=QDialog::Accepted)
		return;

	zconf->setValue(section, "name", dialog.presetName());
	StoreChoices(zconf, section, dialog);
	//Still this tab's with the port and game taken out.
	ZDLLaunchFiles::markBuild(zconf, section, build);
	ZDLLaunchFiles::write(zconf, section, dialog.presetEntries());

	//Written out now, for a shortcut to it started while uZDL is open.
	mw->saveNow();
	newConfig();
}

void ZDLPresetList::send(bool replace)
{
	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	QString section=currentSection();

	if (!zconf||section.isEmpty()) {
		QMessageBox::information(this, ZDL_APP_NAME, "Select a preset first.");
		return;
	}

	//Appending is about files only: it must not silently change the port
	//under a configuration the user assembled by hand.
	if (replace)
		ZDLLaunchFiles::loadPreset(zconf, section, target);
	else
		ZDLLaunchFiles::sendToLaunch(ZDLLaunchFiles::read(zconf, section), false, QString(), QString(), QString(), QString(), target);
}

void ZDLPresetList::loadPreset()
{
	send(true);
}

void ZDLPresetList::appendPreset()
{
	send(false);
}

void ZDLPresetList::launchPreset()
{
	if (currentSection().isEmpty()) {
		QMessageBox::information(this, ZDL_APP_NAME, "Select a preset first.");
		return;
	}

	send(true);
	mw->launchFrom(target);
}

void ZDLPresetList::itemActivated(QListWidgetItem *item)
{
	if (item)
		send(true);
}

void ZDLPresetList::deletePreset()
{
	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	QString section=currentSection();

	if (!zconf||section.isEmpty()) {
		QMessageBox::information(this, ZDL_APP_NAME, "Select a preset to delete.");
		return;
	}

	if (QMessageBox::warning(this, ZDL_APP_NAME, QString("Delete the preset \"%1\"? The files themselves are not touched.").arg(currentName()),
			QMessageBox::Yes|QMessageBox::No, QMessageBox::No)!=QMessageBox::Yes)
		return;

	zconf->deleteSectionByName(section);
	mw->saveNow();
	newConfig();
}

void ZDLPresetList::exportPreset()
{
	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	QString section=currentSection();

	if (!zconf||section.isEmpty())
		return;

	QString filters=
		"ZDL files (*.zdl);;"
		"All files (" QFD_FILTER_ALL ")";

	QString fileName=QFileDialog::getSaveFileName(this, "Export preset", getZdlLastDir(), filters);
	if (fileName.isNull()||fileName.isEmpty())
		return;

	if (!fileName.contains("."))
		fileName+=".zdl";

	//A preset is stored in the same layout as zdl.save, so exporting is just
	//a matter of writing those entries out under that section name. The
	//port, IWAD, save folder and config file it names go along, since import
	//reads them back and a .zdl given on the command line honours them.
	ZDLConf out;
	ZDLLaunchFiles::write(&out, ZDLLaunchFiles::LAUNCH_SECTION, ZDLLaunchFiles::read(zconf, section));
	static const char *carried[]={"port", "iwad", "savedir", "config"};
	for (size_t i=0; i<sizeof(carried)/sizeof(carried[0]); i++) {
		if (zconf->hasValue(section, carried[i]))
			out.setValue(ZDLLaunchFiles::LAUNCH_SECTION, carried[i], zconf->getValue(section, carried[i]));
	}

	saveZdlLastDir(fileName);

	if (out.writeINI(fileName)!=0)
		QMessageBox::critical(this, ZDL_APP_NAME, QString("Unable to write ")+fileName);
}

//The mirror image of exportPreset: a .zdl is a zdl.save section on its own,
//so the reader that serves the launch tab reads it too, port and IWAD
//included when the file has them.
void ZDLPresetList::importPreset()
{
	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	if (!zconf)
		return;

	QString filters=
		"ZDL files (*.zdl);;"
		"All files (" QFD_FILTER_ALL ")";

	QString fileName=QFileDialog::getOpenFileName(this, "Import preset", getZdlLastDir(), filters);
	if (fileName.isEmpty())
		return;

	saveZdlLastDir(fileName);

	ZDLConf zdl;
	if (zdl.readINI(fileName)!=0) {
		QMessageBox::critical(this, ZDL_APP_NAME, QString("Unable to read ")+fileName);
		return;
	}

	QList<ZDLFileEntry> entries=ZDLLaunchFiles::read(&zdl, ZDLLaunchFiles::LAUNCH_SECTION);
	QString port=zdl.hasValue(ZDLLaunchFiles::LAUNCH_SECTION, "port")?zdl.getValue(ZDLLaunchFiles::LAUNCH_SECTION, "port"):QString();
	QString iwad=zdl.hasValue(ZDLLaunchFiles::LAUNCH_SECTION, "iwad")?zdl.getValue(ZDLLaunchFiles::LAUNCH_SECTION, "iwad"):QString();

	if (entries.isEmpty()&&port.isEmpty()&&iwad.isEmpty()) {
		QMessageBox::information(this, ZDL_APP_NAME, "That file holds no external files, source port or IWAD.");
		return;
	}

	//The editor offers this tab's ports and games only, and would drop the
	//other engine's without a word.
	if (ZDLBuildArgs::isBuildChoice(zconf, port, iwad, build)!=build) {
		QMessageBox::information(this, ZDL_APP_NAME, QString("That file is for the %1 tab. Import it there.").arg(build?"Doom Engine":"Build Engine"));
		return;
	}

	//Opened in the editor rather than stored blind, so the name - the file's
	//own by default - and the contents can be adjusted first.
	ZDLPresetDialog dialog(this, QFileInfo(fileName).completeBaseName(), port, iwad, entries, target, build);
	dialog.setKeepApart(Stored(&zdl, ZDLLaunchFiles::LAUNCH_SECTION, "savedir"), Stored(&zdl, ZDLLaunchFiles::LAUNCH_SECTION, "config"));
	if (dialog.exec()!=QDialog::Accepted)
		return;

	QString section=ZDLLaunchFiles::newPresetSection(zconf);
	zconf->setValue(section, "name", dialog.presetName());
	StoreChoices(zconf, section, dialog);
	ZDLLaunchFiles::markBuild(zconf, section, build);
	ZDLLaunchFiles::write(zconf, section, dialog.presetEntries());

	mw->saveNow();
	newConfig();
}

//A shortcut file that starts uZDL with --preset and this preset's name, put
//wherever the user says; the desktop is offered first.
void ZDLPresetList::shortcutPreset()
{
	QString name=currentName();
	if (name.isEmpty()) {
		QMessageBox::information(this, ZDL_APP_NAME, "Select a preset first.");
		return;
	}

	QString ext=ZDLShortcut::extension();
	QString suggested=QDir(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)).filePath(ZDLShortcut::safeFileName(name)+"."+ext);
	QString filters=QString("%1 (*.%2);;All files (" QFD_FILTER_ALL ")").arg(ext=="lnk"?"Shortcuts":"Desktop entries").arg(ext);

	//DontResolveSymlinks: on Windows, picking an existing shortcut to replace
	//would otherwise hand back the file that shortcut points at.
	QString fileName=QFileDialog::getSaveFileName(this, "Create shortcut", suggested, filters, NULL, QFileDialog::DontResolveSymlinks);
	if (fileName.isEmpty())
		return;
	if (!fileName.endsWith("."+ext, Qt::CaseInsensitive))
		fileName+="."+ext;

	QString error;
	if (!ZDLShortcut::create(fileName, name, &error)) {
		QMessageBox::critical(this, ZDL_APP_NAME, "The shortcut could not be made: "+error);
		return;
	}

	//The shortcut starts another uZDL, which reads the configuration file:
	//the preset has to be in it already, not only in this window.
	if (!mw->saveNow())
		QMessageBox::warning(this, ZDL_APP_NAME, QString("The shortcut was made, but the configuration could not be written to\n%1\nso the shortcut finds the preset only once uZDL has closed and saved it.")
			.arg(QDir::toNativeSeparators(QFileInfo(ZDLConfigurationManager::getConfigFileName()).absoluteFilePath())));
}

//Everything the pane can do with a preset, on a right click. On empty space
//only what does not need one: the sort order.
void ZDLPresetList::showMenu(const QPoint &pos)
{
	QListWidgetItem *item=pList->itemAt(pos);
	if (item)
		pList->setCurrentItem(item);

	QMenu menu(this);
	QAction *load=NULL, *append=NULL, *pin=NULL, *edit=NULL, *xport=NULL, *shortcut=NULL, *remove=NULL;

	if (item) {
		load=menu.addAction("Load");
		append=menu.addAction(build?"Add to mods":"Add to external files");
		menu.addSeparator();
		pin=menu.addAction(currentPinned()?"Unpin":"Pin to top");
		edit=menu.addAction("Edit...");
		xport=menu.addAction("Export as .zdl...");
		if (ZDLShortcut::supported())
			shortcut=menu.addAction("Create shortcut...");
		menu.addSeparator();
		remove=menu.addAction("Delete");
		menu.addSeparator();
	}

	QAction *byName=menu.addAction("Sort by name");
	byName->setCheckable(true);
	byName->setChecked(sortByName());

	//The position arrives in viewport coordinates, which is also what itemAt
	//took above; the frame's own margin would shift it otherwise.
	QAction *picked=menu.exec(pList->viewport()->mapToGlobal(pos));
	if (!picked)
		return;

	if (picked==load)
		loadPreset();
	else if (picked==append)
		appendPreset();
	else if (picked==pin)
		togglePin();
	else if (picked==edit)
		editPreset();
	else if (picked==xport)
		exportPreset();
	else if (picked==shortcut)
		shortcutPreset();
	else if (picked==remove)
		deletePreset();
	else if (picked==byName) {
		setSortByName(byName->isChecked());
		mw->saveNow();
		newConfig();
	}
}
