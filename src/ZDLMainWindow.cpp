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
#include <QRegularExpression>
#include <QMainWindow>

#include "ZDLInterface.h"
#include "ZDLPaths.h"
#include "ZDLMainWindow.h"
#include "ZDLConfigurationManager.h"
#include "ZDLImportDialog.h"
#include "ZDLMapFile.h"
#include "ZDLRazeSetup.h"
#include "ZDLBuildArgs.h"
#include "ZDLBuildGames.h"
#include "ZDLBuildTab.h"
#include "ZDLLaunchFiles.h"

#ifdef Q_OS_WIN
#include <windows.h>

//QString::utf16() returns const ushort*. The original Visual Studio
//project set /Zc:wchar_t- so that bound to wchar_t* implicitly; Qt 6's
//own binaries are built with the conforming default, so the conversion
//has to be explicit here instead.
static inline const wchar_t *WStr(const QString &str)
{
	return reinterpret_cast<const wchar_t*>(str.utf16());
}

#else
#include <wordexp.h>
#endif

ZDLMainWindow::~ZDLMainWindow(){
	QSize sze = this->size();
	QPoint pt = this->pos();
	ZDLConf *zconf = ZDLConfigurationManager::getActiveConfiguration();
	if(zconf){
		QString str = QString("%1,%2").arg(sze.width()).arg(sze.height());
		zconf->setValue("zdl.general", "windowsize", str);
		str = QString("%1,%2").arg(pt.x()).arg(pt.y());
		zconf->setValue("zdl.general", "windowpos", str);
	}
	LOGDATAO() << "Closing main window" << Qt::endl;
}

QString ZDLMainWindow::getWindowTitle(){
	QString windowTitle = ZDL_APP_NAME;
	windowTitle += " " ZDL_VERSION_STRING;
	ZDLConfiguration *conf = ZDLConfigurationManager::getConfiguration();
	if(conf){
		QString userConfPath = conf->getPath(ZDLConfiguration::CONF_USER);
		QString currentConf = ZDLConfigurationManager::getConfigFileName();
		if(userConfPath != currentConf){
			windowTitle += " [" + ZDLConfigurationManager::getConfigFileName() + "]";
		}
	}else{
		windowTitle += ZDLConfigurationManager::getConfigFileName();
	}
	LOGDATAO() << "Returning main window title " << windowTitle << Qt::endl;
	return windowTitle;

}

ZDLMainWindow::ZDLMainWindow(QWidget *parent): 
	QMainWindow(parent)
{
	LOGDATAO() << "New main window " << DPTR(this) << Qt::endl;
	QString windowTitle = getWindowTitle();
	setWindowTitle(windowTitle);

	setWindowIcon(ZDLConfigurationManager::getIcon());

	setContentsMargins(0,2,0,0);
	layout()->setContentsMargins(0,0,0,0);
	QTabWidget *widget = new QTabWidget(this);

	ZDLConf *zconf = ZDLConfigurationManager::getActiveConfiguration();
	if(zconf){
		int ok = 0;
		bool qtok = false;
		if(zconf->hasValue("zdl.general", "windowsize")){
			QString size = zconf->getValue("zdl.general", "windowsize", &ok);
			if(size.contains(",")){
				QStringList list = size.split(",");
				int w = list[0].toInt(&qtok);
				if(qtok){
					int h = list[1].toInt(&qtok);
					if(qtok){
						LOGDATAO() << "Resizing to w:" << w << " h:" << h << Qt::endl;
						this->resize(QSize(w,h));
					}
				}
			}
		}
		if(zconf->hasValue("zdl.general", "windowpos")){
			QString size = zconf->getValue("zdl.general", "windowpos", &ok);
			if(size.contains(",")){
				QStringList list = size.split(",");
				int x = list[0].toInt(&qtok);
				if(qtok){
					int y = list[1].toInt(&qtok);
					if(qtok){
						LOGDATAO() << "Moving to x:" << x << " y:" << y << Qt::endl;
						this->move(QPoint(x,y));
					}
				}
			}

		}
	}


	launchSection = ZDLLaunchFiles::LAUNCH_SECTION;
	intr = new ZDLInterface(this);
	buildTab = new ZDLBuildTab(this);
	settings = new ZDLSettingsTab(this);
	appSettings = new ZDLAppSettingsTab(this);

	widget->setDocumentMode(true);
	idgames = new ZDLIdgamesTab(this);

	//The Doom engine's ports and games, and the Build engine's, each on a tab
	//of its own with its own launch configuration, presets and files.
	widget->addTab(intr, "Doom Engine");
	widget->addTab(buildTab, "Build Engine");
	widget->addTab(settings, "General settings");
	widget->addTab(appSettings, "uZDL settings");
	widget->addTab(idgames, "idgames");
	setCentralWidget(widget);

	QAction *qact = new QAction(widget);
	qact->setShortcut(Qt::Key_Return);
	widget->addAction(qact);
	connect(qact, SIGNAL(triggered()), this, SLOT(launch()));

	qact2 = new QAction(widget);
	qact2->setShortcut(Qt::Key_Escape);
	widget->addAction(qact2);

	connect(qact2, SIGNAL(triggered()), this, SLOT(quit()));

	connect(widget, SIGNAL(currentChanged(int)), this, SLOT(tabChange(int)));
	LOGDATAO() << "Main window created." << Qt::endl;
}

