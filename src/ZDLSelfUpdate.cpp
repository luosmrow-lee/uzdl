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
#include "ZDLSelfUpdate.h"
#include "ZDLMainWindow.h"
#include "ZDLPaths.h"
#include "zdlcommon.h"
#include "miniz.h"

extern ZDLMainWindow *mw;

//Everything transient lives under here, inside the program folder, so that
//the swap is a rename on one volume rather than a copy across two.
#define STAGE_DIR "update"
#define MANIFEST "pending.txt"

static QString StagePath()
{
	return QDir(ZDLPaths::appDir()).filePath(STAGE_DIR);
}

static QString HumanSize(qint64 bytes)
{
	if (bytes>=1024*1024)
		return QString("%1 MB").arg(bytes/(1024.0*1024.0), 0, 'f', 1);
	return QString("%1 KB").arg(qMax((qint64)1, bytes/1024));
}

ZDLSelfUpdate::ZDLSelfUpdate(QWidget *parent): QObject(parent), parentWidget(parent), reply(NULL), progress(NULL), digest(QCryptographicHash::Sha256)
{
	nam=new QNetworkAccessManager(this);
}

bool ZDLSelfUpdate::canUpdate(const ZDLReleaseInfo &release, QString *why)
{
	QString reason;

#if !defined(Q_OS_WIN)
	Q_UNUSED(release);
	reason="Installing updates in place is only done for the Windows package.";
#else
	if (release.assetName.isEmpty()) {
		reason="The release has no Windows package attached.";
	} else {
		//A probe rather than a permission check: what matters is whether a
		//file can actually be made here, whatever the ACLs claim.
		QDir app(ZDLPaths::appDir());
		QFile probe(app.filePath(QString(STAGE_DIR)+"/.probe"));
		if (!app.mkpath(STAGE_DIR)||!probe.open(QIODevice::WriteOnly|QIODevice::Truncate)) {
			reason="The program folder cannot be written to, so the update has to be unpacked by hand.";
		} else {
			probe.close();
			probe.remove();
			app.rmdir(STAGE_DIR);
		}
	}
#endif

	if (why)
		*why=reason;
	return reason.isEmpty();
}

static bool DeleteOld(const QStringList &files)
{
	bool all=true;
	QDir app(ZDLPaths::appDir());

	foreach (const QString &rel, files) {
		QString old=app.filePath(rel)+".old";
		if (QFile::exists(old)&&!QFile::remove(old))
			all=false;
	}

	return all;
}

//The executable that started this one is usually still on its way out when
//this runs, holding its own .old file; a few tries some seconds apart cover
//that. What is still held after them waits for the next start.
static void RetryCleanup(QObject *context, const QStringList &files, int attempts)
{
	QTimer::singleShot(3000, context, [context, files, attempts]{
		if (DeleteOld(files)) {
			QFile::remove(StagePath()+"/"+MANIFEST);
			QDir(StagePath()).removeRecursively();
		} else if (attempts>1) {
			RetryCleanup(context, files, attempts-1);
		}
	});
}

QString ZDLSelfUpdate::finishPending(QObject *context)
{
	QFile manifest(StagePath()+"/"+MANIFEST);
	if (!manifest.exists()||!manifest.open(QIODevice::ReadOnly|QIODevice::Text))
		return QString();

	QStringList lines=QString::fromUtf8(manifest.readAll()).split('\n', Qt::SkipEmptyParts);
	manifest.close();
	if (lines.isEmpty())
		return QString();

	QString version=lines.takeFirst().trimmed();
	QStringList files;
	foreach (const QString &line, lines)
		files<<line.trimmed();

	if (DeleteOld(files)) {
		manifest.remove();
		QDir(StagePath()).removeRecursively();
	} else {
		RetryCleanup(context, files, 10);
	}

	return version;
}

