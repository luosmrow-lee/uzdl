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
#include <QFile>
#include <QMap>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QRegularExpression>
#include "ZDLBuildGames.h"
#include "miniz.h"

namespace {

enum Family { DUKE, NAM, WW2GI, REDNECK, BLOOD, SW, POWERSLAVE, WITCHAVEN, TEKWAR, PALADINS, CONVERSIONS, FAMILY_COUNT };

const char *const FAMILY_NAMES[FAMILY_COUNT]={
	"Duke Nukem 3D", "NAM", "WWII GI", "Redneck Rampage", "Blood", "Shadow Warrior", "PowerSlave",
	"Witchaven", "TekWar", "Legend of the Seven Paladins", "Build total conversions"
};

const char *const RUNS_ON[FAMILY_COUNT]={
	"EDuke32, Rednukem, Raze or BuildGDX",
	"EDuke32, Rednukem, Raze or BuildGDX",
	"EDuke32, Rednukem or Raze",
	"Rednukem, Raze or BuildGDX",
	"NBlood, Raze or BuildGDX",
	"VoidSW, Raze or BuildGDX",
	"PCExhumed, Raze or BuildGDX",
	"BuildGDX",
	"BuildGDX",
	"BuildGDX",
	"EDuke32 or Raze"
};

//The base games add-ons name as their dependency.
const quint32 DUKE13=0xBBC9CE44;
const quint32 DUKE15=0xFD3DCFF1;
const quint32 DUKEPP=0xF514A6AC;
const quint32 DUKEWT=0x982AFE4A;
const quint32 BLOOD_OUWB=0xA8FDDA84;
const quint32 SW_REG=0x7545319F;
const quint32 RR=0x19D9BC79;
const quint32 RRRA=0x958018C6;
const quint32 NAM_CRC=0x75C1F07B;
const quint32 WW2GI_CRC=0x907B82BF;

struct Game {
	qint64 size;
	quint32 crc;
	const char *name;
	int family;
	//The CRC32 of the base game an add-on needs; 0 for a game of its own.
	quint32 dependency;
	//Raze's table has it and EDuke32's does not, so EDuke32 does not pick
	//it by its checksum.
	bool razeOnly;
};

//From Raze's engine/grpinfo.txt, with EDuke32's and PCExhumed's tables
//where they differ: EDuke32 has the real CRC32 of Duke!ZONE II for 1.3D,
//and PCExhumed the real sizes of the two demos.
const Game GAMES[]={
	{26524524, DUKE13, "Duke Nukem 3D v1.3D", DUKE, 0},
	{26385383, 0xAA4F6A40, "Duke Nukem 3D (South Korean Censored)", DUKE, 0},
	{44356548, DUKE15, "Duke Nukem 3D: Atomic Edition", DUKE, 0},
	{44356548, DUKEWT, "Duke Nukem 3D: 20th Anniversary World Tour", DUKE, 0},
	{44486702, 0x89792E05, "Duke Nukem 3D: 20th Anniversary World Tour", DUKE, 0, true},
	{44348015, DUKEPP, "Duke Nukem 3D: Plutonium Pak", DUKE, 0},
	{9690241, 0x02F18900, "Duke Nukem 3D Shareware 0.99", DUKE, 0},
	{10429258, 0xA28AA589, "Duke Nukem 3D Shareware 1.0", DUKE, 0},
	{10442980, 0x912E1E8D, "Duke Nukem 3D Shareware 1.1", DUKE, 0},
	{11035779, 0x983AD923, "Duke Nukem 3D Shareware 1.3D", DUKE, 0},
	{10444391, 0xC5F71561, "Duke Nukem 3D Mac Demo", DUKE, 0},
	{10628573, 0x73A15EE7, "Duke Nukem 3D MacUser Demo", DUKE, 0},
	{7926624, 0xA9242158, "Duke it out in D.C. (1.3D)", DUKE, DUKE13},
	{8225517, 0xB79D997F, "Duke it out in D.C.", DUKE, DUKE15},
	{8410183, 0xA8CF80DA, "Duke it out in D.C.", DUKE, DUKE15},
	{8410187, 0x39A692BF, "Duke it out in D.C.", DUKE, DUKE15},
	{8410183, 0x777C77B1, "Duke it out in D.C.", DUKE, DUKE15, true},
	{8410149, 0xC63B6A8B, "Duke it out in D.C.", DUKE, DUKE15},
	{23559381, 0x4A2DBB62, "Duke Caribbean: Life's a Beach (1.3D)", DUKE, DUKE13},
	{22551333, 0x2F4FCCEE, "Duke Caribbean: Life's a Beach (PPak)", DUKE, DUKEPP},
	{22521880, 0xB62B42FD, "Duke Caribbean: Life's a Beach", DUKE, DUKE15},
	{22213819, 0x18F01C5B, "Duke Caribbean: Life's a Beach", DUKE, DUKE15},
	{22397273, 0x65B5F690, "Duke Caribbean: Life's a Beach", DUKE, DUKE15},
	{22201375, 0xB28E883C, "Duke Caribbean: Life's a Beach", DUKE, DUKE15, true},
	{22213795, 0x64CF2351, "Duke Caribbean: Life's a Beach", DUKE, DUKE15},
	{16169365, 0xF1CAE8E4, "Duke: Nuclear Winter", DUKE, DUKE15},
	{10965909, 0xC7EFBFA9, "Duke: Nuclear Winter Demo", DUKE, DUKE15},
	{26135388, 0x82C1B47F, "Duke!ZONE II (1.3D)", DUKE, DUKE13},
	{44100411, 0x7FB6117C, "Duke!ZONE II", DUKE, DUKE15},
	{3186656, 0x1E9516F1, "Duke!ZONE II", DUKE, DUKE15},
	{2112419, 0x7CD82A3B, "Duke Nukem's Penthouse Paradise", DUKE, DUKE15},
	{4247491, 0xCF928A58, "Duke Nukem's Penthouse Paradise", DUKE, DUKE15},
	{43448927, NAM_CRC, "NAM", NAM, 0},
	{44365728, 0x3DE1589A, "NAPALM", NAM, 0},
	{77939508, WW2GI_CRC, "WWII GI", WW2GI, 0},
	{37852572, 0xD1ED8C0C, "Platoon Leader", WW2GI, WW2GI_CRC},
	{141174222, RR, "Redneck Rampage", REDNECK, 0},
	{191798609, RRRA, "Redneck Rampage Rides Again", REDNECK, 0},
	{8200353, 0xB291418F, "Blood v1.0", BLOOD, 0},
	{8231173, 0xC3A99936, "Blood v1.1", BLOOD, 0},
	{9570681, BLOOD_OUWB, "Blood: One Unit Whole Blood", BLOOD, 0},
	{9753361, 0xC6DF85ED, "Blood: Cryptic Passage", BLOOD, BLOOD_OUWB},
	{47536148, SW_REG, "Shadow Warrior", SW, 0},
	{47536148, 0xD4A1E153, "Shadow Warrior (Europe)", SW, 0},
	{47536148, 0x3EE68767, "Shadow Warrior (UK)", SW, 0},
	{47537951, 0x1A8776D2, "Shadow Warrior (Censored)", SW, 0},
	{25702245, 0xDAA6BECE, "Shadow Warrior Shareware 1.0", SW, 0},
	{25833456, 0xF21A6B35, "Shadow Warrior Shareware 1.1", SW, 0},
	{26056769, 0x08A7FA1F, "Shadow Warrior Shareware 1.2", SW, 0},
	{26056769, 0x4227F535, "Shadow Warrior Mac Demo", SW, 0},
	{48698128, 0xA9AAA7B7, "Shadow Warrior: Wanton Destruction", SW, SW_REG},
	{48698128, 0x8B6E8011, "Shadow Warrior: Wanton Destruction", SW, SW_REG},
	{9562689, 0x39893EF4, "Shadow Warrior: Wanton Destruction (ProAsm)", SW, SW_REG},
	{12499012, 0xA1A65BE8, "Shadow Warrior: Twin Dragon", SW, SW_REG},
	{6236287, 0xB5B71277, "Shadow Warrior: Twin Dragon", SW, SW_REG},
	{6235578, 0xACC8DCDE, "Shadow Warrior: Twin Dragon", SW, SW_REG},
	{27020745, 0x303CBD89, "PowerSlave", POWERSLAVE, 0},
	{27108170, 0xE3B172F1, "Exhumed", POWERSLAVE, 0},
	{15904838, 0x1D8C7645, "PowerSlave Demo", POWERSLAVE, 0},
	{16481687, 0x1A6E27FA, "Exhumed Demo", POWERSLAVE, 0},
	{0, 0, NULL, 0, 0, false}
};

//Raze's rules for add-ons in an archive, told by the files they hold, and
//a few more for add-ons no port's table knows in the form they come in.
struct ContentRule {
	const char *name;
	int family;
	quint32 dependency;
	const char *files[6];
	//Raze's own rule, which Raze itself applies to such an archive.
	bool raze;
	const char *runsOn;
};

const ContentRule CONTENT_RULES[]={
	{"Duke: Alien World Order", DUKE, DUKE15, {"FIREFLYTROOPER.CON", "FLAMETHROWER.CON", "MUSIC/E5L1_BULLETDAM.OGG", "SOUND/VO_E5L1_DUKE_CREAMANDSUGAR.OGG", NULL}, true, NULL},
	{"Blood (unknown version)", BLOOD, 0, {"HELP1.QAV", "NORMAL.PLU", "INVERSE.CLU", "COSINE.DAT", "BLOOD.PAL", NULL}, true, NULL},
	{"Blood: Cryptic Passage", BLOOD, BLOOD_OUWB, {"CRYPTIC.INI", "CP01.MAP", "CP02.MAP", NULL}, true, NULL},
	{"Blood: Cryptic Passage", BLOOD, BLOOD_OUWB, {"CRYPTIC/CRYPTIC.INI", "CRYPTIC/CP01.MAP", "CRYPTIC/CP02.MAP", NULL}, true, NULL},
	{"Blood: Cryptic Passage", BLOOD, BLOOD_OUWB, {"ADDONS/CRYPTIC PASSAGE/CRYPTIC.INI", "ADDONS/CRYPTIC PASSAGE/CP01.MAP", "ADDONS/CRYPTIC PASSAGE/CP02.MAP", NULL}, true, NULL},
	{"Redneck Rampage: Suckin' Grits on Route 66", REDNECK, RR, {"TILESA66.ART", "TILESB66.ART", "ROUTE66/CARNIVAL.MAP", "ROUTE66/TRUCKSTP.MAP", "GAME66.CON", NULL}, true, NULL},
	{"Duke Nukem's Penthouse Paradise", DUKE, DUKE15, {"PPAKGAME.CON", "PENTHOUS.MAP", NULL}, false, NULL},
	{"Liquidator 3D", CONVERSIONS, 0, {"LEV10.MAP", "LEV11.MAP", "OFFICE.MAP", "TREE.CON", NULL}, false, NULL},
	{NULL, 0, 0, {NULL}, false, NULL}
};

//By file name: total conversions no port's table has, as the Build Engine
//Games Web Showcase tells them, and the files that pick a game for the
//ports that are given a script or an INI rather than a GRP. A size of 0
//takes any size; a more particular rule comes first.
struct NameRule {
	const char *file;
	qint64 size;
	const char *name;
	int family;
	quint32 dependency;
	const char *runsOn;
};

const NameRule NAME_RULES[]={
	{"liquid.grp", 0, "Liquidator 3D", CONVERSIONS, 0, NULL},
	{"liquid.pak", 0, "Liquidator 3D", CONVERSIONS, 0, NULL},
	{"pirate.grp", 0, "Plunder & Pillage", CONVERSIONS, 0, NULL},
	{"quest1.grp", 0, "Quest for Al-Qa'eda", CONVERSIONS, 0, NULL},
	{"quest2.grp", 0, "Quest for Hussein", CONVERSIONS, 0, NULL},
	{"blood.ini", 10870, "Lenin: First Blood", BLOOD, 0, "NBlood"},
	{"blood.ini", 0, "Blood", BLOOD, 0, "NBlood"},
	{"cryptic.ini", 0, "Blood: Cryptic Passage", BLOOD, BLOOD_OUWB, "NBlood"},
	{NULL, 0, NULL, 0, 0, NULL}
};

//A game a .grpinfo file beside the file describes, as Raze and EDuke32
//read such files from the folders they search.
struct Custom {
	qint64 size;
	quint32 crc;
	QString name;
	int family;
	quint32 dependency;
	//The main script it names, when it is not GAME.CON.
	QString script;
};

//The constant names Raze's own table uses, for a .grpinfo that names one.
quint32 NamedCrc(const QString &name, bool *ok)
{
	static const struct { const char *name; quint32 crc; } names[]={
		{"DUKE13_CRC", DUKE13}, {"DUKE15_CRC", DUKE15}, {"DUKEPP_CRC", DUKEPP},
		{"DUKEWT_CRC", DUKEWT}, {"DUKEWORLDTOUR_CRC", DUKEWT}, {"BLOOD_CRC", BLOOD_OUWB},
		{"SWREG12_CRC", SW_REG}, {"RR_CRC", RR}, {"RRRA_CRC", RRRA},
		{"NAM_CRC", NAM_CRC}, {"WW2GI_CRC", WW2GI_CRC}, {NULL, 0}
	};
	for (int i=0; names[i].name; i++) {
		if (name.compare(names[i].name, Qt::CaseInsensitive)==0) {
			*ok=true;
			return names[i].crc;
		}
	}
	*ok=false;
	return 0;
}

//A number as .grpinfo writes them: hex, decimal, negated, or a name.
quint32 GrpinfoNumber(const QString &text, bool *ok)
{
	QString t=text.trimmed();
	bool negative=t.startsWith('-');
	if (negative)
		t=t.mid(1).trimmed();
	quint64 value;
	if (t.startsWith("0x", Qt::CaseInsensitive))
		value=t.mid(2).toULongLong(ok, 16);
	else if (!t.isEmpty()&&t[0].isDigit())
		value=t.toULongLong(ok, 10);
	else
		value=NamedCrc(t, ok);
	if (!*ok)
		return 0;
	return negative?quint32(0u-quint32(value)):quint32(value);
}

int FamilyOfFlags(const QString &flags, quint32 dependency)
{
	QString f=flags.toUpper();
	if (f.contains("BLOOD"))
		return BLOOD;
	if (f.contains("GAMEFLAG_SW")||f.contains("SWWANTON")||f.contains("SWTWINDRAG"))
		return SW;
	if (f.contains("GAMEFLAG_RR")||f.contains("ROUTE66"))
		return REDNECK;
	if (f.contains("GAMEFLAG_NAM")||f.contains("NAPALM"))
		return NAM;
	if (f.contains("WW2GI"))
		return WW2GI;
	if (f.contains("POWERSLAVE")||f.contains("EXHUMED"))
		return POWERSLAVE;
	for (int i=0; GAMES[i].name; i++) {
		if (GAMES[i].crc==dependency)
			return GAMES[i].family;
	}
	return DUKE;
}

QList<Custom> CustomGames(const QString &folder)
{
	QList<Custom> out;
	if (folder.isEmpty())
		return out;

	QDir dir(folder);
	foreach (const QString &entry, dir.entryList(QStringList()<<"*.grpinfo", QDir::Files)) {
		QFile file(dir.filePath(entry));
		if (file.size()>262144||!file.open(QIODevice::ReadOnly))
			continue;
		QString text=QString::fromUtf8(file.readAll());
		text.remove(QRegularExpression("//[^\\n]*"));

		QRegularExpressionMatchIterator blocks=QRegularExpression("(?:grpinfo|addon)\\s*\\{([^}]*)\\}", QRegularExpression::CaseInsensitiveOption).globalMatch(text);
		while (blocks.hasNext()) {
			QString body=blocks.next().captured(1);
			QRegularExpressionMatch name=QRegularExpression("\\bname\\s+\"([^\"]*)\"", QRegularExpression::CaseInsensitiveOption).match(body);
			QRegularExpressionMatch size=QRegularExpression("\\bsize\\s+(\\d+)", QRegularExpression::CaseInsensitiveOption).match(body);
			QRegularExpressionMatch crc=QRegularExpression("\\bcrc\\s+(-?\\s*\\w+)", QRegularExpression::CaseInsensitiveOption).match(body);
			QRegularExpressionMatch dep=QRegularExpression("\\bdependency\\s+(-?\\s*\\w+)", QRegularExpression::CaseInsensitiveOption).match(body);
			QRegularExpressionMatch flags=QRegularExpression("\\bflags\\s+([^\\n]+)", QRegularExpression::CaseInsensitiveOption).match(body);
			QRegularExpressionMatch script=QRegularExpression("\\bscriptname\\s+\"([^\"]*)\"", QRegularExpression::CaseInsensitiveOption).match(body);
			if (!name.hasMatch()||!size.hasMatch()||!crc.hasMatch())
				continue;

			bool ok=false;
			Custom game;
			game.name=name.captured(1).trimmed();
			game.size=size.captured(1).toLongLong();
			game.crc=GrpinfoNumber(crc.captured(1), &ok);
			if (!ok||game.name.isEmpty()||game.size<=0)
				continue;
			game.dependency=dep.hasMatch()?GrpinfoNumber(dep.captured(1), &ok):0;
			if (!ok)
				game.dependency=0;
			game.family=FamilyOfFlags(flags.hasMatch()?flags.captured(1):QString(), game.dependency);
			if (script.hasMatch())
				game.script=script.captured(1).trimmed();
			out<<game;
		}
	}
	return out;
}

//The name of the game a dependency's CRC32 belongs to.
QString DependencyName(quint32 crc, const QList<Custom> &custom)
{
	if (!crc)
		return QString();
	for (int i=0; GAMES[i].name; i++) {
		if (GAMES[i].crc==crc&&!GAMES[i].dependency)
			return GAMES[i].name;
	}
	foreach (const Custom &game, custom) {
		if (game.crc==crc)
			return game.name;
	}
	return QString("a game with CRC32 0x%1").arg(QString::number(crc, 16).toUpper().rightJustified(8, '0'));
}

ZDLBuildGames::Match MakeMatch(const QString &name, int family, quint32 dependency, const QString &how, const char *runsOn, const QList<Custom> &custom)
{
	ZDLBuildGames::Match match;
	match.name=name;
	match.family=FAMILY_NAMES[family];
	match.how=how;
	match.dependsOn=DependencyName(dependency, custom);
	match.runsOn=runsOn?QString(runsOn):QString(RUNS_ON[family]);
	return match;
}

//miniz reads through a QFile, so that any path a QFile opens will do.
size_t ReadQFile(void *opaque, mz_uint64 offset, void *buffer, size_t length)
{
	QFile *file=static_cast<QFile*>(opaque);
	if (!file->seek(qint64(offset)))
		return 0;
	qint64 read=file->read(static_cast<char*>(buffer), qint64(length));
	return read<0?0:size_t(read);
}

QStringList ZipEntries(const QString &path)
{
	QStringList names;
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
		return names;

	char magic[4];
	if (file.read(magic, 4)!=4||memcmp(magic, "PK\x03\x04", 4))
		return names;

	mz_zip_archive zip;
	memset(&zip, 0, sizeof(zip));
	zip.m_pRead=ReadQFile;
	zip.m_pIO_opaque=&file;
	if (!mz_zip_reader_init(&zip, mz_uint64(file.size()), 0))
		return names;

	mz_uint count=mz_zip_reader_get_num_files(&zip);
	for (mz_uint i=0; i<count&&i<200000; i++) {
		char name[1024];
		if (mz_zip_reader_get_filename(&zip, i, name, sizeof(name)))
			names<<QString::fromUtf8(name).replace('\\', '/').toUpper();
	}
	mz_zip_reader_end(&zip);
	return names;
}

//The rule an archive's files answer to, if any.
const ContentRule *ContentRuleFor(const QStringList &entries)
{
	if (entries.isEmpty())
		return NULL;
	for (int i=0; CONTENT_RULES[i].name; i++) {
		bool all=true;
		for (int j=0; CONTENT_RULES[i].files[j]&&all; j++)
			all=entries.contains(QString::fromLatin1(CONTENT_RULES[i].files[j]));
		if (all)
			return &CONTENT_RULES[i];
	}
	return NULL;
}

//A file in a folder by name, whatever its case.
QString FindCaseless(const QString &folder, const QString &name)
{
	QDir dir(folder);
	foreach (const QString &entry, dir.entryList(QDir::Files)) {
		if (entry.compare(name, Qt::CaseInsensitive)==0)
			return dir.filePath(entry);
	}
	return QString();
}

//The games that come as a folder of loose files rather than one archive,
//told by the files the folder holds: for BuildGDX's own games the ones it
//looks for before it starts each, with its name for the game, for its
//-game switch. Witchaven II comes first, since its folder also has most of
//Witchaven's. Liquidator, unpacked from its zip as it often is, runs from
//its folder in EDuke32, which finds its scripts there by itself.
struct FolderRule {
	const char *name;
	int family;
	const char *buildGdx;
	const char *runsOn;
	const char *files[9];
};

const FolderRule FOLDER_RULES[]={
	{"Witchaven II: Blood Vengeance", WITCHAVEN, "WITCHAVEN_2", NULL, {"F_SONGS", "JOESND", "TILES000.ART", "TABLES.DAT", "PALETTE.DAT", "LOOKUP.DAT", "LEVEL1.MAP", NULL}},
	{"Witchaven", WITCHAVEN, "WITCHAVEN", NULL, {"SONGS", "JOESND", "TILES000.ART", "TABLES.DAT", "PALETTE.DAT", "LOOKUP.DAT", "LEVEL1.MAP", NULL}},
	{"William Shatner's TekWar", TEKWAR, "TEKWAR", NULL, {"SMKMM.SMK", "SONGS", "SOUNDS", "TILES000.ART", "TABLES.DAT", "PALETTE.DAT", NULL}},
	{"Legend of the Seven Paladins", PALADINS, "LEGEND_OF_THE_SEVEN_PALADINS", NULL, {"L0ART000.DAT", "L1ART000.DAT", "LMART000.DAT", "PALETTE.DAT", "TABLES.DAT", NULL}},
	{"Liquidator 3D", CONVERSIONS, NULL, NULL, {"LEV10.MAP", "LEV11.MAP", "LEV20.MAP", "OFFICE.MAP", "TREE.CON", "TILES000.ART", "PALETTE.DAT", "TABLES.DAT", NULL}},
	{NULL, 0, NULL, NULL, {NULL}}
};

const FolderRule *FolderGame(const QString &folder)
{
	QSet<QString> names;
	foreach (const QString &entry, QDir(folder).entryList(QDir::Files))
		names.insert(entry.toUpper());
	for (int i=0; FOLDER_RULES[i].name; i++) {
		bool all=true;
		for (int j=0; FOLDER_RULES[i].files[j]&&all; j++)
			all=names.contains(QString::fromLatin1(FOLDER_RULES[i].files[j]));
		if (all)
			return &FOLDER_RULES[i];
	}
	return NULL;
}

//Add-ons that come as loose files in their base game's folder, as the GOG
//and Steam releases ship Route 66: told by their script with the files
//that have to be beside it, and set up by Raze from that folder with a
//switch of its own, which also loads the extra art.
struct LooseAddon {
	const char *script;
	const char *name;
	int family;
	quint32 dependency;
	const char *files[6];
	const char *razeSwitch;
	const char *runsOn;
};

const LooseAddon LOOSE_ADDONS[]={
	{"GAME66.CON", "Redneck Rampage: Suckin' Grits on Route 66", REDNECK, RR, {"TILESA66.ART", "TILESB66.ART", NULL}, "-route66", "Raze or BuildGDX"},
	{"CRYPTIC.INI", "Blood: Cryptic Passage", BLOOD, BLOOD_OUWB, {"CPART07.AR_", "CPART15.AR_", NULL}, "-cryptic", "NBlood, Raze or BuildGDX"},
	{NULL, NULL, 0, 0, {NULL}, NULL, NULL}
};

const LooseAddon *LooseAddonFor(const QString &file)
{
	QFileInfo fi(file);
	if (!fi.isFile())
		return NULL;
	for (int i=0; LOOSE_ADDONS[i].script; i++) {
		if (fi.fileName().compare(QString::fromLatin1(LOOSE_ADDONS[i].script), Qt::CaseInsensitive)!=0)
			continue;
		bool all=true;
		for (int j=0; LOOSE_ADDONS[i].files[j]&&all; j++)
			all=!FindCaseless(fi.absolutePath(), QString::fromLatin1(LOOSE_ADDONS[i].files[j])).isEmpty();
		if (all)
			return &LOOSE_ADDONS[i];
	}
	return NULL;
}

//The games of the other ports BuildGDX starts, by the main file it looks
//for in the game's folder; Rides Again shares Redneck Rampage's file name
//and is told by its size.
struct GdxMain {
	const char *file;
	qint64 size;
	const char *game;
};

const GdxMain GDX_MAIN[]={
	{"DUKE3D.GRP", 0, "DUKE_NUKEM_3D"},
	{"NAM.GRP", 0, "NAM"},
	{"NAPALM.GRP", 0, "NAM"},
	{"REDNECK.GRP", 191798609, "RR_RIDES_AGAIN"},
	{"REDNECK.GRP", 0, "REDNECK_RAMPAGE"},
	{"BLOOD.RFF", 0, "BLOOD"},
	{"SW.GRP", 0, "SHADOW_WARRIOR"},
	{"STUFF.DAT", 0, "POWERSLAVE"},
	{NULL, 0, NULL}
};

//The main scripts of the add-ons EDuke32's table names, which a copy it
//does not know by its checksum still holds.
const char *const ADDON_SCRIPTS[]={"PPAKGAME.CON", "NWINTER.CON", "DZ-GAME.CON", "VACATION.CON", "DUKEDC.CON", "PLATOONL.DEF", "GAME66.CON", NULL};

//The ports a launch is worked out for.
enum PortKind { PORT_EDUKE32, PORT_REDNUKEM, PORT_RAZE };

//Whether the port picks a game of the table by its checksum: EDuke32 the
//Duke games its own table has, Rednukem the Redneck ones, Raze all of the
//table, which is its own.
bool PortKnows(const Game &game, int port)
{
	if (port==PORT_RAZE)
		return true;
	if (port==PORT_REDNUKEM)
		return game.family==REDNECK;
	return (game.family==DUKE||game.family==NAM||game.family==WW2GI)&&!game.razeOnly;
}

//Which base game to load an add-on over, when the add-on does not say:
//Atomic first, as the add-ons of the time were made for it.
int BaseRank(quint32 crc, int family, int port)
{
	if (port==PORT_REDNUKEM)
		return crc==RR?0:crc==RRRA?1:5;
	static const quint32 order[]={DUKE15, DUKEWT, DUKEPP, DUKE13};
	for (int i=0; i<4; i++) {
		if (crc==order[i])
			return i;
	}
	if (family==DUKE)
		return 4;
	if (family==NAM)
		return 5;
	if (family==WW2GI)
		return 6;
	return family==REDNECK?7:9;
}

//One file out of a GRP or a zip, by its name as the archive lists it;
//empty when it is not there or is larger than the limit.
QByteArray EntryData(const QString &path, const QString &entryName, qint64 limit)
{
	QFile file(path);
	char magic[12];
	if (!file.open(QIODevice::ReadOnly)||file.read(magic, 12)!=12)
		return QByteArray();

	if (!memcmp(magic, "KenSilverman", 12)) {
		quint32 count=0;
		if (file.read(reinterpret_cast<char*>(&count), 4)!=4||count>200000)
			return QByteArray();
		QByteArray directory=file.read(qint64(count)*16);
		qint64 offset=16+qint64(count)*16;
		for (quint32 i=0; i<count&&qint64(i)*16+16<=directory.size(); i++) {
			const char *entry=directory.constData()+i*16;
			quint32 size;
			memcpy(&size, entry+12, 4);
			if (QString::fromLatin1(entry, int(qstrnlen(entry, 12))).compare(entryName, Qt::CaseInsensitive)==0) {
				if (qint64(size)>limit||!file.seek(offset))
					return QByteArray();
				return file.read(size);
			}
			offset+=size;
		}
		return QByteArray();
	}

	if (memcmp(magic, "PK\x03\x04", 4))
		return QByteArray();
	mz_zip_archive zip;
	memset(&zip, 0, sizeof(zip));
	zip.m_pRead=ReadQFile;
	zip.m_pIO_opaque=&file;
	if (!mz_zip_reader_init(&zip, mz_uint64(file.size()), 0))
		return QByteArray();

	QByteArray out;
	mz_uint count=mz_zip_reader_get_num_files(&zip);
	for (mz_uint i=0; i<count&&i<200000; i++) {
		char name[1024];
		if (!mz_zip_reader_get_filename(&zip, i, name, sizeof(name))||QString::fromUtf8(name).replace('\\', '/').compare(entryName, Qt::CaseInsensitive)!=0)
			continue;
		mz_zip_archive_file_stat stat;
		if (mz_zip_reader_file_stat(&zip, i, &stat)&&qint64(stat.m_uncomp_size)<=limit) {
			size_t size=0;
			void *data=mz_zip_reader_extract_to_heap(&zip, i, &size, 0);
			if (data) {
				out=QByteArray(static_cast<const char*>(data), int(size));
				mz_free(data);
			}
		}
		break;
	}
	mz_zip_reader_end(&zip);
	return out;
}

//An episode name in capitals, as the scripts write them, made readable:
//"PLUG AND PRAY" becomes "Plug and Pray".
QString TitleCase(const QString &text)
{
	static const char *const small[]={"a", "an", "and", "at", "for", "in", "of", "on", "or", "the", "to", NULL};
	QStringList words=text.toLower().split(' ', Qt::SkipEmptyParts);
	for (int i=0; i<words.size(); i++) {
		bool keep=false;
		for (int j=0; small[j]&&i>0&&!keep; j++)
			keep=words[i]==QString::fromLatin1(small[j]);
		if (!keep)
			words[i][0]=words[i][0].toUpper();
	}
	return words.join(' ');
}

//A game file as the port sees it.
struct PortGame {
	bool valid;
	QString path;
	quint32 crc;
	quint32 dependency;
	int family;
	int rank;
	//From a .grpinfo: the main script it names.
	QString script;
};

//Which game a file is to the port, by size and CRC32 against its table
//and the .grpinfo files beside it; only base games when basesOnly. The
//CRC32 is taken only for a size that could match, and once per file.
PortGame IdentifyForPort(const QString &path, int port, bool basesOnly, QHash<QString, quint32> &crcs)
{
	PortGame game;
	game.valid=false;
	game.path=path;
	game.crc=0;
	game.dependency=0;
	game.family=-1;
	game.rank=99;

	QFileInfo fi(path);
	if (!fi.isFile())
		return game;
	qint64 size=fi.size();
	QList<Custom> custom=CustomGames(fi.absolutePath());

	bool sized=false;
	for (int i=0; GAMES[i].name&&!sized; i++)
		sized=GAMES[i].size==size&&PortKnows(GAMES[i], port)&&(!basesOnly||!GAMES[i].dependency);
	foreach (const Custom &entry, custom)
		sized=sized||(entry.size==size&&(!basesOnly||!entry.dependency));
	if (!sized)
		return game;

	QString key=fi.absoluteFilePath();
	quint32 crc;
	if (crcs.contains(key)) {
		crc=crcs.value(key);
	} else {
		bool ok=false;
		crc=ZDLBuildGames::crcOf(key, &ok);
		if (!ok)
			return game;
		crcs.insert(key, crc);
	}

	for (int i=0; GAMES[i].name; i++) {
		const Game &entry=GAMES[i];
		if (entry.size==size&&entry.crc==crc&&PortKnows(entry, port)&&(!basesOnly||!entry.dependency)) {
			game.valid=true;
			game.crc=crc;
			game.dependency=entry.dependency;
			game.family=entry.family;
			game.rank=BaseRank(crc, entry.family, port);
			return game;
		}
	}
	foreach (const Custom &entry, custom) {
		if (entry.size==size&&entry.crc==crc&&(!basesOnly||!entry.dependency)) {
			game.valid=true;
			game.crc=crc;
			game.dependency=entry.dependency;
			game.family=entry.family;
			game.rank=BaseRank(crc, entry.family, port)+10;
			game.script=entry.script;
			return game;
		}
	}
	return game;
}

//Duke's own episodes, which an add-on's scripts often list before its own.
bool IsDukeEpisode(const QString &title)
{
	static const char *const own[]={"L.A. MELTDOWN", "LUNAR APOCALYPSE", "SHRAPNEL CITY", "THE BIRTH", "ALIEN WORLD ORDER", NULL};
	for (int i=0; own[i]; i++) {
		if (title.compare(QString::fromLatin1(own[i]), Qt::CaseInsensitive)==0)
			return true;
	}
	return false;
}

//The episodes a script names with definevolumename, by their number; the
//first naming of a number counts.
void ReadEpisodes(const QByteArray &text, QMap<int, QString> &episodes)
{
	static const QRegularExpression volume("^\\s*definevolumename\\s+(\\d+)\\s+(.+)$", QRegularExpression::CaseInsensitiveOption);
	foreach (QByteArray line, text.split('\n')) {
		int comment=line.indexOf("//");
		if (comment>=0)
			line.truncate(comment);
		QRegularExpressionMatch m=volume.match(QString::fromLatin1(line));
		if (!m.hasMatch())
			continue;
		QString title=m.captured(2).simplified();
		int number=m.captured(1).toInt();
		if (!title.isEmpty()&&!episodes.contains(number))
			episodes.insert(number, title);
	}
}

//A name for an add-on or a mod by the episodes its scripts name: its first
//episode, when that is its own; the name it came with when it keeps
//Duke's episodes first and adds its own after them. Empty without an
//episode of its own.
QString TitleFromEpisodes(const QMap<int, QString> &episodes, const QString &fallback)
{
	bool own=false;
	foreach (const QString &title, episodes)
		own=own||!IsDukeEpisode(title);
	if (!own)
		return QString();
	return IsDukeEpisode(episodes.first())?fallback:TitleCase(episodes.first());
}

//Where a script comes in the reading for episodes: USER.CON at the top,
//the other scripts at the top, USER.CON further down, the rest.
int ScriptOrder(const QString &name)
{
	bool top=!name.contains('/');
	bool user=QFileInfo(name).fileName().compare("USER.CON", Qt::CaseInsensitive)==0;
	return top?(user?0:1):(user?2:3);
}

bool SameFolder(const QString &a, const QString &b)
{
#if defined(Q_OS_WIN)
	return QDir::cleanPath(a).compare(QDir::cleanPath(b), Qt::CaseInsensitive)==0;
#else
	return QDir::cleanPath(a)==QDir::cleanPath(b);
#endif
}

}