void ZDLMainWindow::handleImport(){
#if !defined(NO_IMPORT)
	ZDLConfiguration *conf = ZDLConfigurationManager::getConfiguration();
	if(conf){
		QString userConfPath = conf->getPath(ZDLConfiguration::CONF_USER);
		QString currentConf = ZDLConfigurationManager::getConfigFileName();
		if(userConfPath != currentConf){
			LOGDATAO() << "Not currently using user conf" << Qt::endl;
			ZDLConf *zconf = ZDLConfigurationManager::getActiveConfiguration();
			if(zconf->hasValue("zdl.general", "donotimportthis")){
				int ok = 0;
				if(zconf->getValue("zdl.general", "donotimportthis", &ok) == "1"){
					LOGDATAO() << "Don't import current config" << Qt::endl;
					return;
				}
			}
			QFile userFile(userConfPath);
			ZDLConf userconf;
			if(userFile.exists()){
				LOGDATAO() << "Reading user conf" << Qt::endl;
				userconf.readINI(userConfPath);
			}
			if(userconf.hasValue("zdl.general", "nouserconf")){
				int ok = 0;
				if(userconf.getValue("zdl.general", "nouserconf", &ok) == "1"){
					LOGDATAO() << "Do not use user conf" << Qt::endl;
					return;
				}
			}
			if(ZDLConfigurationManager::getWhy() == ZDLConfigurationManager::USER_SPECIFIED){
				LOGDATA() << "The user asked for this ini, don't go forward" << Qt::endl;
				return;
			}
			if(userFile.size() < 10){
				LOGDATA() << "User conf is small, assuming empty" << Qt::endl;
				ZDLImportDialog importd(this);
				importd.exec();
				if(importd.result() == QDialog::Accepted){
					switch(importd.getImportAction()){
						case ZDLImportDialog::IMPORTNOW:
							LOGDATAO() << "Importing now" << Qt::endl;
							if(!userFile.exists()){
								QStringList path = userConfPath.split("/");
								path.removeLast();
								QDir dir;
								if(!dir.mkpath(path.join("/"))){
									break;
								}
							}

							zconf->setValue("zdl.general", "importedfrom", currentConf);
							zconf->setValue("zdl.general", "isimported", "1");
							zconf->setValue("zdl.general", "importdate", QDateTime::currentDateTime().toString(Qt::ISODate));

							zconf->writeINI(userConfPath);
							ZDLConfigurationManager::setConfigFileName(userConfPath);
							break;
						case ZDLImportDialog::DONOTIMPORTTHIS:
							LOGDATAO() << "Tagging this config as not importable" << Qt::endl;
							zconf->setValue("zdl.general", "donotimportthis", "1");
							break;
						case ZDLImportDialog::NEVERIMPORT:
							LOGDATAO() << "Setting NEVERi IMPORT" << Qt::endl;
							userconf.setValue("zdl.general", "nouserconf", "1");
							if(!userFile.exists()){
								QStringList path = userConfPath.split("/");
								path.removeLast();
								QDir dir;
								if(!dir.mkpath(path.join("/"))){
									break;
								}

							}	
							userconf.writeINI(userConfPath);
							break;
						case ZDLImportDialog::ASKLATER:

						case ZDLImportDialog::UNKNOWN:
						default:
							LOGDATAO() << "Not setting anything" << Qt::endl;
							break;
					}
				}
			}
		}
	}
#endif
}

//The tabs left behind write what they hold into the configuration, and the
//one coming up reads it back, so that no two of them disagree.
void ZDLMainWindow::tabChange(int newTab){
	LOGDATAO() << "Tab changed to " << newTab << Qt::endl;
	QTabWidget *tabs = qobject_cast<QTabWidget*>(centralWidget());
	QWidget *current = tabs ? tabs->widget(newTab) : NULL;

	if (current != intr)
		intr->notifyFromParent(NULL);
	if (current != buildTab)
		buildTab->notifyFromParent(NULL);
	if (current != settings)
		settings->notifyFromParent(NULL);
	if (current != appSettings)
		appSettings->rebuild();
	if (current != idgames)
		idgames->rebuild();

	if (current == intr)
		intr->readFromParent(NULL);
	else if (current == buildTab)
		buildTab->readFromParent(NULL);
	else if (current == settings)
		settings->readFromParent(NULL);
	else if (current == appSettings)
		appSettings->newConfig();
	else if (current == idgames)
		idgames->newConfig();
}

void ZDLMainWindow::quit(){
	LOGDATAO() << "quitting" << Qt::endl;
	writeConfig();
	close();
}

//Whether the IWAD list has an entry by this name. A preset can carry one
//from before the list changed, and the launch tab, which would have dropped
//it on a re-read, is not consulted on the command-line path.
static bool IwadKnown(ZDLConf *zconf, const QString &name)
{
	if (name.isEmpty())
		return false;

	ZDLSection *section = zconf->getSection("zdl.iwads");
	if (!section)
		return false;

	QVector<ZDLLine*> names;
	section->getRegex("^i[0-9]+n$", names);
	foreach (ZDLLine *line, names) {
		if (line->getValue().compare(name) == 0)
			return true;
	}

	return false;
}

//From the tab on show: Enter on the Build Engine tab starts its game.
void ZDLMainWindow::launch(){
	QTabWidget *tabs = qobject_cast<QTabWidget*>(centralWidget());
	launchFrom(tabs && tabs->currentWidget() == buildTab ? QString(ZDLLaunchFiles::BUILD_SECTION) : QString(ZDLLaunchFiles::LAUNCH_SECTION));
}

