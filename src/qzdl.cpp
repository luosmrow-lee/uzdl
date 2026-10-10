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
#include "ZDLBuildArgs.h"
#include "ZDLLaunchFiles.h"
#include "ZDLFinderDialog.h"
#include "ZDLFinder.h"
#include "ZDLFileInspector.h"
#include "ZDLRazeSetup.h"
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

	//--preset <name> loads that preset and starts the game with it, which is
	//what a desktop shortcut or a Steam entry made for a preset passes. It
	//comes out of the arguments here, before the name can be mistaken for a
	//file to load. The name itself is read further down from QApplication's
	//view of the command line: on Windows argv arrives in the ANSI code page,
	//and a name with a character outside it would not survive the trip.
	//--command-line <out>, a diagnostic: taken off the list here, before
	//the loop below would take its file for one to launch with; written
	//once the window has read the configuration.
	QString commandLineOut;
	int cmdArg = eatenArgs.indexOf("--command-line");
	if (cmdArg >= 0){
		if (cmdArg+1 < eatenArgs.size())
			commandLineOut = eatenArgs[cmdArg+1];
		eatenArgs.remove(cmdArg, cmdArg+1 < eatenArgs.size() ? 2 : 1);
	}
	//--build with it: the Build Engine tab's launch configuration instead.
	bool commandLineBuild = false;
	int buildArg = eatenArgs.indexOf("--build");
	if (buildArg >= 0){
		commandLineBuild = true;
		eatenArgs.remove(buildArg, 1);
	}

	int presetArg = eatenArgs.indexOf("--preset");
	bool presetGiven = presetArg >= 0;
	if (presetGiven)
		eatenArgs.remove(presetArg, presetArg+1 < eatenArgs.size() ? 2 : 1);
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

	QString presetToLaunch;
	if (presetGiven){
		QStringList args = a.arguments();
		int at = args.indexOf("--preset");
		if (at >= 0 && at+1 < args.size())
			presetToLaunch = args[at+1];
	}
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

	//The diagnostic switches read everything after them from the
	//arguments as given. None of it may be taken below for a configuration
	//to load, which would then be saved over at exit, or for a file to
	//launch with; and none of them gets as far as saving anything.
	static const char *const diagnostics[]={"--raze-setup", "--file-report", "--find-report", NULL};
	for (int d = 0; diagnostics[d]; d++){
		int at = eatenArgs.indexOf(diagnostics[d]);
		if (at >= 0)
			eatenArgs = eatenArgs.mid(0, at);
	}

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

	//--raze-setup <raze.exe or .ini> <out> [folder...]: the Raze setup done
	//on that executable, or only the search folders added to that ini, with
	//the folders given on top of the IWAD list's; the report goes to out.
	//Read from the arguments as given, as are the other diagnostics: they
	//were kept from the configuration and launch file handling above.
	QStringList rawArgs = a.arguments();
	int razeArg = rawArgs.indexOf("--raze-setup");
	if (razeArg >= 0 && razeArg+2 >= rawArgs.size())
		return 0;
	if (razeArg >= 0){
		QString target = rawArgs[razeArg+1];
		QStringList folders = rawArgs.mid(razeArg+3);
		QString report;
		if (target.endsWith(".ini", Qt::CaseInsensitive)){
			bool ok;
			QString beside = ZDLRazeSetup::gamesFolderBeside(QFileInfo(target).absolutePath());
			if (!beside.isEmpty())
				folders << beside;
			QStringList added = ZDLRazeSetup::addSearchFolders(target, QFileInfo(target).absolutePath(), folders, &ok);
			report = ok ? "added: " + added.join(" | ") : "could not write " + target;
		} else {
			report = ZDLRazeSetup::setUp(tconf, target, folders);
		}
		QFile out(rawArgs[razeArg+2]);
		if (out.open(QIODevice::WriteOnly|QIODevice::Truncate|QIODevice::Text))
			out.write(report.toUtf8());
		return 0;
	}

	//--file-report <file> <out>: the File information report for a file,
	//written to out with no window shown.
	int fileReportArg = rawArgs.indexOf("--file-report");
	if (fileReportArg >= 0){
		if (fileReportArg+2 < rawArgs.size()){
			QFile out(rawArgs[fileReportArg+2]);
			if (out.open(QIODevice::WriteOnly|QIODevice::Truncate|QIODevice::Text))
				out.write(ZDLFileInspector::report(rawArgs[fileReportArg+1]).toUtf8());
		}
		return 0;
	}

	//--find-report <file> [folder...]: what Find installed would offer,
	//written to the file with no window shown, for checking the search on a
	//machine at hand. Any folders after the file are looked in as well.
	int reportArg = rawArgs.indexOf("--find-report");
	if (reportArg >= 0){
		QString reportFile = reportArg+1 < rawArgs.size() ? rawArgs[reportArg+1] : QString("uzdl-find-report.txt");
		QStringList extra = rawArgs.mid(reportArg+2);
		QStringList lines;
		foreach (const ZDLFinder::Found &port, ZDLFinder::findPorts(extra))
			lines << "port\t" + port.name + "\t" + QDir::toNativeSeparators(port.path) + "\t" + port.where + "\tversion=" + port.version;
		foreach (const ZDLFinder::Found &iwad, ZDLFinder::findIwads(extra))
			lines << "iwad\t" + iwad.name + "\t" + QDir::toNativeSeparators(iwad.path) + "\t" + iwad.where + "\t" + iwad.family + "\t" + QString::fromLatin1(iwad.digest.toHex()) + (iwad.also.isEmpty() ? QString() : "\talso: " + iwad.also.join(" | "));
		QFile report(reportFile);
		if (report.open(QIODevice::WriteOnly|QIODevice::Truncate|QIODevice::Text))
			report.write((lines.join("\n") + "\n").toUtf8());
		return 0;
	}

	bool clear_on_args=true;
	bool hasZDLFile=false;
	//The tab the .zdl went to: one naming a Build port or game is the Build
	//Engine tab's.
	QString zdlTarget;
	foreach (const QString &item, eatenArgs) {
		if (item.endsWith(".zdl", Qt::CaseInsensitive)) {
			LOGDATA() << "Found a .zdl on the command line, replacing a launch configuration" << Qt::endl;
			ZDLConf zdlFile;
			zdlFile.readINI(item);
			zdlTarget = ZDLLaunchFiles::loadZdl(tconf, &zdlFile);
			if(!zdlTarget.isEmpty()){
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
	//A shortcut's preset starts its game without the window coming up.
	if (presetToLaunch.isEmpty())
		mw->show();
	QObject::connect(&a, SIGNAL(lastWindowClosed()), &a, SLOT(quit()));
	mw->startRead();

	//--command-line <out>: the executable and the command line a launch
	//would get from the configuration as loaded, written to out; then done.
	if (!commandLineOut.isEmpty()){
		if (commandLineBuild)
			mw->setLaunchSection(ZDLLaunchFiles::BUILD_SECTION);
		QFile out(commandLineOut);
		if (out.open(QIODevice::WriteOnly|QIODevice::Truncate|QIODevice::Text))
			out.write((mw->getExecutable()+"\n"+mw->getArgumentsString(true)+"\n").toUtf8());
		return 0;
	}

	if(hasZDLFile){
		LOGDATA() << "A .zdl file as passed as a command line option" << Qt::endl;
		if(tconf->hasValue("zdl.general", "zdllaunch")){
			int ok = 0;
			QString rc = tconf->getValue("zdl.general", "zdllaunch", &ok);
			if(rc.length() > 0){
				if(rc.compare("1") == 0){
					LOGDATA() << "Launching configuration NOW" << Qt::endl;
					mw->launchFrom(zdlTarget);
					LOGDATA() << "ZDL QUIT" << Qt::endl;
					return 0;
				}
			}
		}
		mw->showLaunchTab(zdlTarget);
	}

	//The shortcut case: the preset goes straight into the launch
	//configuration and the port is started from that, with no pane in
	//between; then uZDL is done. Nothing is written back, so the launch tab
	//is found as it was left. A wrong name gets a dialog, since a shortcut
	//is the one place there is no other way to say so, and then uZDL is
	//done too: left open, it would write this configuration back when
	//closed, over whatever a uZDL already open saves to it.
	if (!presetToLaunch.isEmpty()){
		QString section = ZDLLaunchFiles::presetSection(tconf, presetToLaunch);
		if (section.isEmpty()){
			QMessageBox::warning(mw, ZDL_APP_NAME, QString("There is no preset named \"%1\" in\n%2\n\nA shortcut finds its preset by its name, so one renamed or deleted since the shortcut was made is not found.")
				.arg(presetToLaunch, QDir::toNativeSeparators(QFileInfo(ZDLConfigurationManager::getConfigFileName()).absoluteFilePath())));
			LOGDATA() << "ZDL QUIT, no such preset" << Qt::endl;
			return 0;
		} else {
			LOGDATA() << "Launching preset " << presetToLaunch << Qt::endl;
			//Into the launch configuration of the tab the preset is listed on.
			QString target = ZDLBuildArgs::isBuildLaunch(tconf, section) ? QString(ZDLLaunchFiles::BUILD_SECTION) : QString(ZDLLaunchFiles::LAUNCH_SECTION);
			ZDLLaunchFiles::applyPreset(tconf, section, target);
			mw->setLaunchSection(target);
			mw->launchNow();
			LOGDATA() << "ZDL QUIT" << Qt::endl;
			return 0;
		}
	}

	mw->handleImport();
	//A first start with nothing set up: offer to look for the games.
	ZDLFinderDialog::offerOnFirstStart(mw);
	//A Raze in the port list not set up for uZDL yet: offer that.
	ZDLRazeSetup::offerForNewPorts(mw);
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
	ZDLConfigurationManager::writeConfigFile();
	LOGDATA() << "ZDL QUIT" << Qt::endl;
	return ret;
}

