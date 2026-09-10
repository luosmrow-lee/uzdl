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
#include "ZDLPresetDialog.h"
#include "ZDLPaths.h"
#include "ZDLConfigurationManager.h"
#include "ZDLMainWindow.h"

extern ZDLMainWindow *mw;

//Excluded entries are shown struck through, matching how the external file
//list draws them, and the flag rides along in the item's data.
#define ENTRY_EXCLUDED_ROLE (Qt::UserRole+1)

ZDLPresetDialog::ZDLPresetDialog(QWidget *parent, const QString &name, const QString &port, const QString &iwad, const QList<ZDLFileEntry> &entries):
	QDialog(parent)
{
	setWindowTitle("Edit preset");
	setWindowFlags(windowFlags()&~Qt::WindowContextHelpButtonHint);
	resize(1060, 560);

	QVBoxLayout *column=new QVBoxLayout(this);

	QHBoxLayout *nameRow=new QHBoxLayout();
	nameRow->addWidget(new QLabel("Name", this));
	nameEdit=new QLineEdit(name, this);
	nameRow->addWidget(nameEdit, 1);
	column->addLayout(nameRow);

	//On its own row rather than sharing with the name: the preset button row
	//below already makes this dialog's layout wide, and anything added
	//alongside the name edit ends up past the right edge.
	//
	//A preset can carry the port it is meant to run under, so switching to a
	//Boom set does not leave a GZDoom-only port selected. Left on the first
	//entry it does not touch whatever is chosen on the launch tab.
	QHBoxLayout *portRow=new QHBoxLayout();
	portRow->addWidget(new QLabel("Source port", this));
	portBox=new QComboBox(this);
	portBox->addItem("(leave unchanged)");

	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	ZDLSection *ports=zconf?zconf->getSection("zdl.ports"):NULL;
	if (ports) {
		QVector<ZDLLine*> fileVctr;
		ports->getRegex("^p[0-9]+f$", fileVctr);
		for (int i=0; i<fileVctr.size(); i++) {
			QString var=fileVctr[i]->getVariable();
			QVector<ZDLLine*> nameVctr;
			ports->getRegex("^p"+var.mid(1, var.length()-2)+"n$", nameVctr);
			if (nameVctr.size()==1)
				portBox->addItem(nameVctr[0]->getValue());
		}
	}

	int chosen=port.isEmpty()?-1:portBox->findText(port);
	portBox->setCurrentIndex(chosen>=0?chosen:0);
	portRow->addWidget(portBox);

	//And the IWAD, for the same reason: a Heretic set is no use launched
	//against DOOM2.WAD, and the launch tab keeps whatever was there before.
	portRow->addWidget(new QLabel("IWAD", this));
	iwadBox=new QComboBox(this);
	iwadBox->addItem("(leave unchanged)");

	ZDLSection *iwads=zconf?zconf->getSection("zdl.iwads"):NULL;
	if (iwads) {
		QVector<ZDLLine*> iwadVctr;
		iwads->getRegex("^i[0-9]+f$", iwadVctr);
		for (int i=0; i<iwadVctr.size(); i++) {
			QString var=iwadVctr[i]->getVariable();
			QVector<ZDLLine*> nameVctr;
			iwads->getRegex("^i"+var.mid(1, var.length()-2)+"n$", nameVctr);
			if (nameVctr.size()==1)
				iwadBox->addItem(nameVctr[0]->getValue());
		}
	}

	int chosenIwad=iwad.isEmpty()?-1:iwadBox->findText(iwad);
	iwadBox->setCurrentIndex(chosenIwad>=0?chosenIwad:0);
	portRow->addWidget(iwadBox);

	portRow->addStretch();
	column->addLayout(portRow);

	//The library sits beside the preset so files can be picked without
	//going out to the launch tab and back. It is the same pane the launch
	//tab uses, told to hand its choice here rather than to the launch list.
	QSplitter *split=new QSplitter(this);

	library=new ZDLPwadList(split);
	library->setForwardToLaunch(false);
	library->newConfig();

	QWidget *presetSide=new QWidget(split);
	QVBoxLayout *presetColumn=new QVBoxLayout(presetSide);
	presetColumn->setContentsMargins(0,0,0,0);
	presetColumn->addWidget(new QLabel("In this preset", presetSide));

	fileList=new QListWidget(presetSide);
	fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	fileList->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
	presetColumn->addWidget(fileList, 1);

	split->addWidget(library);
	split->addWidget(presetSide);
	split->setStretchFactor(0, 2);
	split->setStretchFactor(1, 3);
	column->addWidget(split, 1);

	foreach (const ZDLFileEntry &entry, entries)
		appendEntry(entry);

	QHBoxLayout *buttonRow=new QHBoxLayout();
	QPushButton *btnAdd=new QPushButton("Add files...", this);
	QPushButton *btnFolder=new QPushButton("Add folder...", this);
	QPushButton *btnTake=new QPushButton("Take from launch", this);
	btnTake->setToolTip("Replace the contents with whatever the external file list currently holds");
	QPushButton *btnRemove=new QPushButton("Remove", this);
	QPushButton *btnExclude=new QPushButton("Exclude", this);
	btnExclude->setToolTip("Keep the file in the preset but do not pass it to the source port");
	QPushButton *btnUp=new QPushButton("Up", this);
	QPushButton *btnDown=new QPushButton("Down", this);

	buttonRow->addWidget(btnAdd);
	buttonRow->addWidget(btnFolder);
	buttonRow->addWidget(btnTake);
	buttonRow->addStretch();
	buttonRow->addWidget(btnExclude);
	buttonRow->addWidget(btnRemove);
	buttonRow->addWidget(btnUp);
	buttonRow->addWidget(btnDown);
	presetColumn->addLayout(buttonRow);

	QDialogButtonBox *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel, Qt::Horizontal, this);
	column->addWidget(buttons);

	QObject::connect(library, SIGNAL(filesChosen(QStringList)), this, SLOT(addFromLibrary(QStringList)));
	QObject::connect(btnAdd, SIGNAL(clicked()), this, SLOT(addFiles()));
	QObject::connect(btnFolder, SIGNAL(clicked()), this, SLOT(addFolder()));
	QObject::connect(btnTake, SIGNAL(clicked()), this, SLOT(takeFromLaunch()));
	QObject::connect(btnRemove, SIGNAL(clicked()), this, SLOT(removeSelected()));
	QObject::connect(btnExclude, SIGNAL(clicked()), this, SLOT(toggleExcluded()));
	QObject::connect(btnUp, SIGNAL(clicked()), this, SLOT(moveUp()));
	QObject::connect(btnDown, SIGNAL(clicked()), this, SLOT(moveDown()));
	QObject::connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
	QObject::connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));
}