bool ZDLBuildGames::isCandidate(const QString &ext)
{
	QString e=ext.toLower();
	return e=="grp"||e=="rff"||e=="dat"||e=="ssi";
}

bool ZDLBuildGames::sizeKnown(qint64 size, const QString &folder)
{
	for (int i=0; GAMES[i].name; i++) {
		if (GAMES[i].size==size)
			return true;
	}
	foreach (const Custom &game, CustomGames(folder)) {
		if (game.size==size)
			return true;
	}
	return false;
}

ZDLBuildGames::Match ZDLBuildGames::bySizeCrc(qint64 size, quint32 crc, const QString &folder)
{
	QList<Custom> custom=CustomGames(folder);
	for (int i=0; GAMES[i].name; i++) {
		if (GAMES[i].size==size&&GAMES[i].crc==crc)
			return MakeMatch(GAMES[i].name, GAMES[i].family, GAMES[i].dependency, "its size and CRC32, as Raze and EDuke32 tell games apart", NULL, custom);
	}
	foreach (const Custom &game, custom) {
		if (game.size==size&&game.crc==crc)
			return MakeMatch(game.name, game.family, game.dependency, "its size and CRC32, by a .grpinfo file beside it", NULL, custom);
	}
	return Match();
}

ZDLBuildGames::Match ZDLBuildGames::byContents(const QString &file)
{
	const ContentRule *rule=ContentRuleFor(ZipEntries(file));
	if (!rule)
		return Match();
	return MakeMatch(rule->name, rule->family, rule->dependency, rule->raze?"the files it holds, by Raze's rules":"the files it holds", rule->runsOn, QList<Custom>());
}

