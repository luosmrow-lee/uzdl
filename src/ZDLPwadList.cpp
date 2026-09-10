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

#include <QDirIterator>
#include <QFileSystemWatcher>
#include <algorithm>
#include "ZDLPwadList.h"
#include "ZDLPaths.h"
#include "ZDLLaunchFiles.h"
#include "ZDLConfigurationManager.h"
#include "gph_fld.xpm"

//Scanned extensions: everything a source port will take as an added file,
//minus the IWAD-only ones, which belong in the IWAD list instead.
static const char *PWAD_EXTENSIONS[]={
	"*.wad", "*.pk3", "*.pk7", "*.pkz", "*.pke", "*.zip", "*.7z", "*.p7z",
	"*.deh", "*.bex", "*.cfg", "*.lmp", NULL
};

//Absolute path of the file a leaf stands for. Folder rows carry nothing, so
//the presence of this is what separates a file row from a folder row.
#define PWAD_PATH_ROLE (Qt::UserRole+1)

//Unpacking a download touches a directory many times in a row; wait for the
//dust to settle rather than rescanning on every one.
#define RESCAN_DELAY_MS 750

#define VIEW_FOLDERS 0
#define VIEW_NEWEST  1
#define VIEW_NAME    2

ZDLPwadList::ZDLPwadList(QWidget *parent): ZDLWidget(parent)
{
	LOGDATAO() << "New ZDLPwadList" << Qt::endl;

	QVBoxLayout *column=new QVBoxLayout(this);
	column->setSpacing(2);

	column->addWidget(new QLabel("PWADs", this));

	QHBoxLayout *filterRow=new QHBoxLayout();
	filter=new QLineEdit(this);
	filter->setPlaceholderText("Filter");
	filter->setClearButtonEnabled(true);
	filterRow->addWidget(filter, 1);

	viewBox=new QComboBox(this);
	viewBox->addItem("Folders");
	viewBox->addItem("Newest");
	viewBox->addItem("Name");
	viewBox->setToolTip("Folders groups by directory; Newest and Name list every file flat");
	filterRow->addWidget(viewBox);
	filterRow->setSpacing(2);
	column->addLayout(filterRow);

	tree=new QTreeWidget(this);
	tree->setColumnCount(2);
	tree->setHeaderLabels(QStringList()<<"Name"<<"Modified");
	tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
	tree->setUniformRowHeights(true);
	tree->setSortingEnabled(false);
	tree->header()->setStretchLastSection(false);
	tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	//A tree of long relative paths would otherwise report a width hint wide
	//enough to shove the panes beside it off the window.
	tree->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
	tree->setMinimumHeight(80);
	column->addWidget(tree);

	status=new QLabel(this);
	//A QLabel's minimum width is its whole text, so the scanned folder goes
	//in the tooltip and the label itself stays short.
	status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	column->addWidget(status);

	QHBoxLayout *buttonRow=new QHBoxLayout();

	QPushButton *btnFolder=new QPushButton(this);
	btnFolder->setIcon(QPixmap(glyph_folder));
	btnFolder->setToolTip("Choose the folder to scan for PWADs");

	QPushButton *btnRescan=new QPushButton("Rescan", this);
	btnRescan->setToolTip("Scan the folder again. Normally unnecessary: the folder is watched for changes.");

	QPushButton *btnAdd=new QPushButton("Add \342\206\222", this);
	btnAdd->setToolTip("Add the selected PWADs to the external file list. Selecting a folder adds everything under it.");

	buttonRow->addWidget(btnFolder);
	buttonRow->addWidget(btnRescan);
	buttonRow->addStretch();
	buttonRow->addWidget(btnAdd);
	buttonRow->setSpacing(0);
	column->addLayout(buttonRow);

	column->setContentsMargins(0,0,0,0);
	setContentsMargins(0,0,0,0);

	forwardToLaunch=true;

	watcher=new QFileSystemWatcher(this);

	rescanTimer=new QTimer(this);
	rescanTimer->setSingleShot(true);

	QObject::connect(btnFolder, SIGNAL(clicked()), this, SLOT(chooseFolder()));
	QObject::connect(btnRescan, SIGNAL(clicked()), this, SLOT(rescan()));
	QObject::connect(btnAdd, SIGNAL(clicked()), this, SLOT(addSelected()));
	QObject::connect(filter, SIGNAL(textChanged(QString)), this, SLOT(filterChanged(QString)));
	QObject::connect(viewBox, SIGNAL(currentIndexChanged(int)), this, SLOT(viewChanged(int)));
	QObject::connect(tree, SIGNAL(itemActivated(QTreeWidgetItem*,int)), this, SLOT(itemActivated(QTreeWidgetItem*,int)));
	QObject::connect(watcher, SIGNAL(directoryChanged(QString)), this, SLOT(treeChanged(QString)));
	QObject::connect(rescanTimer, SIGNAL(timeout()), this, SLOT(rescanTimeout()));
}

