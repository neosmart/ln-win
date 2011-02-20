/*
 * NeoSmart JunctionPoint Library
 * Author: Mahmoud Al-Qudsi <mqudsi@neosmart.net>
 * Copyright (C) 2011 by NeoSmart Technologies
 * This code is released under the terms of the MIT License
*/

#include "stdafx.h"
#include "JunctionPoint.h"
#include "Internal.h"

#include <Shlobj.h>
#include <atlstr.h>
#include <memory>

using namespace std;

namespace neosmart
{
	__declspec(thread) int recurseDepth = 0;

	bool CreateJunctionPoint(LPCTSTR origin, LPCTSTR junction)
	{
		CString nativeTarget;

		//Prepend \??\ to path to mark it as not-for-parsing
		nativeTarget.Format(_T("\\??\\%s"), origin);

		//This API only supports Windows-style slashes.
		nativeTarget.Replace(_T('/'), _T('\\'));

		//Make sure there's a trailing slash
		if(nativeTarget.Right(1) != _T("\\"))
			nativeTarget += _T("\\");

		size_t size = sizeof(REPARSE_DATA_BUFFER) - sizeof(TCHAR) + nativeTarget.GetLength() * sizeof(TCHAR);
		auto_ptr<REPARSE_DATA_BUFFER> reparseBuffer((REPARSE_DATA_BUFFER*) new unsigned char[size]);

		//Fill the reparse buffer
		reparseBuffer->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
		reparseBuffer->Reserved = NULL;

		reparseBuffer->MountPointReparseBuffer.SubstituteNameOffset = 0;
		reparseBuffer->MountPointReparseBuffer.SubstituteNameLength = nativeTarget.GetLength() * (int) sizeof(TCHAR);

		//No substitute name, point it outside the bounds of the string
		reparseBuffer->MountPointReparseBuffer.PrintNameOffset = reparseBuffer->MountPointReparseBuffer.SubstituteNameLength + (int) sizeof(TCHAR);
		reparseBuffer->MountPointReparseBuffer.PrintNameLength = 0;

		//Copy the actual string
		//_tcscpy(reparseBuffer->MountPointReparseBuffer.PathBuffer, nativeTarget);
		memcpy(reparseBuffer->MountPointReparseBuffer.PathBuffer, (LPCTSTR) nativeTarget, reparseBuffer->MountPointReparseBuffer.SubstituteNameLength);

		//Set ReparseDataLength to the size of the MountPointReparseBuffer
		//Kind in mind that with the padding for the union (given that SymbolicLinkReparseBuffer is larger),
		//this is NOT equal to sizeof(MountPointReparseBuffer)
		reparseBuffer->ReparseDataLength = sizeof(REPARSE_DATA_BUFFER) - REPARSE_DATA_BUFFER_HEADER_SIZE - sizeof(TCHAR) + reparseBuffer->MountPointReparseBuffer.SubstituteNameLength;

		//Create the junction directory
		CreateDirectory(junction, NULL);

		//Set the reparse point
		SafeHandle hDir;
		hDir.Handle = CreateFile(junction, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);

		if(hDir.IsInvalid())
		{
			_tprintf(_T("Failed to open directory!\r\n"));
			return false;
		}

		DWORD bytesReturned = 0;
		if(!DeviceIoControl(hDir.Handle, FSCTL_SET_REPARSE_POINT, reparseBuffer.get(), (unsigned int) size, NULL, 0, &bytesReturned, NULL))
		{
			RemoveDirectory(junction);
			_tprintf(_T("Error issuing DeviceIoControl FSCTL_SET_REPARSE_POINT: 0x%x.\r\n"), GetLastError());
			return false;
		}

		return true;
	}

