#pragma once

#ifndef _CONTEXT_H
#define _CONTEXT_H

#include "common.h"

typedef struct _STREAM_CONTEXT
{
	UNICODE_STRING uniFileName;

	WCHAR wsVolumeName[64];

	LARGE_INTEGER liFileValidLength;

	LARGE_INTEGER liFileSize;

	ULONG uTrailLength;

	ULONG uAccess;

	BOOLEAN bHasFileEncrypted;					//if file has been encrypted, if the file has the tail
	BOOLEAN bNeedEncryptOnWrite;
	BOOLEAN bNeedDecryptOnRead;
	BOOLEAN bHasWrittenData;


	PERESOURCE resource;

	KSPIN_LOCK resource1;

}STREAM_CONTEXT, *PSTREAM_CONTEXT;


typedef struct _PRE_2_POST_CONTEXT
{
	PSTREAM_CONTEXT pStreamCtx;     //carry the file information

	PVOID SwappedBuffer;

}PRE_2_POST_CONTEXT, *PPRE_2_POST_CONTEXT;



VOID SC_iLOCK(PERESOURCE pResource);
VOID SC_iUNLOCK(PERESOURCE pResource);

VOID
SC_LOCK(PSTREAM_CONTEXT SC, PKIRQL OldIrql);

VOID
SC_UNLOCK(PSTREAM_CONTEXT SC, KIRQL OldIrql);



NTSTATUS
Ctx_FindOrCreateStreamContext(
	__in PFLT_CALLBACK_DATA Cbd,
	__in PFLT_RELATED_OBJECTS FltObjects,
	__in BOOLEAN CreateIfNotFound,
	__deref_out PSTREAM_CONTEXT *StreamContext,
	__out_opt BOOLEAN* ContextCreated
);

NTSTATUS
Ctx_UpdateNameInStreamContext(
	__in PUNICODE_STRING DirectoryName,
	__inout PSTREAM_CONTEXT StreamContext
);


VOID CleanupStreamContext(
	_In_ PFLT_CONTEXT Context,
	_In_ FLT_CONTEXT_TYPE ContextType
);


extern NPAGED_LOOKASIDE_LIST Pre2PostContextList;


#endif