void ZDLMainWindow::showLaunchTab(const QString &section){
	QTabWidget *tabs = qobject_cast<QTabWidget*>(centralWidget());
	if (tabs)
		tabs->setCurrentWidget(section == ZDLLaunchFiles::BUILD_SECTION ? (QWidget*)buildTab : (QWidget*)intr);
}

void ZDLMainWindow::launchFrom(const QString &section){
	LOGDATAO() << "Launching from " << section << Qt::endl;
	writeConfig();
	QString previous = launchSection;
	setLaunchSection(section);
	launchNow();
	setLaunchSection(previous);
}

//The launch itself, from the configuration as it stands. launch flushes the
//panes into it first; a launch asked for on the command line writes what it
//wants straight in and comes here, so that nothing a pane holds, or has yet
//to be told, can get between the preset and the port.
void ZDLMainWindow::launchNow(){
	ZDLConf *zconf = ZDLConfigurationManager::getActiveConfiguration();

	QString exec=getExecutable();
	if (exec.length() < 1){
		QMessageBox::warning(this, ZDL_APP_NAME, "Please select a source port.");
		return;
	}
	//An IWAD can be deselected on purpose so the port falls back on its own
	//default, so this asks rather than refuses. Launching silently ends in
	//the port's error dialog instead, which is a worse place to find out.
	//A name the IWAD list no longer has counts as none.
	if (!zconf->hasValue(launchSection, "iwad")||!IwadKnown(zconf, zconf->getValue(launchSection, "iwad"))){
		if (QMessageBox::question(this, ZDL_APP_NAME,
				"No IWAD is selected. Launch anyway and let the source port choose one?",
				QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes)!=QMessageBox::Yes)
			return;
	}

	QFileInfo exec_fi(exec);
	bool no_err=true;

	//UZDL_DIR for the port to inherit, and for a Raze set up for uZDL its
	//search list brought up to date first.
	ZDLRazeSetup::prepareLaunch(zconf, exec_fi.absoluteFilePath(), launchSection);

	//A preset's own save folder and config file: made here, since not every
	//port creates a folder it was told to save into.
	if (zconf->hasValue(launchSection, "savedir"))
		QDir().mkpath(ZDLPaths::resolve(zconf->getValue(launchSection, "savedir")));
	if (zconf->hasValue(launchSection, "config"))
		QDir().mkpath(QFileInfo(ZDLPaths::resolve(zconf->getValue(launchSection, "config"))).absolutePath());

#ifdef Q_OS_WIN
	PROCESS_INFORMATION pi={};
	STARTUPINFO si={sizeof(STARTUPINFO), NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, STARTF_USESHOWWINDOW, SW_SHOWNORMAL};

	QString cmdline="\""+QDir::toNativeSeparators(exec_fi.absoluteFilePath())+"\" "+getArgumentsString(true);
	QString cwd=QDir::toNativeSeparators(exec_fi.absolutePath());

	if (CreateProcess(NULL, const_cast<wchar_t*>(WStr(cmdline)), NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS|CREATE_UNICODE_ENVIRONMENT|CREATE_NEW_CONSOLE, NULL, WStr(cwd), &si, &pi)) {
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	} else {
		QMessageBox::warning(this, ZDL_APP_NAME, "Failed to launch the application executable.");
		no_err=false;
	}
#else
	if (!QProcess::startDetached(exec_fi.absoluteFilePath(), getArgumentsList(), exec_fi.absolutePath())) {
		QMessageBox::warning(this, ZDL_APP_NAME, "Failed to launch the application executable.");
		no_err=false;
	}
#endif
	if (no_err) {
		QString aclose=zconf->getValue("zdl.general", "autoclose");
		if (aclose=="1"||aclose=="true"){
			LOGDATAO()<<"Asked to exit... closing"<<Qt::endl;
			close();
		}
	}
}

QStringList WarpBackwardCompat(const QString& iwad_path, const QString& map_name)
{
	if (iwad_path.length()) {
		bool iwad_mapxx=false;

		if (ZDLMapFile *mapfile=ZDLMapFile::getMapFile(iwad_path)) {
			iwad_mapxx=mapfile->isMAPXX();
			delete mapfile;
		}

		if (iwad_mapxx) {
			QRegularExpression mapxx_re("^MAP(\\d\\d)$", QRegularExpression::CaseInsensitiveOption);
			QRegularExpressionMatch mapxx_m=mapxx_re.match(map_name);
			if (mapxx_m.hasMatch())
				return QStringList()<<"-warp"<<mapxx_m.captured(1);
		} else {
			QRegularExpression exmy_re("^E(\\d)M([1-9])$", QRegularExpression::CaseInsensitiveOption);
			QRegularExpressionMatch exmy_m=exmy_re.match(map_name);
			if (exmy_m.hasMatch())
				return QStringList()<<"-warp"<<exmy_m.captured(1)<<exmy_m.captured(2);
		}
	}

	return QStringList();
}

#ifdef Q_OS_WIN

