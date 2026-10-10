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

#include <cstring>
#include <algorithm>
#include "ZDLFileInspector.h"
#include "ZDLFileInfo.h"
#include "ZDLGamePatches.h"
#include "ZDLMapFile.h"
#include "ZDLLaunchFiles.h"
#include "ZDLPaths.h"
#include "ZDLConfigurationManager.h"
#include "miniz.h"

namespace {

//A report is sections of key and value lines, laid out as text at the end.
struct Row {
	QString key;
	QString value;
};

struct Section {
	QString title;
	QList<Row> rows;

	Section(const QString &heading=QString()): title(heading) {}

	void add(const QString &key, const QString &value)
	{
		if (value.isEmpty())
			return;
		Row row;
		row.key=key;
		row.value=value;
		rows<<row;
	}
};

QString Human(qint64 bytes)
{
	if (bytes>=1024*1024)
		return QString("%1 MB").arg(bytes/(1024.0*1024.0), 0, 'f', 1);
	if (bytes>=1024)
		return QString("%1 KB").arg(bytes/1024.0, 0, 'f', 1);
	return QString("%1 bytes").arg(bytes);
}

QString Plural(qint64 count, const char *one, const char *many)
{
	return QString("%1 %2").arg(count).arg(count==1?one:many);
}

//Names joined, up to a screenful; the rest is counted.
QString Listed(const QStringList &names, int most=48)
{
	if (names.isEmpty())
		return QString();
	if (names.size()<=most)
		return names.join(" ");
	return names.mid(0, most).join(" ")+QString(" ... and %1 more").arg(names.size()-most);
}

QString Md5Of(const QString &file)
{
	QFile f(file);
	if (!f.open(QIODevice::ReadOnly))
		return QString();
	QCryptographicHash hash(QCryptographicHash::Md5);
	if (!hash.addData(&f))
		return QString();
	return QString::fromLatin1(hash.result().toHex());
}

QByteArray Head(const QString &file, int bytes)
{
	QFile f(file);
	if (!f.open(QIODevice::ReadOnly))
		return QByteArray();
	return f.read(bytes);
}

//The directory of a WAD: every lump with where it is and how big.
struct Lump {
	QString name;
	qint32 pos;
	qint32 size;
};

struct Wad {
	bool ok;
	bool iwad;
	QList<Lump> lumps;
	qint64 lumpBytes;
};

Wad ReadWad(const QString &file)
{
	Wad wad;
	wad.ok=false;
	wad.iwad=false;
	wad.lumpBytes=0;

	QFile f(file);
	if (!f.open(QIODevice::ReadOnly))
		return wad;

	char magic[4];
	qint32 count=0;
	qint32 offset=0;
	if (f.read(magic, 4)!=4||f.read((char*)&count, 4)!=4||f.read((char*)&offset, 4)!=4)
		return wad;

	wad.iwad=!memcmp(magic, "IWAD", 4);
	if (!wad.iwad&&memcmp(magic, "PWAD", 4))
		return wad;
	if (count<0||count>1000000||!f.seek(offset))
		return wad;

	for (qint32 i=0; i<count; i++) {
		char entry[16];
		if (f.read(entry, 16)!=16)
			break;
		Lump lump;
		memcpy(&lump.pos, entry, 4);
		memcpy(&lump.size, entry+4, 4);
		lump.name=QString::fromLatin1(entry+8, qstrnlen(entry+8, 8)).toUpper();
		wad.lumps<<lump;
		if (lump.size>0)
			wad.lumpBytes+=lump.size;
	}

	wad.ok=true;
	return wad;
}

QByteArray ReadLump(const QString &file, const Lump &lump, int most=65536)
{
	QFile f(file);
	if (lump.size<=0||!f.open(QIODevice::ReadOnly)||!f.seek(lump.pos))
		return QByteArray();
	return f.read(qMin<qint64>(lump.size, most));
}

const char *const MAP_LUMPS[]={
	"THINGS", "LINEDEFS", "SIDEDEFS", "VERTEXES", "SEGS", "SSECTORS", "NODES", "SECTORS",
	"REJECT", "BLOCKMAP", "BEHAVIOR", "SCRIPTS", "ZNODES", "DIALOGUE", NULL
};

bool IsMapLump(const QString &name)
{
	for (int i=0; MAP_LUMPS[i]; i++) {
		if (name==MAP_LUMPS[i])
			return true;
	}
	return false;
}

//The maps in a WAD and their formats: a marker followed by THINGS is a
//Doom or Hexen format map, the latter when BEHAVIOR is among its lumps;
//one followed by TEXTMAP is UDMF and runs to ENDMAP.
struct Maps {
	QStringList names;
	int doom;
	int hexen;
	int udmf;
};

Maps ScanMaps(const QList<Lump> &lumps)
{
	Maps maps;
	maps.doom=maps.hexen=maps.udmf=0;

	for (int i=0; i<lumps.size(); i++) {
		if (i+1<lumps.size()&&lumps[i+1].name=="TEXTMAP") {
			maps.names<<lumps[i].name;
			maps.udmf++;
			while (i+1<lumps.size()&&lumps[i+1].name!="ENDMAP")
				i++;
		} else if (i+1<lumps.size()&&lumps[i+1].name=="THINGS") {
			maps.names<<lumps[i].name;
			bool behavior=false;
			int j=i+1;
			while (j<lumps.size()&&IsMapLump(lumps[j].name)) {
				if (lumps[j].name=="BEHAVIOR")
					behavior=true;
				j++;
			}
			if (behavior)
				maps.hexen++;
			else
				maps.doom++;
			i=j-1;
		}
	}

	return maps;
}

//Lumps between S_START and S_END, F_START and F_END, and so on, the SS_
//and FF_ spellings PWADs use included. Nested markers are not counted.
int Between(const QList<Lump> &lumps, const QString &start, const QString &altStart, const QString &end, const QString &altEnd)
{
	int count=0;
	bool inside=false;
	foreach (const Lump &lump, lumps) {
		if (lump.name==start||lump.name==altStart) {
			inside=true;
		} else if (lump.name==end||lump.name==altEnd) {
			inside=false;
		} else if (inside&&!lump.name.endsWith("_START")&&!lump.name.endsWith("_END")) {
			count++;
		}
	}
	return count;
}

const char *const DEFINITION_LUMPS[]={
	"DEHACKED", "ZSCRIPT", "DECORATE", "MAPINFO", "ZMAPINFO", "UMAPINFO", "EMAPINFO", "DMAPINFO", "GAMECONF",
	"SBARINFO", "MENUDEF", "GLDEFS", "MODELDEF", "VOXELDEF", "SNDINFO", "SNDSEQ", "MUSINFO", "ANIMDEFS", "ANIMATED",
	"SWITCHES", "TEXTURES", "LANGUAGE", "KEYCONF", "CVARINFO", "LOCKDEFS", "TERRAIN", "LOADACS", "DECALDEF",
	"FONTDEFS", "COMPLVL", "GAMEINFO", "IWADINFO", "SECRETS", NULL
};

const char *const GAME_DATA_LUMPS[]={
	"PLAYPAL", "COLORMAP", "TRANMAP", "TEXTURE1", "TEXTURE2", "PNAMES", "ENDOOM", "TITLEPIC", "INTERPIC",
	"CREDIT", "HELP", "BOSSBACK", "M_DOOM", "DEMO1", "DEMO2", "DEMO3", NULL
};

QStringList Present(const QSet<QString> &have, const char *const *names)
{
	QStringList present;
	for (int i=0; names[i]; i++) {
		if (have.contains(names[i]))
			present<<names[i];
	}
	return present;
}

//What a DeHackEd patch says about itself and how much it changes.
void DescribePatch(Section &section, const QString &text)
{
	QRegularExpressionMatch version=QRegularExpression("^\\s*Doom version\\s*=\\s*(\\d+)", QRegularExpression::MultilineOption).match(text);
	QRegularExpressionMatch format=QRegularExpression("^\\s*Patch format\\s*=\\s*(\\d+)", QRegularExpression::MultilineOption).match(text);
	if (version.hasMatch())
		section.add("Doom version", version.captured(1));
	if (format.hasMatch())
		section.add("Patch format", format.captured(1));

	QStringList kinds;
	static const char *const blocks[]={"Thing", "Frame", "Sound", "Text", "Pointer", "Weapon", "Ammo", "Cheat", "Misc", "Sprite", NULL};
	for (int i=0; blocks[i]; i++) {
		int n=text.count(QRegularExpression(QString("^\\s*%1\\b").arg(blocks[i]), QRegularExpression::MultilineOption));
		if (n)
			kinds<<QString("%1 %2").arg(n).arg(blocks[i]);
	}
	section.add("Changes", kinds.join(", "));

	QStringList bex;
	QRegularExpressionMatchIterator it=QRegularExpression("^\\s*\\[([A-Z]+)\\]", QRegularExpression::MultilineOption).globalMatch(text);
	while (it.hasNext())
		bex<<it.next().captured(1);
	bex.removeDuplicates();
	if (!bex.isEmpty())
		section.add("BEX sections", bex.join(", ")+" (Boom and later)");
}

//Which ports can be expected to take the file, from what it asks for. Map
//information comes in one dialect per port family, and a file that ships
//several is meant for all of them, so only what a single family
//understands decides.
QString RunsOn(const QSet<QString> &have, int udmf, int hexen, bool pk3, bool zscriptFolder)
{
	if (have.contains("ZSCRIPT")||zscriptFolder)
		return "GZDoom or UZDoom: it carries ZScript";

	QStringList zdoomOnly;
	static const char *const zdoomLumps[]={"DECORATE", "SBARINFO", "MENUDEF", "GLDEFS", "MODELDEF", "VOXELDEF", "CVARINFO", "KEYCONF", "LOADACS", "DECALDEF", "TERRAIN", "SNDSEQ", NULL};
	for (int i=0; zdoomLumps[i]; i++) {
		if (have.contains(zdoomLumps[i]))
			zdoomOnly<<zdoomLumps[i];
	}
	if (udmf) zdoomOnly<<"UDMF maps";
	if (hexen) zdoomOnly<<"Hexen-format maps with scripts";
	if (pk3) zdoomOnly<<"an archive rather than a WAD";
	if (!zdoomOnly.isEmpty())
		return "the ZDoom family (GZDoom, UZDoom, Zandronum): "+zdoomOnly.join(", ");

	QStringList dialects;
	if (have.contains("ZMAPINFO")||have.contains("MAPINFO")) dialects<<"the ZDoom family";
	if (have.contains("UMAPINFO")) dialects<<"UMAPINFO ports (Woof, DSDA-Doom, Nugget Doom, Crispy Doom)";
	if (have.contains("EMAPINFO")) dialects<<"Eternity";
	if (have.contains("GAMECONF")) dialects<<"the KEX re-release";
	if (dialects.size()>1)
		return "any port; it carries map information for "+dialects.join(", ");
	if (dialects.size()==1)
		return "any port, its map information read by "+dialects.first();
	if (have.contains("DEHACKED"))
		return "any current port; the DeHackEd patch is read from the DEHACKED lump";
	return "any port";
}

//Map names in the order people expect, each once: a WAD lists them as
//they were saved and an archive may name a map twice, in maps/ and again
//in its MAPINFO.
QStringList Ordered(QStringList names)
{
	QCollator collator;
	collator.setNumericMode(true);
	collator.setCaseSensitivity(Qt::CaseInsensitive);
	std::sort(names.begin(), names.end(), [&collator](const QString &l, const QString &r) {
		return collator.compare(l, r)<0;
	});
	names.removeDuplicates();
	return names;
}

//MAPxx or ExMy, which says which games the maps are for.
QString MapStyle(const QStringList &names)
{
	bool mapxx=false;
	bool exmy=false;
	foreach (const QString &name, names) {
		if (QRegularExpression("^MAP\\d\\d$").match(name).hasMatch())
			mapxx=true;
		else if (QRegularExpression("^E\\dM\\d$").match(name).hasMatch())
			exmy=true;
	}

	QStringList style;
	if (mapxx) style<<"MAPxx, as Doom II, Final Doom, Hexen and Strife name them";
	if (exmy) style<<"ExMy, as Doom and Heretic name them";
	return style.join("; ");
}

void DescribeWad(const QString &file, const Wad &wad, QList<Section> &sections, QString &title)
{
	Section id("Identification");
	ZDLIwadInfo info(file);
	QString name=info.Identify();
	if (wad.iwad) {
		if (name.isEmpty()) {
			id.add("Game", "not one uZDL knows; the header says IWAD all the same");
		} else {
			title=name;
			id.add("Game", name);
			id.add("Family", info.Family());
			id.add("Told by", info.IdentifiedBy());
		}
	} else if (!name.isEmpty()&&!info.IdentifiedBy().startsWith("the lumps")) {
		//A PWAD with a MAP01 passes the lump rules for Doom II, which says
		//nothing; a hash or an IWADINFO match does.
		id.add("Looks like", name+", but the header says PWAD");
	}
	sections<<id;

	Maps maps=ScanMaps(wad.lumps);
	QStringList mapNames=Ordered(maps.names);
	Section mapSection(mapNames.isEmpty()?QString("Maps"):QString("Maps (%1)").arg(mapNames.size()));
	if (mapNames.isEmpty()) {
		mapSection.add("None", "no maps in this file");
	} else {
		QStringList formats;
		if (maps.doom) formats<<QString("%1 Doom format").arg(maps.doom);
		if (maps.hexen) formats<<QString("%1 Hexen format").arg(maps.hexen);
		if (maps.udmf) formats<<QString("%1 UDMF").arg(maps.udmf);
		mapSection.add("Format", formats.join(", "));
		mapSection.add("Style", MapStyle(mapNames));
		mapSection.add("Names", Listed(mapNames));
	}
	sections<<mapSection;

	QSet<QString> have;
	foreach (const Lump &lump, wad.lumps)
		have.insert(lump.name);

	int sounds=0, music=0;
	foreach (const Lump &lump, wad.lumps) {
		if (lump.name.startsWith("DS")&&lump.size>0) sounds++;
		else if (lump.name.startsWith("D_")&&lump.size>0) music++;
	}

	Section contents(QString("Contents (%1, %2)").arg(Plural(wad.lumps.size(), "lump", "lumps")).arg(Human(wad.lumpBytes)));
	int sprites=Between(wad.lumps, "S_START", "SS_START", "S_END", "SS_END");
	int flats=Between(wad.lumps, "F_START", "FF_START", "F_END", "FF_END");
	int patches=Between(wad.lumps, "P_START", "PP_START", "P_END", "PP_END");
	if (sprites) contents.add("Sprites", QString::number(sprites));
	if (flats) contents.add("Flats", QString::number(flats));
	if (patches) contents.add("Patches", QString::number(patches));
	if (sounds) contents.add("Sounds", QString("%1 (DS lumps)").arg(sounds));
	if (music) contents.add("Music", QString("%1 (D_ lumps)").arg(music));
	contents.add("Definitions", Present(have, DEFINITION_LUMPS).join(" "));
	contents.add("Game data", Present(have, GAME_DATA_LUMPS).join(" "));
	sections<<contents;

	foreach (const Lump &lump, wad.lumps) {
		if (lump.name=="DEHACKED"&&lump.size>0) {
			Section patch(QString("DeHackEd lump (%1)").arg(Human(lump.size)));
			DescribePatch(patch, QString::fromLatin1(ReadLump(file, lump)));
			sections<<patch;
			break;
		}
	}
	foreach (const Lump &lump, wad.lumps) {
		if (lump.name=="COMPLVL"&&lump.size>0) {
			Section level("Compatibility level (COMPLVL)");
			level.add("Asks for", QString::fromLatin1(ReadLump(file, lump, 64)).trimmed());
			sections<<level;
			break;
		}
	}

	if (!wad.iwad) {
		Section needs("Runs on");
		needs.add("Expected", RunsOn(have, maps.udmf, maps.hexen, false, false));
		sections<<needs;
	}
}

//An archive or a folder laid out like one: pk3 and the ZDoom family's
//directory conventions.
struct ArchiveEntry {
	QString path;
	qint64 size;
};

void DescribeArchive(const QString &file, const QList<ArchiveEntry> &entries, bool folder, QList<Section> &sections, QString &title)
{
	QSet<QString> rootNames;
	QMap<QString, int> folders;
	QStringList maps;
	qint64 total=0;
	QString iwadinfoName;

	foreach (const ArchiveEntry &entry, entries) {
		total+=entry.size;
		QString path=QString(entry.path).replace('\\', '/');
		int slash=path.indexOf('/');
		if (slash<0) {
			rootNames.insert(QFileInfo(path).baseName().toUpper());
		} else {
			folders[path.left(slash).toLower()]++;
			if (path.startsWith("maps/", Qt::CaseInsensitive))
				maps<<QFileInfo(path).baseName().left(8).toUpper();
		}
	}

	if (ZDLMapFile *mapfile=ZDLMapFile::getMapFile(file)) {
		iwadinfoName=mapfile->getIwadinfoName();
		maps=mapfile->getMapNames();
		delete mapfile;
	}

	Section id("Identification");
	if (!iwadinfoName.isEmpty()) {
		title=iwadinfoName;
		id.add("Game", iwadinfoName);
		id.add("Told by", "its own IWADINFO");
	} else {
		id.add("Game", folder?"none: a folder of resources, loaded as a mod":"none: a mod archive");
	}
	sections<<id;

	maps=Ordered(maps);
	Section mapSection(maps.isEmpty()?QString("Maps"):QString("Maps (%1)").arg(maps.size()));
	if (maps.isEmpty()) {
		mapSection.add("None", "no maps in this file");
	} else {
		mapSection.add("Style", MapStyle(maps));
		mapSection.add("Names", Listed(maps));
	}
	sections<<mapSection;

	Section contents(QString("Contents (%1, %2)").arg(Plural(entries.size(), "file", "files")).arg(Human(total)));
	QStringList folderList;
	foreach (const QString &name, folders.keys())
		folderList<<QString("%1/ (%2)").arg(name).arg(folders[name]);
	contents.add("Folders", Listed(folderList));
	contents.add("Definitions", Present(rootNames, DEFINITION_LUMPS).join(" "));
	sections<<contents;

	if (iwadinfoName.isEmpty()) {
		Section needs("Runs on");
		needs.add("Expected", RunsOn(rootNames, 0, 0, true, folders.contains("zscript")));
		sections<<needs;
	}
}

void DescribePk3(const QString &file, QList<Section> &sections, QString &title)
{
	QList<ArchiveEntry> entries;

	mz_zip_archive zip={};
	if (mz_zip_reader_init_file(&zip, qPrintable(file), 0)) {
		mz_uint count=mz_zip_reader_get_num_files(&zip);
		mz_zip_archive_file_stat stat;
		for (mz_uint i=0; i<count; i++) {
			if (!mz_zip_reader_is_file_a_directory(&zip, i)&&mz_zip_reader_file_stat(&zip, i, &stat)) {
				ArchiveEntry entry;
				entry.path=QString::fromUtf8(stat.m_filename);
				entry.size=(qint64)stat.m_uncomp_size;
				entries<<entry;
			}
		}
		mz_zip_reader_end(&zip);
	}

	DescribeArchive(file, entries, false, sections, title);
}

void DescribeFolder(const QString &dir, QList<Section> &sections, QString &title)
{
	QList<ArchiveEntry> entries;
	QDir root(dir);

	QDirIterator it(dir, QDir::Files|QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		it.next();
		ArchiveEntry entry;
		entry.path=root.relativeFilePath(it.filePath());
		entry.size=it.fileInfo().size();
		entries<<entry;
	}

	DescribeArchive(dir, entries, true, sections, title);
}

//A recorded game. Vanilla and Final Doom demos open with the version the
//game was, then the settings; Boom and later put a signature first, and
//the ZDoom family its own.
void DescribeDemo(const QByteArray &head, QList<Section> &sections)
{
	Section demo("Demo");
	if (head.size()<13) {
		demo.add("Note", "too short to be a Doom demo");
		sections<<demo;
		return;
	}

	unsigned char version=(unsigned char)head[0];
	if (head.startsWith("ZDEM")) {
		demo.add("Recorded with", "a ZDoom-family port");
	} else if (version>=104&&version<=111) {
		static const char *const games[]={"Doom 1.4", "Doom 1.5", "Doom 1.6", "Doom 1.7", "Doom 1.8", "Doom 1.9", "Doom 1.9 (Final Doom / TASDoom)", "Doom 1.91 (Final Doom Anthology)"};
		demo.add("Recorded with", QString("%1, version byte %2").arg(games[version-104]).arg(version));
		int skill=(unsigned char)head[1]+1;
		int episode=(unsigned char)head[2];
		int map=(unsigned char)head[3];
		demo.add("Map", episode>0?QString("E%1M%2").arg(episode).arg(map):QString("MAP%1").arg(map, 2, 10, QChar('0')));
		demo.add("Skill", QString::number(skill));
		QStringList flags;
		if (head[4]) flags<<"deathmatch";
		if (head[5]) flags<<"respawn";
		if (head[6]) flags<<"fast";
		if (head[7]) flags<<"no monsters";
		demo.add("Options", flags.join(", "));
		int players=0;
		for (int i=9; i<13; i++)
			if (head[i]) players++;
		demo.add("Players", QString::number(players));
	} else if (version>=200&&version<=203) {
		demo.add("Recorded with", QString("Boom or MBF, version byte %1").arg(version));
	} else if (version>=210&&version<=214) {
		demo.add("Recorded with", QString("PrBoom+ or a descendant, version byte %1").arg(version));
	} else if (version==221) {
		demo.add("Recorded with", "an MBF21 port, version byte 221");
	} else if (version<=4) {
		demo.add("Recorded with", QString("Doom before 1.4, or Heretic or Hexen, first byte %1").arg(version));
	} else {
		demo.add("Recorded with", QString("unknown, first byte %1").arg(version));
	}
	sections<<demo;
}

void DescribeLaunchFile(const QString &file, QList<Section> &sections)
{
	Section launch("Launch file");
	ZDLConf conf;
	if (conf.readINI(file)!=0) {
		launch.add("Note", "could not be read");
		sections<<launch;
		return;
	}

	const char *section=ZDLLaunchFiles::LAUNCH_SECTION;
	if (conf.hasValue(section, "port")) launch.add("Source port", conf.getValue(section, "port"));
	if (conf.hasValue(section, "iwad")) launch.add("IWAD", conf.getValue(section, "iwad"));
	if (conf.hasValue(section, "warp")) launch.add("Map", conf.getValue(section, "warp"));
	if (conf.hasValue(section, "skill")) launch.add("Skill", conf.getValue(section, "skill"));
	if (conf.hasValue(section, "savedir")) launch.add("Save folder", conf.getValue(section, "savedir"));
	if (conf.hasValue(section, "config")) launch.add("Config file", conf.getValue(section, "config"));
	QStringList files;
	foreach (const ZDLFileEntry &entry, ZDLLaunchFiles::read(&conf, section))
		files<<(entry.disabled?"("+entry.file+")":entry.file);
	launch.add("Files", files.isEmpty()?QString("none"):files.join("\n              "));
	sections<<launch;
}

void DescribeExecutable(const QString &file, QList<Section> &sections, QString &title)
{
	ZDLAppInfo app(file);
	Section port("Source port");
	if (app.IsKnownPort()) {
		title=app.GetFileDescription();
		port.add("Port", app.GetProductName());
	} else {
		port.add("Port", "not one uZDL knows by name");
	}
	port.add("Version", app.GetVersion());

	static const char *const fields[]={"ProductName", "ProductVersion", "FileVersion", "FileDescription", "CompanyName", "LegalCopyright", "OriginalFilename", NULL};
	for (int i=0; fields[i]; i++)
		port.add(fields[i], app.VersionField(fields[i]));
	sections<<port;
}

//A game of the Build engine: which, as the ports tell it, what a GRP
//holds, and the ports that run it.
void DescribeBuild(const QString &file, const ZDLBuildGames::Match &match, QList<Section> &sections, QString &title)
{
	Section id("Identification");
	if (match.valid()) {
		title=match.name;
		id.add("Game", match.name);
		id.add("Family", match.family);
		id.add("Told by", match.how);
		id.add("Add-on to", match.dependsOn);
	} else {
		id.add("Game", "not one uZDL knows: the ports tell a game by its size and CRC32, and these are in no table");
	}
	bool ok=false;
	quint32 crc=QFileInfo(file).size()<1024LL*1024*1024?ZDLBuildGames::crcOf(file, &ok):0;
	if (ok)
		id.add("CRC32", QString("0x%1").arg(QString::number(crc, 16).toUpper().rightJustified(8, '0')));
	sections<<id;

	QList<QPair<QString, qint64> > entries=ZDLBuildGames::grpEntries(file);
	if (!entries.isEmpty()) {
		QStringList maps, scripts, definitions;
		int tiles=0, sounds=0, music=0;
		qint64 total=0;
		for (int i=0; i<entries.size(); i++) {
			QString name=entries[i].first.toUpper();
			QString entryExt=QFileInfo(name).suffix();
			total+=entries[i].second;
			if (entryExt=="MAP") maps<<name;
			else if (entryExt=="CON") scripts<<name;
			else if (entryExt=="DEF") definitions<<name;
			else if (entryExt=="ART") tiles++;
			else if (entryExt=="VOC"||entryExt=="WAV") sounds++;
			else if (entryExt=="MID"||entryExt=="OGG"||entryExt=="MUS") music++;
		}
		Section contents(QString("Contents (%1, %2)").arg(Plural(entries.size(), "file", "files")).arg(Human(total)));
		contents.add("Maps", maps.isEmpty()?QString():QString("%1: %2").arg(maps.size()).arg(Listed(Ordered(maps))));
		contents.add("Scripts", scripts.join(" "));
		contents.add("Definitions", definitions.join(" "));
		if (tiles) contents.add("Tile files", QString::number(tiles));
		if (sounds) contents.add("Sounds", QString::number(sounds));
		if (music) contents.add("Music", QString::number(music));
		sections<<contents;
	}

	if (match.valid()) {
		Section runs("Runs on");
		runs.add("Ports", match.runsOn);
		sections<<runs;
	}
}

QString Render(const QString &title, const QString &path, const QList<Section> &sections)
{
	QString out=title+"\n"+QDir::toNativeSeparators(path)+"\n";

	foreach (const Section &section, sections) {
		if (section.rows.isEmpty())
			continue;
		out+="\n"+section.title+"\n";
		int width=0;
		foreach (const Row &row, section.rows)
			width=qMax(width, (int)row.key.size());
		foreach (const Row &row, section.rows)
			out+="  "+row.key.leftJustified(width+2)+row.value+"\n";
	}

	return out;
}

}

