/*
 * NeoSmart JunctionPoint Library
 * Author: Mahmoud Al-Qudsi <mqudsi@neosmart.net>
 * Copyright (C) 2011 by NeoSmart Technologies
 * This code is released under the terms of the MIT License
*/

#pragma once

#define SYMLINK_FLAG_RELATIVE 1
#define REPARSE_DATA_BUFFER_HEADER_SIZE offsetof(REPARSE_DATA_BUFFER, GenericReparseBuffer)

namespace neosmart
{
    struct SafeHandle
    {
        HANDLE Handle;

        SafeHandle()
        {
            Handle = NULL;
        }

        ~SafeHandle()
        {
            ForceClose();
        }

        bool IsInvalid()
        {
            return Handle == INVALID_HANDLE_VALUE || Handle == NULL;
        }

        void ForceClose()
        {
            if(!IsInvalid())
                CloseHandle(Handle);
            Handle = NULL;
        }
    };

#pragma pack(push, 1)
    typedef struct _REPARSE_DATA_BUFFER
    {
        DWORD ReparseTag;
        WORD  ReparseDataLength;
        WORD  Reserved;
        union
        {
            struct
            {
                WORD SubstituteNameOffset;
                WORD SubstituteNameLength;
                WORD PrintNameOffset;
                WORD PrintNameLength;
                ULONG Flags;
                WCHAR PathBuffer[1];
            } SymbolicLinkReparseBuffer;

            struct
            {
                WORD SubstituteNameOffset;
                WORD SubstituteNameLength;
                WORD PrintNameOffset;
                WORD PrintNameLength;
                WCHAR PathBuffer[1];
            } MountPointReparseBuffer;

            struct
            {
                BYTE DataBuffer[1];
            } GenericReparseBuffer;
        };
    } REPARSE_DATA_BUFFER;
#pragma pack(pop)
}
