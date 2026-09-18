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

#include <QtNetwork>
#include <QRegularExpression>
#include <algorithm>
#include <QDesktopServices>
#include "ZDLIdgamesTab.h"
#include "ZDLPaths.h"
#include "ZDLConfigurationManager.h"
#include "miniz.h"

//Mirrors of the idgames archive. These serve the tree and the listings over
//plain HTTPS, unlike the API, which refuses non-browser clients.
static const char *IDGAMES_MIRRORS[]={
	"https://youfailit.net/pub/idgames",
	"https://ftpmirror1.infania.net/pub/idgames",
	"https://www.gamers.org/pub/idgames",
	NULL
};

//Filling the view with all twenty odd thousand entries costs more than it is
//worth; a search matching this many wants narrowing anyway.
#define MAX_RESULTS 800

//Cache marker. Bumped when the stored layout changes, so an older file is
//discarded rather than misread.
#define INDEX_CACHE_MAGIC "#uzdl-idgames 2"

//How many days back each entry of the age filter reaches. 0 means no limit.
static const int AGE_DAYS[]={0, 1, 7, 30, 90, 365};

#define COL_NAME 0
#define COL_TYPE 1
#define COL_DATE 2
#define COL_SIZE 3
#define COL_INSTALLED 4

#define ENTRY_INDEX_ROLE (Qt::UserRole+1)
//Date and size sort by their real values; comparing the display text would
//order 9 MB after 10 KB and 2026-01 after 2025-12 only by luck.
#define ENTRY_SORT_ROLE  (Qt::UserRole+2)

QString ZDLIdgamesEntry::category() const
{
	int slash=path.indexOf('/');
	return slash>0?path.left(slash):QString("(root)");
}

QString ZDLIdgamesEntry::name() const
{
	int slash=path.lastIndexOf('/');
	return slash>=0?path.mid(slash+1):path;
}

QString ZDLIdgamesEntry::directory() const
{
	int slash=path.lastIndexOf('/');
	return slash>=0?path.left(slash):QString();
}

bool ZDLIdgamesEntry::staging() const
{
	return path.startsWith("newstuff/")||path.startsWith("incoming");
}

//Left in a folder uZDL unpacked, naming the entry it came from, so
//reinstalling that same entry repairs the folder in place while a folder
//belonging to anything else is left alone.
#define INSTALL_MARKER ".uzdl-idgames"

//QDir::isEmpty() defaults to AllEntries|NoDotAndDotDot, which excludes hidden
//and system files - a folder holding only a desktop.ini or a .git would be
//reported empty and unpacked into.
static bool DirHasContent(const QDir &dir)
{
	return dir.exists()&&!dir.isEmpty(QDir::AllEntries|QDir::NoDotAndDotDot|QDir::Hidden|QDir::System);
}

static bool MarkerMatches(const QDir &dir, const QString &path)
{
	QFile marker(dir.filePath(INSTALL_MARKER));
	if (!marker.open(QIODevice::ReadOnly|QIODevice::Text))
		return false;

	QString stored=QString::fromUtf8(marker.readAll()).trimmed();
	marker.close();

	return stored==path;
}

static QString HumanSize(qint64 bytes)
{
	if (bytes>=1024*1024)
		return QString("%1 MB").arg(bytes/(1024.0*1024.0), 0, 'f', 1);
	return QString("%1 KB").arg(qMax((qint64)1, bytes/1024));
}

//Sorts on the value behind the cell rather than the text drawn in it, so
//clicking the Size or Modified header orders numerically and chronologically.
class ZDLIdgamesItem: public QTreeWidgetItem {
	public:
		ZDLIdgamesItem(QTreeWidget *parent): QTreeWidgetItem(parent) {}

		virtual bool operator<(const QTreeWidgetItem &other) const
		{
			int col=treeWidget()?treeWidget()->sortColumn():COL_NAME;

			if (col==COL_SIZE)
				return data(COL_SIZE, ENTRY_SORT_ROLE).toLongLong()<other.data(COL_SIZE, ENTRY_SORT_ROLE).toLongLong();
			if (col==COL_DATE)
				return data(COL_DATE, ENTRY_SORT_ROLE).toDate()<other.data(COL_DATE, ENTRY_SORT_ROLE).toDate();

			return text(col).compare(other.text(col), Qt::CaseInsensitive)<0;
		}
};