void ZDLSelfUpdate::run(const ZDLReleaseInfo &wanted)
{
	if (reply)
		return;

	release=wanted;

	QString why;
	if (!canUpdate(release, &why)) {
		emit failed(why);
		return;
	}

	stageDir=StagePath();
	QDir().mkpath(stageDir);
	//Whatever an earlier, interrupted attempt left behind.
	QDir(stageDir+"/new").removeRecursively();
	unpacked.clear();
	swapped.clear();

	download.setFileName(stageDir+"/"+release.assetName);
	if (!download.open(QIODevice::WriteOnly|QIODevice::Truncate)) {
		emit failed("Cannot write "+QDir::toNativeSeparators(download.fileName()));
		return;
	}
	digest.reset();

	progress=new QProgressDialog(QString("Downloading uZDL %1...").arg(release.version()), "Cancel", 0, 1000, parentWidget);
	progress->setWindowTitle(ZDL_APP_NAME);
	progress->setWindowModality(Qt::WindowModal);
	progress->setMinimumDuration(0);
	progress->setAutoClose(false);
	progress->setAutoReset(false);
	progress->setMinimumWidth(380);
	QObject::connect(progress, SIGNAL(canceled()), this, SLOT(cancelled()));
	progress->setValue(0);

	QNetworkRequest request((QUrl(release.assetUrl)));
	request.setHeader(QNetworkRequest::UserAgentHeader, QString(ZDL_APP_NAME)+"/"+ZDL_VERSION_STRING);
	//The asset URL answers with a redirect to the store that holds the file.
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

	LOGDATAO() << "Downloading " << release.assetUrl << Qt::endl;

	reply=nam->get(request);
	QObject::connect(reply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(downloadProgress(qint64,qint64)));
	QObject::connect(reply, SIGNAL(readyRead()), this, SLOT(downloadReadyRead()));
	QObject::connect(reply, SIGNAL(finished()), this, SLOT(downloadFinished()));
}

void ZDLSelfUpdate::downloadProgress(qint64 received, qint64 total)
{
	if (!progress)
		return;

	if (total<=0)
		total=release.assetSize;

	if (total>0) {
		progress->setValue((int)(received*1000/total));
		progress->setLabelText(QString("Downloading uZDL %1 - %2 of %3").arg(release.version()).arg(HumanSize(received)).arg(HumanSize(total)));
	}
}

//Written out and hashed as it arrives, so the whole package never has to
//sit in memory and the checksum is ready the moment the download ends.
void ZDLSelfUpdate::downloadReadyRead()
{
	if (!reply)
		return;

	QByteArray chunk=reply->readAll();
	download.write(chunk);
	digest.addData(chunk);
}

void ZDLSelfUpdate::cancelled()
{
	//downloadFinished does the tidying, with the reply marked as cancelled.
	if (reply)
		reply->abort();
}

void ZDLSelfUpdate::downloadFinished()
{
	QNetworkReply *done=reply;
	reply=NULL;

	if (!done)
		return;
	done->deleteLater();

	QByteArray rest=done->readAll();
	download.write(rest);
	digest.addData(rest);
	download.close();

	if (done->error()==QNetworkReply::OperationCanceledError) {
		fail(QString());
		return;
	}

	if (done->error()!=QNetworkReply::NoError) {
		fail(done->errorString());
		return;
	}

	int status=done->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	if (status!=200) {
		fail(QString("HTTP %1 while downloading the package").arg(status));
		return;
	}

	if (progress) {
		progress->setValue(1000);
		progress->setLabelText("Checking and unpacking the download...");
		QCoreApplication::processEvents();
	}

	QString error;
	if (!verify(error)||!extract(error)||!install(error)) {
		fail(error);
		return;
	}

	relaunch();
}

void ZDLSelfUpdate::fail(const QString &error)
{
	if (progress) {
		progress->deleteLater();
		progress=NULL;
	}

	QDir(stageDir+"/new").removeRecursively();
	QFile::remove(download.fileName());

	if (!error.isEmpty())
		LOGDATAO() << "Update failed: " << error << Qt::endl;

	emit failed(error);
}

bool ZDLSelfUpdate::verify(QString &error)
{
	QFileInfo fi(download.fileName());
	if (release.assetSize>0&&fi.size()!=release.assetSize) {
		error=QString("The download is %1 bytes, the release says %2").arg(fi.size()).arg(release.assetSize);
		return false;
	}

	QString got=QString::fromLatin1(digest.result().toHex());
	if (release.assetSha256.isEmpty()) {
		//Nothing to check against. GitHub publishes a digest for everything
		//it stores, so this is unexpected, but it is not evidence of anything.
		LOGDATAO() << "No digest published for " << release.assetName << Qt::endl;
	} else if (got.compare(release.assetSha256, Qt::CaseInsensitive)!=0) {
		error="The download does not match the checksum GitHub published for it";
		return false;
	}

	return true;
}

