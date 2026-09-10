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

#include <QRegularExpression>
#include <algorithm>
#include "ZDLLaunchFiles.h"
#include "ZDLPaths.h"
#include "ZDLConfigurationManager.h"
#include "ZDLMainWindow.h"

extern ZDLMainWindow *mw;

const char *ZDLLaunchFiles::LAUNCH_SECTION = "zdl.save";
const char *ZDLLaunchFiles::PRESET_PREFIX = "zdl.mix";

QList<ZDLFileEntry> ZDLLaunchFiles::read(ZDLConf *zconf, const QString &section)
{
	QList<ZDLFileEntry> entries;

	if (!zconf)
		return entries;

	ZDLSection *sect=zconf->getSection(section);
	if (!sect)
		return entries;

	QVector<ZDLLine*> vctr;
	sect->getRegex("^file[0-9]+d?$", vctr);

	for (int i=0; i<vctr.size(); i++)
		entries<<ZDLFileEntry(vctr[i]->getValue(), vctr[i]->getVariable().endsWith("d", Qt::CaseInsensitive));

	return entries;
}

void ZDLLaunchFiles::write(ZDLConf *zconf, const QString &section, const QList<ZDLFileEntry> &entries)
{
	if (!zconf)
		return;

	ZDLSection *sect=zconf->getSection(section);
	if (sect) {
		QVector<ZDLLine*> vctr;
		sect->getRegex("^file[0-9]+d?$", vctr);

		//The lines handed back are copies, so removal has to go through the
		//configuration rather than the section - same as ZDLFileList does.
		for (int i=0; i<vctr.size(); i++)
			zconf->deleteValue(section, vctr[i]->getVariable());
	}

	for (int i=0; i<entries.size(); i++) {
		QString name=QString("file%1").arg(i);
		if (entries[i].disabled)
			name.append("d");
		//Inside the uZDL folder the entry is stored relative to it.
		zconf->setValue(section, name, ZDLPaths::preferRelative(entries[i].file));
	}
}

void ZDLLaunchFiles::sendToLaunch(const QList<ZDLFileEntry> &entries, bool replace, const QString &port, const QString &iwad)
{
	if (entries.isEmpty()&&!replace&&port.isEmpty()&&iwad.isEmpty())
		return;

	//Flush what the panes currently hold, otherwise anything reordered or
	//excluded since the last read is lost when they re-read below.
	mw->writeConfig();

	ZDLConf *zconf=ZDLConfigurationManager::getActiveConfiguration();
	if (!zconf)
		return;

	QList<ZDLFileEntry> merged;
	if (!replace)
		merged=read(zconf, LAUNCH_SECTION);

	foreach (const ZDLFileEntry &entry, entries) {
		bool present=false;
		foreach (const ZDLFileEntry &existing, merged) {
			//Compared resolved: either side may hold the relative form.
			if (!ZDLPaths::resolve(existing.file).compare(ZDLPaths::resolve(entry.file), Qt::CaseInsensitive)) {
				present=true;
				break;
			}
		}
		if (!present)
			merged<<entry;
	}

	write(zconf, LAUNCH_SECTION, merged);

	if (!port.isEmpty())
		zconf->setValue(LAUNCH_SECTION, "port", port);

	if (!iwad.isEmpty())
		zconf->setValue(LAUNCH_SECTION, "iwad", iwad);

	mw->startRead();
}

QStringList ZDLLaunchFiles::presetSections(ZDLConf *zconf)
{
	QStringList names;

	if (!zconf)
		return names;

	QRegularExpression mix_re(QRegularExpression::anchoredPattern(QString(PRESET_PREFIX)+"([0-9]+)"), QRegularExpression::CaseInsensitiveOption);
	QList<int> ids;

	for (int i=0; i<zconf->sections.size(); i++) {
		QRegularExpressionMatch mix_m=mix_re.match(zconf->sections[i]->getName());
		if (mix_m.hasMatch())
			ids<<mix_m.captured(1).toInt();
	}

	std::sort(ids.begin(), ids.end());

	foreach (int id, ids)
		names<<QString(PRESET_PREFIX)+QString::number(id);

	return names;
}

QString ZDLLaunchFiles::newPresetSection(ZDLConf *zconf)
{
	int next=0;

	foreach (const QString &name, presetSections(zconf)) {
		int id=name.mid(qstrlen(PRESET_PREFIX)).toInt();
		if (id>=next)
			next=id+1;
	}

	return QString(PRESET_PREFIX)+QString::number(next);
}
