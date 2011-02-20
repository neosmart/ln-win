/*
 * NeoSmart JunctionPoint Library
 * Author: Mahmoud Al-Qudsi <mqudsi@neosmart.net>
 * Copyright (C) 2011 by NeoSmart Technologies
 * This code is released under the terms of the MIT License
*/

#pragma once

#include <Windows.h>

#define MAX_RECURSE_DEPTH 10

namespace neosmart
{
	bool CreateJunctionPoint(LPCTSTR origin, LPCTSTR junction);
	bool IsDirectoryJunction(LPCTSTR path, DWORD attributes = 0);
	bool GetJunctionDestination(LPCTSTR path, OUT LPTSTR destination, DWORD attributes = 0);
	bool DeleteJunctionPoint(LPCTSTR path);
}
