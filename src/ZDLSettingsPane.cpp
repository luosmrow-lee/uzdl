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
#include <algorithm>
#include <QApplication>
#include <QComboBox>
#include "ZDLMapFile.h"
#include "ZDLPaths.h"

#include "ZDLConfigurationManager.h"
#include "ZDLSettingsPane.h"
#include "ZDLMainWindow.h"
#include "ZDLRazeSetup.h"
#include "ZDLBuildArgs.h"
#include "ZDLBuildGames.h"
#include "ZDLLaunchFiles.h"

extern ZDLMainWindow *mw;

void AlwaysFocusedDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const 
{
	QStyleOptionViewItem new_option(option);
	if (new_option.state&QStyle::State_Selected) new_option.state=new_option.state|QStyle::State_Active;
	QItemDelegate::paint(painter, new_option, index);
}

ZDLSettingsPane::ZDLSettingsPane(QWidget *parent, const QString &launchSection, bool buildEngine):ZDLWidget(parent){
	LOGDATAO() << "New ZDLSettingsPane" << Qt::endl;
	launch = launchSection.isEmpty() ? QString(ZDLLaunchFiles::LAUNCH_SECTION) : launchSection;
	build = buildEngine;
	QVBoxLayout *box = new QVBoxLayout(this);
	setContentsMargins(0,0,0,0);
	layout()->setContentsMargins(0,0,0,0);
	box->setSpacing(2);

	box->addWidget(new QLabel("Source port",this));

	sourceList = new QComboBox(this);

	//Some ports - UZDoom among them - run an online updater when started on
	//their own, which never happens through a normal launch because ZDL
	//always hands them an IWAD and a file list.
	btnRunPort = new QPushButton("Run", this);
	btnRunPort->setToolTip("Start the selected source port on its own, with no IWAD, files or parameters");
	btnRunPort->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));

	QHBoxLayout *portRow = new QHBoxLayout();
	portRow->addWidget(sourceList);
	portRow->addWidget(btnRunPort);
	portRow->setSpacing(2);
	box->addLayout(portRow);

	QObject::connect(btnRunPort, SIGNAL(clicked()), this, SLOT(runSourcePort()));

	box->addWidget(new QLabel(build ? "Game" : "IWAD",this));

	IWADList = new DeselectableListWidget(this);
	IWADList->setItemDelegate(new AlwaysFocusedDelegate());
	box->addWidget(IWADList);

	QHBoxLayout *box2 = new QHBoxLayout();
	box->addLayout(box2);

	QVBoxLayout *warpBox = new QVBoxLayout();
	box2->addLayout(warpBox);

	QVBoxLayout *skillBox = new QVBoxLayout();
	box2->addLayout(skillBox);

	warpCombo = new VerboseComboBox(this);
	connect(warpCombo, SIGNAL(activated(int)), this, SLOT(currentRowChanged(int)));
	connect(warpCombo, SIGNAL(onPopup()), this, SLOT(VerbosePopup()));
	connect(warpCombo, SIGNAL(onHidePopup()), this, SLOT(HidePopup()));
	warpCombo->setInsertPolicy(QComboBox::NoInsert);
	warpCombo->setEditable(true);
	warpCombo->setValidator(new EvilValidator(this));
	warpCombo->setCompleter(NULL);
	warpCombo->lineEdit()->setPlaceholderText("(Default)");
	warpCombo->addItem("(Default)");
	warpBox->addWidget(new QLabel("Map",this));
	warpBox->addWidget(warpCombo);
	warpBox->setSpacing(2);

	diffList = new QComboBox(this);
	diffList->addItem("(Default)");
	diffList->addItem("V. Easy");
	diffList->addItem("Easy");
	diffList->addItem("Medium");
	diffList->addItem("Hard");
	diffList->addItem("V. Hard");
	diffList->addItem("No monsters");
	skillBox->addWidget(new QLabel("Skill",this));
	skillBox->addWidget(diffList);

	//GZDoom's compatibility presets, in the order its own menu lists them,
	//each carrying the number +compatmode takes. Default passes nothing, so
	//the port keeps whatever it was last set to.
	QVBoxLayout *compatBox = new QVBoxLayout();
	box2->addLayout(compatBox);
	compatList = new QComboBox(this);
	compatList->addItem("(Default)", -1);
	compatList->addItem("Doom", 1);
	compatList->addItem("Doom (strict)", 2);
	compatList->addItem("Boom", 3);
	compatList->addItem("Boom (strict)", 6);
	compatList->addItem("MBF", 5);
	compatList->addItem("MBF (strict)", 7);
	compatList->addItem("MBF 21", 8);
	compatList->addItem("MBF 21 (strict)", 9);
	compatList->addItem("ZDoom 2.0.63", 4);
	compatList->setToolTip("Passed as +compatmode: the same choice as GZDoom's compatibility mode menu");
	QLabel *compatLabel = new QLabel("Compatibility",this);
	compatBox->addWidget(compatLabel);
	compatBox->addWidget(compatList);
	compatBox->setSpacing(2);
	//GZDoom's compatibility modes mean nothing to the Build engine's ports.
	if (build){
		compatLabel->hide();
		compatList->hide();
	}

	QHBoxLayout *monsterRow = new QHBoxLayout();
	fastCheck = new QCheckBox("Fast monsters", this);
	fastCheck->setToolTip("-fast: monsters move and attack as on Nightmare");
	respawnCheck = new QCheckBox("Respawning monsters", this);
	respawnCheck->setToolTip("-respawn: killed monsters come back, as on Nightmare");
	monsterRow->addWidget(fastCheck);
	monsterRow->addWidget(respawnCheck);
	monsterRow->addStretch();
	box->addLayout(monsterRow);
	//Nor have they fast monsters to switch on; respawning they have, as -t1
	//in the Duke ports.
	if (build){
		fastCheck->hide();
		respawnCheck->setToolTip("Killed monsters come back: -t1 in EDuke32 and Rednukem");
	}

	//A preset's own save folder and settings file go with the launch it was
	//loaded into, and showed nowhere, so they went on applying to every
	//game started after it. They are shown here while they apply, and
	//dropped once another port or game is picked by hand.
	loading=false;
	lastPortIndex=-1;
	lastIwadRow=-1;
	keptRow=new QWidget(this);
	QHBoxLayout *keptLayout=new QHBoxLayout(keptRow);
	keptLayout->setContentsMargins(0, 2, 0, 0);
	keptLabel=new QLabel(keptRow);
	keptLabel->setWordWrap(true);
	keptLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
	keptDrop=new QPushButton("Stop using", keptRow);
	keptDrop->setToolTip("Launch without the preset's save folder and settings file");
	keptDrop->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
	keptLayout->addWidget(keptLabel, 1);
	keptLayout->addWidget(keptDrop, 0, Qt::AlignTop);
	box->addWidget(keptRow);
	keptRow->hide();
	connect(keptDrop, SIGNAL(clicked()), this, SLOT(dropKeptApart()));
	connect(sourceList, SIGNAL(currentIndexChanged(int)), this, SLOT(portPicked(int)));
	connect(IWADList, SIGNAL(currentRowChanged(int)), this, SLOT(iwadPicked(int)));
	LOGDATAO() << "Done" << Qt::endl;
}

