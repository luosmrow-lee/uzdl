/*
 * This file is part of uZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2019  Lcferrum
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
 

#include "ZDLListable.h"
#include "ZDLPaths.h"
#include "ZDLFileListable.h"
#include <QFileInfo>

//A file inside the uZDL folder is kept relative to it whatever form it
//arrived in, so the list shows the path as it will be stored.
ZDLFileListable::ZDLFileListable( QListWidget *parent, int type, QString file):ZDLNameListable(parent, type, ZDLPaths::preferRelative(file), QFileInfo(file).fileName()){
	fileName = ZDLPaths::preferRelative(file);
}

QString ZDLFileListable::getFile(){
	return fileName;
}
	