//tinfl handles raw deflate; the gzip wrapper around it has to come off first.
static QByteArray Gunzip(const QByteArray &in)
{
	if (in.size()<18)
		return QByteArray();

	const unsigned char *p=(const unsigned char*)in.constData();
	if (p[0]!=0x1f||p[1]!=0x8b||p[2]!=8)
		return QByteArray();

	int flg=p[3];
	qsizetype pos=10;

	if (flg&4) {															//FEXTRA
		if (pos+2>in.size())
			return QByteArray();
		pos+=2+(p[pos]|(p[pos+1]<<8));
	}
	if (flg&8) { while (pos<in.size()&&p[pos]) pos++; pos++; }				//FNAME
	if (flg&16) { while (pos<in.size()&&p[pos]) pos++; pos++; }				//FCOMMENT
	if (flg&2) pos+=2;														//FHCRC

	//The last 8 bytes are the CRC32 and the uncompressed size, not deflate data.
	if (pos>=in.size()-8)
		return QByteArray();

	size_t out_len=0;
	void *out=tinfl_decompress_mem_to_heap(in.constData()+pos, (size_t)(in.size()-pos-8), &out_len, 0);
	if (!out)
		return QByteArray();

	QByteArray result((const char*)out, (qsizetype)out_len);
	mz_free(out);

	return result;
}

ZDLIdgamesTab::ZDLIdgamesTab(QWidget *parent): ZDLWidget(parent)
{
	LOGDATAO() << "New ZDLIdgamesTab" << Qt::endl;

	nam=new QNetworkAccessManager(this);
	indexReply=NULL;
	textReply=NULL;
	fileReply=NULL;

	detailsIndex=-1;
	queueCurrent=-1;
	queueTotal=0;
	queueDone=0;

	QVBoxLayout *column=new QVBoxLayout(this);
	column->setSpacing(4);

	QHBoxLayout *topRow=new QHBoxLayout();
	topRow->addWidget(new QLabel("Mirror", this));
	mirrorBox=new QComboBox(this);
	for (int i=0; IDGAMES_MIRRORS[i]; i++)
		mirrorBox->addItem(IDGAMES_MIRRORS[i]);
	mirrorBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	topRow->addWidget(mirrorBox);
	btnUpdate=new QPushButton("Update index", this);
	btnUpdate->setToolTip("Fetch the archive listing from the mirror. About 400 KB; only needed occasionally.");
	topRow->addWidget(btnUpdate);
	column->addLayout(topRow);

	QHBoxLayout *filterRow=new QHBoxLayout();
	filterRow->addWidget(new QLabel("Type", this));
	categoryBox=new QComboBox(this);
	filterRow->addWidget(categoryBox);

	filterRow->addWidget(new QLabel("Added", this));
	ageBox=new QComboBox(this);
	ageBox->addItem("Any time");
	ageBox->addItem("Last 24 hours");
	ageBox->addItem("Last 7 days");
	ageBox->addItem("Last 30 days");
	ageBox->addItem("Last 90 days");
	ageBox->addItem("Last year");
	ageBox->setToolTip("Measured from the newest upload the index knows about, not from today");
	filterRow->addWidget(ageBox);

	filterRow->addWidget(new QLabel("Sort", this));
	sortBox=new QComboBox(this);
	sortBox->addItem("Name");
	sortBox->addItem("Newest first");
	sortBox->addItem("Largest first");
	sortBox->setToolTip("Also picks which entries survive the display limit; the column headers re-sort what is shown");
	filterRow->addWidget(sortBox);

	filterRow->addWidget(new QLabel("Search", this));
	searchBox=new QLineEdit(this);
	searchBox->setPlaceholderText("Part of a file or folder name");
	searchBox->setClearButtonEnabled(true);
	filterRow->addWidget(searchBox, 1);
	column->addLayout(filterRow);

	idgamesSplit=new QSplitter(this);
	QSplitter *split=idgamesSplit;

	results=new QTreeWidget(split);
	results->setColumnCount(5);
	results->setHeaderLabels(QStringList()<<"Name"<<"Type"<<"Modified"<<"Size"<<"Installed");
	results->setRootIsDecorated(false);
	results->setUniformRowHeights(true);
	results->setSelectionMode(QAbstractItemView::ExtendedSelection);
	results->setSortingEnabled(true);
	results->header()->setStretchLastSection(false);
	results->header()->setSectionResizeMode(COL_NAME, QHeaderView::Stretch);
	results->header()->setSectionResizeMode(COL_TYPE, QHeaderView::ResizeToContents);
	results->header()->setSectionResizeMode(COL_DATE, QHeaderView::ResizeToContents);
	results->header()->setSectionResizeMode(COL_SIZE, QHeaderView::ResizeToContents);
	results->header()->setSectionResizeMode(COL_INSTALLED, QHeaderView::ResizeToContents);
	results->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);

	details=new QPlainTextEdit(split);
	details->setReadOnly(true);
	details->setLineWrapMode(QPlainTextEdit::NoWrap);
	details->setPlaceholderText("Select an entry to read its description");
	details->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);

	split->addWidget(results);
	split->addWidget(details);
	split->setStretchFactor(0, 3);
	split->setStretchFactor(1, 2);
	column->addWidget(split, 1);

	QHBoxLayout *bottomRow=new QHBoxLayout();
	status=new QLabel(this);
	status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	bottomRow->addWidget(status, 1);
	btnDoomworld=new QPushButton("Open on Doomworld", this);
	btnDoomworld->setToolTip("Open the selected entry's page on Doomworld in your browser: reviews, screenshots and votes, which the mirrors do not carry");
	btnDoomworld->setEnabled(false);
	bottomRow->addWidget(btnDoomworld);
	btnDownload=new QPushButton("Download and install", this);
	btnDownload->setToolTip("Download the selected entries and unpack them into the PWAD folder, one after another");
	bottomRow->addWidget(btnDownload);
	column->addLayout(bottomRow);

	setContentsMargins(4,4,4,4);

	QObject::connect(btnUpdate, SIGNAL(clicked()), this, SLOT(updateIndex()));
	QObject::connect(btnDownload, SIGNAL(clicked()), this, SLOT(download()));
	QObject::connect(btnDoomworld, SIGNAL(clicked()), this, SLOT(openDoomworld()));
	QObject::connect(searchBox, SIGNAL(textChanged(QString)), this, SLOT(filterChanged()));
	QObject::connect(categoryBox, SIGNAL(currentIndexChanged(int)), this, SLOT(filterChanged()));
	QObject::connect(ageBox, SIGNAL(currentIndexChanged(int)), this, SLOT(filterChanged()));
	QObject::connect(sortBox, SIGNAL(currentIndexChanged(int)), this, SLOT(sortChanged(int)));
	QObject::connect(mirrorBox, SIGNAL(currentIndexChanged(int)), this, SLOT(mirrorChanged(int)));
	QObject::connect(results, SIGNAL(itemSelectionChanged()), this, SLOT(entrySelected()));
}