void ZDLSettingsPane::portPicked(int index)
{
	if (loading||index==lastPortIndex)
		return;
	lastPortIndex=index;
	dropKeptApart();
}

void ZDLSettingsPane::iwadPicked(int row)
{
	if (loading||row==lastIwadRow)
		return;
	lastIwadRow=row;
	dropKeptApart();
}

void ZDLSettingsPane::dropKeptApart()
{
	if (ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration()) {
		zconf->deleteValue(launch, "savedir");
		zconf->deleteValue(launch, "config");
	}
	showKeptApart();
}

void ZDLSettingsPane::showKeptApart()
{
	//The names on the row, which stays short; the whole paths in its tooltip.
	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	QStringList lines, paths;
	if (zconf&&zconf->hasValue(launch, "savedir")) {
		QString path=QDir::toNativeSeparators(QDir::cleanPath(ZDLPaths::resolve(zconf->getValue(launch, "savedir"))));
		lines<<QString("Preset's save folder: %1").arg(QFileInfo(path).fileName());
		paths<<QString("Save folder: %1").arg(path);
	}
	if (zconf&&zconf->hasValue(launch, "config")) {
		QString path=QDir::toNativeSeparators(QDir::cleanPath(ZDLPaths::resolve(zconf->getValue(launch, "config"))));
		lines<<QString("Preset's settings file: %1").arg(QFileInfo(path).fileName());
		paths<<QString("Settings file: %1").arg(path);
	}
	keptLabel->setText(lines.join("\n"));
	keptLabel->setToolTip(paths.join("\n")+"\n\nThe preset loaded last keeps its savegames and settings apart from the port's usual ones. "
		"They are passed as -savedir and -config, and to the EDuke32 family as -game_dir and -cfg, "
		"where that folder is also searched for game files. Picking another port or game here stops using them.");
	keptRow->setVisible(!lines.isEmpty());
}