//Unpacks into update/new, minus the package's own top folder, refusing any
//member that names a path outside it - the same suspicion the idgames
//installer applies, since this archive too came off the network.
bool ZDLSelfUpdate::extract(QString &error)
{
	QFile zipFile(download.fileName());
	if (!zipFile.open(QIODevice::ReadOnly)) {
		error="Cannot read the download back";
		return false;
	}
	QByteArray zip=zipFile.readAll();
	zipFile.close();

	mz_zip_archive archive;
	memset(&archive, 0, sizeof(archive));

	if (!mz_zip_reader_init_mem(&archive, zip.constData(), (size_t)zip.size(), 0)) {
		error="The download is not a readable zip archive";
		return false;
	}

	mz_uint count=mz_zip_reader_get_num_files(&archive);

	//The package wraps everything in uZDL-<version>-win64/. That folder is
	//dropped when every member sits under one and the same top folder.
	QString top;
	bool wrapped=count>0;
	for (mz_uint i=0; i<count&&wrapped; i++) {
		mz_zip_archive_file_stat stat;
		if (!mz_zip_reader_file_stat(&archive, i, &stat))
			continue;

		QString member=QString::fromUtf8(stat.m_filename);
		member.replace("\\", "/");
		QString first=member.section('/', 0, 0);

		if (first.isEmpty()||!member.contains('/'))
			wrapped=false;
		else if (top.isEmpty())
			top=first;
		else if (first!=top)
			wrapped=false;
	}

	QDir fresh(stageDir+"/new");
	bool ok=true;

	for (mz_uint i=0; i<count; i++) {
		mz_zip_archive_file_stat stat;
		if (!mz_zip_reader_file_stat(&archive, i, &stat))
			continue;
		if (mz_zip_reader_is_file_a_directory(&archive, i))
			continue;

		QString member=QString::fromUtf8(stat.m_filename);
		member.replace("\\", "/");
		if (wrapped)
			member=member.mid(top.size()+1);

		if (member.isEmpty()||member.contains("..")||member.startsWith("/")||member.contains(":")) {
			error="The package names a path outside its folder: "+member;
			ok=false;
			break;
		}

		//The configuration is the user's, whatever a package might carry.
		if (!member.compare(ZDL_PORTABLE_INI, Qt::CaseInsensitive))
			continue;

		size_t len=0;
		void *data=mz_zip_reader_extract_to_heap(&archive, i, &len, 0);
		if (!data) {
			error="Could not unpack "+member;
			ok=false;
			break;
		}

		QString dest=fresh.filePath(member);
		QDir().mkpath(QFileInfo(dest).absolutePath());

		QFile out(dest);
		if (out.open(QIODevice::WriteOnly|QIODevice::Truncate)) {
			out.write((const char*)data, (qint64)len);
			out.close();
			unpacked<<member;
		} else {
			error="Could not write "+dest;
			ok=false;
		}

		mz_free(data);

		if (!ok)
			break;
	}

	mz_zip_reader_end(&archive);

	if (ok&&!unpacked.contains("uzdl.exe", Qt::CaseInsensitive)) {
		error="The package holds no uzdl.exe";
		ok=false;
	}

	return ok;
}

//The swap itself. The manifest goes first, so that even a crash in the
//middle leaves the next start knowing which .old files to clear away.
bool ZDLSelfUpdate::install(QString &error)
{
	QDir app(ZDLPaths::appDir());
	QDir fresh(stageDir+"/new");

	QFile manifest(stageDir+"/"+MANIFEST);
	if (!manifest.open(QIODevice::WriteOnly|QIODevice::Truncate|QIODevice::Text)) {
		error="Cannot write the update manifest";
		return false;
	}
	manifest.write((release.version()+"\n"+unpacked.join("\n")+"\n").toUtf8());
	manifest.close();

	foreach (const QString &rel, unpacked) {
		QString target=app.filePath(rel);
		QString old=target+".old";
		QString source=fresh.filePath(rel);

		QDir().mkpath(QFileInfo(target).absolutePath());

		if (QFile::exists(old)&&!QFile::remove(old)) {
			error="A leftover "+rel+".old is in the way and cannot be removed";
			rollback();
			return false;
		}

		if (QFile::exists(target)&&!QFile::rename(target, old)) {
			error="Could not step "+rel+" aside; something else may be holding it";
			rollback();
			return false;
		}

		if (!QFile::rename(source, target)) {
			QFile::rename(old, target);
			error="Could not move the new "+rel+" into place";
			rollback();
			return false;
		}

		swapped<<rel;
	}

	//Only the manifest stays behind for the next start.
	fresh.removeRecursively();
	QFile::remove(download.fileName());

	return true;
}