//Mirrors are volunteer-run and some are old Apache setups. Identify the
//client honestly, and stay on HTTP/1.1: Qt 6 negotiates HTTP/2 over TLS by
//default and not every mirror handles it well.
static QNetworkRequest IdgamesRequest(const QUrl &url)
{
	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::UserAgentHeader, QString("uZDL/") + ZDL_VERSION_STRING);
	request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
	return request;
}

//Reports the HTTP status alongside Qt's message, so a refusal by the mirror
//is distinguishable from a connection that never got there.
static QString ReplyError(QNetworkReply *reply)
{
	int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	QString where=reply->url().toString();
	if (status)
		return QString("HTTP %1 from %2").arg(status).arg(where);
	return reply->errorString()+" ("+where+")";
}

QString ZDLIdgamesTab::mirror()
{
	QString base=mirrorBox->currentText();
	while (base.endsWith("/"))
		base.chop(1);
	return base;
}

//The index lives beside the configuration, so a portable install carries its
//cached archive listing along with everything else.
QString ZDLIdgamesTab::indexCachePath()
{
	QFileInfo conf(ZDLConfigurationManager::getConfigFileName());
	QString dir=conf.absolutePath();
	if (dir.isEmpty())
		dir=ZDLPaths::appDir();
	return QDir(dir).filePath("idgames.idx");
}

void ZDLIdgamesTab::rebuild()
{
	saveSplitterSizes("splitidgames", idgamesSplit);

	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	if (zconf)
		zconf->setValue("zdl.general", "idgamesmirror", mirrorBox->currentText());
}

void ZDLIdgamesTab::newConfig()
{
	restoreSplitterSizes("splitidgames", idgamesSplit);

	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();

	if (zconf&&zconf->hasValue("zdl.general", "idgamesmirror")) {
		int idx=mirrorBox->findText(zconf->getValue("zdl.general", "idgamesmirror"));
		if (idx>=0)
			mirrorBox->setCurrentIndex(idx);
	}

	scanInstalled();

	if (entries.isEmpty()) {
		if (loadCachedIndex())
			populate();
		else
			status->setText("No index yet - press Update index");
	} else {
		markInstalled();
	}
}

bool ZDLIdgamesTab::loadCachedIndex()
{
	QFile file(indexCachePath());
	if (!file.open(QIODevice::ReadOnly|QIODevice::Text))
		return false;

	QTextStream in(&file);

	//An index written by an older build has no dates in it; discard it rather
	//than show a listing that cannot be sorted or filtered by age.
	QString header=in.readLine();
	if (!header.startsWith(INDEX_CACHE_MAGIC)) {
		file.close();
		return false;
	}

	entries.clear();
	indexDate=QDate();

	while (!in.atEnd()) {
		QStringList parts=in.readLine().split("\t");
		if (parts.size()<3)
			continue;
		ZDLIdgamesEntry entry;
		entry.path=parts[0];
		entry.size=parts[1].toLongLong();
		entry.date=QDate::fromString(parts[2], Qt::ISODate);
		if (entry.date.isValid()&&(!indexDate.isValid()||entry.date>indexDate))
			indexDate=entry.date;
		entries<<entry;
	}

	file.close();

	return !entries.isEmpty();
}