void ZDLPwadList::rebuild()
{
	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	if (!zconf)
		return;

	if (root.isEmpty())
		zconf->deleteValue("zdl.general", "pwaddir");
	else
		//Inside the uZDL folder the PWAD folder is stored relative to it.
		zconf->setValue("zdl.general", "pwaddir", ZDLPaths::preferRelative(root));
}

void ZDLPwadList::newConfig()
{
	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	if (!zconf)
		return;

	QString conf_root;
	if (zconf->hasValue("zdl.general", "pwaddir"))
		conf_root=ZDLPaths::resolve(zconf->getValue("zdl.general", "pwaddir"));

	//Rescanning is not free and newConfig runs on every refresh, so only walk
	//the tree again when the folder changed or nothing has been scanned yet.
	//Anything appearing on disc afterwards arrives through the watcher.
	if (conf_root==root&&(!found.isEmpty()||root.isEmpty()))
		return;

	root=conf_root;
	scan();
}

void ZDLPwadList::scan()
{
	found.clear();

	if (root.isEmpty()||!QDir(root).exists()) {
		build();
		return;
	}

	QDir root_dir(root);

	QStringList filters;
	for (int i=0; PWAD_EXTENSIONS[i]; i++)
		filters<<PWAD_EXTENSIONS[i];

	QDirIterator it(root, filters, QDir::Files|QDir::Readable, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		it.next();
		Found entry;
		entry.path=it.filePath();
		entry.rel=root_dir.relativeFilePath(entry.path);
		entry.modified=it.fileInfo().lastModified();
		found<<entry;
	}

	watchTree();
	build();
}