//Puts every swapped file back, newest first, and withdraws the manifest so
//the next start does not report an update that never happened.
void ZDLSelfUpdate::rollback()
{
	QDir app(ZDLPaths::appDir());

	for (int i=swapped.size()-1; i>=0; i--) {
		QString target=app.filePath(swapped[i]);
		QFile::remove(target);
		QFile::rename(target+".old", target);
	}

	swapped.clear();
	QFile::remove(stageDir+"/"+MANIFEST);
}

void ZDLSelfUpdate::relaunch()
{
	if (progress)
		progress->setLabelText("Restarting...");

	//Flush what the panes hold before the new instance reads it.
	if (mw)
		mw->writeConfig();

	QString exe=QDir(ZDLPaths::appDir()).filePath("uzdl.exe");
	if (!QProcess::startDetached(exe, QStringList(), ZDLPaths::appDir())) {
		if (progress) {
			progress->deleteLater();
			progress=NULL;
		}
		emit failed("The new version is in place but could not be started. Start uZDL again by hand.");
		return;
	}

	if (progress) {
		progress->deleteLater();
		progress=NULL;
	}

	//quit writes the configuration once more and closes the window, which
	//ends the application; the new instance takes it from there.
	if (mw)
		QMetaObject::invokeMethod(mw, "quit", Qt::QueuedConnection);
	else
		qApp->quit();
}

ZDLUpdateDialog::ZDLUpdateDialog(QWidget *parent, const ZDLReleaseInfo &release, bool canInstall, const QString &whyNot): QDialog(parent), chosen(Later)
{
	setWindowTitle(ZDL_APP_NAME);
	setWindowFlags(windowFlags()&~Qt::WindowContextHelpButtonHint);

	QVBoxLayout *box=new QVBoxLayout(this);

	QLabel *head=new QLabel(QString("<b>uZDL %1 is available.</b> This build is %2.").arg(release.version()).arg(ZDL_VERSION_STRING), this);
	box->addWidget(head);

	if (!canInstall) {
		QLabel *why=new QLabel(whyNot+" The release page has the download.", this);
		why->setWordWrap(true);
		box->addWidget(why);
	}

	QPlainTextEdit *notes=new QPlainTextEdit(this);
	notes->setReadOnly(true);
	notes->setPlainText(release.notes.trimmed().isEmpty()?QString("(no release notes)"):release.notes.trimmed());
	notes->setMinimumSize(540, 260);
	box->addWidget(notes, 1);

	QHBoxLayout *buttons=new QHBoxLayout();
	QPushButton *primary=new QPushButton(canInstall?"Update and restart":"Open release page", this);
	primary->setDefault(true);
	QPushButton *skip=new QPushButton("Skip this version", this);
	skip->setToolTip("Say nothing more about this release; the next one is offered again");
	QPushButton *later=new QPushButton("Not now", this);
	buttons->addStretch();
	buttons->addWidget(primary);
	buttons->addWidget(skip);
	buttons->addWidget(later);
	box->addLayout(buttons);

	QObject::connect(primary, SIGNAL(clicked()), this, canInstall?SLOT(chooseInstall()):SLOT(chooseOpenPage()));
	QObject::connect(skip, SIGNAL(clicked()), this, SLOT(chooseSkip()));
	QObject::connect(later, SIGNAL(clicked()), this, SLOT(reject()));
}

void ZDLUpdateDialog::chooseInstall()
{
	chosen=Install;
	accept();
}

void ZDLUpdateDialog::chooseSkip()
{
	chosen=Skip;
	accept();
}

void ZDLUpdateDialog::chooseOpenPage()
{
	chosen=OpenPage;
	accept();
}