QString ZDLFileInspector::report(const QString &file)
{
	QFileInfo fi(file);
	if (!fi.exists())
		return QDir::toNativeSeparators(file)+"\n\nThere is no such file.";

	QList<Section> sections;
	QString title=fi.fileName();
	QString ext=fi.suffix().toLower();
	QByteArray head=fi.isDir()?QByteArray():Head(file, 16);

	Section about("File");
	QString kind;

	//What is known of it as a game of the Build engine, for the kinds of
	//file those games come in.
	ZDLBuildGames::Match build;
	bool grp=head.startsWith("KenSilverman");
	bool rff=head.startsWith("RFF\x1a");
	bool buildKind=!fi.isDir()&&(grp||rff||ZDLBuildGames::isCandidate(ext)||ext=="pak"||ext=="ini"||ext=="con"||(ext=="zip"&&head.startsWith("PK\x03\x04")));
	if (buildKind) {
		ZDLIwadInfo info(file);
		info.Identify();
		build=info.BuildMatch();
	}
	//A game that is a folder of loose files is told by that folder, for the
	//folder and for each of the game's own files.
	if (!build.valid())
		build=ZDLBuildGames::byFolder(file);
	//A mod of loose files over Duke Nukem 3D, for its folder as for its main
	//script.
	bool modFolder=false;
	if (!build.valid()&&fi.isDir()) {
		QString script=ZDLBuildGames::modFolderScript(file);
		if (!script.isEmpty()) {
			build=ZDLBuildGames::byModScripts(script);
			modFolder=build.valid();
		}
	}
	//An add-on of Blood's, for its INI and for a folder that holds it.
	QString bloodIni=fi.isDir()?ZDLBuildGames::bloodModIni(file):(ZDLBuildGames::byBloodIni(file).valid()?file:QString());
	if (!build.valid()&&!bloodIni.isEmpty())
		build=ZDLBuildGames::byBloodIni(bloodIni);

	if (fi.isDir()) {
		kind="folder";
	} else if (grp) {
		kind="GRP, a Build engine game archive";
	} else if (rff) {
		kind="RFF, a Blood archive";
	} else if (ext=="ssi") {
		kind="SSI, a Sunstorm Interactive add-on archive";
	} else if (head.startsWith("IWAD")) {
		kind="IWAD, the data of a whole game";
	} else if (head.startsWith("PWAD")) {
		kind="PWAD, a patch loaded on top of a game";
	} else if (head.startsWith("PK\x03\x04")) {
		kind=ext=="ipk3"?"IPK3, a game in a zip archive":ext=="pke"?"PKE, an Eternity archive":ext=="grp"?"GRP in zip form, a Build engine game archive":"PK3, a zip archive of resources";
	} else if (head.startsWith("7z\xBC\xAF\x27\x1C")) {
		kind=ext=="ipk7"?"IPK7, a game in a 7-Zip archive":"PK7, a 7-Zip archive of resources; its contents cannot be listed here";
	} else if (head.startsWith("MZ")) {
		kind="Windows executable";
	} else if (head.startsWith("\x7F""ELF")) {
		kind="Linux executable";
	} else if (ext=="deh"||ext=="bex") {
		kind=ext=="bex"?"DeHackEd patch with BEX extensions":"DeHackEd patch";
	} else if (ext=="lmp") {
		kind="demo recording";
	} else if (ext=="zdl") {
		kind="uZDL launch file";
	} else if (ext=="ini") {
		kind=bloodIni.isEmpty()?"configuration file":"INI that defines Blood episodes";
	} else if (ext=="cfg") {
		kind="console script or configuration, run with +exec";
	} else if (ext=="txt"||ext=="md") {
		kind="text";
	} else if (ext=="con") {
		kind="CON script, the game code of Duke Nukem 3D and the games built on it";
	} else if (ext=="art") {
		kind="ART, Build engine tiles";
	} else if (ext=="map") {
		kind="Build engine map";
	} else if (ext=="grpinfo") {
		kind="game definitions, read by Raze and EDuke32";
	} else if (build.valid()) {
		kind="game data of the Build engine";
	} else {
		kind="not a file uZDL knows";
	}

	about.add("Kind", kind);
	if (!fi.isDir()) {
		about.add("Size", QString("%1 (%2 bytes)").arg(Human(fi.size())).arg(fi.size()));
		about.add("Modified", fi.lastModified().toString("yyyy-MM-dd hh:mm"));
	}
	about.add("Folder", QDir::toNativeSeparators(fi.absolutePath()));
	if (!fi.isDir()&&fi.size()<256*1024*1024&&(head.startsWith("IWAD")||head.startsWith("PWAD")||head.startsWith("PK\x03\x04")||ext=="ipk7"||ext=="pk7"))
		about.add("MD5", Md5Of(file));
	if (!fi.isDir()) {
		QStringList notes=ZDLGamePatches::notesFor(file);
		about.add("Patch", notes.join("\n            "));
	}
	sections<<about;

	if (fi.isDir()) {
		DescribeFolder(file, sections, title);
	} else if (head.startsWith("IWAD")||head.startsWith("PWAD")) {
		Wad wad=ReadWad(file);
		if (wad.ok)
			DescribeWad(file, wad, sections, title);
	} else if (head.startsWith("PK\x03\x04")) {
		DescribePk3(file, sections, title);
	} else if (head.startsWith("MZ")||(!ext.isEmpty()&&ext!="lmp"&&ext!="deh"&&ext!="bex"&&ext!="zdl"&&ext!="ini"&&ext!="cfg"&&ext!="txt"&&fi.isExecutable()&&head.startsWith("\x7F""ELF"))||(ext.isEmpty()&&fi.isExecutable())) {
		DescribeExecutable(file, sections, title);
	} else if (ext=="deh"||ext=="bex") {
		Section patch("Patch");
		QFile f(file);
		if (f.open(QIODevice::ReadOnly|QIODevice::Text))
			DescribePatch(patch, QString::fromLatin1(f.read(1024*1024)));
		sections<<patch;
	} else if (ext=="lmp") {
		DescribeDemo(Head(file, 32), sections);
	} else if (ext=="zdl") {
		DescribeLaunchFile(file, sections);
	}

	if (build.valid()||grp||rff||ext=="grp") {
		//A zip that is a Build add-on is no mod archive: the archive's own
		//guesses give way.
		for (int i=sections.size()-1; i>=0; i--) {
			if (sections[i].title=="Identification"||sections[i].title=="Runs on")
				sections.removeAt(i);
		}
		DescribeBuild(file, build, sections, title);
	}
	if (modFolder) {
		Section use("Use");
		use.add("As a mod", "add this folder to the mods on the Build Engine tab; EDuke32 runs it over Duke Nukem 3D, which is picked for it when no game is");
		sections<<use;
	}
	if (!bloodIni.isEmpty()&&ZDLBuildGames::byBloodIni(bloodIni).valid()) {
		Section use("Use");
		use.add("In NBlood", QString("add %1 to the mods on the Build Engine tab, over Blood or with no game picked; NBlood plays its episodes when given -ini %2, which it gets")
			.arg(fi.isDir()?"this folder":"it or its folder").arg(QFileInfo(bloodIni).fileName()));
		sections<<use;
	}

	return Render(title, file, sections);
}