void ZDLIdgamesTab::saveCachedIndex()
{
	QFile file(indexCachePath());
	if (!file.open(QIODevice::WriteOnly|QIODevice::Truncate|QIODevice::Text)) {
		LOGDATAO() << "Could not cache index at " << indexCachePath() << Qt::endl;
		return;
	}

	QTextStream out(&file);
	out<<INDEX_CACHE_MAGIC<<"\n";
	foreach (const ZDLIdgamesEntry &entry, entries)
		out<<entry.path<<"\t"<<entry.size<<"\t"<<entry.date.toString(Qt::ISODate)<<"\n";

	file.close();
}

//fullsort lists the whole archive as "YYYY/MM/DD  size  path". It is used in
//preference to ls-laR because that one prints a clock time instead of a year
//for anything uploaded recently, which leaves no way to sort by date.
bool ZDLIdgamesTab::parseListing(const QByteArray &listing)
{
	QVector<ZDLIdgamesEntry> parsed;
	QDate newest;

	QRegularExpression line_re("^(\\d{4})/(\\d{2})/(\\d{2})\\s+(\\d+)\\s+(.+)$");

	foreach (const QString &raw, QString::fromUtf8(listing).split('\n')) {
		QRegularExpressionMatch line_m=line_re.match(raw.trimmed());
		if (!line_m.hasMatch())
			continue;

		QString path=line_m.captured(5).trimmed();
		if (!path.endsWith(".zip", Qt::CaseInsensitive))
			continue;

		ZDLIdgamesEntry entry;
		entry.path=path;
		entry.size=line_m.captured(4).toLongLong();
		entry.date=QDate(line_m.captured(1).toInt(), line_m.captured(2).toInt(), line_m.captured(3).toInt());
		if (entry.date.isValid()&&(!newest.isValid()||entry.date>newest))
			newest=entry.date;

		parsed<<entry;
	}

	//Mirrors answer 200 with an HTML error page when a file is missing, and
	//that sails through Gunzip untouched. Committing it would clear the view
	//while leaving rows on screen pointing into an empty vector.
	if (parsed.isEmpty())
		return false;

	entries=parsed;
	indexDate=newest;

	//Indices into the old vector mean nothing now.
	detailsIndex=-1;
	detailsText.clear();

	dropStagingDuplicates();

	return true;
}

//Recent uploads are hardlinked into newstuff and incoming as well as their
//real category, so the archive lists them twice. Drop the staging copy when
//the same file is already filed properly. Matching on name alone would be
//wrong: about a thousand names occur legitimately in more than one category,
//so the size has to agree too - and since these are hardlinks, it does.
void ZDLIdgamesTab::dropStagingDuplicates()
{
	QSet<QString> filed;

	foreach (const ZDLIdgamesEntry &entry, entries) {
		if (!entry.staging())
			filed.insert(entry.name().toLower()+"/"+QString::number(entry.size));
	}

	QVector<ZDLIdgamesEntry> kept;
	kept.reserve(entries.size());

	foreach (const ZDLIdgamesEntry &entry, entries) {
		if (entry.staging()&&filed.contains(entry.name().toLower()+"/"+QString::number(entry.size)))
			continue;
		kept<<entry;
	}

	LOGDATAO() << "Index: " << entries.size() << " entries, " << (entries.size()-kept.size()) << " staging duplicates dropped" << Qt::endl;

	entries=kept;
}

void ZDLIdgamesTab::populate()
{
	//Rebuild the type list from what the index actually holds.
	QString wanted=categoryBox->currentText();
	QStringList cats;
	foreach (const ZDLIdgamesEntry &entry, entries) {
		if (!cats.contains(entry.category()))
			cats<<entry.category();
	}
	cats.sort(Qt::CaseInsensitive);

	categoryBox->blockSignals(true);
	categoryBox->clear();
	categoryBox->addItem("All");
	categoryBox->addItems(cats);
	int keep=categoryBox->findText(wanted);
	categoryBox->setCurrentIndex(keep>=0?keep:0);
	categoryBox->blockSignals(false);

	filterChanged();
}

void ZDLIdgamesTab::sortChanged(int index)
{
	Q_UNUSED(index);
	filterChanged();
}

//Written straight away rather than waiting for writeConfig. Leaving the tab
//calls newConfig on the way back, which reloads the combo from the stored
//value - so a choice that was never stored silently reverts.
void ZDLIdgamesTab::mirrorChanged(int index)
{
	Q_UNUSED(index);
	rebuild();
}