void DeselectableListWidget::mousePressEvent(QMouseEvent *event)
{
	QListWidgetItem *item=itemAt(event->pos());

	if (item&&item->isSelected()) {
		QListWidget::mousePressEvent(event);
		//clearSelection();
		setCurrentRow(-1);
	} else {
		QListWidget::mousePressEvent(event);
	}
}

void ZDLSettingsPane::VerbosePopup()
{
	warpCombo->lineEdit()->setPlaceholderText("");
	QString current=warpCombo->currentText();
	int idx;
	emit buildParent(this);
	warpCombo->setUpdatesEnabled(false);
	reloadMapList();
	if (current.isEmpty()) {
		warpCombo->setCurrentIndex(0);
		warpCombo->clearEditText();
	} else if ((idx=warpCombo->findText(current, Qt::MatchFixedString))>0) {
		warpCombo->setCurrentIndex(idx);
	} else {
		warpCombo->setEditText(current);
	}
	warpCombo->setUpdatesEnabled(true);
}

void ZDLSettingsPane::HidePopup()
{
	warpCombo->lineEdit()->setPlaceholderText("(Default)");
}


void ZDLSettingsPane::currentRowChanged(int idx){
	if (!idx) {
		warpCombo->setCurrentIndex(-1);
	}
}

QStringList ZDLSettingsPane::getFilesMaps(){
	if (ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration()) {
		if (ZDLSection *files=zconf->getSection(launch)) {
			QVector<ZDLLine*> vctr;

			files->getRegex("^file[0-9]+$", vctr);
			if (vctr.size()) {
				QStringList maps;

				foreach (ZDLLine *line, vctr) {
					if (ZDLMapFile *mapfile=ZDLMapFile::getMapFile(ZDLPaths::resolve(line->getValue()))) {
						maps+=mapfile->getMapNames();
						delete mapfile;
					}
				}

				return maps;
			}
		}
	}

	return QStringList();
}

bool ZDLSettingsPane::naturalSortLess(const QString &left, const QString &right)
{
	//Less comparison algorithm for natural sorting
	//Based on "The Alphanum Algorithm" by David Koelle
	//http://www.davekoelle.com/alphanum.html
	//Released under MIT License (https://opensource.org/licenses/MIT)

	bool mode_letter=true;	//If it's not letter mode, then it's digit mode
	QString::const_iterator li=left.begin();
	QString::const_iterator ri=right.begin();
	bool l_is_digit, r_is_digit;
	unsigned int l_as_uint, r_as_uint, l_digits, r_digits;

	while (li!=left.end()&&ri!=right.end()) {
		if (mode_letter) {
			while (li!=left.end()&&ri!=right.end()) {
				//Check if these are digit characters
				l_is_digit=li->isDigit();
				r_is_digit=ri->isDigit();
				
				//If both characters are digits, we continue in digit mode
				if (l_is_digit&&r_is_digit) {
					mode_letter=false;
					break;
				}
				
				//If one of the characters is a digit, we have a result
				if (l_is_digit) return true;
				if (r_is_digit) return false;

				//Else, compare both characters and if they differ we have a result
				if (*li<*ri) return true;
				if (*li>*ri) return false;

				//Otherwise, process next characters
				++li;
				++ri;
			}
		} else {
			//Get left and right numbers
			//To prevent overflow we process maximum of 9 digits (ignoring leading zeroes)
			//It's ok for WAD map names because they are limited to 8 characters anyway

			l_as_uint=0;
			l_digits=0;
			while (li!=left.end()&&li->isDigit()&&l_digits<9) {
				l_as_uint=l_as_uint*10+li->digitValue();
				if (l_as_uint) l_digits++;
				++li;
			}

			r_as_uint=0;
			r_digits=0;
			while (ri!=right.end()&&ri->isDigit()&&r_digits<9) {
				r_as_uint=r_as_uint*10+ri->digitValue();
				if (r_as_uint) r_digits++;
				++ri;
			}

			//If numbers differ, we have a comparison result
			if (l_as_uint<r_as_uint) return true;
			if (l_as_uint>r_as_uint) return false;

			//Otherwise we process the next substring in letter mode
			mode_letter=true;
		}
	}

	//We got here, so one of the strings (or both) is out of characters
	//If right string still has some characters left, then left string is out of characters, so it is "less" than right
	if (ri!=right.end()) return true;

	//Otherwise right is "less" then left or "equal" to it
	return false;
}