ZDLFileInspector::ZDLFileInspector(QWidget *parent, const QString &file):
	QDialog(parent)
{
	setWindowTitle("File information");
	setWindowFlags(windowFlags()&~Qt::WindowContextHelpButtonHint);
	setAcceptDrops(true);
	resize(640, 520);

	QVBoxLayout *column=new QVBoxLayout(this);

	QHBoxLayout *pathRow=new QHBoxLayout();
	pathEdit=new QLineEdit(this);
	pathEdit->setPlaceholderText("A file to look at, or drop one here");
	pathRow->addWidget(pathEdit, 1);
	QPushButton *btnBrowse=new QPushButton("Browse...", this);
	pathRow->addWidget(btnBrowse);
	column->addLayout(pathRow);

	view=new QPlainTextEdit(this);
	view->setReadOnly(true);
	view->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
	view->setLineWrapMode(QPlainTextEdit::WidgetWidth);
	view->setAcceptDrops(false);
	column->addWidget(view, 1);

	QHBoxLayout *buttons=new QHBoxLayout();
	QPushButton *btnCopy=new QPushButton("Copy report", this);
	QPushButton *btnClose=new QPushButton("Close", this);
	btnClose->setDefault(true);
	buttons->addWidget(btnCopy);
	buttons->addStretch();
	buttons->addWidget(btnClose);
	column->addLayout(buttons);

	QObject::connect(btnBrowse, SIGNAL(clicked()), this, SLOT(browse()));
	QObject::connect(btnCopy, SIGNAL(clicked()), this, SLOT(copyReport()));
	QObject::connect(btnClose, SIGNAL(clicked()), this, SLOT(close()));
	QObject::connect(pathEdit, SIGNAL(editingFinished()), this, SLOT(inspectTyped()));

	if (!file.isEmpty())
		inspect(file);
}