ZDLBuildGames::Match ZDLBuildGames::byName(const QString &file)
{
	QFileInfo fi(file);
	QString name=fi.fileName().toLower();
	for (int i=0; NAME_RULES[i].file; i++) {
		const NameRule &rule=NAME_RULES[i];
		if (name==rule.file&&(!rule.size||fi.size()==rule.size))
			return MakeMatch(rule.name, rule.family, rule.dependency, rule.size?"its file name and size":"its file name", rule.runsOn, QList<Custom>());
	}
	return Match();
}

bool ZDLBuildGames::isBuildGameFile(const QString &path)
{
	static QHash<QString, bool> known;
	QString key=QDir::cleanPath(path);
	QHash<QString, bool>::const_iterator found=known.constFind(key);
	if (found!=known.constEnd())
		return found.value();

	QFileInfo fi(path);
	QString ext=fi.suffix().toLower();
	bool build;
	if (fi.isDir())
		build=true;
	else if (ext=="wad"||ext=="iwad"||ext=="ipk3"||ext=="ipk7"||ext=="pk3"||ext=="pk7"||ext=="pke"||ext=="pkz"||ext=="p7z"||ext=="7z"||ext=="deh"||ext=="bex"||ext=="lmp")
		build=false;
	else if (ext=="zip")
		build=byContents(path).valid()||byScripts(path).valid();
	else
		build=true;
	known.insert(key, build);
	return build;
}