void ZDLPresetDialog::appendEntry(const ZDLFileEntry &entry)
{
	//Shown as it will be stored: relative when inside the uZDL folder.
	QListWidgetItem *item=new QListWidgetItem(ZDLPaths::preferRelative(entry.file), fileList);
	item->setData(ENTRY_EXCLUDED_ROLE, entry.disabled);

	if (entry.disabled) {
		QFont font=item->font();
		font.setStrikeOut(true);
		item->setFont(font);
	}
}

void ZDLPresetDialog::accept()
{
	if (presetName().isEmpty()) {
		QMessageBox::information(this, ZDL_APP_NAME, "A preset needs a name.");
		nameEdit->setFocus();
		nameEdit->selectAll();
		return;
	}

	QDialog::accept();
}

QString ZDLPresetDialog::presetPort()
{
	return portBox->currentIndex()<=0?QString():portBox->currentText();
}

QString ZDLPresetDialog::presetIwad()
{
	return iwadBox->currentIndex()<=0?QString():iwadBox->currentText();
}

QString ZDLPresetDialog::presetName()
{
	return nameEdit->text().trimmed();
}

QList<ZDLFileEntry> ZDLPresetDialog::presetEntries()
{
	QList<ZDLFileEntry> entries;

	for (int i=0; i<fileList->count(); i++) {
		QListWidgetItem *item=fileList->item(i);
		entries<<ZDLFileEntry(item->text(), item->data(ENTRY_EXCLUDED_ROLE).toBool());
	}

	return entries;
}

