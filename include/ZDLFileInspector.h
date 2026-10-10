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

#ifndef _ZDLFILEINSPECTOR_H_
#define _ZDLFILEINSPECTOR_H_

#include <QtWidgets>
#include <QObject>

//Tells what a file is and what it holds: an IWAD and which release, a
//PWAD with its maps and the definitions in it, an archive, a DeHackEd
//patch, a demo, a launch file, a source port and its version. Any file can
//be typed, browsed to or dropped on it.
class ZDLFileInspector: public QDialog {
	Q_OBJECT
	public:
		ZDLFileInspector(QWidget *parent, const QString &file);

		//Opens an inspector on file, or on one the user picks when file is
		//empty. The window is not modal, so it can stay open beside the
		//lists, and it closes itself away.
		static void open(QWidget *parent, const QString &file=QString());

		//The report as text, for anything that wants it without a window.
		static QString report(const QString &file);
	protected:
		void dragEnterEvent(QDragEnterEvent *event);
		void dropEvent(QDropEvent *event);
	private slots:
		void browse();
		void copyReport();
		void inspectTyped();
	private:
		void inspect(const QString &file);

		QLineEdit *pathEdit;
		QPlainTextEdit *view;
};

#endif
