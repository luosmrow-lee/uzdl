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
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "ZDLUpdateCheck.h"
#include "zdlcommon.h"

QString ZDLReleaseInfo::version() const
{
	QString v=tag;
	if (v.size()>1&&(v[0]=='v'||v[0]=='V')&&v[1].isDigit())
		v.remove(0, 1);
	return v;
}

ZDLUpdateCheck::ZDLUpdateCheck(QObject *parent): QObject(parent)
{
	nam=new QNetworkAccessManager(this);
	reply=NULL;
}

bool ZDLUpdateCheck::busy()
{
	return reply!=NULL;
}

//Every run of digits in the string, in order: "v4-0.0" gives 4, 0, 0. Any
//prefix, suffix or separator style therefore compares the same way, which
//matters because release tags are spelled however their author felt.
static QList<int> VersionParts(const QString &version)
{
	QList<int> parts;

	QRegularExpression digits("\\d+");
	QRegularExpressionMatchIterator it=digits.globalMatch(version);
	while (it.hasNext())
		parts<<it.next().captured(0).toInt();

	return parts;
}

bool ZDLUpdateCheck::isNewer(const QString &candidate, const QString &current)
{
	QList<int> a=VersionParts(candidate);
	QList<int> b=VersionParts(current);

	//A tag with no digits at all cannot be shown to be newer, so it is not.
	if (a.isEmpty())
		return false;

	for (int i=0; i<qMax(a.size(), b.size()); i++) {
		int x=i<a.size()?a[i]:0;
		int y=i<b.size()?b[i]:0;
		if (x!=y)
			return x>y;
	}

	return false;
}

void ZDLUpdateCheck::check(const QString &repo)
{
	if (reply)
		return;

	QString trimmed=repo.trimmed();
	while (trimmed.endsWith("/"))
		trimmed.chop(1);

	if (trimmed.isEmpty()) {
		emit finished(false, ZDLReleaseInfo(), "No update repository is set");
		return;
	}

	//Accept a full URL as well as owner/repo, since that is what anyone
	//pasting from a browser will have.
	trimmed.remove(QRegularExpression("^https?://(www\\.)?github\\.com/", QRegularExpression::CaseInsensitiveOption));

	if (!QRegularExpression(QRegularExpression::anchoredPattern("[A-Za-z0-9._-]+/[A-Za-z0-9._-]+")).match(trimmed).hasMatch()) {
		emit finished(false, ZDLReleaseInfo(), "Not an owner/repo name: "+repo);
		return;
	}

	QNetworkRequest request(QUrl("https://api.github.com/repos/"+trimmed+"/releases/latest"));
	//GitHub refuses requests that do not identify themselves.
	request.setHeader(QNetworkRequest::UserAgentHeader, QString(ZDL_APP_NAME)+"/"+ZDL_VERSION_STRING);
	request.setRawHeader("Accept", "application/vnd.github+json");

	LOGDATAO() << "Checking " << request.url().toString() << Qt::endl;

	reply=nam->get(request);
	QObject::connect(reply, SIGNAL(finished()), this, SLOT(replyDone()));
}

//The package for this platform, by name: the release workflow calls it
//uZDL-<version>-win64.zip. Any other asset a release might carry is ignored.
static bool PlatformAsset(const QString &name)
{
#if defined(Q_OS_WIN)
	return QRegularExpression("^uZDL-.*-win64\\.zip$", QRegularExpression::CaseInsensitiveOption).match(name).hasMatch();
#else
	Q_UNUSED(name);
	return false;
#endif
}

void ZDLUpdateCheck::replyDone()
{
	QNetworkReply *done=reply;
	reply=NULL;

	if (!done)
		return;
	done->deleteLater();

	int status=done->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

	if (done->error()!=QNetworkReply::NoError) {
		//A repository with no releases at all answers 404, which is a
		//statement about the repository rather than a failure to reach it.
		if (status==404)
			emit finished(false, ZDLReleaseInfo(), "The repository has no releases yet");
		else if (status==403)
			emit finished(false, ZDLReleaseInfo(), "GitHub rate limit reached, try later");
		else
			emit finished(false, ZDLReleaseInfo(), done->errorString());
		return;
	}

	QJsonParseError parse;
	QJsonDocument doc=QJsonDocument::fromJson(done->readAll(), &parse);
	if (parse.error!=QJsonParseError::NoError||!doc.isObject()) {
		emit finished(false, ZDLReleaseInfo(), "GitHub returned something that is not a release");
		return;
	}

	QJsonObject release=doc.object();

	ZDLReleaseInfo info;
	info.tag=release.value("tag_name").toString();
	info.url=release.value("html_url").toString();
	info.notes=release.value("body").toString();

	//GitHub publishes a SHA-256 digest with every asset it stores, which is
	//what makes installing the download without a second thought possible.
	foreach (const QJsonValue &value, release.value("assets").toArray()) {
		QJsonObject asset=value.toObject();
		QString name=asset.value("name").toString();
		if (!PlatformAsset(name))
			continue;

		info.assetName=name;
		info.assetUrl=asset.value("browser_download_url").toString();
		info.assetSize=(qint64)asset.value("size").toDouble();

		QString digest=asset.value("digest").toString();
		if (digest.startsWith("sha256:", Qt::CaseInsensitive))
			info.assetSha256=digest.mid(7).trimmed();
		break;
	}

	if (info.tag.isEmpty()) {
		emit finished(false, info, "The latest release has no tag");
		return;
	}

	if (release.value("draft").toBool()||release.value("prerelease").toBool()) {
		emit finished(false, info, "The latest release is a draft or prerelease");
		return;
	}

	emit finished(isNewer(info.tag, ZDL_VERSION_STRING), info, QString());
}
