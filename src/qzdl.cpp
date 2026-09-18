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
#include <algorithm>
#include <QApplication>
#include <QMainWindow>
#include <QDir>

#include "ZDLConfigurationManager.h"
#include "ZDLPaths.h"
#include "ZDLMainWindow.h"

#include "ZDLNullDevice.h"

#if defined(Q_OS_WIN)
#include "windows.h"
#endif

QApplication *qapp;
ZDLMainWindow *mw;

void clearFiles(ZDLConf* zconf)
{
	ZDLSection *section = zconf->getSection("zdl.save");
	if (section){
		QVector <ZDLLine*> vctr;
		section->getRegex("^file[0-9]+d?$", vctr);
		for(int i = 0; i < vctr.size(); i++){
			zconf->deleteValue("zdl.save", vctr[i]->getVariable());
		}
	}
}

void addFile(QString file, ZDLConf* zconf)
{
	LOGDATA() << "Adding " << file << " to " << (void*)zconf << Qt::endl;
	ZDLSection *section = zconf->getSection("zdl.save");
	if(!section){
		zconf->setValue("zdl.save", "file0", file);
		return;
	}
	QVector<ZDLLine*> vctr;
	section->getRegex("^file[0-9]+$", vctr);
	if(vctr.size() == 0){
		zconf->setValue("zdl.save", "file0", file);
		return;
	}
	QVector<int> numbers;
	for(int i = 0; i < vctr.size(); i++){
		bool ok = false;
		QString value = vctr[i]->getVariable();
		value.remove(0,4);
		int val = value.toInt(&ok);
		if(!ok){
			return;
		}
		numbers.push_back(val);
	}
	std::sort(numbers.begin(), numbers.end());
	int highest = numbers.last();
	zconf->setValue("zdl.save", "file"+QString::number(highest+1), file);
}

QDebug *zdlDebug;

#if defined(Q_OS_WIN)
	extern Q_CORE_EXPORT int qt_ntfs_permission_lookup;
#endif