QString QuoteParam(const QString& param)
{
	//Based on "Everyone quotes command line arguments the wrong way" by Daniel Colascione
	//http://blogs.msdn.com/b/twistylittlepassagesallalike/archive/2011/04/23/everyone-quotes-arguments-the-wrong-way.aspx

	if (!param.isEmpty()&&param.indexOf(QRegularExpression("[\\s\"]"))<0) {
		return param;
	} else {
		QString qparam('"');

		for (QString::const_iterator it=param.constBegin();; it++) {
			int backslash_count=0;

			while (it!=param.constEnd()&&*it=='\\') {
				it++;
				backslash_count++;
			}

			if (it==param.constEnd()) {
				qparam.append(QString(backslash_count*2, '\\'));
				break;
			} else if (*it==L'"') {
				qparam.append(QString(backslash_count*2+1, '\\'));
				qparam.append(*it);
			} else {
				qparam.append(QString(backslash_count, '\\'));
				qparam.append(*it);
			}
		}

		qparam.append('"');

		return qparam;
	}
}

QString ExpandEnvironmentStringsWrapper(QString args)
{
	wchar_t dummy_buf;

	//Documentation says that lpDst parameter is optional but Win 95 version of this function actually fails if lpDst is NULL
	//So using dummy buffer to get needed buffer length (function returns length in characters including terminating NULL)
	//If returned length is 0 - it is an error
	if (DWORD buf_len=ExpandEnvironmentStrings(WStr(args), &dummy_buf, 0)) {
		wchar_t* expanded_buf=new wchar_t[buf_len];

		//Ensuring that returned length is expected length
		if (ExpandEnvironmentStrings(WStr(args), expanded_buf, buf_len)<=buf_len) 
			args.setUtf16(reinterpret_cast<const char16_t*>(expanded_buf), buf_len-1);

		delete[] expanded_buf;
	}
	
	return args;
}

#define IF_NATIVE_SEP(p)	(native_sep?QDir::toNativeSeparators(p):p)

