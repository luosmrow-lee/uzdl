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
 
#ifndef _ZDLCOMMON_H_
#define _ZDLCOMMON_H_
#include <QtCore>

#define ZDL_FLAG_NAMELESS	0x00001

//The name shown to the user. Distinct from the zdl.* config keys and the
//.zdl file extension, which are data and must keep their spelling.
#define ZDL_APP_NAME "uZDL"
#define ZDL_VERSION_STRING "1.0.0"
#define ZDL_PORTABLE_INI "uzdl_portable.ini"
//GitHub project the update check asks for releases, unless the
//configuration names another as updaterepo under [zdl.general].
#define ZDL_UPDATE_REPO "luosmrow-lee/uzdl"
#define ZDL_DEV_BUILD 0
//Shown in the About box. uZDL numbers itself from 1.0.0 rather than
//continuing ZDL 3-1.2, which is where it was forked from.
#define ZDL_PRIVATE_VERSION_STRING "1.0.0 (Qt 6, forked from ZDL 3-1.2)"

#ifdef Q_OS_WIN
#define QFD_FILTER_DELIM    ";"
#define QFD_FILTER_ALL      "*.*"
#define QFD_QT_SEP(x)       QDir::fromNativeSeparators(x)
#else
#define QFD_FILTER_DELIM    " "
#define QFD_FILTER_ALL      "*"
#define QFD_QT_SEP(x)       x
#endif

extern QDebug *zdlDebug;

#if defined(ZDL_BLACKBOX)
#include <QtCore>

//__PRETTY_FUNCTION__ is a GCC extension; MSVC spells the equivalent
//__FUNCSIG__. Without this the blackbox logger does not compile on MSVC.
#if defined(_MSC_VER)
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

#define LOGDATA() (*zdlDebug) << (QDateTime::currentDateTime().toString("[yyyy:MM:dd/hh:mm:ss.zzz]").append("@").append(__PRETTY_FUNCTION__).append("@").append(__FILE__).append(":").append(QString::number(__LINE__)).append("\t"))
#define LOGDATAO() (*zdlDebug) << (QDateTime::currentDateTime().toString("[yyyy:MM:dd/hh:mm:ss.zzz]").append("@").append(__PRETTY_FUNCTION__).append("@").append(__FILE__).append(":").append(QString::number(__LINE__)).append("#this=").append(DPTR(this)).append("\t"))

#if !defined(Q_OS_MACOS)

#if UINTPTR_MAX == 0xffffffff
#ifndef _ZDL_NO_WARNINGS
#ifdef __GNUC__
#warning 32bit
#else
#pragma message("Warning: 32bit")
#endif
#endif
#define DPTR(ptr) QString("0x").append(QString::number((quintptr)ptr,16))
#else
#define DPTR(ptr) QString("0x").append(QString::number((qulonglong)ptr,16))
#endif

#else

#define DPTR(ptr) QString("0x").append("PTR")

#endif

#else
#define LOGDATA() (*zdlDebug)
#define LOGDATAO() (*zdlDebug)
#define DPTR(ptr) QString("")
#endif

// ZDLConf and ZDLSection re-enter their own lock in a mixed mode: readINI
// holds the write lock and then calls getSection, which takes a read lock on
// the same object from the same thread. Only a recursive mutex tolerates
// that. A recursive QReadWriteLock does not -- it re-enters in the same mode
// only, so lockForRead while this thread is the writer waits on itself.
// Qt 6 removed QMutex(QMutex::Recursive) in favour of QRecursiveMutex.
#define LOCK_CLASS               QRecursiveMutex
#define LOCK_BUILDER()           new QRecursiveMutex()
#define GET_READLOCK(mlock)      (mlock)->lock()
#define RELEASE_READLOCK(mlock)  (mlock)->unlock()
#define GET_WRITELOCK(mlock)     (mlock)->lock()
#define RELEASE_WRITELOCK(mlock) (mlock)->unlock()
#define TRY_READLOCK(mlock, to)  (mlock)->tryLock(to)
#define TRY_WRITELOCK(mlock, to) (mlock)->tryLock(to)

#include "zdlline.hpp"
#include "zdlsection.hpp"
#include "zdlconf.hpp"

#endif
