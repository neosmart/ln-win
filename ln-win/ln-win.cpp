/*
 * NeoSmart JunctionPoint Library
 * Author: Mahmoud Al-Qudsi <mqudsi@neosmart.net>
 * Copyright (C) 2011 by NeoSmart Technologies
 * This code is released under the terms of the MIT License
*/

#include "stdafx.h"
#include <atlstr.h>

#include "JunctionPoint.h"

using namespace std;
using namespace neosmart;

int PrintSyntax()
{
	_tprintf(_T("ln [/s] source destination"));
	return -1;
}

int _tmain(int argc, _TCHAR* argv[])
{
	if(argc < 3 || argc > 4 || (argc == 4 && _tcsicmp(argv[1], _T("-s")) != 0 && _tcsicmp(argv[1], _T("/s"))))
	{
		return PrintSyntax();
	}

	bool softlink = argc == 4;

	CString source, destination;
	int index = 1;

	if(softlink)
		++index;

	source = argv[index++];
	destination = argv[index];

	if(GetFileAttributes(destination) != INVALID_FILE_ATTRIBUTES)
	{
		_tprintf(_T("Destination already exists!\r\n"));
		return -1;
	}

	bool isDirectory = (GetFileAttributes(source) & FILE_ATTRIBUTE_DIRECTORY) != 0;
	
	if(softlink)
		CreateSymbolicLink(destination, source, isDirectory ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0);
	else if(isDirectory)
		CreateJunctionPoint(source, destination);
	else
		CreateHardLink(destination, source, NULL);
	
	return 0;
}