bool ZDLBuildGames::isFamily(const QString &family)
{
	return familyOrder(family)<99;
}

int ZDLBuildGames::familyOrder(const QString &family)
{
	for (int i=0; i<FAMILY_COUNT; i++) {
		if (family==FAMILY_NAMES[i])
			return 10+i;
	}
	return 99;
}

quint32 ZDLBuildGames::crcOf(const QString &file, bool *ok)
{
	if (ok)
		*ok=false;
	QFile f(file);
	if (!f.open(QIODevice::ReadOnly))
		return 0;

	mz_ulong crc=MZ_CRC32_INIT;
	QByteArray buffer(1<<20, 0);
	qint64 length;
	while ((length=f.read(buffer.data(), buffer.size()))>0)
		crc=mz_crc32(crc, reinterpret_cast<const unsigned char*>(buffer.constData()), size_t(length));
	if (ok)
		*ok=f.atEnd();
	return quint32(crc);
}

QList<QPair<QString, qint64> > ZDLBuildGames::grpEntries(const QString &file)
{
	QList<QPair<QString, qint64> > entries;
	QFile f(file);
	if (!f.open(QIODevice::ReadOnly))
		return entries;

	char magic[12];
	quint32 count=0;
	if (f.read(magic, 12)!=12||memcmp(magic, "KenSilverman", 12)||f.read(reinterpret_cast<char*>(&count), 4)!=4)
		return entries;
	if (count>200000)
		return entries;

	QByteArray directory=f.read(qint64(count)*16);
	for (quint32 i=0; i<count&&qint64(i)*16+16<=directory.size(); i++) {
		const char *entry=directory.constData()+i*16;
		quint32 size;
		memcpy(&size, entry+12, 4);
		entries<<qMakePair(QString::fromLatin1(entry, int(qstrnlen(entry, 12))), qint64(size));
	}
	return entries;
}

