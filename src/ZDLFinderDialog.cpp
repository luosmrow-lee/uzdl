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

#include "ZDLFinderDialog.h"
#include "ZDLFileInspector.h"
#include "ZDLRazeSetup.h"
#include "ZDLBuildGames.h"
#include "ZDLPaths.h"
#include "ZDLConfigurationManager.h"
#include "ZDLMainWindow.h"

extern ZDLMainWindow *mw;

//Which list a row belongs to, and which entry of it.
#define ROW_PORT_ROLE (Qt::UserRole+1)
#define ROW_INDEX_ROLE (Qt::UserRole+2)

//The files a configured list names, resolved, for greying out what is
//already there.
static QStringList ConfiguredFiles(const char *sectionName, const char *pattern)
{
	QStringList files;

	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	ZDLSection *section=zconf?zconf->getSection(sectionName):NULL;
	if (!section)
		return files;

	QVector<ZDLLine*> lines;
	section->getRegex(pattern, lines);
	foreach (ZDLLine *line, lines) {
		QString real=QFileInfo(ZDLPaths::resolve(line->getValue())).canonicalFilePath();
		files<<(real.isEmpty()?ZDLPaths::resolve(line->getValue()):real);
	}

	return files;
}

static bool Configured(const QStringList &have, const QString &path)
{
	QString real=QFileInfo(path).canonicalFilePath();
	foreach (const QString &known, have) {
		if (!known.compare(real.isEmpty()?path:real, Qt::CaseInsensitive))
			return true;
	}
	return false;
}

//The index the next entry of a list section takes, one past the highest
//in use; the lists renumber from zero on every flush anyway.
static int NextIndex(ZDLConf *zconf, const char *sectionName, const char *pattern)
{
	int next=0;

	ZDLSection *section=zconf->getSection(sectionName);
	if (!section)
		return next;

	QVector<ZDLLine*> lines;
	section->getRegex(pattern, lines);
	foreach (ZDLLine *line, lines) {
		QString var=line->getVariable();
		int number=var.mid(1, var.length()-2).toInt();
		if (number>=next)
			next=number+1;
	}

	return next;
}

ZDLFinderDialog::ZDLFinderDialog(QWidget *parent, bool ports, bool iwads):
	QDialog(parent), wantPorts(ports), wantIwads(iwads), added(false)
{
	setWindowTitle(ports&&iwads?"Found games":ports?"Found source ports":"Found IWADs");
	setWindowFlags(windowFlags()&~Qt::WindowContextHelpButtonHint);
	resize(760, 440);

	QVBoxLayout *column=new QVBoxLayout(this);

	status=new QLabel(this);
	status->setWordWrap(true);
	column->addWidget(status);

	tree=new QTreeWidget(this);
	tree->setColumnCount(3);
	tree->setHeaderLabels(QStringList()<<"Name"<<"File"<<"Found in");
	tree->header()->setStretchLastSection(false);
	tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	tree->setRootIsDecorated(false);
	tree->setUniformRowHeights(true);
	column->addWidget(tree, 1);

	//Referenced where they are, or copied under uZDL: the second is what a
	//portable install wants, so the folder still moves as one.
	copyIwads=new QCheckBox("Copy IWADs into uZDL's iwads folder", this);
	copyIwads->setToolTip("Otherwise the IWAD is used where it was found. Source ports are never copied: they are more than one file.");
	ZDLConfiguration *conf=ZDLConfigurationManager::getConfiguration();
	copyIwads->setChecked(conf&&conf->isPortable());
	copyIwads->setVisible(iwads);
	column->addWidget(copyIwads);

	QHBoxLayout *buttons=new QHBoxLayout();
	QPushButton *btnElsewhere=new QPushButton("Also look in...", this);
	btnElsewhere->setToolTip("Search a folder of your own, all the way down");
	QPushButton *btnDetails=new QPushButton("Details...", this);
	btnDetails->setToolTip("Everything uZDL can tell about the highlighted file; a double click does the same");
	QPushButton *btnCancel=new QPushButton("Cancel", this);
	QPushButton *btnAdd=new QPushButton("Add selected", this);
	btnAdd->setDefault(true);
	buttons->addWidget(btnElsewhere);
	buttons->addWidget(btnDetails);
	buttons->addStretch();
	buttons->addWidget(btnCancel);
	buttons->addWidget(btnAdd);
	column->addLayout(buttons);

	QObject::connect(btnElsewhere, SIGNAL(clicked()), this, SLOT(lookElsewhere()));
	QObject::connect(btnDetails, SIGNAL(clicked()), this, SLOT(showDetails()));
	QObject::connect(tree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), this, SLOT(showDetails()));
	QObject::connect(btnCancel, SIGNAL(clicked()), this, SLOT(reject()));
	QObject::connect(btnAdd, SIGNAL(clicked()), this, SLOT(addSelected()));

	havePorts=ConfiguredFiles("zdl.ports", "^p[0-9]+f$");
	haveIwads=ConfiguredFiles("zdl.iwads", "^i[0-9]+f$");
}

