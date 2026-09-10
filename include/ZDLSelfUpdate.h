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

#ifndef _ZDLSELFUPDATE_H_
#define _ZDLSELFUPDATE_H_

#include <QtWidgets>
#include <QObject>
#include <QCryptographicHash>
#include <QFile>
#include "ZDLUpdateCheck.h"

class QNetworkAccessManager;
class QNetworkReply;

//Replaces this installation with a release's package, in place.
//
//Windows will not let a running program or its loaded libraries be
//overwritten, but it lets them be renamed. So the package is downloaded,
//checked against the digest GitHub publishes for it, unpacked beside the
//program, and every file it brings is swapped in by renaming the current
//one to .old first. Then the new executable is started, this one quits,
//and the new one deletes the .old files once this one is gone. Nothing
//outside the package - the configuration, ports, IWADs, PWADs - is ever
//touched, and a swap that fails part way is undone before it is reported.
class ZDLSelfUpdate: public QObject {
	Q_OBJECT
	public:
		ZDLSelfUpdate(QWidget *parent);

		//Whether this build can install the release: there is a package for
		//this platform and the program folder can be written. Elsewhere the
		//release page is all that can be offered; why says which it was.
		static bool canUpdate(const ZDLReleaseInfo &release, QString *why=NULL);

		//Deletes what the previous version left behind after handing over and
		//returns the version it installed, or an empty string when nothing was
		//pending. Files still held by the exiting process are retried for a
		//while, and again at the next start if need be.
		static QString finishPending(QObject *context);

		//Downloads, verifies, unpacks and installs the release, then starts the
		//new executable and asks the application to quit. The progress dialog
		//is modal; anything going wrong arrives through failed().
		void run(const ZDLReleaseInfo &release);
	signals:
		//An empty error is the user cancelling the download.
		void failed(const QString &error);
	private slots:
		void downloadProgress(qint64 received, qint64 total);
		void downloadReadyRead();
		void downloadFinished();
		void cancelled();
	private:
		void fail(const QString &error);
		bool verify(QString &error);
		bool extract(QString &error);
		bool install(QString &error);
		void rollback();
		void relaunch();

		QWidget *parentWidget;
		ZDLReleaseInfo release;
		QNetworkAccessManager *nam;
		QNetworkReply *reply;
		QProgressDialog *progress;
		QFile download;
		QCryptographicHash digest;
		QString stageDir;
		QStringList unpacked;	//relative paths the package brought
		QStringList swapped;	//of those, the ones already in place
};

//Tells about a newer release and asks what to do with it.
class ZDLUpdateDialog: public QDialog {
	Q_OBJECT
	public:
		enum Choice { Later, Install, Skip, OpenPage };

		ZDLUpdateDialog(QWidget *parent, const ZDLReleaseInfo &release, bool canInstall, const QString &whyNot);
		Choice choice() const { return chosen; }
	private slots:
		void chooseInstall();
		void chooseSkip();
		void chooseOpenPage();
	private:
		Choice chosen;
};

#endif
