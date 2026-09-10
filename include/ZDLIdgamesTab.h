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

#ifndef _ZDLIDGAMESTAB_H_
#define _ZDLIDGAMESTAB_H_

#include <QtWidgets>
#include <QObject>
#include <QDate>
#include <QVector>
#include "ZDLWidget.h"

class QNetworkAccessManager;
class QNetworkReply;

//One archive entry, as read out of the mirror's fullsort listing.
struct ZDLIdgamesEntry {
	QString path;	//levels/doom2/Ports/megawads/1024cla2.zip
	qint64 size;
	QDate date;

	ZDLIdgamesEntry(): size(0) {}

	//First path segment: levels, graphics, music, sounds, skins and so on.
	//The archive's own tree is the only classification present for every
	//entry, so it is what the category filter and the Type column work from.
	QString category() const;
	QString name() const;
	//Directory the entry sits in, which is the finer grained classification -
	//levels/doom2/Ports/megawads rather than just levels.
	QString directory() const;
	//newstuff and incoming hold hardlinked copies of recent uploads that
	//also live under their real category.
	bool staging() const;
};

//Browses the /idgames archive through one of its public mirrors.
//
//Doomworld's idgames API sits behind a bot check that answers 403 to every
//non-browser client, so this talks to the file mirrors instead. They carry a
//gzipped listing of the whole archive, which is enough to browse and search
//offline once fetched, plus the .txt description beside every entry.
class ZDLIdgamesTab: public ZDLWidget {
	Q_OBJECT
	public:
		ZDLIdgamesTab(QWidget *parent=0);
		virtual void rebuild();
		virtual void newConfig();
	protected slots:
		void updateIndex();
		void indexDone();
		void filterChanged();
		void sortChanged(int index);
		void mirrorChanged(int index);
		void entrySelected();
		void textDone();
		void download();
		void downloadDone();
		void transferProgress(qint64 received, qint64 total);
		void openDoomworld();
	private:
		QString mirror();
		QString indexCachePath();
		bool loadCachedIndex();
		void saveCachedIndex();
		//Returns false and leaves the existing index untouched when the body
		//held nothing usable, so a mirror serving an error page cannot wipe a
		//working listing.
		bool parseListing(const QByteArray &listing);
		void dropStagingDuplicates();
		void populate();
		void setBusy(bool busy);
		bool havePwadFolder();
		//Reads the install markers in the PWAD folder; marks the rows shown.
		void scanInstalled();
		void markInstalled();
		void startNextDownload();
		void finishQueue();
		bool installArchive(const QByteArray &zip, const ZDLIdgamesEntry &entry, const QString &description, QString &error);

		QComboBox *mirrorBox;
		QComboBox *categoryBox;
		QComboBox *sortBox;
		QComboBox *ageBox;
		QLineEdit *searchBox;
		QSplitter *idgamesSplit;
		QTreeWidget *results;
		QPlainTextEdit *details;
		QPushButton *btnUpdate;
		QPushButton *btnDownload;
		QPushButton *btnDoomworld;
		QLabel *status;

		QNetworkAccessManager *nam;
		QNetworkReply *indexReply;
		QNetworkReply *textReply;
		QNetworkReply *fileReply;

		QVector<ZDLIdgamesEntry> entries;
		//Entries already in the PWAD folder, by archive path, with the folder
		//each sits in. Rebuilt from the install markers whenever the tab shows.
		QHash<QString, QString> installed;
		//Newest upload the index knows about, which is what "last 7 days" and
		//the like are measured against - not today, so a stale index narrows
		//honestly instead of silently returning nothing.
		QDate indexDate;

		//Entry the description pane is about, and the description itself.
		//Kept apart from the pane because the pane also holds placeholders and
		//error strings; an install must never write those. A description is
		//only usable when detailsIndex matches AND detailsText is non-empty -
		//the index is set when the fetch starts, the text only when it lands.
		int detailsIndex;
		QString detailsText;

		//Downloads run one at a time, in order, so a batch cannot open a
		//dozen sockets at a volunteer-run mirror at once.
		QVector<int> queue;
		//Both captured when the batch starts. Re-reading either per item would
		//split a batch across two mirrors or two folders if the user changed
		//one underneath it - nothing outside this tab is disabled meanwhile.
		QString queueMirror;
		QString queuePwadDir;
		int queueCurrent;
		int queueTotal;
		int queueDone;
		QStringList queueFailed;
};

#endif