void ZDLFinderDialog::search()
{
	QApplication::setOverrideCursor(Qt::WaitCursor);
	if (wantPorts)
		ports=ZDLFinder::findPorts(extraFolders);
	if (wantIwads)
		iwads=ZDLFinder::findIwads(extraFolders);
	QApplication::restoreOverrideCursor();

	fill();
}

QTreeWidgetItem *ZDLFinderDialog::addRow(QTreeWidgetItem *group, const ZDLFinder::Found &found, bool port, int index)
{
	QTreeWidgetItem *item=group?new QTreeWidgetItem(group):new QTreeWidgetItem(tree);
	bool have=Configured(port?havePorts:haveIwads, found.path);

	item->setText(0, found.name);
	item->setText(1, QFileInfo(found.path).fileName());
	item->setText(2, have?"already in the list":found.where+(found.also.isEmpty()?QString():QString(", and %1 more").arg(found.also.size())));

	QString tip=QDir::toNativeSeparators(found.path);
	foreach (const QString &other, found.also)
		tip+="\n"+QDir::toNativeSeparators(other);
	item->setToolTip(0, tip);
	item->setToolTip(1, tip);
	item->setToolTip(2, tip);

	item->setData(0, ROW_PORT_ROLE, port);
	item->setData(0, ROW_INDEX_ROLE, index);
	item->setFlags((item->flags()|Qt::ItemIsUserCheckable)&~Qt::ItemIsEditable);
	item->setCheckState(0, have?Qt::Unchecked:Qt::Checked);
	if (have)
		item->setDisabled(true);

	return item;
}

void ZDLFinderDialog::fill()
{
	tree->clear();

	//Both kinds under headings; one kind flat.
	QTreeWidgetItem *portGroup=NULL;
	QTreeWidgetItem *iwadGroup=NULL;
	if (wantPorts&&wantIwads) {
		tree->setRootIsDecorated(true);
		portGroup=new QTreeWidgetItem(tree, QStringList("Source ports"));
		iwadGroup=new QTreeWidgetItem(tree, QStringList("IWADs"));
		QFont bold=portGroup->font(0);
		bold.setBold(true);
		portGroup->setFont(0, bold);
		iwadGroup->setFont(0, bold);
		portGroup->setFlags(portGroup->flags()&~Qt::ItemIsUserCheckable&~Qt::ItemIsSelectable);
		iwadGroup->setFlags(iwadGroup->flags()&~Qt::ItemIsUserCheckable&~Qt::ItemIsSelectable);
	}

	for (int i=0; i<ports.size(); i++)
		addRow(portGroup, ports[i], true, i);
	for (int i=0; i<iwads.size(); i++)
		addRow(iwadGroup, iwads[i], false, i);

	tree->expandAll();

	QStringList counts;
	if (wantPorts)
		counts<<QString("%1 source port%2").arg(ports.size()).arg(ports.size()==1?"":"s");
	if (wantIwads)
		counts<<QString("%1 IWAD%2").arg(iwads.size()).arg(iwads.size()==1?"":"s");
	if (ports.isEmpty()&&iwads.isEmpty())
		status->setText("Nothing found in the usual places. Also look in a folder of your own, or add files by hand.");
	else
		status->setText(counts.join(" and ")+" found. Tick what to add; each IWAD was identified by its contents.");
}

void ZDLFinderDialog::showDetails()
{
	QTreeWidgetItem *item=tree->currentItem();
	if (!item||!(item->flags()&Qt::ItemIsUserCheckable))
		return;

	int index=item->data(0, ROW_INDEX_ROLE).toInt();
	const QList<ZDLFinder::Found> &list=item->data(0, ROW_PORT_ROLE).toBool()?ports:iwads;
	if (index>=0&&index<list.size())
		ZDLFileInspector::open(this, list[index].path);
}