QString ZDLMainWindow::getArgumentsString(bool native_sep)
{
	LOGDATAO() << "Getting arguments" << Qt::endl;
	QString args;
	ZDLConf *zconf = ZDLConfigurationManager::getActiveConfiguration();
	ZDLSection *section = NULL;

	//A port of the EDuke32 family gets its own command line in place of
	//the ZDoom one; only the user's parameters are shared.
	QString exe = getExecutable();
	if (ZDLBuildArgs::handles(exe)){
		foreach (const QString &item, ZDLBuildArgs::arguments(zconf, exe, launchSection)){
			if (!args.isEmpty())
				args.append(' ');
			args.append(QuoteParam(IF_NATIVE_SEP(item)));
		}
		appendUserParams(args);
		LOGDATAO() << "args: " << args << Qt::endl;
		return args.trimmed();
	}

	QString iwadName = zconf->getValue(launchSection, "iwad");
	QString iwadPath;

	section = zconf->getSection("zdl.iwads");
	if (section&&iwadName.length()){
		QVector<ZDLLine*> fileVctr;
		section->getRegex("^i[0-9]+n$", fileVctr);

		for(int i = 0; i < fileVctr.size(); i++){
			ZDLLine *line = fileVctr[i];
			if(line->getValue().compare(iwadName) == 0){
				QString var = line->getVariable();
				if(var.length() >= 3){
					var = var.mid(1,var.length()-2);
					QVector<ZDLLine*> nameVctr;
					var = QString("i") + var + QString("f");
					section->getRegex("^" + var + "$",nameVctr);
					if(nameVctr.size() == 1){
						iwadPath=ZDLPaths::resolve(nameVctr[0]->getValue());
						//Raze: what it does not pick by itself goes over a base game.
						ZDLBuildGames::RazeLaunch raze;
						raze.iwad=iwadPath;
						if (ZDLRazeSetup::isRaze(exe))
							raze=ZDLBuildGames::forRaze(iwadPath, ZDLBuildArgs::libraryFiles(zconf));
						if (!raze.iwad.isEmpty()){
							args.append("-iwad ");
							args.append(QuoteParam(IF_NATIVE_SEP(raze.iwad)));
						}
						if (!raze.file.isEmpty()){
							args.append(" -file ");
							args.append(QuoteParam(IF_NATIVE_SEP(raze.file)));
						}
						if (!raze.script.isEmpty()){
							args.append(" -con ");
							args.append(QuoteParam(raze.script));
						}
						foreach (const QString &item, raze.switches){
							args.append(' ');
							args.append(item);
						}
					}
				}
			}
		}
	}

	if (zconf->hasValue(launchSection, "skill")){
		bool ok;
		QString s_skill=zconf->getValue(launchSection, "skill");
		int i_skill=s_skill.toInt(&ok, 10);

		if (i_skill>0&&i_skill<6) {
			args.append(" -skill ");
			args.append(s_skill);
		} else if (i_skill==6) {
			args.append(" -nomonsters");
		}
	}

	if (zconf->hasValue(launchSection, "compatmode")){
		args.append(" +compatmode ");
		args.append(zconf->getValue(launchSection, "compatmode"));
	}
	if (zconf->hasValue(launchSection, "fast")&&zconf->getValue(launchSection, "fast")=="1")
		args.append(" -fast");
	if (zconf->hasValue(launchSection, "respawn")&&zconf->getValue(launchSection, "respawn")=="1")
		args.append(" -respawn");

	//A preset's own save folder and settings file, kept apart from the
	//port's usual places; see ZDLPresetDialog.
	if (zconf->hasValue(launchSection, "savedir")){
		args.append(" -savedir ");
		args.append(QuoteParam(IF_NATIVE_SEP(ZDLPaths::resolve(zconf->getValue(launchSection, "savedir")))));
	}
	if (zconf->hasValue(launchSection, "config")){
		args.append(" -config ");
		args.append(QuoteParam(IF_NATIVE_SEP(ZDLPaths::resolve(zconf->getValue(launchSection, "config")))));
	}

	if (zconf->hasValue(launchSection, "warp")){
		QString map_arg=zconf->getValue(launchSection, "warp");
		QStringList warp_args=WarpBackwardCompat(iwadPath, map_arg);

		if (warp_args.length()) {
			args.append(' ');
			args.append(warp_args.join(" "));
		} else {
			args.append(" +map ");
			args.append(QuoteParam(map_arg));
		}
	}

	section = zconf->getSection(launchSection);
	QStringList pwads;
	QStringList dehs;
	QStringList bexs;
	QStringList lumps;
	QStringList autoexecs;
	char deh_last=1;
	if (section){
		QVector<ZDLLine*> fileVctr;
		section->getRegex("^file[0-9]+$", fileVctr);

		if (fileVctr.size() > 0){
			for(int i = 0; i < fileVctr.size(); i++){
				QString file=ZDLPaths::resolve(fileVctr[i]->getValue());
				if(file.endsWith(".bex",Qt::CaseInsensitive)) {
					deh_last=0;
					bexs << file;
				} else if(file.endsWith(".deh",Qt::CaseInsensitive)) {
					deh_last=1;
					dehs << file;
				} else if(file.endsWith(".cfg",Qt::CaseInsensitive)) {
					autoexecs << file;
				} else if(file.endsWith(".lmp",Qt::CaseInsensitive)) {
					lumps << file;
				} else {
					pwads << file;
				}
			}
		}
	}

	if(pwads.size() > 0){
		args.append(" -file");
		foreach (const QString &str, pwads) {
			args.append(' ');
			args.append(QuoteParam(IF_NATIVE_SEP(str)));
		}
	}

	do {
		if (deh_last%2) {
			foreach (const QString &str, bexs) {
				args.append(" -bex ");
				args.append(QuoteParam(IF_NATIVE_SEP(str)));
			}
		} else {
			foreach (const QString &str, dehs) {
				args.append(" -deh ");
				args.append(QuoteParam(IF_NATIVE_SEP(str)));
			}
		}
		deh_last+=3;
	} while (deh_last<=4);

	foreach (const QString &str, autoexecs) {
		args.append(" +exec ");
		args.append(QuoteParam(IF_NATIVE_SEP(str)));
	}

	foreach (const QString &str, lumps) {
		args.append(" -playdemo ");
		args.append(QuoteParam(IF_NATIVE_SEP(str)));
	}

	if(zconf->hasValue(launchSection,"gametype")){
		QString tGameType = zconf->getValue(launchSection,"gametype");
		if(tGameType != "0"){
			if (zconf->hasValue(launchSection, "dmflags")){
				args.append(" +set dmflags ");
				args.append(zconf->getValue(launchSection, "dmflags"));
			}

			if (zconf->hasValue(launchSection, "dmflags2")){
				args.append(" +set dmflags2 ");
				args.append(zconf->getValue(launchSection, "dmflags2"));
			}

			if (tGameType == "2"){
				args.append(" -deathmath");
			} else if (tGameType == "3"){
				args.append(" -altdeath");
			}

			int players = 0;
			if(zconf->hasValue(launchSection,"players")){
				bool ok;
				players = zconf->getValue(launchSection,"players").toInt(&ok, 10);
			}
			if(players > 0){
				args.append(" -host ");
				args.append(QString::number(players));
				if(zconf->hasValue(launchSection,"mp_port")){
					args.append(" -port ");
					args.append(zconf->getValue(launchSection,"mp_port"));
				}
			}else if(players == 0){
				if(zconf->hasValue(launchSection, "host")) {
					args.append(" -join ");
					if (zconf->hasValue(launchSection, "mp_port")) {
						QRegularExpression trailing_port(":\\d*\\s*$");
						args.append(zconf->getValue(launchSection, "host").remove(trailing_port)+":"+zconf->getValue(launchSection, "mp_port"));
					} else {
						args.append(zconf->getValue(launchSection, "host"));
					}
				}
			}
			if(zconf->hasValue(launchSection,"fraglimit")){
				args.append(" +set fraglimit ");
				args.append(zconf->getValue(launchSection,"fraglimit"));
			}
			if(zconf->hasValue(launchSection,"timelimit")){
				args.append(" +set timelimit ");
				args.append(zconf->getValue(launchSection,"timelimit"));
			}
			if(zconf->hasValue(launchSection,"extratic")){
				QString tVal = zconf->getValue(launchSection,"extratic");
				if(tVal == "1"){
					args.append(" -extratic");
				}
			}
			if(zconf->hasValue(launchSection,"netmode")){
				QString tVal = zconf->getValue(launchSection,"netmode");
				if(tVal != "-1"){
					args.append(" -netmode ");
					args.append(tVal);
				}
			}
			if(zconf->hasValue(launchSection,"dup")){
				QString tVal = zconf->getValue(launchSection,"dup");
				if(tVal != "0"){
					args.append(" -dup ");
					args.append(tVal);
				}
			}
			if(zconf->hasValue(launchSection,"savegame")){
				args.append(" -loadgame ");
				args.append(QuoteParam(IF_NATIVE_SEP(zconf->getValue(launchSection,"savegame"))));
			}
		}
	}

	appendUserParams(args);

	LOGDATAO() << "args: " << args << Qt::endl;
	return args.trimmed();
}