void ZDLIdgamesTab::filterChanged()
{
	QString needle=searchBox->text().trimmed();
	QString cat=categoryBox->currentIndex()<=0?QString():categoryBox->currentText();

	int age_idx=ageBox->currentIndex();
	int days=(age_idx>=0&&age_idx<(int)(sizeof(AGE_DAYS)/sizeof(AGE_DAYS[0])))?AGE_DAYS[age_idx]:0;

	//Measured from the newest thing in the index rather than from today: a
	//month-old index would otherwise answer "last 7 days" with nothing at all
	//and look broken.
	QDate cutoff;
	if (days>0&&indexDate.isValid())
		cutoff=indexDate.addDays(-days);

	QVector<int> matched;
	matched.reserve(entries.size());

	for (int i=0; i<entries.size(); i++) {
		const ZDLIdgamesEntry &entry=entries[i];

		if (!cat.isEmpty()&&entry.category()!=cat)
			continue;
		if (!needle.isEmpty()&&!entry.path.contains(needle, Qt::CaseInsensitive))
			continue;
		if (cutoff.isValid()&&(!entry.date.isValid()||entry.date<cutoff))
			continue;

		matched<<i;
	}

	const QVector<ZDLIdgamesEntry> &all=entries;
	int mode=sortBox->currentIndex();

	//This ordering decides which entries survive the display limit. The tree
	//re-sorts whatever made it through when a header is clicked.
	std::sort(matched.begin(), matched.end(), [&all, mode](int l, int r) {
		if (mode==1) {
			if (all[l].date!=all[r].date)
				return all[l].date>all[r].date;
		} else if (mode==2) {
			if (all[l].size!=all[r].size)
				return all[l].size>all[r].size;
		}
		return all[l].path.compare(all[r].path, Qt::CaseInsensitive)<0;
	});

	//Filling a sorted tree re-sorts on every insert; switch it off meanwhile.
	results->setSortingEnabled(false);
	results->clear();

	int shown=qMin(matched.size(), MAX_RESULTS);
	for (int n=0; n<shown; n++) {
		const ZDLIdgamesEntry &entry=all[matched[n]];

		ZDLIdgamesItem *item=new ZDLIdgamesItem(results);
		item->setText(COL_NAME, entry.name());
		item->setText(COL_TYPE, entry.category());
		item->setText(COL_DATE, entry.date.isValid()?entry.date.toString(Qt::ISODate):QString("?"));
		item->setText(COL_SIZE, HumanSize(entry.size));
		item->setTextAlignment(COL_SIZE, Qt::AlignRight|Qt::AlignVCenter);
		item->setData(COL_NAME, ENTRY_INDEX_ROLE, matched[n]);
		item->setData(COL_DATE, ENTRY_SORT_ROLE, entry.date);
		item->setData(COL_SIZE, ENTRY_SORT_ROLE, entry.size);
		item->setToolTip(COL_NAME, entry.path);
		item->setToolTip(COL_TYPE, entry.directory());
	}

	markInstalled();

	if (mode==1)
		results->sortByColumn(COL_DATE, Qt::DescendingOrder);
	else if (mode==2)
		results->sortByColumn(COL_SIZE, Qt::DescendingOrder);
	else
		results->sortByColumn(COL_NAME, Qt::AscendingOrder);

	results->setSortingEnabled(true);

	QString stamp=indexDate.isValid()?QString(", index to %1").arg(indexDate.toString(Qt::ISODate)):QString();

	if (entries.isEmpty())
		status->setText("No index yet - press Update index");
	else if (matched.size()>shown)
		status->setText(QString("%1 matches, showing %2 - narrow the search%3").arg(matched.size()).arg(shown).arg(stamp));
	else
		status->setText(QString("%1 of %2 entries%3").arg(matched.size()).arg(entries.size()).arg(stamp));
}

void ZDLIdgamesTab::setBusy(bool busy)
{
	btnUpdate->setEnabled(!busy);
	btnDownload->setEnabled(!busy);
}

void ZDLIdgamesTab::updateIndex()
{
	if (indexReply)
		return;

	setBusy(true);
	status->setText("Fetching archive index...");

	indexReply=nam->get(IdgamesRequest(QUrl(mirror()+"/fullsort.gz")));
	QObject::connect(indexReply, SIGNAL(finished()), this, SLOT(indexDone()));
	QObject::connect(indexReply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(transferProgress(qint64,qint64)));
}

void ZDLIdgamesTab::indexDone()
{
	QNetworkReply *reply=indexReply;
	indexReply=NULL;
	setBusy(false);

	if (!reply)
		return;
	reply->deleteLater();

	if (reply->error()!=QNetworkReply::NoError) {
		status->setText("Index download failed: "+ReplyError(reply));
		return;
	}

	QByteArray payload=reply->readAll();
	QByteArray listing;

	//Mirrors disagree about what a .gz file is. Some send it as an opaque
	//body, which arrives still compressed; others label it
	//Content-Encoding: gzip, and Qt transparently decodes it before we ever
	//see it. Decide from the payload rather than trusting the headers.
	if (payload.size()>2&&(unsigned char)payload[0]==0x1f&&(unsigned char)payload[1]==0x8b)
		listing=Gunzip(payload);
	else
		listing=payload;

	if (listing.isEmpty()) {
		status->setText("Index was empty or could not be decompressed");
		return;
	}

	if (!parseListing(listing)) {
		status->setText("Index held no archive entries - wrong file, or an unexpected format");
		return;
	}

	saveCachedIndex();
	populate();
}

