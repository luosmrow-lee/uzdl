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

#ifndef _ZDLUPDATECHECK_H_
#define _ZDLUPDATECHECK_H_

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

//What the latest release is, as far as an update needs to know. The asset
//fields describe the package for this platform and stay empty when the
//release carries none, in which case only the release page can be offered.
struct ZDLReleaseInfo {
	QString tag;		//v1.0.1, as the release was tagged
	QString url;		//the release page
	QString notes;		//the release body, as written on GitHub
	QString assetName;	//uZDL-1.0.1-win64.zip
	QString assetUrl;
	qint64 assetSize;
	QString assetSha256;	//hex digits; empty when GitHub published no digest

	ZDLReleaseInfo(): assetSize(0) {}

	//The tag without its leading v: what people call the version.
	QString version() const;
};

//Asks GitHub whether a repository has a release newer than this build.
//
//It only ever reports. Installing is ZDLSelfUpdate's business, and only
//where a package exists for the platform and the folder can be written.
class ZDLUpdateCheck: public QObject {
	Q_OBJECT
	public:
		ZDLUpdateCheck(QObject *parent=0);

		//"owner/repo". Empty means no repository is configured yet, which is
		//reported rather than treated as an error.
		void check(const QString &repo);
		bool busy();

		//True when candidate orders above current. Compares the runs of
		//digits in each, so v4-0.0, 4.0.0 and 4-0 all compare equal.
		static bool isNewer(const QString &candidate, const QString &current);
	signals:
		//available is false either because this build is current or because
		//the check could not be made, which error distinguishes. release is
		//filled in whenever a release was actually seen.
		void finished(bool available, const ZDLReleaseInfo &release, const QString &error);
	private slots:
		void replyDone();
	private:
		QNetworkAccessManager *nam;
		QNetworkReply *reply;
};

#endif