	bool GetJunctionDestination(LPCTSTR path, OUT LPTSTR destination, DWORD attributes)
	{
		if(attributes == 0)
			attributes = GetFileAttributes(path);

		if(!IsDirectoryJunction(path, attributes))
			return false;

		SafeHandle hDir;
		hDir.Handle = CreateFile(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
		if (hDir.IsInvalid())
			return false;  // Failed to open directory

		BYTE buf[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
		REPARSE_DATA_BUFFER &ReparseBuffer = (REPARSE_DATA_BUFFER&)buf;

		DWORD dwRet;
		if (!DeviceIoControl(hDir.Handle, FSCTL_GET_REPARSE_POINT, NULL, 0, &ReparseBuffer, MAXIMUM_REPARSE_DATA_BUFFER_SIZE, &dwRet, NULL)) 
		{  
			_tprintf(_T("Error getting reparse destination: 0x%x"), GetLastError());
			return false;
		}

		if(ReparseBuffer.ReparseTag == IO_REPARSE_TAG_MOUNT_POINT)
		{
			//Junction point
			ReparseBuffer.MountPointReparseBuffer.SubstituteNameLength /= sizeof(TCHAR);
			ReparseBuffer.MountPointReparseBuffer.SubstituteNameOffset /= sizeof(TCHAR);

			LPWSTR pPath = ReparseBuffer.MountPointReparseBuffer.PathBuffer + ReparseBuffer.MountPointReparseBuffer.SubstituteNameOffset;
			pPath[ReparseBuffer.MountPointReparseBuffer.SubstituteNameLength] = _T('\0');

			if (wcsncmp(pPath, L"\\??\\", 4) == 0) 
				pPath += 4;  // Skip 'non-parsed' prefix

			_tcscpy_s(destination, MAX_PATH, pPath);
		}
		else if(ReparseBuffer.ReparseTag == IO_REPARSE_TAG_SYMLINK)
		{
			//Symlink
			ReparseBuffer.SymbolicLinkReparseBuffer.SubstituteNameLength /= sizeof(TCHAR);
			ReparseBuffer.SymbolicLinkReparseBuffer.SubstituteNameOffset /= sizeof(TCHAR);

			LPWSTR pPath = ReparseBuffer.SymbolicLinkReparseBuffer.PathBuffer + ReparseBuffer.SymbolicLinkReparseBuffer.SubstituteNameOffset;
			pPath[ReparseBuffer.SymbolicLinkReparseBuffer.SubstituteNameLength] = _T('\0');

			if (wcsncmp(pPath, L"\\??\\", 4) == 0) 
				pPath += 4;  // Skip 'non-parsed' prefix

			if(ReparseBuffer.SymbolicLinkReparseBuffer.Flags & SYMLINK_FLAG_RELATIVE)
			{
				//It's a relative path. Construct the relative path and resolve destination
				_tcscpy_s(destination, MAX_PATH, path);
				PathAddBackslash(destination);
				_tcscat_s(destination, MAX_PATH, pPath);
				PathResolve(destination, NULL, PRF_REQUIREABSOLUTE);
			}
			else
			{
				_tcscpy_s(destination, MAX_PATH, pPath);
			}
		}
		else
		{
			//Unsupported mount point
			return false;
		}

		//We may have to do this recursively
		++recurseDepth;
		bool result;
		if(recurseDepth < MAX_RECURSE_DEPTH)
		{
			DWORD newAttributes = GetFileAttributes(destination);
			if(IsDirectoryJunction(destination, newAttributes))
				result = GetJunctionDestination(CString(destination), destination, newAttributes);
		}
		else
		{
			result = false;
		}
	
		--recurseDepth;
		return result;
	}

	//Doesn't support symlink'd files :(
	bool IsDirectoryJunction(LPCTSTR path, DWORD attributes)
	{
		if(attributes == 0)
			attributes = ::GetFileAttributes(path);

		if (attributes == INVALID_FILE_ATTRIBUTES) 
			return false;  //Doesn't exist

		if ((attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))
			return false;  //Not a directory or not a reparse point

		return true;
	}

	//This removes the reparse point but does not delete the directory!
	bool DeleteJunctionPoint(LPCTSTR path)
	{
		REPARSE_GUID_DATA_BUFFER reparsePoint = {0};
		reparsePoint.ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;

		SafeHandle hDir;
		hDir.Handle = CreateFile(path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);

		//Does the path exist?
		if(hDir.IsInvalid())
			return false;

		DWORD returnedBytes;
		DWORD result = DeviceIoControl(hDir.Handle, FSCTL_DELETE_REPARSE_POINT, &reparsePoint, REPARSE_GUID_DATA_BUFFER_HEADER_SIZE, NULL, 0, &returnedBytes, NULL) != 0;
		if(result == 0)
		{
			_tprintf_s(_T("Failed to issue FSCTL_DELETE_REPARSE_POINT. Last error: 0x%x"), GetLastError());
			return false;
		}
		else
		{
			return true;
		}
	}
}