void ZDLIdgamesTab::entrySelected()
{
	QList<QTreeWidgetItem*> selected=results->selectedItems();

	btnDoomworld->setEnabled(selected.size()==1);

	//With a multiple selection there is no single description to show, and
	//fetching one per click would hammer the mirror while dragging.
	if (selected.size()!=1)
		return;

	int idx=selected.first()->data(COL_NAME, ENTRY_INDEX_ROLE).toInt();
	if (idx<0||idx>=entries.size())
		return;

	if (idx==detailsIndex)
		return;

	//Every release has a .txt beside it holding the author's description. It
	//is the only per-entry metadata the mirrors carry, and roughly half of
	//them follow the upload template closely enough to name what is inside.
	QString txt=entries[idx].path;
	txt.chop(4);
	txt.append(".txt");

	detailsIndex=idx;
	detailsText.clear();
	details->setPlainText("Loading "+txt+" ...");

	if (textReply) {
		textReply->abort();
		textReply=NULL;
	}

	textReply=nam->get(IdgamesRequest(QUrl(mirror()+"/"+txt)));
	QObject::connect(textReply, SIGNAL(finished()), this, SLOT(textDone()));
}

void ZDLIdgamesTab::textDone()
{
	QNetworkReply *reply=textReply;
	textReply=NULL;

	if (!reply)
		return;
	reply->deleteLater();

	if (reply->error()==QNetworkReply::OperationCanceledError)
		return;

	if (reply->error()!=QNetworkReply::NoError) {
		details->setPlainText("No description available ("+ReplyError(reply)+")");
		//Clearing the index too, so re-selecting the row retries rather than
		//being short circuited as "already showing that one".
		detailsIndex=-1;
		detailsText.clear();
		return;
	}

	QString text=QString::fromLatin1(reply->readAll());

	//These mirrors answer 200 with an HTML error page when a file is missing,
	//which is why parseListing checks what it got rather than the status. A
	//description can be written to disk, so it needs the same suspicion.
	QString head=text.left(200).trimmed().toLower();
	if (head.startsWith("<!doctype html")||head.startsWith("<html")||head.startsWith("<?xml")) {
		details->setPlainText("No description available (the mirror returned a web page, not a text file)");
		detailsIndex=-1;
		detailsText.clear();
		return;
	}

	//Shown with its folder when it is already installed; the text itself is
	//kept clean below, since that is what an install writes to disk.
	QString folder=(detailsIndex>=0&&detailsIndex<entries.size())?installed.value(entries[detailsIndex].path):QString();
	if (folder.isEmpty())
		details->setPlainText(text);
	else
		details->setPlainText(QString("Installed in %1\n\n%2").arg(QDir::toNativeSeparators(folder)).arg(text));

	//A re-index between the request and the reply invalidates the index this
	//text belonged to; show it, but never let it reach disk.
	if (detailsIndex>=0)
		detailsText=text;
}

void ZDLIdgamesTab::transferProgress(qint64 received, qint64 total)
{
	QString prefix;
	if (queueTotal>1)
		prefix=QString("[%1/%2] ").arg(queueDone+1).arg(queueTotal);

	if (total>0)
		status->setText(prefix+QString("%1 of %2").arg(HumanSize(received)).arg(HumanSize(total)));
	else
		status->setText(prefix+HumanSize(received));
}

bool ZDLIdgamesTab::havePwadFolder()
{
	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	return zconf&&zconf->hasValue("zdl.general", "pwaddir")&&!zconf->getValue("zdl.general", "pwaddir").isEmpty();
}

void ZDLIdgamesTab::download()
{
	QList<QTreeWidgetItem*> selected=results->selectedItems();

	if (selected.isEmpty()) {
		QMessageBox::information(this, ZDL_APP_NAME, "Select one or more entries to download.");
		return;
	}

	if (!havePwadFolder()) {
		QMessageBox::information(this, ZDL_APP_NAME, "Choose a PWAD folder first, on the Launch config tab. Downloads are unpacked into it.");
		return;
	}

	if (fileReply)
		return;

	queue.clear();
	queueFailed.clear();
	queueDone=0;

	foreach (QTreeWidgetItem *item, selected) {
		int idx=item->data(COL_NAME, ENTRY_INDEX_ROLE).toInt();
		if (idx>=0&&idx<entries.size()&&!queue.contains(idx))
			queue<<idx;
	}

	if (queue.isEmpty()) {
		QMessageBox::information(this, ZDL_APP_NAME, "The selected rows are stale - refresh the list and try again.");
		return;
	}

	queueTotal=queue.size();
	queueMirror=mirror();

	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	queuePwadDir=zconf?ZDLPaths::resolve(zconf->getValue("zdl.general", "pwaddir")):QString();

	//A batch of ten is a lot to ask of a volunteer-run mirror by accident.
	if (queueTotal>1) {
		qint64 bytes=0;
		foreach (int idx, queue)
			bytes+=entries[idx].size;

		if (QMessageBox::question(this, ZDL_APP_NAME,
				QString("Download %1 entries, %2 in total?").arg(queueTotal).arg(HumanSize(bytes)),
				QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes)!=QMessageBox::Yes) {
			queue.clear();
			queueTotal=0;
			return;
		}
	}

	setBusy(true);
	startNextDownload();
}