void ZDLSettingsPane::reloadMapList(){
	LOGDATAO() << "reloadMapList START" << Qt::endl;
	
	warpCombo->clear();
	warpCombo->addItem("(Default)");

	QStringList wadMaps;

	if(QListWidgetItem *item=IWADList->currentItem()) {
		if (ZDLMapFile *mapfile=ZDLMapFile::getMapFile(item->data(32).toString())) {
			wadMaps+=mapfile->getMapNames();
			delete mapfile;
		}
	}

	wadMaps.append(getFilesMaps());

	if (wadMaps.size()) {
		std::sort(wadMaps.begin(), wadMaps.end(), naturalSortLess);
		wadMaps.removeDuplicates();
		warpCombo->addItems(wadMaps);
	}

	warpCombo->setCurrentIndex(-1);
}

void ZDLSettingsPane::runSourcePort()
{
	//The combo box may have moved since the configuration was last written,
	//and getExecutable resolves the port through the launch configuration,
	//so flush first.
	mw->writeConfig();

	QString exec=mw->getExecutable(launch);
	if (exec.isEmpty()) {
		QMessageBox::warning(this, ZDL_APP_NAME, "Please select a source port.");
		return;
	}

	QFileInfo exec_fi(exec);
	if (!exec_fi.exists()) {
		QMessageBox::warning(this, ZDL_APP_NAME, QString("The source port executable is missing: ")+QDir::toNativeSeparators(exec));
		return;
	}

	//UZDL_DIR for the port to inherit, and a Raze's search list brought up
	//to date, as for a launch.
	ZDLRazeSetup::prepareLaunch(ZDLConfigurationManager::getActiveConfiguration(), exec_fi.absoluteFilePath(), launch);

	//Deliberately no arguments: started bare, a port shows its own front end
	//or updater rather than jumping straight into a game.
	if (!QProcess::startDetached(exec_fi.absoluteFilePath(), QStringList(), exec_fi.absolutePath()))
		QMessageBox::warning(this, ZDL_APP_NAME, "Failed to launch the source port executable.");
}

void ZDLSettingsPane::rebuild(){
	LOGDATAO() << "Saving config" << Qt::endl;
	ZDLConf *zconf = ZDLConfigurationManager::getActiveConfiguration();
	if(diffList->currentIndex() > 0){
		zconf->setValue(launch, "skill", diffList->currentIndex());
	}else{
		zconf->deleteValue(launch, "skill");
	}

	if(!warpCombo->currentText().isEmpty()){
		zconf->setValue(launch, "warp", warpCombo->currentText());
	}else{
		zconf->deleteValue(launch, "warp");
	}

	if (!build && compatList->currentIndex() > 0){
		zconf->setValue(launch, "compatmode", compatList->currentData().toInt());
	}else{
		zconf->deleteValue(launch, "compatmode");
	}

	if (!build && fastCheck->isChecked()){
		zconf->setValue(launch, "fast", "1");
	}else{
		zconf->deleteValue(launch, "fast");
	}

	if (respawnCheck->isChecked()){
		zconf->setValue(launch, "respawn", "1");
	}else{
		zconf->deleteValue(launch, "respawn");
	}

	//The lists show only this tab's ports and games, so the one picked is
	//stored by its name rather than by where it stands in the full list.
	if (sourceList->currentIndex() >= 0)
		zconf->setValue(launch, "port", sourceList->currentText());
	else
		zconf->deleteValue(launch, "port");

	QListWidgetItem *game = IWADList->currentRow() >= 0 ? IWADList->currentItem() : NULL;
	if (game)
		zconf->setValue(launch, "iwad", game->text());
	else
		zconf->deleteValue(launch, "iwad");
}