int main( int argc, char **argv ){
	QStringList eatenArgs;
	for(int i = 1; i < argc; i++){
		eatenArgs << argv[i];
	}
	ZDLNullDevice nullDev;
#if defined(ZDL_BLACKBOX)
	QFile *loggingFile = NULL;
	zdlDebug = NULL;
	int logger = eatenArgs.indexOf("--enable-logger");
	
	if(logger >= 0){
		eatenArgs.removeAt(logger);
		loggingFile = new QFile("zdl.log");
		if(loggingFile->exists()){
			loggingFile->remove();
		}
		loggingFile->open(QIODevice::ReadWrite);
		zdlDebug = new QDebug(loggingFile);
		qDebug() << "Logger is enabled";
	}else{
		zdlDebug = new QDebug(&nullDev);
	}
#else
	zdlDebug = new QDebug(&nullDev);
#endif
	LOGDATA() << ZDL_APP_NAME << " booting at " << QDateTime::currentDateTime().toString() << Qt::endl;

#if defined(Q_OS_MACOS)
	QFont::insertSubstitution(".Lucida Grande UI", "Lucida Grande");
#endif

	QApplication a( argc, argv );
	qapp = &a;
	ZDLConfigurationManager::setArgv(eatenArgs);
	//Not argv[0], which on Linux is whatever the parent passed - "uzdl"
	//alone when started from a menu or PATH - and so resolves nowhere.
	LOGDATA() << "Executable path: " << QCoreApplication::applicationFilePath() << Qt::endl;
	ZDLConfigurationManager::setExec(QCoreApplication::applicationFilePath());

	LOGDATA() << "ZDL Version: " << ZDL_PRIVATE_VERSION_STRING << Qt::endl;
	LOGDATA() << "Built on " << __DATE__ << " at " << __TIME__ <<Qt::endl;
#if ZDL_DEV_BUILD==1
	LOGDATA() << "This is development build" <<Qt::endl;
#endif

	QDir cwd = QDir::current();
	ZDLConfigurationManager::init();
	ZDLConfigurationManager::setCurrentDirectory(cwd.absolutePath());

	ZDLConf* tconf = new ZDLConf();
	ZDLConfigurationManager::setConfigFileName("");

	ZDLConfigurationManager::setWhy(ZDLConfigurationManager::UNKNOWN);

	//If the user has specified an alternative .ini
	for(int i = 0; i < eatenArgs.size(); i++){
		if(eatenArgs[i].endsWith(".ini", Qt::CaseInsensitive)){
			LOGDATA() << "Loading command line INI configuration " << eatenArgs[i] << Qt::endl;
			ZDLConfigurationManager::setConfigFileName(eatenArgs[i]);
			eatenArgs.removeAt(i);
			ZDLConfigurationManager::setWhy(ZDLConfigurationManager::USER_SPECIFIED);
			break;
		}
	}

	//In portable mode the marker file is the configuration outright, with no
	//discovery: a stray zdl.ini in the program directory must not take
	//precedence over it, and neither must the per-user config. An explicit
	//.ini on the command line still wins, having been handled just above.
	if(ZDLConfigurationManager::getConfigFileName().isEmpty()){
		ZDLConfiguration *conf = ZDLConfigurationManager::getConfiguration();
		if(conf&&conf->isPortable()){
			ZDLConfigurationManager::setConfigFileName(conf->getPath(ZDLConfiguration::CONF_USER));
			LOGDATA() << "Portable mode, using " << conf->getPath(ZDLConfiguration::CONF_USER) << Qt::endl;
			//Homes for ports and IWADs kept with uZDL. Anything added from inside
			//the program folder is stored relative to it, so the folder moves as one.
			QDir().mkpath(ZDLPaths::sourcePortsDir());
			QDir().mkpath(ZDLPaths::iwadsDir());
		}
	}

	if(ZDLConfigurationManager::getConfigFileName().isEmpty()){
		ZDLConfiguration *conf = ZDLConfigurationManager::getConfiguration();
		if(conf){
			QString userConfPath = conf->getPath(ZDLConfiguration::CONF_USER);
			if(QFile::exists(userConfPath)){
				QFile cfile(userConfPath);
				if(cfile.size() > 20){
					ZDLConf conf;
					conf.readINI(userConfPath);
					if(!conf.hasValue("zdl.general","nouserconf")){
						ZDLConfigurationManager::setConfigFileName(userConfPath);
						LOGDATA() << "Using user-level config file at " << userConfPath << Qt::endl;
					}else{
						LOGDATA() << "Config file specified nouserconf" << Qt::endl;
					}			
				}else{
					LOGDATA() << "User config file is small" << Qt::endl;
				}
			}else{
				LOGDATA() << "User conf file doesn't exist" << Qt::endl;
			}
		}else{
			LOGDATA() << "No conf yet" << Qt::endl;
		}
	}

	if(ZDLConfigurationManager::getConfigFileName().isEmpty()){
		QDir ini_dir(QFileInfo(ZDLConfigurationManager::getExec()).dir());
		if (ini_dir.exists("zdl.ini")) {
			LOGDATA() << "Using zdl.ini at " << ini_dir.filePath("zdl.ini") << Qt::endl;
			ZDLConfigurationManager::setConfigFileName(ini_dir.filePath("zdl.ini"));
		}
	}

	if(ZDLConfigurationManager::getConfigFileName().isEmpty()){
		ZDLConfiguration *conf = ZDLConfigurationManager::getConfiguration();
		if(conf){
			ZDLConfigurationManager::setConfigFileName(conf->getPath(ZDLConfiguration::CONF_USER));
			LOGDATA() << "Falling back on user config at " << conf->getPath(ZDLConfiguration::CONF_USER) << Qt::endl;
		}else{
			ZDLConfigurationManager::setConfigFileName("zdl.ini");
			LOGDATA() << "No conf, going to have to use local zdl.ini" << Qt::endl;
		}
	}

	tconf->readINI(ZDLConfigurationManager::getConfigFileName());
	ZDLConfigurationManager::setActiveConfiguration(tconf);

	bool clear_on_args=true;
	bool hasZDLFile=false;
	foreach (const QString &item, eatenArgs) {
		if (item.endsWith(".zdl", Qt::CaseInsensitive)) {
			LOGDATA() << "Found a .zdl on the command line, replacing current zdl.save" << Qt::endl;
			tconf->deleteSectionByName("zdl.save");
			ZDLConf zdlFile;
			zdlFile.readINI(item);
			ZDLSection *section = zdlFile.getSection("zdl.save");
			if(section){
				tconf->addSection(section->clone());
				hasZDLFile=true;
				clear_on_args=false;
			}
			break;
		}
	}

	foreach (const QString &item, eatenArgs) {
		if (item.endsWith(".zdl", Qt::CaseInsensitive)||item.endsWith(".ini", Qt::CaseInsensitive)||item.startsWith("-"))
			continue;

		if (clear_on_args) {
			clearFiles(tconf);
			clear_on_args=false;
		}

		addFile(item, tconf);
	}

	mw = new ZDLMainWindow();
	mw->show();
	QObject::connect(&a, SIGNAL(lastWindowClosed()), &a, SLOT(quit()));
	mw->startRead();

	if(hasZDLFile){
		LOGDATA() << "A .zdl file as passed as a command line option" << Qt::endl;
		if(tconf->hasValue("zdl.general", "zdllaunch")){
			int ok = 0;
			QString rc = tconf->getValue("zdl.general", "zdllaunch", &ok);
			if(rc.length() > 0){
				if(rc.compare("1") == 0){
					LOGDATA() << "Launching configuration NOW" << Qt::endl;
					mw->launch();
					LOGDATA() << "ZDL QUIT" << Qt::endl;
					return 0;
				}
			}
		}
	}

	mw->handleImport();
	LOGDATA() << "-----------------------------------" << Qt::endl;
	int ret = a.exec();
	LOGDATA() << "-----------------------------------" << Qt::endl;
	LOGDATA() << "Starting shutdown" << Qt::endl;
	if (ret != 0){
		LOGDATA() << "ZDL QUIT, ERROR CONDITION" << Qt::endl;
		return ret;
	}
	mw->writeConfig();
	QString qscwd = ZDLConfigurationManager::getCurrentDirectory();
	tconf = ZDLConfigurationManager::getActiveConfiguration();
	QDir::setCurrent(qscwd);
	delete mw;
	// Set version information
	tconf->setValue("zdl.general", "engine", "ZDL");
	tconf->setValue("zdl.general", "version", ZDL_PRIVATE_VERSION_STRING);

	bool doSave = true;
	if (tconf->hasValue("zdl.general", "rememberFilelist")){
		int ok = 0;
		QString val = tconf->getValue("zdl.general", "rememberFilelist", &ok);
		if (val == "0"){
			doSave = false;
		} else {
			doSave = true;
		}
	}
	if (!doSave){
		tconf->deleteRegex("zdl.save", "^file[0-9]+$");
	}

	tconf->writeINI(ZDLConfigurationManager::getConfigFileName());
	LOGDATA() << "ZDL QUIT" << Qt::endl;
	return ret;
}