//One at a time, in order. Downloads are chained through downloadDone rather
//than started together: a dozen parallel sockets is not a reasonable thing
//to point at these mirrors, and serial progress is legible.
void ZDLIdgamesTab::startNextDownload()
{
	if (queue.isEmpty()) {
		finishQueue();
		return;
	}

	queueCurrent=queue.takeFirst();

	const ZDLIdgamesEntry &entry=entries[queueCurrent];
	status->setText(QString("[%1/%2] Downloading %3 ...").arg(queueDone+1).arg(queueTotal).arg(entry.name()));

	fileReply=nam->get(IdgamesRequest(QUrl(queueMirror+"/"+entry.path)));
	QObject::connect(fileReply, SIGNAL(finished()), this, SLOT(downloadDone()));
	QObject::connect(fileReply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(transferProgress(qint64,qint64)));
}

void ZDLIdgamesTab::finishQueue()
{
	setBusy(false);
	queueCurrent=-1;

	int ok=queueDone-queueFailed.size();

	if (queueFailed.isEmpty()) {
		status->setText(QString("Installed %1 into the PWAD folder").arg(ok==1?QString("1 entry"):QString("%1 entries").arg(ok)));
		if (queueTotal==1)
			QMessageBox::information(this, ZDL_APP_NAME, "Installed. It appears in the PWAD list on the Launch config tab.");
	} else {
		status->setText(QString("%1 installed, %2 failed").arg(ok).arg(queueFailed.size()));
		QMessageBox::warning(this, ZDL_APP_NAME, QString("%1 of %2 could not be installed:\n\n").arg(queueFailed.size()).arg(queueTotal)+queueFailed.join("\n"));
	}

	//The markers just written are what the Installed column reads.
	scanInstalled();
	markInstalled();

	queueTotal=0;
	queueDone=0;
	queueFailed.clear();
}

//Which entries are already in the PWAD folder, by the marker each install
//leaves behind. Installs only ever go into the folder's immediate
//subfolders, so this is one listing and a handful of small reads.
void ZDLIdgamesTab::scanInstalled()
{
	installed.clear();

	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	QString pwaddir=zconf?ZDLPaths::resolve(zconf->getValue("zdl.general", "pwaddir")):QString();
	if (pwaddir.isEmpty())
		return;

	QDir root(pwaddir);
	foreach (const QString &name, root.entryList(QDir::Dirs|QDir::NoDotAndDotDot)) {
		QFile marker(root.filePath(name+"/"+INSTALL_MARKER));
		if (!marker.open(QIODevice::ReadOnly|QIODevice::Text))
			continue;

		QString path=QString::fromUtf8(marker.readAll()).trimmed();
		marker.close();

		if (!path.isEmpty()&&!installed.contains(path))
			installed.insert(path, root.filePath(name));
	}
}

//Refreshes the Installed column in place, so a batch that just finished
//shows up without the list being rebuilt and the selection lost.
void ZDLIdgamesTab::markInstalled()
{
	for (int i=0; i<results->topLevelItemCount(); i++) {
		QTreeWidgetItem *item=results->topLevelItem(i);
		int idx=item->data(COL_NAME, ENTRY_INDEX_ROLE).toInt();
		QString folder=(idx>=0&&idx<entries.size())?installed.value(entries[idx].path):QString();

		item->setText(COL_INSTALLED, folder.isEmpty()?QString():QString("\342\234\223"));
		item->setToolTip(COL_INSTALLED, folder.isEmpty()?QString():QDir::toNativeSeparators(folder));
		item->setTextAlignment(COL_INSTALLED, Qt::AlignCenter);
	}
}

//Doomworld's own page for the entry carries what the mirrors do not:
//reviews, screenshots and votes. Its API refuses anything but a browser,
//so a browser is what gets sent.
void ZDLIdgamesTab::openDoomworld()
{
	QList<QTreeWidgetItem*> selected=results->selectedItems();
	if (selected.size()!=1)
		return;

	int idx=selected.first()->data(COL_NAME, ENTRY_INDEX_ROLE).toInt();
	if (idx<0||idx>=entries.size())
		return;

	//The page is the archive path without its extension.
	QString page=entries[idx].path;
	if (page.endsWith(".zip", Qt::CaseInsensitive))
		page.chop(4);

	QDesktopServices::openUrl(QUrl("https://www.doomworld.com/idgames/"+page));
}