QStringList ZDLBuildGames::entryNames(const QString &file)
{
	QStringList names=ZipEntries(file);
	if (names.isEmpty()) {
		QList<QPair<QString, qint64> > entries=grpEntries(file);
		for (int i=0; i<entries.size(); i++)
			names<<entries[i].first.toUpper();
	}
	return names;
}

ZDLBuildGames::Match ZDLBuildGames::byFolder(const QString &path)
{
	QFileInfo fi(path);
	bool dir=fi.isDir();
	const FolderRule *rule=FolderGame(dir?fi.absoluteFilePath():fi.absolutePath());
	if (!rule)
		return Match();

	//A file speaks for the folder when it is one of the game's own: one the
	//rule names, or its tiles, data, maps, scripts or definitions.
	if (!dir) {
		QString name=fi.fileName().toUpper();
		QString ext=fi.suffix().toUpper();
		bool own=ext=="ART"||ext=="DAT"||ext=="MAP"||ext=="CON"||ext=="DEF";
		for (int j=0; rule->files[j]&&!own; j++)
			own=name==QString::fromLatin1(rule->files[j]);
		if (!own)
			return Match();
	}
	QString how=rule->buildGdx?"the files %1, as BuildGDX tells the game":"the files %1";
	return MakeMatch(rule->name, rule->family, 0, how.arg(dir?"it holds":"in its folder"), rule->runsOn, QList<Custom>());
}

