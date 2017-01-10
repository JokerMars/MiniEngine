#pragma once

#ifndef _FILE_DEFINED_H
#define _FILE_DEFINED_H

#include "common.h"
#include "ctx.h"

#pragma pack(1)

typedef struct _FILE_FLAG
{
	UCHAR strFileFlagHeader[FILE_GUID_LENGTH];

	UCHAR strFileKeyHash[HASH_SIZE];

	LONGLONG llFileValidLength;

	UCHAR Reserved[SECTOR_SIZE - HASH_SIZE - FILE_GUID_LENGTH - 8];

}FILE_FLAG, *PFILE_FLAG;

#pragma pack()

#define FILE_FLAG_LENGTH sizeof(FILE_FLAG)


NTSTATUS
File_GetFileOffset(
	_In_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Out_ PLARGE_INTEGER FileOffset
);

NTSTATUS
File_SetFileOffset(
	_In_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ PLARGE_INTEGER FileOffset
);


NTSTATUS
File_GetFileSize(
	_In_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Out_ PLARGE_INTEGER FileSize
);

NTSTATUS
File_SetFileSize(
	_In_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ PLARGE_INTEGER FileSize
);

NTSTATUS
File_GetFileStandardInfo(
	_In_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ PLARGE_INTEGER FileAllocationSize,
	_In_ PLARGE_INTEGER FileSize,
	_In_ PBOOLEAN isDirectory
);



NTSTATUS
File_ReadWriteFile(
	_In_ ULONG MajorFunction,
	_In_ PFLT_INSTANCE Instance,
	_In_ PFILE_OBJECT FileObject,
	_In_ PLARGE_INTEGER ByteOffset,
	_In_ ULONG Length,
	_In_ PVOID Buffer,
	_Out_ PULONG BytesReadWrite,
	_In_ FLT_IO_OPERATION_FLAGS FltFlags
);



NTSTATUS
File_WriteFileFlag(
	_In_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ PFILE_OBJECT FileObject,
	_In_ PSTREAM_CONTEXT pStreamCtx
);


NTSTATUS
File_ReadFileFlag(
	_In_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ PVOID Buffer
);


NTSTATUS
File_InitFileFlag();

NTSTATUS
File_UninitFileFlag();

extern PFILE_FLAG g_pFileFlag;


#endif
