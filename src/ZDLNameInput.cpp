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
#include "ZDLNameInput.h"
#include "ZDLPaths.h"
#include "ZDLConfigurationManager.h"

ZDLNameInput::ZDLNameInput(QWidget *parent, const QString &last_used_dir, ZDLFileInfo *zdl_fi, bool alllow_dirs, bool allow_params):
	QDialog(parent), zdl_fi(zdl_fi), last_used_dir(last_used_dir), alllow_dirs(alllow_dirs), params_offset(allow_params?2:0)
{
	setWindowFlags(windowFlags()&~Qt::WindowContextHelpButtonHint); 
	QVBoxLayout *lays = new QVBoxLayout(this);
	QGridLayout *inputGrid = new QGridLayout();
	QHBoxLayout *ctrlButtons = new QHBoxLayout();
		
	QPushButton *btnOK = new QPushButton("OK", this);
	QPushButton *btnCancel = new QPushButton("Cancel", this);
	
	lparams = NULL;
	lname = new QLineEdit(this);
	lfile = new QLineEdit(this);
	btnBrowse = new QPushButton("...", this);
	btnBrowse->setMaximumWidth(26);

	inputGrid->addWidget(new QLabel("Name", this),0,0,1,2);
	inputGrid->addWidget(lname,1,0,1,2);
	inputGrid->addWidget(new QLabel("File", this),params_offset+2,0,1,2);
	inputGrid->addWidget(lfile,params_offset+3,0);
	inputGrid->addWidget(btnBrowse,params_offset+3,1);

	if (params_offset) {
		lparams = new QLineEdit(this);
		lparams->setPlaceholderText("(Optional)");
		lparams->setToolTip("Added to the command line whenever this source port is used, before the launch tab's own extra parameters");
		inputGrid->addWidget(new QLabel("Parameters", this),2,0,1,2);
		inputGrid->addWidget(lparams,3,0,1,2);
	}

	lrelative = new QCheckBox("Relative to the uZDL folder", this);
	lrelative->setToolTip("Store the path relative to the folder uZDL runs from, so the two can be moved together.\nOnly a file inside that folder can be stored this way: put it under sourceports or iwads there, then add it.");
	lrelative->setEnabled(false);
	inputGrid->addWidget(lrelative,params_offset+4,0,1,2);

	inputGrid->setSpacing(2);
	inputGrid->setContentsMargins(0,0,0,0);

	ctrlButtons->addStretch();
	ctrlButtons->addWidget(btnOK);
	ctrlButtons->addWidget(btnCancel);
	ctrlButtons->setSpacing(4);
	ctrlButtons->setContentsMargins(0,0,0,0);

	lays->addLayout(inputGrid);
	lays->addLayout(ctrlButtons);
	lays->setSpacing(4);

	setContentsMargins(4,4,4,4);
	layout()->setContentsMargins(0,0,0,0);
	setFixedHeight(sizeHint().height());
	resize(350, sizeHint().height());
	
	connect(btnBrowse, SIGNAL(clicked()), this, SLOT(browse()));
	connect(btnOK, SIGNAL(clicked()), this, SLOT(okClick()));
	connect(btnCancel, SIGNAL(clicked()), this, SLOT(reject()));
	connect(lrelative, SIGNAL(toggled(bool)), this, SLOT(relativeToggled(bool)));
	connect(lfile, SIGNAL(textEdited(QString)), this, SLOT(pathEdited(QString)));
}

void ZDLNameInput::browse(){
	QString fileName = QFileDialog::getOpenFileName(this, "Add file", last_used_dir, filters);
	if (!fileName.isEmpty()) {
		//Inside the uZDL folder the path is stored relative unless the box is
		//cleared; anywhere else it can only be absolute.
		setPath(QFD_QT_SEP(fileName), true);
		if (zdl_fi) {
			zdl_fi->setFile(fileName);
			lname->setText(zdl_fi->GetFileDescription());
		}
	}
}

void ZDLNameInput::okClick(){
	QFileInfo selected_file(ZDLPaths::resolve(lfile->text()));

	if (selected_file.exists()&&(alllow_dirs||selected_file.isFile())) {
		if (lname->text().length()) {
			accept();
		} else {
			QMessageBox::warning(this, ZDL_APP_NAME, "Name can't be empty.");
		}
	} else {
		QMessageBox::warning(this, ZDL_APP_NAME, "File path is invalid.");
	}
}

void ZDLNameInput::fromUrl(QUrl url){
	setPath(url.path(), true);
}

void ZDLNameInput::basedOff(ZDLNameListable *listable){
	if (listable){
		setPath(listable->getFile(), ZDLPaths::isRelative(listable->getFile()));
		lname->setText(listable->getName());
		if (lparams)
			lparams->setText(listable->getParams());
	}
}

void ZDLNameInput::setFilter(const QString &inFilters){
	filters = inFilters;
}

QString ZDLNameInput::getName() {
	return lname->text();
}

QString ZDLNameInput::getParams() {
	//Only a source port dialog has this field.
	return lparams?lparams->text():QString();
}

QString ZDLNameInput::getFile() {
	QString abs=ZDLPaths::resolve(QFD_QT_SEP(lfile->text()));
	if (lrelative->isChecked()) {
		QString rel=ZDLPaths::relativeToAppDir(abs);
		if (!rel.isEmpty())
			return rel;
	}
	return abs;
}

void ZDLNameInput::setPath(const QString &path, bool prefer_relative){
	QString abs=ZDLPaths::resolve(path);
	QString rel=ZDLPaths::relativeToAppDir(abs);
	bool relative=!rel.isEmpty()&&prefer_relative;

	lrelative->blockSignals(true);
	lrelative->setEnabled(!rel.isEmpty());
	lrelative->setChecked(relative);
	lrelative->blockSignals(false);

	lfile->setText(relative?rel:abs);
}

void ZDLNameInput::relativeToggled(bool on){
	setPath(lfile->text(), on);
}

//A path being typed is left alone; only the box follows it, so it cannot
//claim a relative form for a file that is not inside the folder.
void ZDLNameInput::pathEdited(const QString &text){
	QString rel=ZDLPaths::relativeToAppDir(ZDLPaths::resolve(text));
	lrelative->blockSignals(true);
	lrelative->setEnabled(!rel.isEmpty());
	if (rel.isEmpty())
		lrelative->setChecked(false);
	lrelative->blockSignals(false);
}