//The port's own parameters go first: they are that port's defaults, and
//the global and per-launch ones below are the user's overrides.
void ZDLMainWindow::appendUserParams(QString &args)
{
	ZDLConf *zconf = ZDLConfigurationManager::getActiveConfiguration();

	QString portParams = getPortParams();
	if (!portParams.isEmpty()){
		args.append(' ');
		args.append(ExpandEnvironmentStringsWrapper(portParams));
	}

	if (zconf->hasValue("zdl.general", "alwaysadd")){
		args.append(' ');
		args.append(ExpandEnvironmentStringsWrapper(zconf->getValue("zdl.general", "alwaysadd")));
	}

	if (zconf->hasValue(launchSection, "extra")){
		args.append(' ');
		args.append(ExpandEnvironmentStringsWrapper(zconf->getValue(launchSection, "extra")));
	}
}

void ZDLMainWindow::appendUserParams(QStringList &args)
{
	Q_UNUSED(args);
}

QStringList ZDLMainWindow::getArgumentsList()
{
	return QStringList();
}

#else

//Shell-style expansion of a parameter string - quotes, ~, $VAR and globs -
//as the original ZDL for Linux did. Command substitution is refused: the
//string may have arrived in a .zdl file rather than been typed. Where
//wordexp will not have the string at all, over a stray | or ( as much as
//over a $(...) attempt, it is split on whitespace with quotes honoured
//instead of being dropped, which is what Windows gets.
QStringList ParseParams(const QString& params)
{
	QStringList plist;
	wordexp_t result;

	switch (wordexp(qPrintable(params), &result, WRDE_NOCMD)) {
		case 0:
			for (size_t i=0; i<result.we_wordc; i++)
				plist<<QString::fromLocal8Bit(result.we_wordv[i]);
			wordfree(&result);
			return plist;
		case WRDE_NOSPACE:	//Part of we_wordv may have been allocated
			wordfree(&result);
			break;
	}

	return QProcess::splitCommand(params);
}