//Watch every directory under the root, so a download unpacked into a brand
//new subfolder is noticed as well as one dropped into an existing one.
void ZDLPwadList::watchTree()
{
	if (!watcher->directories().isEmpty())
		watcher->removePaths(watcher->directories());

	if (root.isEmpty()||!QDir(root).exists())
		return;

	QStringList dirs;
	dirs<<root;

	QDirIterator it(root, QDir::Dirs|QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
	while (it.hasNext())
		dirs<<it.next();

	watcher->addPaths(dirs);
}

void ZDLPwadList::treeChanged(const QString &path)
{
	Q_UNUSED(path);
	rescanTimer->start(RESCAN_DELAY_MS);
}

void ZDLPwadList::rescanTimeout()
{
	LOGDATAO() << "PWAD folder changed, rescanning" << Qt::endl;
	scan();
}

void ZDLPwadList::build()
{
	tree->clear();

	if (root.isEmpty()) {
		status->setText("No folder chosen");
		status->setToolTip(QString());
		return;
	}

	if (!QDir(root).exists()) {
		status->setText("Folder is missing");
		status->setToolTip(QDir::toNativeSeparators(root));
		return;
	}

	int mode=viewBox->currentIndex();

	QVector<int> order;
	order.reserve(found.size());
	for (int i=0; i<found.size(); i++)
		order<<i;

	const QVector<Found> &all=found;
	std::sort(order.begin(), order.end(), [&all, mode](int l, int r) {
		if (mode==VIEW_NEWEST&&all[l].modified!=all[r].modified)
			return all[l].modified>all[r].modified;
		return all[l].rel.compare(all[r].rel, Qt::CaseInsensitive)<0;
	});

	if (mode==VIEW_FOLDERS) {
		//Folder rows are made on demand and remembered, so each directory in
		//the relative path becomes one node however many files sit under it.
		QHash<QString, QTreeWidgetItem*> folders;

		foreach (int idx, order) {
			const Found &entry=all[idx];
			QStringList parts=entry.rel.split('/', Qt::SkipEmptyParts);
			if (parts.isEmpty())
				continue;

			QString file=parts.takeLast();

			QTreeWidgetItem *parent=NULL;
			QString sofar;
			foreach (const QString &part, parts) {
				sofar=sofar.isEmpty()?part:sofar+"/"+part;
				QTreeWidgetItem *node=folders.value(sofar, NULL);
				if (!node) {
					node=parent?new QTreeWidgetItem(parent):new QTreeWidgetItem(tree);
					node->setText(0, part);
					folders.insert(sofar, node);
				}
				parent=node;
			}

			QTreeWidgetItem *leaf=parent?new QTreeWidgetItem(parent):new QTreeWidgetItem(tree);
			leaf->setText(0, file);
			leaf->setText(1, entry.modified.toString("yyyy-MM-dd"));
			leaf->setData(0, PWAD_PATH_ROLE, entry.path);
			leaf->setToolTip(0, entry.path);
		}
	} else {
		foreach (int idx, order) {
			const Found &entry=all[idx];
			QTreeWidgetItem *leaf=new QTreeWidgetItem(tree);
			leaf->setText(0, entry.rel);
			leaf->setText(1, entry.modified.toString("yyyy-MM-dd"));
			leaf->setData(0, PWAD_PATH_ROLE, entry.path);
			leaf->setToolTip(0, entry.path);
		}
	}

	status->setText(QString("%1 files").arg(found.size()));
	status->setToolTip(QDir::toNativeSeparators(root));

	applyFilter();
}

void ZDLPwadList::viewChanged(int index)
{
	Q_UNUSED(index);
	build();
}

//Hides anything that does not match, and any folder left with nothing
//visible under it. Returns whether this row survived.
bool ZDLPwadList::filterItem(QTreeWidgetItem *item, const QString &needle)
{
	bool self=needle.isEmpty()||item->text(0).contains(needle, Qt::CaseInsensitive);
	bool child=false;

	for (int i=0; i<item->childCount(); i++) {
		if (filterItem(item->child(i), needle))
			child=true;
	}

	//A folder whose own name matches keeps everything under it, which is how
	//typing an addon pack's name gets you the whole pack.
	if (self&&item->childCount()) {
		for (int i=0; i<item->childCount(); i++)
			item->child(i)->setHidden(false);
		child=true;
	}

	bool visible=self||child;
	item->setHidden(!visible);

	if (visible&&child&&!needle.isEmpty())
		item->setExpanded(true);

	return visible;
}

void ZDLPwadList::applyFilter()
{
	QString needle=filter->text().trimmed();

	for (int i=0; i<tree->topLevelItemCount(); i++)
		filterItem(tree->topLevelItem(i), needle);
}

void ZDLPwadList::filterChanged(const QString &text)
{
	Q_UNUSED(text);
	applyFilter();
}

void ZDLPwadList::chooseFolder()
{
	QString chosen=QFileDialog::getExistingDirectory(this, "Choose PWAD folder", root.isEmpty()?getWadLastDir():root, QFileDialog::ShowDirsOnly);

	if (chosen.isEmpty())
		return;

	root=QFD_QT_SEP(chosen);
	scan();
	rebuild();
}

void ZDLPwadList::rescan()
{
	scan();
}

//Gathers the files a row stands for: itself if it is a file, everything
//beneath it if it is a folder.
void ZDLPwadList::collectFiles(QTreeWidgetItem *item, QStringList &out)
{
	if (item->isHidden())
		return;

	QString path=item->data(0, PWAD_PATH_ROLE).toString();
	if (!path.isEmpty()) {
		if (!out.contains(path))
			out<<path;
		return;
	}

	for (int i=0; i<item->childCount(); i++)
		collectFiles(item->child(i), out);
}

QStringList ZDLPwadList::selectedFiles()
{
	QStringList files;

	foreach (QTreeWidgetItem *item, tree->selectedItems())
		collectFiles(item, files);

	return files;
}

void ZDLPwadList::addSelected()
{
	QStringList files=selectedFiles();

	if (files.isEmpty()) {
		QMessageBox::information(this, ZDL_APP_NAME, "Select one or more PWADs, or a folder, to add.");
		return;
	}

	emit filesChosen(files);

	if (!forwardToLaunch)
		return;

	QList<ZDLFileEntry> entries;
	foreach (const QString &file, files)
		entries<<ZDLFileEntry(file, false);

	ZDLLaunchFiles::sendToLaunch(entries, false);
}

void ZDLPwadList::setForwardToLaunch(bool forward)
{
	forwardToLaunch=forward;
}

void ZDLPwadList::itemActivated(QTreeWidgetItem *item, int column)
{
	Q_UNUSED(column);

	if (!item)
		return;

	QString path=item->data(0, PWAD_PATH_ROLE).toString();
	if (path.isEmpty()) {
		//Double clicking a folder opens it rather than adding a dozen files
		//nobody asked for.
		item->setExpanded(!item->isExpanded());
		return;
	}

	emit filesChosen(QStringList()<<path);

	if (!forwardToLaunch)
		return;

	QList<ZDLFileEntry> entries;
	entries<<ZDLFileEntry(path, false);
	ZDLLaunchFiles::sendToLaunch(entries, false);
}
