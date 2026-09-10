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

#include "ZDLPresetList.h"
#include "ZDLPresetDialog.h"
#include "ZDLLaunchFiles.h"
#include "ZDLConfigurationManager.h"
#include "ZDLMainWindow.h"

extern ZDLMainWindow *mw;

//Section the highlighted item stands for, e.g. "zdl.mix2".
#define PRESET_SECTION_ROLE (Qt::UserRole+1)

ZDLPresetList::ZDLPresetList(QWidget *parent): ZDLWidget(parent)
{
	LOGDATAO() << "New ZDLPresetList" << Qt::endl;

	QVBoxLayout *column=new QVBoxLayout(this);
	column->setSpacing(2);

	column->addWidget(new QLabel("Presets", this));

	pList=new QListWidget(this);
	pList->setSelectionMode(QAbstractItemView::SingleSelection);
	pList->setContextMenuPolicy(Qt::CustomContextMenu);
	pList->setToolTip("Named external file lists. Double click to load one;\nright click for more, including export to a .zdl file.");
	//Keep the pane from demanding the width of its longest preset name.
	pList->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
	pList->setMinimumHeight(80);
	column->addWidget(pList);

	QHBoxLayout *buttonRow=new QHBoxLayout();

	QPushButton *btnEdit=new QPushButton("Edit", this);
	btnEdit->setToolTip("Rename this preset and change which files it holds");

	QPushButton *btnLoad=new QPushButton("Load", this);
	btnLoad->setToolTip("Replace the external file list with this preset");

	QPushButton *btnLaunch=new QPushButton("Launch", this);
	btnLaunch->setToolTip("Load this preset and start the game with it");

	QPushButton *btnDelete=new QPushButton("Delete", this);
	btnDelete->setToolTip("Delete this preset. The files themselves are not touched.");

	QPushButton *btnImport=new QPushButton("Import", this);
	btnImport->setToolTip("Make a preset from a .zdl file");

	QPushButton *btnAppend=new QPushButton("Add \342\206\222", this);
	btnAppend->setToolTip("Append this preset to the external file list, skipping files already there");

	buttonRow->addWidget(btnEdit);
	buttonRow->addWidget(btnDelete);
	buttonRow->addWidget(btnImport);
	buttonRow->addStretch();
	buttonRow->addWidget(btnLoad);
	buttonRow->addWidget(btnAppend);
	buttonRow->addWidget(btnLaunch);
	buttonRow->setSpacing(0);
	column->addLayout(buttonRow);

	column->setContentsMargins(0,0,0,0);
	setContentsMargins(0,0,0,0);

	QObject::connect(btnEdit, SIGNAL(clicked()), this, SLOT(editPreset()));
	QObject::connect(btnDelete, SIGNAL(clicked()), this, SLOT(deletePreset()));
	QObject::connect(btnImport, SIGNAL(clicked()), this, SLOT(importPreset()));
	QObject::connect(btnLoad, SIGNAL(clicked()), this, SLOT(loadPreset()));
	QObject::connect(btnLaunch, SIGNAL(clicked()), this, SLOT(launchPreset()));
	QObject::connect(btnAppend, SIGNAL(clicked()), this, SLOT(appendPreset()));
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

	foreach (const QString &section, ZDLLaunchFiles::presetSections(zconf)) {
		QString name=zconf->hasValue(section, "name")?zconf->getValue(section, "name"):section;
		int files=ZDLLaunchFiles::read(zconf, section).size();

		QListWidgetItem *item=new QListWidgetItem(QString("%1 (%2)").arg(name).arg(files), pList);
		item->setData(PRESET_SECTION_ROLE, section);

		if (section==previous)
			pList->setCurrentItem(item);
	}
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

	ZDLPresetDialog dialog(this, currentName(), currentPort(), currentIwad(), ZDLLaunchFiles::read(zconf, section));
	//The dialog will not close without a name, so there is nothing to check.
	if (dialog.exec()!=QDialog::Accepted)
		return;

	zconf->setValue(section, "name", dialog.presetName());
	if (dialog.presetPort().isEmpty())
		zconf->deleteValue(section, "port");
	else
		zconf->setValue(section, "port", dialog.presetPort());
	if (dialog.presetIwad().isEmpty())
		zconf->deleteValue(section, "iwad");
	else
		zconf->setValue(section, "iwad", dialog.presetIwad());
	ZDLLaunchFiles::write(zconf, section, dialog.presetEntries());

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
	ZDLLaunchFiles::sendToLaunch(ZDLLaunchFiles::read(zconf, section), replace, replace?currentPort():QString(), replace?currentIwad():QString());
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
	mw->launch();
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
	//a matter of writing those entries out under that section name.
	ZDLConf out;
	ZDLLaunchFiles::write(&out, ZDLLaunchFiles::LAUNCH_SECTION, ZDLLaunchFiles::read(zconf, section));

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

	//Opened in the editor rather than stored blind, so the name - the file's
	//own by default - and the contents can be adjusted first.
	ZDLPresetDialog dialog(this, QFileInfo(fileName).completeBaseName(), port, iwad, entries);
	if (dialog.exec()!=QDialog::Accepted)
		return;

	QString section=ZDLLaunchFiles::newPresetSection(zconf);
	zconf->setValue(section, "name", dialog.presetName());
	if (!dialog.presetPort().isEmpty())
		zconf->setValue(section, "port", dialog.presetPort());
	if (!dialog.presetIwad().isEmpty())
		zconf->setValue(section, "iwad", dialog.presetIwad());
	ZDLLaunchFiles::write(zconf, section, dialog.presetEntries());

	newConfig();
}

void ZDLPresetList::showMenu(const QPoint &pos)
{
	QListWidgetItem *item=pList->itemAt(pos);
	if (!item)
		return;

	pList->setCurrentItem(item);

	QMenu menu(this);
	QAction *load=menu.addAction("Load");
	QAction *append=menu.addAction("Add to external files");
	menu.addSeparator();
	QAction *edit=menu.addAction("Edit...");
	QAction *xport=menu.addAction("Export as .zdl...");
	menu.addSeparator();
	QAction *remove=menu.addAction("Delete");

	QAction *picked=menu.exec(pList->mapToGlobal(pos));

	if (picked==load)
		loadPreset();
	else if (picked==append)
		appendPreset();
	else if (picked==edit)
		editPreset();
	else if (picked==xport)
		exportPreset();
	else if (picked==remove)
		deletePreset();
}