QStringList ZDLMainWindow::getArgumentsList()
{
	LOGDATAO() << "Getting arguments" << Qt::endl;
	QStringList args;
	ZDLConf *zconf = ZDLConfigurationManager::getActiveConfiguration();
	ZDLSection *section = NULL;

	//A port of the EDuke32 family gets its own command line in place of
	//the ZDoom one; only the user's parameters are shared.
	QString exe = getExecutable();
	if (ZDLBuildArgs::handles(exe)){
		args<<ZDLBuildArgs::arguments(zconf, exe, launchSection);
		appendUserParams(args);
		LOGDATAO() << "args: " << args << Qt::endl;
		return args;
	}

	QString iwadPath;
	QString iwadName = zconf->getValue(launchSection, "iwad");

	section = zconf->getSection("zdl.iwads");
	if (section&&iwadName.length()){
		QVector<ZDLLine*> fileVctr;
		section->getRegex("^i[0-9]+n$", fileVctr);

		for(int i = 0; i < fileVctr.size(); i++){
			ZDLLine *line = fileVctr[i];
			if(line->getValue().compare(iwadName) == 0){
				QString var = line->getVariable();
				if(var.length() >= 3){
					var = var.mid(1,var.length()-2);
					QVector<ZDLLine*> nameVctr;
					var = QString("i") + var + QString("f");
					section->getRegex("^" + var + "$",nameVctr);
					if(nameVctr.size() == 1){
						iwadPath=ZDLPaths::resolve(nameVctr[0]->getValue());
						//Raze: what it does not pick by itself goes over a base game.
						ZDLBuildGames::RazeLaunch raze;
						raze.iwad=iwadPath;
						if (ZDLRazeSetup::isRaze(exe))
							raze=ZDLBuildGames::forRaze(iwadPath, ZDLBuildArgs::libraryFiles(zconf));
						if (!raze.iwad.isEmpty())
							args<<"-iwad"<<raze.iwad;
						if (!raze.file.isEmpty())
							args<<"-file"<<raze.file;
						if (!raze.script.isEmpty())
							args<<"-con"<<raze.script;
						args<<raze.switches;
					}
				}
			}
		}
	}

	if (zconf->hasValue(launchSection, "skill")){
		bool ok;
		QString s_skill=zconf->getValue(launchSection, "skill");
		int i_skill=s_skill.toInt(&ok, 10);

		if (i_skill>0&&i_skill<6) {
			args<<"-skill"<<s_skill;
		} else if (i_skill==6) {
			args<<"-nomonsters";
		}
	}

	if (zconf->hasValue(launchSection, "compatmode"))
		args<<"+compatmode"<<zconf->getValue(launchSection, "compatmode");
	if (zconf->hasValue(launchSection, "fast")&&zconf->getValue(launchSection, "fast")=="1")
		args<<"-fast";
	if (zconf->hasValue(launchSection, "respawn")&&zconf->getValue(launchSection, "respawn")=="1")
		args<<"-respawn";

	//A preset's own save folder and settings file, kept apart from the
	//port's usual places; see ZDLPresetDialog.
	if (zconf->hasValue(launchSection, "savedir"))
		args<<"-savedir"<<ZDLPaths::resolve(zconf->getValue(launchSection, "savedir"));
	if (zconf->hasValue(launchSection, "config"))
		args<<"-config"<<ZDLPaths::resolve(zconf->getValue(launchSection, "config"));

	if (zconf->hasValue(launchSection, "warp")){
		QString map_arg=zconf->getValue(launchSection, "warp");
		QStringList warp_args=WarpBackwardCompat(iwadPath, map_arg);

		if (warp_args.length()) {
			args<<warp_args;
		} else {
			args<<"+map"<<map_arg;
		}
	}

	section = zconf->getSection(launchSection);
	QStringList pwads;
	QStringList dehs;
	QStringList bexs;
	QStringList autoexecs;
	QStringList lumps;
	char deh_last=1;
	if (section){
		QVector<ZDLLine*> fileVctr;
		section->getRegex("^file[0-9]+$", fileVctr);

		if (fileVctr.size() > 0){
			for(int i = 0; i < fileVctr.size(); i++){
				QString file=ZDLPaths::resolve(fileVctr[i]->getValue());
				if(file.endsWith(".bex",Qt::CaseInsensitive)) {
					deh_last=0;
					bexs << file;
				} else if(file.endsWith(".deh",Qt::CaseInsensitive)) {
					deh_last=1;
					dehs << file;
				} else if(file.endsWith(".cfg",Qt::CaseInsensitive)) {
					autoexecs << file;
				} else if(file.endsWith(".lmp",Qt::CaseInsensitive)) {
					lumps << file;
				} else {
					pwads << file;
				}
			}
		}
	}

	if(pwads.size() > 0){
		args<<"-file";
		foreach (const QString &str, pwads) {
			args<<str;
		}
	}

	do {
		if (deh_last%2) {
			foreach (const QString &str, bexs) {
				args<<"-bex"<<str;
			}
		} else {
			foreach (const QString &str, dehs) {
				args<<"-deh"<<str;
			}
		}
		deh_last+=3;
	} while (deh_last<=4);

	foreach (const QString &str, autoexecs) {
		args<<"+exec"<<str;
	}

	foreach (const QString &str, lumps) {
		args<<"-playdemo"<<str;
	}

	if(zconf->hasValue(launchSection,"gametype")){
		QString tGameType = zconf->getValue(launchSection,"gametype");
		if(tGameType != "0"){
			if (zconf->hasValue(launchSection, "dmflags")){
				args<<"+set"<<"dmflags"<<zconf->getValue(launchSection, "dmflags");
			}

			if (zconf->hasValue(launchSection, "dmflags2")){
				args<<"+set"<<"dmflags2"<<zconf->getValue(launchSection, "dmflags2");
			}

			if (tGameType == "2"){
				args<<"-deathmath";
			} else if (tGameType == "3"){
				args<<"-altdeath";
			}

			int players = 0;
			if(zconf->hasValue(launchSection,"players")){
				bool ok;
				players = zconf->getValue(launchSection,"players").toInt(&ok, 10);
			}
			if(players > 0){
				args<<"-host"<<QString::number(players);
				if(zconf->hasValue(launchSection,"mp_port")){
					args<<"-port"<<zconf->getValue(launchSection,"mp_port");
				}
			}else if(players == 0){
				if(zconf->hasValue(launchSection, "host")) {
					args<<"-join";
					if (zconf->hasValue(launchSection, "mp_port")) {
						QRegularExpression trailing_port(":\\d*\\s*$");
						args<<zconf->getValue(launchSection, "host").remove(trailing_port)+":"+zconf->getValue(launchSection, "mp_port");
					} else {
						args<<zconf->getValue(launchSection, "host");
					}
				}
			}
			if(zconf->hasValue(launchSection,"fraglimit")){
				args<<"+set"<<"fraglimit"<<zconf->getValue(launchSection,"fraglimit");
			}
			if(zconf->hasValue(launchSection,"timelimit")){
				args<<"+set"<<"timelimit"<<zconf->getValue(launchSection,"timelimit");
			}
			if(zconf->hasValue(launchSection,"extratic")){
				QString tVal = zconf->getValue(launchSection,"extratic");
				if(tVal == "1"){
					args<<"-extratic";
				}
			}
			if(zconf->hasValue(launchSection,"netmode")){
				QString tVal = zconf->getValue(launchSection,"netmode");
				if(tVal != "-1"){
					args<<"-netmode"<<tVal;
				}
			}
			if(zconf->hasValue(launchSection,"dup")){
				QString tVal = zconf->getValue(launchSection,"dup");
				if(tVal != "0"){
					args<<"-dup"<<tVal;
				}
			}
			if(zconf->hasValue(launchSection,"savegame")){
				args<<"-loadgame"<<zconf->getValue(launchSection,"savegame");
			}
		}
	}

	appendUserParams(args);

	LOGDATAO() << "args: " << args << Qt::endl;
	return args;
}

//The port's own parameters go first: they are that port's defaults, and
//the global and per-launch ones below are the user's overrides.
void ZDLMainWindow::appendUserParams(QStringList &args)
{
	ZDLConf *zconf = ZDLConfigurationManager::getActiveConfiguration();

	QString portParams = getPortParams();
	if (!portParams.isEmpty()){
		args<<ParseParams(portParams);
	}

	if (zconf->hasValue("zdl.general", "alwaysadd")){
		args<<ParseParams(zconf->getValue("zdl.general", "alwaysadd"));
	}

	if (zconf->hasValue(launchSection, "extra")){
		args<<ParseParams(zconf->getValue(launchSection, "extra"));
	}
}

void ZDLMainWindow::appendUserParams(QString &args)
{
	Q_UNUSED(args);
}

