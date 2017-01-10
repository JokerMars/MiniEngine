#include "CallbackRoutines.h"


FLT_PREOP_CALLBACK_STATUS
PreCreate(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
{
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);

	return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}



FLT_POSTOP_CALLBACK_STATUS
PostCreate(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
)
{
	NTSTATUS status;
	PFLT_FILE_NAME_INFORMATION pfNameInfo = NULL;
	PSTREAM_CONTEXT pStreamCtx = NULL;

	BOOLEAN bNewCreated = FALSE;
	KIRQL oldIrql;
	ULONG uAccess = Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess;

	LARGE_INTEGER liFileSize = { 0 };
	LARGE_INTEGER liFileOffset = { 0 };
	PFILE_FLAG pFileFlag = NULL;

	LARGE_INTEGER liByteOffset = { 0 };
	ULONG uReadLen = 0;

	try
	{
		if (!NT_SUCCESS(Data->IoStatus.Status))
		{
			leave;
		}

		//
		// if it's a directory, just fail the operation.
		//

		BOOLEAN isDir = FALSE;
		status = FltIsDirectory(FltObjects->FileObject,
			FltObjects->Instance, &isDir);
		if (!NT_SUCCESS(status) || isDir)
		{
			leave;
		}

		//
		// Get the process
		//

		PCHAR procName = GetProcessName();
		if (!IsMonitoredProcess(procName))
		{
			leave;
		}

		//
		// Get the file name
		//

		status = FltGetFileNameInformation(
			Data,
			FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
			&pfNameInfo
		);
		if (!NT_SUCCESS(status) || NULL == pfNameInfo)
		{
			leave;
		}
		status = FltParseFileNameInformation(pfNameInfo);
		if (!NT_SUCCESS(status))
		{
			leave;
		}

		if (!IsMonitoredFileExt(&pfNameInfo->Extension))
		{
			leave;
		}

#if DBG
		KdPrint(("\nIRP_MJ_CREATE\n"));
		KdPrint(("    Process Name: %s\n", procName));
		KdPrint(("    The File Name: %wZ\n", &(pfNameInfo->Name)));
		KdPrint(("    The File Ext: %wZ\n", &(pfNameInfo->Extension)));
		KdPrint(("    The Volume: %wZ\n", &(pfNameInfo->Volume)));
#endif

		//
		// In case, there is data in system cache, once open then clear the cache
		//

		Cc_ClearFileCache(FltObjects->FileObject, TRUE, NULL, 0);

		//
		// Create or get stream context
		//

		status = Ctx_FindOrCreateStreamContext(Data, FltObjects, TRUE, &pStreamCtx, &bNewCreated);
		if (!NT_SUCCESS(status))
		{
			leave;
		}

		status = Ctx_UpdateNameInStreamContext(&pfNameInfo->Name, pStreamCtx);
		if (!NT_SUCCESS(status))
		{
			leave;
		}

		if (!bNewCreated)
		{
#if DBG
			KdPrint(("    The Stream Context Has already Created, Just Use it."));
#endif
			SC_LOCK(pStreamCtx, &oldIrql);

			pStreamCtx->uAccess = uAccess;

			SC_UNLOCK(pStreamCtx, oldIrql);
			leave;
		}

		//
		// The file first open, initialize the stream context
		//

		SC_LOCK(pStreamCtx, &oldIrql);

		RtlCopyMemory(pStreamCtx->wsVolumeName, pfNameInfo->Volume.Buffer, pfNameInfo->Volume.Length);
		pStreamCtx->bHasFileEncrypted = FALSE;
		pStreamCtx->bNeedDecryptOnRead = FALSE;
		pStreamCtx->bNeedEncryptOnWrite = TRUE;
		pStreamCtx->bHasWrittenData = FALSE;
		pStreamCtx->uAccess = uAccess;
		pStreamCtx->uTrailLength = FILE_FLAG_LENGTH;

		SC_UNLOCK(pStreamCtx, oldIrql);

		//
		// Get file size
		//

		status = File_GetFileStandardInfo(Data, FltObjects, NULL, &liFileSize, NULL);
		if (!NT_SUCCESS(status))
		{
			leave;
		}

		SC_LOCK(pStreamCtx, &oldIrql);
		pStreamCtx->liFileSize = liFileSize;
		SC_UNLOCK(pStreamCtx, oldIrql);

		if ((0 == liFileSize.QuadPart) && (uAccess & (FILE_WRITE_DATA | FILE_APPEND_DATA)))
		{//new created file with write or append access
			leave;
		}
		if ((0 == liFileSize.QuadPart) && !(uAccess & (FILE_WRITE_DATA | FILE_APPEND_DATA)))
		{// file size is zero, but without write or append access
			leave;
		}

		//if file size is less than file flag length, the file is not encrypted yet but need to be encrypted
		if (liFileSize.QuadPart < FILE_FLAG_LENGTH)
		{
			leave;
		}

		//
		// hold the original bytesoffset
		//

		File_GetFileOffset(Data, FltObjects, &liFileOffset);

		pFileFlag = (PFILE_FLAG)ExAllocatePoolWithTag(NonPagedPool, FILE_FLAG_LENGTH, FILEFLAG_POOL_TAG);
		liByteOffset.QuadPart = liFileSize.QuadPart - FILE_FLAG_LENGTH;
		status = File_ReadWriteFile(
			IRP_MJ_READ,
			FltObjects->Instance,
			FltObjects->FileObject,
			&liByteOffset,
			FILE_FLAG_LENGTH,
			pFileFlag,
			&uReadLen,
			FLTFL_IO_OPERATION_NON_CACHED | FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET
		);
		if (!NT_SUCCESS(status))
		{
			leave;
		}

		File_SetFileOffset(Data, FltObjects, &liFileOffset);


		//
		// Compare guid
		//

		if (FILE_GUID_LENGTH != RtlCompareMemory(g_pFileFlag, pFileFlag, FILE_GUID_LENGTH))
		{//not equal, so the file has not been encrypted yet.
			SC_LOCK(pStreamCtx, &oldIrql);
			pStreamCtx->liFileValidLength = liFileSize; //file is existing and has no tail, so filevalidlength equals filesize.
			SC_UNLOCK(pStreamCtx, oldIrql);

#if DBG
			KdPrint(("    The File Has Not Been Encrypted."));
#endif
			leave;
		}

		//
		// file has been encrypted, reset some fileds
		//

		SC_LOCK(pStreamCtx, &oldIrql);

		pStreamCtx->liFileValidLength.QuadPart = pFileFlag->llFileValidLength;
		pStreamCtx->bHasFileEncrypted = TRUE;
		pStreamCtx->bNeedDecryptOnRead = TRUE;
		pStreamCtx->bNeedEncryptOnWrite = TRUE;
		pStreamCtx->uTrailLength = FILE_FLAG_LENGTH;

		SC_UNLOCK(pStreamCtx, oldIrql);

#if DBG
		KdPrint(("    The File Has Already Been Encrypted."));
#endif

	}
	finally
	{
		//
		// This resources we don't need anymore, so
		// free all of them
		//

		if (NULL != pfNameInfo)
		{
			FltReleaseFileNameInformation(pfNameInfo);
		}

		if (NULL != pStreamCtx)
		{
			FltReleaseContext(pStreamCtx);
		}
	}


	return FLT_POSTOP_FINISHED_PROCESSING;
}