void ZDLFileInspector::inspect(const QString &file)
{
	pathEdit->setText(QDir::toNativeSeparators(file));
	QApplication::setOverrideCursor(Qt::WaitCursor);
	view->setPlainText(report(file));
	QApplication::restoreOverrideCursor();
}

void ZDLFileInspector::inspectTyped()
{
	QString typed=QFD_QT_SEP(pathEdit->text().trimmed());
	if (!typed.isEmpty()&&QFileInfo(typed).exists())
		inspect(typed);
}

void ZDLFileInspector::browse()
{
	QString start=pathEdit->text().isEmpty()?getWadLastDir():QFileInfo(QFD_QT_SEP(pathEdit->text())).absolutePath();
	QString file=QFileDialog::getOpenFileName(this, "Inspect file", start, "All files (" QFD_FILTER_ALL ")");
	if (!file.isEmpty())
		inspect(QFD_QT_SEP(file));
}

void ZDLFileInspector::copyReport()
{
	QApplication::clipboard()->setText(view->toPlainText());
}

void ZDLFileInspector::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasUrls())
		event->acceptProposedAction();
}

void ZDLFileInspector::dropEvent(QDropEvent *event)
{
	foreach (const QUrl &url, event->mimeData()->urls()) {
		if (url.isLocalFile()) {
			inspect(url.toLocalFile());
			event->acceptProposedAction();
			return;
		}
	}
}

void ZDLFileInspector::open(QWidget *parent, const QString &file)
{
	QString chosen=file;
	if (chosen.isEmpty()) {
		chosen=QFileDialog::getOpenFileName(parent, "Inspect file", getWadLastDir(), "All files (" QFD_FILTER_ALL ")");
		if (chosen.isEmpty())
			return;
		chosen=QFD_QT_SEP(chosen);
	}

	ZDLFileInspector *inspector=new ZDLFileInspector(parent, chosen);
	inspector->setAttribute(Qt::WA_DeleteOnClose);
	inspector->show();
}