QString ZDLMainWindow::getArgumentsString(bool native_sep)
{
	Q_UNUSED(native_sep);

	QString args;

	foreach (const QString &str, getArgumentsList()) {
		if (str.indexOf(QRegularExpression("\\s"))!=-1) {
			args.append('"');
			args.append(str);
			args.append('"');
		} else {
			args.append(str);
		}
		args.append(' ');
	}

	return args;
}

#endif

//Same lookup as getExecutable, reading p<N>p instead of p<N>f.
QString ZDLMainWindow::getPortParams(){
	ZDLConf *zconf = ZDLConfigurationManager::getActiveConfiguration();
	if(!zconf||!zconf->hasValue(launchSection, "port"))
		return QString();

	ZDLSection *section = zconf->getSection("zdl.ports");
	if(!section)
		return QString();

	int stat;
	QString portName = zconf->getValue(launchSection, "port", &stat);
	QVector<ZDLLine*> nameVctr;
	section->getRegex("^p[0-9]+n$", nameVctr);

	for(int i = 0; i < nameVctr.size(); i++){
		if(nameVctr[i]->getValue().compare(portName) != 0)
			continue;
		QString var = nameVctr[i]->getVariable();
		if(var.length() < 3)
			continue;
		QVector<ZDLLine*> paramVctr;
		section->getRegex("^p" + var.mid(1, var.length()-2) + "p$", paramVctr);
		if(paramVctr.size() == 1)
			return paramVctr[0]->getValue();
	}
	return QString();
}

QString ZDLMainWindow::getExecutable(){
	return getExecutable(launchSection);
}

QString ZDLMainWindow::getExecutable(const QString &launch){
	LOGDATAO() << "Getting exec" << Qt::endl;
	ZDLConf *zconf = ZDLConfigurationManager::getActiveConfiguration();
	int stat;
	QString portName = "";
	if(zconf->hasValue(launch, "port")){
		ZDLSection *section = zconf->getSection("zdl.ports");
		//A preset can name a port in a configuration that has no port list
		//at all, and then there is nothing to look it up in.
		if (!section){
			LOGDATAO() << "No port list" << Qt::endl;
			return QString("");
		}
		portName = zconf->getValue(launch, "port", &stat);
		QVector<ZDLLine*> fileVctr;
		section->getRegex("^p[0-9]+n$", fileVctr);

		for(int i = 0; i < fileVctr.size(); i++){
			ZDLLine *line = fileVctr[i];
			if(line->getValue().compare(portName) == 0){
				QString var = line->getVariable();
				if(var.length() >= 3){
					var = var.mid(1,var.length()-2);
					QVector<ZDLLine*> nameVctr;
					var = QString("p") + var + QString("f");
					section->getRegex("^" + var + "$",nameVctr);
					if(nameVctr.size() == 1){
						LOGDATAO() << "Executable: " << nameVctr[0]->getValue() << Qt::endl;
						return ZDLPaths::resolve(nameVctr[0]->getValue());
					}
				}
			}
		}
	}
	LOGDATAO() << "No executable" << Qt::endl;
	return QString("");
}


//Before the Build Engine tab, Build games were started from the one launch
//configuration the Doom Engine tab now has to itself. A Build port left
//there, with the game, files and choices that go with it, moves once to the
//Build Engine tab's own configuration, before the Doom Engine tab, which
//no longer lists Build ports, drops them on reading.
static void MoveBuildLaunch(ZDLConf *zconf)
{
	if (!zconf||zconf->getSection(ZDLLaunchFiles::BUILD_SECTION))
		return;
	ZDLSection *section=zconf->getSection(ZDLLaunchFiles::LAUNCH_SECTION);
	if (!section||!ZDLBuildArgs::isBuildLaunch(zconf, ZDLLaunchFiles::LAUNCH_SECTION))
		return;

	QVector<ZDLLine*> lines;
	section->getRegex("^(port|iwad|skill|warp|respawn|savedir|config|extra|file[0-9]+d?)$", lines);
	QList<QPair<QString, QString> > moved;
	foreach (ZDLLine *line, lines)
		moved<<qMakePair(line->getVariable(), line->getValue());
	for (int i=0; i<moved.size(); i++) {
		zconf->setValue(ZDLLaunchFiles::BUILD_SECTION, moved[i].first, moved[i].second);
		zconf->deleteValue(ZDLLaunchFiles::LAUNCH_SECTION, moved[i].first);
	}
}

void ZDLMainWindow::setLaunchSection(const QString &section){
	launchSection = section.isEmpty() ? QString(ZDLLaunchFiles::LAUNCH_SECTION) : section;
}

//Pass through functions.
void ZDLMainWindow::startRead(){
	LOGDATAO() << "Starting to read configuration" << Qt::endl;
	MoveBuildLaunch(ZDLConfigurationManager::getActiveConfiguration());
	intr->startRead();
	buildTab->startRead();
	settings->startRead();
	appSettings->newConfig();
	idgames->newConfig();
	QString windowTitle = getWindowTitle();
	setWindowTitle(windowTitle);
}

void ZDLMainWindow::writeConfig(){
	LOGDATAO() << "Writing configuration" << Qt::endl;
	intr->writeConfig();
	buildTab->writeConfig();
	settings->writeConfig();
	appSettings->rebuild();
	idgames->rebuild();
}

bool ZDLMainWindow::saveNow(){
	writeConfig();
	return ZDLConfigurationManager::writeConfigFile() == 0;
}