//Unpacks into its own folder under the PWAD directory. idgames releases are
//zips wrapping a wad plus its text file, and a source port will not load a
//map wad that is still inside a zip, so leaving the archive intact would make
//the download useless.
bool ZDLIdgamesTab::installArchive(const QByteArray &zip, const ZDLIdgamesEntry &entry, const QString &description, QString &error)
{
	//Captured when the batch started. Reading it again here would follow the
	//user changing the PWAD folder, or loading another configuration, part
	//way through - nothing outside this tab is disabled while a batch runs.
	QString pwaddir=queuePwadDir;
	if (pwaddir.isEmpty()) {
		error="No PWAD folder";
		return false;
	}

	QString base=entry.name();
	base.chop(4);

	//Around a thousand names occur in more than one category, so deriving the
	//folder from the leaf name alone would unpack two unrelated releases on
	//top of each other - and on top of a folder the user made themselves.
	//An occupied folder gets a suffix, unless its marker says this same entry
	//put it there, in which case reinstalling repairs it in place rather than
	//leaving a numbered copy behind every time.
	QDir target(QDir(pwaddir).filePath(base));
	if (DirHasContent(target)&&!MarkerMatches(target, entry.path)) {
		bool placed=false;
		for (int n=2; n<100; n++) {
			QDir alt(QDir(pwaddir).filePath(QString("%1 (%2)").arg(base).arg(n)));
			if (!DirHasContent(alt)||MarkerMatches(alt, entry.path)) {
				target=alt;
				placed=true;
				break;
			}
		}
		if (!placed) {
			error="Too many folders already named like "+base;
			return false;
		}
	}

	if (!target.exists()&&!QDir().mkpath(target.absolutePath())) {
		error="Could not create "+target.absolutePath();
		return false;
	}

	mz_zip_archive archive;
	memset(&archive, 0, sizeof(archive));

	if (!mz_zip_reader_init_mem(&archive, zip.constData(), (size_t)zip.size(), 0)) {
		error="Not a readable zip archive";
		return false;
	}

	//Claim the folder before unpacking, not after. If extraction fails part
	//way, a retry then finds its own marker and repairs the folder in place,
	//instead of seeing an occupied stranger and diverting to a numbered copy
	//with the broken one left behind.
	{
		QFile marker(target.filePath(INSTALL_MARKER));
		if (marker.open(QIODevice::WriteOnly|QIODevice::Truncate|QIODevice::Text)) {
			marker.write(entry.path.toUtf8());
			marker.close();
		}
	}

	bool ok=true;
	mz_uint count=mz_zip_reader_get_num_files(&archive);

	for (mz_uint i=0; i<count; i++) {
		mz_zip_archive_file_stat stat;
		if (!mz_zip_reader_file_stat(&archive, i, &stat))
			continue;
		if (mz_zip_reader_is_file_a_directory(&archive, i))
			continue;

		//Zip entries can name paths outside the target; refuse those rather
		//than writing wherever the archive asks.
		QString member=QString::fromUtf8(stat.m_filename);
		member.replace("\\", "/");
		if (member.contains("..")||member.startsWith("/")||member.contains(":")) {
			LOGDATAO() << "Skipping suspicious zip member " << member << Qt::endl;
			continue;
		}

		size_t out_len=0;
		void *data=mz_zip_reader_extract_to_heap(&archive, i, &out_len, 0);
		if (!data) {
			ok=false;
			error="Could not unpack "+member;
			break;
		}

		QString dest=target.filePath(member);
		QDir().mkpath(QFileInfo(dest).absolutePath());

		QFile out(dest);
		if (out.open(QIODevice::WriteOnly|QIODevice::Truncate)) {
			out.write((const char*)data, (qint64)out_len);
			out.close();
		} else {
			ok=false;
			error="Could not write "+dest;
		}

		mz_free(data);

		if (!ok)
			break;
	}

	mz_zip_reader_end(&archive);

	//Keep the description beside the files it describes, but never over one
	//the archive brought itself. Testing the destination rather than tracking
	//what was unpacked covers a release whose text file is in a subfolder or
	//under another name, and the platform quietly renaming a member onto it.
	QString txt_path=target.filePath(base+".txt");
	if (ok&&!description.isEmpty()&&!QFile::exists(txt_path)) {
		QFile txt(txt_path);
		//Written as bytes: the text already carries the mirror's CRLF endings,
		//and text mode would double the CR on Windows.
		if (txt.open(QIODevice::WriteOnly|QIODevice::Truncate)) {
			txt.write(description.toLatin1());
			txt.close();
		}
	}

	return ok;
}

void ZDLIdgamesTab::downloadDone()
{
	QNetworkReply *reply=fileReply;
	fileReply=NULL;

	if (!reply)
		return;
	reply->deleteLater();

	//Which entry this is was captured when the request went out; the
	//selection may well have moved on since.
	int idx=queueCurrent;
	if (idx<0||idx>=entries.size()) {
		finishQueue();
		return;
	}

	const ZDLIdgamesEntry &entry=entries[idx];
	queueDone++;

	if (reply->error()!=QNetworkReply::NoError) {
		queueFailed<<entry.name()+" - "+ReplyError(reply);
	} else {
		QString error;
		if (!installArchive(reply->readAll(), entry, idx==detailsIndex?detailsText:QString(), error))
			queueFailed<<entry.name()+" - "+error;
	}

	startNextDownload();
}