void ZDLSettingsPane::newConfig(){
	LOGDATAO() << "Loading new config" << Qt::endl;
	ZDLConf *zconf = ZDLConfigurationManager::getActiveConfiguration();
	loading = true;
	if(zconf->hasValue(launch, "skill")){
		int index = 0;
		int stat = 0;
		QString rc = zconf->getValue(launch, "skill", &stat);
		if (rc.length() > 0){
			index = rc.toInt();
		}
		//Six entries above Default: the five skills and No monsters, which
		//the old bound of five threw away on every re-read.
		if (index >= 0 && index <= 6){
			diffList->setCurrentIndex(index);
		}else{
			zconf->setValue(launch, "skill", 0);
			diffList->setCurrentIndex(0);
		}

	}else{
		diffList->setCurrentIndex(0);
	}

	if (zconf->hasValue(launch, "warp")){
		warpCombo->setEditText(zconf->getValue(launch, "warp"));
	}else{
		warpCombo->clearEditText();
	}

	int compat = zconf->hasValue(launch, "compatmode") ? compatList->findData(zconf->getValue(launch, "compatmode").toInt()) : -1;
	compatList->setCurrentIndex(compat > 0 ? compat : 0);
	fastCheck->setChecked(zconf->hasValue(launch, "fast") && zconf->getValue(launch, "fast") == "1");
	respawnCheck->setChecked(zconf->hasValue(launch, "respawn") && zconf->getValue(launch, "respawn") == "1");

	//This tab's ports: the Build engine's on the Build Engine tab, the rest
	//on the Doom Engine tab.
	sourceList->clear();
	if (ZDLSection *ports = zconf->getSection("zdl.ports")){
		QVector<ZDLLine*> fileVctr;
		ports->getRegex("^p[0-9]+f$", fileVctr);

		for(int i = 0; i < fileVctr.size(); i++){
			QString value = fileVctr[i]->getVariable();

			QString number = "^p";
			number.append(value.mid(1, value.length()-2));
			number.append("n$");
			int stat = 0;
			QVector<ZDLLine*> nameVctr;
			ports->getRegex(number, nameVctr);
			if (nameVctr.size() == 1 && ZDLBuildArgs::isBuildPort(ZDLPaths::resolve(fileVctr[i]->getValue())) == build){
				sourceList->addItem(nameVctr[0]->getValue(),stat);
			}
		}
	}
	if(zconf->hasValue(launch, "port")){
		int set = 0;
		int stat = 0;
		QString rc = zconf->getValue(launch, "port", &stat);

		if(rc.length() > 0){
			for(int i = 0; i < sourceList->count(); i++){
				if(sourceList->itemText(i).compare(rc) == 0){
					sourceList->setCurrentIndex(i);
					set = 1;
					break;
				}
			}
		}

		if(!set){
			zconf->deleteValue(launch, "port");
		}
	}

	//This tab's games, the same way.
	IWADList->clear();
	if (ZDLSection *iwads = zconf->getSection("zdl.iwads")){
		QVector<ZDLLine*> fileVctr;
		iwads->getRegex("^i[0-9]+f$", fileVctr);

		for(int i = 0; i < fileVctr.size(); i++){
			QString value = fileVctr[i]->getVariable();

			QString number = "^i";
			number.append(value.mid(1, value.length()-2));
			number.append("n$");

			QVector<ZDLLine*> nameVctr;
			iwads->getRegex(number, nameVctr);
			QString file = ZDLPaths::resolve(fileVctr[i]->getValue());
			if (nameVctr.size() == 1 && ZDLBuildGames::isBuildGameFile(file) == build){
				//Constructing with the list as parent already inserts the item.
				QListWidgetItem *item = new QListWidgetItem(nameVctr[0]->getValue(),IWADList, 1001);
				item->setData(32,file);
			}
		}
	}

	if(zconf->hasValue(launch, "iwad")){
		int set = 0;
		int stat = 0;
		QString rc = zconf->getValue(launch, "iwad", &stat);
		if (rc.length() > 0){
			for(int i = 0; i < IWADList->count(); i++){
				QListWidgetItem *item = IWADList->item(i);
				QString text = item->text();
				if(text.compare(rc) == 0){
					set = 1;
					IWADList->setCurrentRow(i);
					break;
				}
			}
		}
		if(!set){
			zconf->deleteValue(launch, "iwad");
		}
	}

	lastPortIndex = sourceList->currentIndex();
	lastIwadRow = IWADList->currentRow();
	loading = false;
	showKeptApart();
}