QString ZDLBuildGames::folderGameFile(const QString &path)
{
	QFileInfo fi(path);
	QString folder=fi.isDir()?fi.absoluteFilePath():fi.absolutePath();
	if (!FolderGame(folder))
		return QString();
	QString tables=FindCaseless(folder, "TABLES.DAT");
	return tables.isEmpty()?fi.absoluteFilePath():QFileInfo(tables).absoluteFilePath();
}

QString ZDLBuildGames::buildGdxGame(const QString &path)
{
	QFileInfo fi(path);
	QString folder=fi.isDir()?fi.absoluteFilePath():fi.absolutePath();
	const FolderRule *rule=FolderGame(folder);
	if (rule&&rule->buildGdx)
		return QString::fromLatin1(rule->buildGdx);

	//The chosen file first, when it is a game's main file, so that NAM.GRP
	//beside DUKE3D.GRP starts NAM; then whatever main file the folder has.
	QString chosen=fi.isDir()?QString():fi.fileName();
	for (int pass=0; pass<2; pass++) {
		for (int i=0; GDX_MAIN[i].file; i++) {
			QString name=QString::fromLatin1(GDX_MAIN[i].file);
			QString candidate;
			if (pass==0)
				candidate=chosen.compare(name, Qt::CaseInsensitive)==0?fi.absoluteFilePath():QString();
			else
				candidate=FindCaseless(folder, name);
			if (candidate.isEmpty())
				continue;
			if (GDX_MAIN[i].size&&QFileInfo(candidate).size()!=GDX_MAIN[i].size)
				continue;
			return QString::fromLatin1(GDX_MAIN[i].game);
		}
	}
	return QString();
}

//The base games at hand for a file: in the folder it is in, and elsewhere
//in the library, the files of the IWAD list.
static void BasesAround(const QString &folder, const QString &self, const QStringList &library, int port, QHash<QString, quint32> &crcs, QList<PortGame> &here, QList<PortGame> &elsewhere)
{
	foreach (const QFileInfo &entry, QDir(folder).entryInfoList(QDir::Files)) {
		if (SameFolder(entry.absoluteFilePath(), self))
			continue;
		PortGame game=IdentifyForPort(entry.absoluteFilePath(), port, true, crcs);
		if (game.valid)
			here<<game;
	}
	foreach (const QString &path, library) {
		QFileInfo entry(path);
		if (!entry.isFile()||SameFolder(entry.absolutePath(), folder))
			continue;
		PortGame game=IdentifyForPort(entry.absoluteFilePath(), port, true, crcs);
		if (game.valid)
			elsewhere<<game;
	}
}

//The base game for a script picked as the game, a mod's above all: beside
//it, in the two folders above, or elsewhere in the library, nearest first
//and by rank within each, Atomic first for EDuke32 and Raze, Redneck
//Rampage for Rednukem. Empty when there is none.
static QString BaseFor(const QString &file, const QStringList &library, int port)
{
	QFileInfo fi(file);
	QHash<QString, quint32> crcs;
	QList<PortGame> here, elsewhere, above;
	BasesAround(fi.absolutePath(), fi.absoluteFilePath(), library, port, crcs, here, elsewhere);
	QDir dir(fi.absolutePath());
	for (int level=0; level<2&&dir.cdUp(); level++) {
		foreach (const QFileInfo &entry, dir.entryInfoList(QDir::Files)) {
			PortGame game=IdentifyForPort(entry.absoluteFilePath(), port, true, crcs);
			if (game.valid)
				above<<game;
		}
	}
	for (int pass=0; pass<3; pass++) {
		const QList<PortGame> &list=pass==0?here:pass==1?above:elsewhere;
		const PortGame *best=NULL;
		for (int i=0; i<list.size(); i++) {
			if (list[i].rank<7&&(!best||list[i].rank<best->rank))
				best=&list[i];
		}
		if (best)
			return best->path;
	}
	return QString();
}

//The add-on's main script, where the port would not find it by itself: the
//one its .grpinfo names, or one of the known add-ons' among its files.
static QString AddonScript(const QString &file, const QString &named, bool raze)
{
	if (!named.isEmpty())
		return named;
	QFileInfo fi(file);
	QStringList names;
	if (fi.isDir()) {
		foreach (const QString &entry, QDir(file).entryList(QDir::Files))
			names<<entry.toUpper();
	} else {
		names=ZDLBuildGames::entryNames(file);
	}
	//Raze's own script first, which an add-on made for every port carries
	//beside the others.
	if (raze&&names.contains("RAZE.CON"))
		return "RAZE.CON";
	for (int i=0; ADDON_SCRIPTS[i]; i++) {
		if (names.contains(QString::fromLatin1(ADDON_SCRIPTS[i])))
			return QString::fromLatin1(ADDON_SCRIPTS[i]);
	}
	return QString();
}