void ZDLPresetDialog::addFiles()
{
	QString filters=
		"Doom resource files (*.wad" QFD_FILTER_DELIM "*.pk3" QFD_FILTER_DELIM "*.pk7" QFD_FILTER_DELIM "*.zip" QFD_FILTER_DELIM "*.7z" QFD_FILTER_DELIM "*.deh" QFD_FILTER_DELIM "*.bex" QFD_FILTER_DELIM "*.cfg" QFD_FILTER_DELIM "*.lmp);;"
		"All files (" QFD_FILTER_ALL ")";

	QStringList chosen=QFileDialog::getOpenFileNames(this, "Add files to preset", getWadLastDir(), filters);

	foreach (const QString &file, chosen) {
		saveWadLastDir(file);
		appendEntry(ZDLFileEntry(QFD_QT_SEP(file), false));
	}
}

void ZDLPresetDialog::addFolder()
{
	QString chosen=QFileDialog::getExistingDirectory(this, "Add folder to preset", getWadLastDir(), QFileDialog::ShowDirsOnly);

	if (chosen.isEmpty())
		return;

	saveWadLastDir(chosen, NULL, true);
	appendEntry(ZDLFileEntry(QFD_QT_SEP(chosen), false));
}

//Handy when a preset is nearly right: set the launch tab up the way you want
//it, then pull that straight in rather than rebuilding it by hand here.
void ZDLPresetDialog::takeFromLaunch()
{
	mw->writeConfig();

	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	if (!zconf)
		return;

	QList<ZDLFileEntry> current=ZDLLaunchFiles::read(zconf, ZDLLaunchFiles::LAUNCH_SECTION);
	if (current.isEmpty()) {
		QMessageBox::information(this, ZDL_APP_NAME, "The external file list is empty.");
		return;
	}

	if (fileList->count()&&QMessageBox::question(this, ZDL_APP_NAME,
			"Replace this preset's contents with the current external file list?",
			QMessageBox::Yes|QMessageBox::No, QMessageBox::No)!=QMessageBox::Yes)
		return;

	fileList->clear();
	foreach (const ZDLFileEntry &entry, current)
		appendEntry(entry);
}

//Skips anything the preset already holds, so adding the same pack twice
//does not double it up.
void ZDLPresetDialog::addFromLibrary(const QStringList &files)
{
	foreach (const QString &file, files) {
		bool present=false;
		for (int i=0; i<fileList->count(); i++) {
			if (!ZDLPaths::resolve(fileList->item(i)->text()).compare(ZDLPaths::resolve(file), Qt::CaseInsensitive)) {
				present=true;
				break;
			}
		}
		if (!present)
			appendEntry(ZDLFileEntry(file, false));
	}
}

void ZDLPresetDialog::removeSelected()
{
	foreach (QListWidgetItem *item, fileList->selectedItems())
		delete item;
}

void ZDLPresetDialog::toggleExcluded()
{
	foreach (QListWidgetItem *item, fileList->selectedItems()) {
		bool excluded=!item->data(ENTRY_EXCLUDED_ROLE).toBool();
		item->setData(ENTRY_EXCLUDED_ROLE, excluded);
		QFont font=item->font();
		font.setStrikeOut(excluded);
		item->setFont(font);
	}
}

//Rows move one at a time, walking in the direction of travel so a block of
//selected rows keeps its order and cannot fall off the end.
void ZDLPresetDialog::moveBy(int delta)
{
	QList<QListWidgetItem*> selected=fileList->selectedItems();
	if (selected.isEmpty())
		return;

	QList<int> rows;
	foreach (QListWidgetItem *item, selected)
		rows<<fileList->row(item);
	std::sort(rows.begin(), rows.end());

	if (delta>0)
		std::reverse(rows.begin(), rows.end());

	foreach (int row, rows) {
		int dest=row+delta;
		if (dest<0||dest>=fileList->count())
			return;

		QListWidgetItem *item=fileList->takeItem(row);
		fileList->insertItem(dest, item);
		item->setSelected(true);
	}
}

void ZDLPresetDialog::moveUp()
{
	moveBy(-1);
}

void ZDLPresetDialog::moveDown()
{
	moveBy(1);
}