void ZDLFinderDialog::lookElsewhere()
{
	QString dir=QFileDialog::getExistingDirectory(this, "Also look in", QString(), QFileDialog::ShowDirsOnly);
	if (dir.isEmpty())
		return;

	extraFolders<<QFD_QT_SEP(dir);
	search();
}

void ZDLFinderDialog::addSelected()
{
	QList<int> portRows;
	QList<int> iwadRows;

	QTreeWidgetItemIterator it(tree);
	for (; *it; ++it) {
		QTreeWidgetItem *item=*it;
		if (!(item->flags()&Qt::ItemIsUserCheckable)||item->isDisabled()||item->checkState(0)!=Qt::Checked)
			continue;
		if (item->data(0, ROW_PORT_ROLE).toBool())
			portRows<<item->data(0, ROW_INDEX_ROLE).toInt();
		else
			iwadRows<<item->data(0, ROW_INDEX_ROLE).toInt();
	}

	if (portRows.isEmpty()&&iwadRows.isEmpty()) {
		QMessageBox::information(this, ZDL_APP_NAME, "Nothing is ticked.");
		return;
	}

	//The panes are flushed first, so that what they hold survives being
	//re-read with the additions, then re-read, which is how the lists on
	//the settings tab pick the new entries up.
	mw->writeConfig();

	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	if (!zconf)
		return;

	int next=NextIndex(zconf, "zdl.ports", "^p[0-9]+f$");
	foreach (int row, portRows) {
		zconf->setValue("zdl.ports", QString("p%1n").arg(next), ports[row].name);
		zconf->setValue("zdl.ports", QString("p%1f").arg(next), ZDLPaths::preferRelative(ports[row].path));
		next++;
	}

	QStringList notCopied;
	next=NextIndex(zconf, "zdl.iwads", "^i[0-9]+f$");
	foreach (int row, iwadRows) {
		QString path=iwads[row].path;

		//A Build game stays where it is: its GRP is no use without the
		//scripts, music and add-ons around it.
		if (copyIwads->isChecked()&&!ZDLBuildGames::isFamily(iwads[row].family)) {
			QDir().mkpath(ZDLPaths::iwadsDir());
			QString target=QDir(ZDLPaths::iwadsDir()).filePath(QFileInfo(path).fileName());
			//A file of that name already there is taken to be this one: an
			//IWAD's name is as good as its identity within one folder.
			if (QFile::exists(target)||QFile::copy(path, target))
				path=target;
			else
				notCopied<<QDir::toNativeSeparators(path);
		}

		zconf->setValue("zdl.iwads", QString("i%1n").arg(next), iwads[row].name);
		zconf->setValue("zdl.iwads", QString("i%1f").arg(next), ZDLPaths::preferRelative(path));
		next++;
	}

	mw->startRead();
	added=true;
	if (!portRows.isEmpty())
		ZDLRazeSetup::offerForNewPorts(this);

	if (!notCopied.isEmpty())
		QMessageBox::warning(this, ZDL_APP_NAME, "Could not be copied, so they are used where they are:\n\n"+notCopied.join("\n"));

	accept();
}

bool ZDLFinderDialog::run(QWidget *parent, bool ports, bool iwads)
{
	ZDLFinderDialog dialog(parent, ports, iwads);
	dialog.search();
	dialog.exec();
	return dialog.added;
}

void ZDLFinderDialog::offerOnFirstStart(QWidget *parent)
{
	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	if (!zconf||zconf->hasValue("zdl.general", "askedfind"))
		return;

	//Whatever the answer, the question is not asked again; the buttons on
	//the settings tab stay.
	zconf->setValue("zdl.general", "askedfind", "1");

	if (!ConfiguredFiles("zdl.ports", "^p[0-9]+f$").isEmpty()||!ConfiguredFiles("zdl.iwads", "^i[0-9]+f$").isEmpty())
		return;

	if (QMessageBox::question(parent, ZDL_APP_NAME,
			"No source ports or IWADs are set up yet. Look for games installed on this computer?\n\nThe same search is behind the Find installed buttons on the General settings tab.",
			QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes)!=QMessageBox::Yes)
		return;

	run(parent, true, true);
}