ZDLBuildGames::DukeLaunch ZDLBuildGames::forDukePorts(const QString &file, const QStringList &library, bool redneck)
{
	DukeLaunch launch;
	QFileInfo fi(file);
	QString folder=fi.absolutePath();
	int port=redneck?PORT_REDNUKEM:PORT_EDUKE32;
	QHash<QString, quint32> crcs;
	PortGame self=IdentifyForPort(fi.absoluteFilePath(), port, false, crcs);
	QList<PortGame> here, elsewhere;
	BasesAround(folder, fi.absoluteFilePath(), library, port, crcs, here, elsewhere);

	//A game the port knows is picked by its own name, once the port can
	//find the base game an add-on needs: beside it, or in the folder of
	//the library's copy, which is then searched too.
	const PortGame *base=NULL;
	if (self.valid&&self.dependency) {
		for (int i=0; i<here.size()&&!base; i++)
			base=here[i].crc==self.dependency?&here[i]:NULL;
		for (int i=0; i<elsewhere.size()&&!base; i++)
			base=elsewhere[i].crc==self.dependency?&elsewhere[i]:NULL;
	}
	if (self.valid&&(!self.dependency||base)) {
		launch.gameGrp=fi.fileName();
		if (base&&!SameFolder(QFileInfo(base->path).absolutePath(), folder))
			launch.folders<<QFileInfo(base->path).absolutePath();
		return launch;
	}

	//Any other file the port does not pick when it finds a game it knows:
	//it starts that game instead. So the file is loaded with -g over a base
	//game picked by name, the one beside it first. A total conversion,
	//which needs none, only takes the one beside it, to keep the port from
	//starting a game of its own choosing; with none, the file is named as
	//the game as well, which the port takes when it finds no other.
	Match named=byName(file);
	bool conversion=named.valid()&&named.family==QString::fromLatin1(FAMILY_NAMES[CONVERSIONS]);
	for (int i=0; i<here.size(); i++) {
		if (!base||here[i].rank<base->rank)
			base=&here[i];
	}
	for (int i=0; i<elsewhere.size()&&!conversion&&!here.size(); i++) {
		if (!base||elsewhere[i].rank<base->rank)
			base=&elsewhere[i];
	}
	if (base) {
		launch.gameGrp=QFileInfo(base->path).fileName();
		if (!SameFolder(QFileInfo(base->path).absolutePath(), folder))
			launch.folders<<QFileInfo(base->path).absolutePath();
	} else {
		launch.gameGrp=fi.fileName();
	}
	launch.overlay=fi.absoluteFilePath();
	launch.script=AddonScript(file, self.script, false);
	return launch;
}

namespace {
QString BaseNear(const QString &file, const QStringList &library, quint32 dependency, int family);
}

QString ZDLBuildGames::looseAddonBase(const QString &file, const QStringList &library)
{
	const LooseAddon *addon=LooseAddonFor(file);
	if (!addon)
		return QString();
	return BaseNear(file, library, addon->dependency, addon->family);
}

QString ZDLBuildGames::bloodAddonBase(const QString &file, const QStringList &library)
{
	if (!byBloodIni(file).valid())
		return QString();
	return BaseNear(file, library, BLOOD_OUWB, BLOOD);
}

ZDLBuildGames::Match ZDLBuildGames::byBloodIni(const QString &file)
{
	//Blood's own INI is Blood itself, and Cryptic Passage's is told by the
	//art beside it; any other with a first episode is an add-on, as NBlood
	//takes the INIs it offers.
	QFileInfo fi(file);
	if (!fi.isFile()||fi.suffix().compare("ini", Qt::CaseInsensitive)!=0||fi.fileName().compare("BLOOD.INI", Qt::CaseInsensitive)==0||fi.size()>4*1024*1024)
		return Match();
	QFile ini(file);
	if (!ini.open(QIODevice::ReadOnly))
		return Match();
	QString text=QString::fromLatin1(ini.readAll());
	QRegularExpressionMatch episode=QRegularExpression("^\\s*\\[Episode1\\]\\s*$((?:(?!^\\s*\\[)[\\s\\S])*)", QRegularExpression::MultilineOption|QRegularExpression::CaseInsensitiveOption).match(text);
	if (!episode.hasMatch())
		return Match();

	//Named after its folder when it has one of its own, as add-ons are
	//shipped; in Blood's own folder, after its first episode.
	QString title=fi.absoluteDir().dirName();
	if (!FindCaseless(fi.absolutePath(), "BLOOD.RFF").isEmpty()||title.isEmpty()) {
		QRegularExpressionMatch named=QRegularExpression("^\\s*Title\\s*=\\s*(.*?)\\s*$", QRegularExpression::MultilineOption|QRegularExpression::CaseInsensitiveOption).match(episode.captured(1));
		title=named.hasMatch()&&!named.captured(1).isEmpty()?named.captured(1):fi.completeBaseName();
	}
	return MakeMatch("Blood add-on: "+title, BLOOD, BLOOD_OUWB, "its INI, which defines episodes of its own", "NBlood or BuildGDX", QList<Custom>());
}

QString ZDLBuildGames::bloodModIni(const QString &folder)
{
	QString found;
	foreach (const QFileInfo &entry, QDir(folder).entryInfoList(QDir::Files)) {
		if (entry.suffix().compare("ini", Qt::CaseInsensitive)!=0)
			continue;
		if (!byBloodIni(entry.absoluteFilePath()).valid()&&!byLooseFiles(entry.absoluteFilePath()).valid())
			continue;
		//More than one: which to play is the user's to say.
		if (!found.isEmpty())
			return QString();
		found=entry.absoluteFilePath();
	}
	return found;
}

namespace {

//The base game of an add-on: the game it was made for, then any game of
//its family; beside it first each time, then in the two folders above it,
//then elsewhere in the library.
QString BaseNear(const QString &file, const QStringList &library, quint32 dependency, int family)
{
	QFileInfo fi(file);
	QHash<QString, quint32> crcs;
	QList<PortGame> here, elsewhere;
	BasesAround(fi.absolutePath(), fi.absoluteFilePath(), library, PORT_RAZE, crcs, here, elsewhere);

	//An add-on folder inside its game's folder, as Fresh Supply keeps
	//Cryptic Passage in addons\Cryptic Passage: the two folders above it,
	//nearest first, before the rest of the library.
	QList<PortGame> above;
	QDir dir(fi.absolutePath());
	for (int level=0; level<2&&dir.cdUp(); level++) {
		foreach (const QFileInfo &entry, dir.entryInfoList(QDir::Files)) {
			PortGame game=IdentifyForPort(entry.absoluteFilePath(), PORT_RAZE, true, crcs);
			if (game.valid)
				above<<game;
		}
	}
	above<<elsewhere;

	//The game it was made for, then any game of its family: beside it
	//first each time.
	for (int pass=0; pass<4; pass++) {
		const QList<PortGame> &list=pass%2==0?here:above;
		for (int i=0; i<list.size(); i++) {
			if (pass<2?list[i].crc==dependency:list[i].family==family)
				return list[i].path;
		}
	}
	return QString();
}

}

ZDLBuildGames::Match ZDLBuildGames::byLooseFiles(const QString &file)
{
	const LooseAddon *addon=LooseAddonFor(file);
	if (!addon)
		return Match();
	return MakeMatch(addon->name, addon->family, addon->dependency, "its script with the add-on's files beside it", addon->runsOn, QList<Custom>());
}

ZDLBuildGames::RazeLaunch ZDLBuildGames::forRaze(const QString &file, const QStringList &library)
{
	RazeLaunch launch;
	launch.iwad=file;
	QFileInfo fi(file);

	//An add-on of loose files with its base game, Route 66 or Cryptic
	//Passage: Raze's own switch for it, over the base game. Raze reads loose
	//files from the folder of the game it is given, so an add-on kept apart
	//from its base game goes in by -file; with no base game found, Raze's
	//switch names the base game's file for it to look up itself.
	if (const LooseAddon *addon=LooseAddonFor(file)) {
		QString base=looseAddonBase(file, library);
		launch.iwad=base;
		if (!base.isEmpty()&&!SameFolder(QFileInfo(base).absolutePath(), fi.absolutePath()))
			launch.file=fi.absolutePath();
		launch.switches<<QString::fromLatin1(addon->razeSwitch);
		return launch;
	}

	//What is to be loaded over a base game: a folder that is a total
	//conversion, Liquidator unpacked; a conversion Raze's table does not
	//have; an add-on in a form Raze does not know, told by what it holds or
	//by the episodes its scripts name. Anything else Raze picks by itself,
	//or is no Duke game at all, and goes as it is.
	QString overlay;
	Match folderGame=byFolder(file);
	if (folderGame.valid()) {
		if (folderGame.family!=QString::fromLatin1(FAMILY_NAMES[CONVERSIONS]))
			return launch;
		overlay=fi.isDir()?fi.absoluteFilePath():fi.absolutePath();
	} else if (byModScripts(file).valid()) {
		//A mod of loose files picked by its main script: its folder over a
		//Duke game, and that script as the main one.
		QString base=BaseFor(file, library, PORT_RAZE);
		if (base.isEmpty())
			return launch;
		launch.iwad=base;
		launch.file=fi.absolutePath();
		launch.script=fi.fileName();
		return launch;
	} else if (fi.isFile()) {
		QHash<QString, quint32> crcs;
		if (IdentifyForPort(fi.absoluteFilePath(), PORT_RAZE, false, crcs).valid)
			return launch;
		Match named=byName(file);
		const ContentRule *rule=ContentRuleFor(ZipEntries(file));
		if (named.valid()&&named.family==QString::fromLatin1(FAMILY_NAMES[CONVERSIONS]))
			overlay=fi.absoluteFilePath();
		else if (rule&&!rule->raze)
			overlay=fi.absoluteFilePath();
		else if (!rule&&byScripts(file).valid())
			overlay=fi.absoluteFilePath();
	}
	if (overlay.isEmpty())
		return launch;

	//Raze always needs a game of its own to load these over: one of the Duke
	//games, Atomic first, the one beside the file before one elsewhere in
	//the library.
	QHash<QString, quint32> crcs;
	QList<PortGame> here, elsewhere;
	QString folder=fi.isDir()?QFileInfo(overlay).absolutePath():fi.absolutePath();
	BasesAround(folder, fi.absoluteFilePath(), library, PORT_RAZE, crcs, here, elsewhere);
	const PortGame *base=NULL;
	for (int pass=0; pass<2&&!base; pass++) {
		const QList<PortGame> &list=pass==0?here:elsewhere;
		for (int i=0; i<list.size(); i++) {
			if (list[i].rank<7&&(!base||list[i].rank<base->rank))
				base=&list[i];
		}
	}
	if (!base)
		return launch;

	launch.iwad=base->path;
	launch.file=overlay;
	launch.script=AddonScript(overlay, QString(), true);
	return launch;
}

ZDLBuildGames::Match ZDLBuildGames::byScripts(const QString &file)
{
	//A playable add-on has maps, and scripts that name its episodes.
	QStringList names=entryNames(file);
	bool maps=false;
	QStringList scripts;
	foreach (const QString &name, names) {
		QString ext=QFileInfo(name).suffix().toUpper();
		if (ext=="MAP")
			maps=true;
		else if (ext=="CON")
			scripts<<name;
	}
	if (!maps||scripts.isEmpty())
		return Match();

	//USER.CON and the scripts at the top first, where the episodes are
	//named; an add-on can carry dozens more of its own below them.
	std::stable_sort(scripts.begin(), scripts.end(), [](const QString &a, const QString &b) {
		return ScriptOrder(a)<ScriptOrder(b);
	});
	QMap<int, QString> episodes;
	for (int i=0; i<scripts.size()&&i<24; i++)
		ReadEpisodes(EntryData(file, scripts[i], 1024*1024), episodes);
	QString title=TitleFromEpisodes(episodes, QFileInfo(file).completeBaseName().replace('_', ' '));
	if (title.isEmpty())
		return Match();
	return MakeMatch("Duke Nukem 3D add-on: "+title, DUKE, DUKE15, "the episodes of its own its scripts define", NULL, QList<Custom>());
}

ZDLBuildGames::Match ZDLBuildGames::byModScripts(const QString &file)
{
	//The main script of the folder: EDUKE.CON, which EDuke32 takes first,
	//or GAME.CON where there is none.
	QFileInfo fi(file);
	if (!fi.isFile())
		return Match();
	QString folder=fi.absolutePath();
	QString name=fi.fileName().toUpper();
	bool eduke=!FindCaseless(folder, "EDUKE.CON").isEmpty();
	if (!(name=="EDUKE.CON"||(name=="GAME.CON"&&!eduke)))
		return Match();

	//Maps of its own, in the folder or a folder just below it.
	QDir dir(folder);
	bool maps=false;
	foreach (const QFileInfo &entry, dir.entryInfoList(QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot)) {
		if (entry.isDir()) {
			foreach (const QFileInfo &inner, QDir(entry.absoluteFilePath()).entryInfoList(QDir::Files))
				maps=maps||inner.suffix().compare("map", Qt::CaseInsensitive)==0;
		} else {
			maps=maps||entry.suffix().compare("map", Qt::CaseInsensitive)==0;
		}
		if (maps)
			break;
	}
	if (!maps)
		return Match();

	//The episodes its scripts name, USER.CON first.
	QStringList scripts;
	foreach (const QString &entry, dir.entryList(QDir::Files)) {
		if (QFileInfo(entry).suffix().compare("con", Qt::CaseInsensitive)==0)
			scripts<<entry.toUpper();
	}
	std::stable_sort(scripts.begin(), scripts.end(), [](const QString &a, const QString &b) {
		return ScriptOrder(a)<ScriptOrder(b);
	});
	QMap<int, QString> episodes;
	for (int i=0; i<scripts.size()&&i<24; i++) {
		QFile script(FindCaseless(folder, scripts[i]));
		if (script.size()<=1024*1024&&script.open(QIODevice::ReadOnly))
			ReadEpisodes(script.readAll(), episodes);
	}
	QString title=TitleFromEpisodes(episodes, dir.dirName());
	if (title.isEmpty())
		return Match();
	return MakeMatch("Duke Nukem 3D mod: "+title, DUKE, DUKE15, "its main script, in a folder with maps and episodes of its own", "EDuke32", QList<Custom>());
}

QString ZDLBuildGames::modFolderScript(const QString &folder)
{
	if (!QFileInfo(folder).isDir())
		return QString();
	QString script=FindCaseless(folder, "EDUKE.CON");
	if (script.isEmpty())
		script=FindCaseless(folder, "GAME.CON");
	return !script.isEmpty()&&byModScripts(script).valid()?script:QString();
}

QString ZDLBuildGames::scriptBase(const QString &file, const QStringList &library, bool redneck)
{
	return BaseFor(file, library, redneck?PORT_REDNUKEM:PORT_EDUKE32);
}
