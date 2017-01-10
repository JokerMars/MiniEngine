#include "file.h"

PFILE_FLAG g_pFileFlag = NULL;

static UCHAR gc_sGuid[FILE_GUID_LENGTH] =
{
	0x3a, 0x8a, 0x2c, 0xd1, 0x50, 0xe8, 0x47, 0x5f,
	0xbe, 0xdb, 0xd7, 0x6c, 0xa2, 0xe9, 0x8e, 0x1d
};


NTSTATUS
File_GetFileOffset(
	_In_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Out_ PLARGE_INTEGER FileOffset
)
{
	NTSTATUS status;
	FILE_POSITION_INFORMATION NewPos;

	status = FltQueryInformationFile(
		FltObjects->Instance,
		FltObjects->FileObject,
		&NewPos,
		sizeof(FILE_POSITION_INFORMATION),
		FilePositionInformation,
		NULL
	);

	if (NT_SUCCESS(status))
	{
		FileOffset->QuadPart = NewPos.CurrentByteOffset.QuadPart;
	}

	return status;
}

NTSTATUS
File_SetFileOffset(
	_In_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ PLARGE_INTEGER FileOffset
)
{
	NTSTATUS status;
	FILE_POSITION_INFORMATION NewPos;

	LARGE_INTEGER NewOffset = { 0 };

	NewOffset.QuadPart = FileOffset->QuadPart;
	NewOffset.LowPart = FileOffset->LowPart;

	NewPos.CurrentByteOffset = NewOffset;

	status = FltSetInformationFile(
		FltObjects->Instance,
		FltObjects->FileObject,
		&NewPos,
		sizeof(FILE_POSITION_INFORMATION),
		FilePositionInformation
	);

	return status;
}


NTSTATUS
File_GetFileSize(
	_In_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Out_ PLARGE_INTEGER FileSize
)
{
	NTSTATUS status;
	FILE_STANDARD_INFORMATION fileInfo;

	status = FltQueryInformationFile(
		FltObjects->Instance,
		FltObjects->FileObject,
		&fileInfo,
		sizeof(FILE_STANDARD_INFORMATION),
		FileStandardInformation,
		NULL
	);

	if (NT_SUCCESS(status))
	{
		FileSize->QuadPart = fileInfo.EndOfFile.QuadPart;
	}
	else
	{
		FileSize->QuadPart = 0;
	}

	return status;
}

NTSTATUS
File_SetFileSize(
	_In_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ PLARGE_INTEGER FileSize
)
{
	NTSTATUS status = STATUS_SUCCESS;
	FILE_END_OF_FILE_INFORMATION EndOfFile;

	EndOfFile.EndOfFile.QuadPart = FileSize->QuadPart;
	EndOfFile.EndOfFile.LowPart = FileSize->LowPart;

	status = FltSetInformationFile(
		FltObjects->Instance,
		FltObjects->FileObject,
		&EndOfFile,
		sizeof(FILE_END_OF_FILE_INFORMATION),
		FileEndOfFileInformation
	);

	return status;
}

NTSTATUS
File_GetFileStandardInfo(
	_In_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ PLARGE_INTEGER FileAllocationSize,
	_In_ PLARGE_INTEGER FileSize,
	_In_ PBOOLEAN isDirectory
)
{
	NTSTATUS status = STATUS_SUCCESS;
	FILE_STANDARD_INFORMATION sFileStandardInfo;


	status = FltQueryInformationFile(FltObjects->Instance,
		FltObjects->FileObject,
		&sFileStandardInfo,
		sizeof(FILE_STANDARD_INFORMATION),
		FileStandardInformation,
		NULL
	);

	if (NT_SUCCESS(status))
	{
		if (NULL != FileSize)
			*FileSize = sFileStandardInfo.EndOfFile;
		if (NULL != FileAllocationSize)
			*FileAllocationSize = sFileStandardInfo.AllocationSize;
		if (NULL != isDirectory)
			*isDirectory = sFileStandardInfo.Directory;
	}

	return status;
}


static
NTSTATUS File_ReadWriteFileComplete(
	PDEVICE_OBJECT dev,
	PIRP irp,
	PVOID context
)
{
	*irp->UserIosb = irp->IoStatus;
	KeSetEvent(irp->UserEvent, 0, FALSE);
	IoFreeIrp(irp);
	return STATUS_MORE_PROCESSING_REQUIRED;
}


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
)
{
	ULONG i;
	PIRP irp;
	KEVENT Event;
	PIO_STACK_LOCATION ioStackLocation;
	IO_STATUS_BLOCK IoStatusBlock = { 0 };

	PDEVICE_OBJECT pVolumeDevObj = NULL;
	PDEVICE_OBJECT pFileSysDevObj = NULL;
	PDEVICE_OBJECT pNextDevObj = NULL;

	//获取minifilter相邻下层的设备对象
	pVolumeDevObj = IoGetDeviceAttachmentBaseRef(FileObject->DeviceObject);
	if (NULL == pVolumeDevObj)
	{
		return STATUS_UNSUCCESSFUL;
	}
	pFileSysDevObj = pVolumeDevObj->Vpb->DeviceObject;
	pNextDevObj = pFileSysDevObj;

	if (NULL == pNextDevObj)
	{
		ObDereferenceObject(pVolumeDevObj);
		return STATUS_UNSUCCESSFUL;
	}

	//开始构建读写IRP
	KeInitializeEvent(&Event, SynchronizationEvent, FALSE);

	// 分配irp.
	irp = IoAllocateIrp(pNextDevObj->StackSize, FALSE);
	if (irp == NULL) {
		ObDereferenceObject(pVolumeDevObj);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	irp->AssociatedIrp.SystemBuffer = NULL;
	irp->MdlAddress = NULL;
	irp->UserBuffer = Buffer;
	irp->UserEvent = &Event;
	irp->UserIosb = &IoStatusBlock;
	irp->Tail.Overlay.Thread = PsGetCurrentThread();
	irp->RequestorMode = KernelMode;
	if (MajorFunction == IRP_MJ_READ)
		irp->Flags = IRP_DEFER_IO_COMPLETION | IRP_READ_OPERATION | IRP_NOCACHE;
	else if (MajorFunction == IRP_MJ_WRITE)
		irp->Flags = IRP_DEFER_IO_COMPLETION | IRP_WRITE_OPERATION | IRP_NOCACHE;
	else
	{
		ObDereferenceObject(pVolumeDevObj);
		return STATUS_UNSUCCESSFUL;
	}

	if ((FltFlags & FLTFL_IO_OPERATION_PAGING) == FLTFL_IO_OPERATION_PAGING)
	{
		irp->Flags |= IRP_PAGING_IO;
	}

	// 填写irpsp
	ioStackLocation = IoGetNextIrpStackLocation(irp);
	ioStackLocation->MajorFunction = (UCHAR)MajorFunction;
	ioStackLocation->MinorFunction = (UCHAR)IRP_MN_NORMAL;
	ioStackLocation->DeviceObject = pNextDevObj;
	ioStackLocation->FileObject = FileObject;
	if (MajorFunction == IRP_MJ_READ)
	{
		ioStackLocation->Parameters.Read.ByteOffset = *ByteOffset;
		ioStackLocation->Parameters.Read.Length = Length;
	}
	else
	{
		ioStackLocation->Parameters.Write.ByteOffset = *ByteOffset;
		ioStackLocation->Parameters.Write.Length = Length;
	}

	// 设置完成
	IoSetCompletionRoutine(irp, File_ReadWriteFileComplete, 0, TRUE, TRUE, TRUE);
	(void)IoCallDriver(pNextDevObj, irp);
	KeWaitForSingleObject(&Event, Executive, KernelMode, TRUE, 0);
	*BytesReadWrite = IoStatusBlock.Information;

	ObDereferenceObject(pVolumeDevObj);

	return IoStatusBlock.Status;


}



NTSTATUS
File_WriteFileFlag(
	_In_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ PFILE_OBJECT FileObject,
	_In_ PSTREAM_CONTEXT pStreamCtx
)
{
	NTSTATUS status = STATUS_SUCCESS;
	LARGE_INTEGER ByteOffset = { 0 };
	LARGE_INTEGER FileSize = { 0 };
	ULONG BytesWritten = 0;
	ULONG writelen = sizeof(FILE_FLAG);
	PFILE_FLAG psFileFlag = NULL;

	try {

		File_GetFileSize(Data, FltObjects, &FileSize);

		//allocate local file flag buffer
		psFileFlag = (PFILE_FLAG)ExAllocatePoolWithTag(NonPagedPool, FILE_FLAG_LENGTH, FILEFLAG_POOL_TAG);
		if (NULL == psFileFlag)
		{
			status = STATUS_INSUFFICIENT_RESOURCES;
			__leave;
		}
		RtlCopyMemory(psFileFlag, g_pFileFlag, FILE_FLAG_LENGTH); //实际上这里应该是当前文件自身的flag

																  //set current file size into file flag buffer
		psFileFlag->llFileValidLength = pStreamCtx->liFileValidLength.QuadPart;
		FileSize.QuadPart = pStreamCtx->liFileValidLength.QuadPart;

		//calculate padded file size
		if (FileSize.QuadPart % SECTOR_SIZE)
		{//file size is not multiply of sector size
			FileSize.QuadPart = FileSize.QuadPart + (SECTOR_SIZE - FileSize.QuadPart % SECTOR_SIZE) + FILE_FLAG_LENGTH;
		}
		else
		{//file size is multiply of sector size
			FileSize.QuadPart += FILE_FLAG_LENGTH;
		}

		//RtlCopyMemory(psFileFlag->FileKeyHash, pStreamCtx->szKeyHash, HASH_SIZE);

		//File_SetFileSize(Data, FltObjects, &FileSize);

		//write file flag into file trail
		ByteOffset.QuadPart = FileSize.QuadPart - FILE_FLAG_LENGTH;
		status = File_ReadWriteFile(
			IRP_MJ_WRITE,
			FltObjects->Instance,
			FileObject,
			&ByteOffset,
			FILE_FLAG_LENGTH,
			psFileFlag,
			&BytesWritten,
			FLTFL_IO_OPERATION_NON_CACHED | FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET);
	}
	finally{

	}

	return status;
}


NTSTATUS
File_ReadFileFlag(
	_In_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Inout_ PVOID Buffer
)
{
	NTSTATUS status = STATUS_SUCCESS;
	LARGE_INTEGER ByteOffset;
	LARGE_INTEGER FileSize;
	ULONG BytesRead = 0;

	try
	{
		File_GetFileSize(Data, FltObjects, &FileSize);

		if (FileSize.QuadPart < FILE_FLAG_LENGTH)
		{
			return STATUS_UNSUCCESSFUL;
		}

		ByteOffset.QuadPart = FileSize.QuadPart - FILE_FLAG_LENGTH;

		status = File_ReadWriteFile(
			IRP_MJ_READ,
			FltObjects->Instance,
			FltObjects->FileObject,
			&ByteOffset,
			FILE_FLAG_LENGTH,
			Buffer,
			&BytesRead,
			FLTFL_IO_OPERATION_NON_CACHED | FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET
		);

	}
	finally
	{

	}


	return status;
}


NTSTATUS
File_InitFileFlag()
{
	if (NULL != g_pFileFlag)
	{
		return STATUS_SUCCESS;
	}

	g_pFileFlag = ExAllocatePoolWithTag(NonPagedPool, FILE_FLAG_LENGTH, FILEFLAG_POOL_TAG);
	if (NULL == g_pFileFlag)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}


	RtlZeroMemory(g_pFileFlag, FILE_FLAG_LENGTH);

	RtlCopyMemory(g_pFileFlag->strFileFlagHeader, gc_sGuid, FILE_GUID_LENGTH);

	return STATUS_SUCCESS;

}

NTSTATUS
File_UninitFileFlag()
{
	if (NULL != g_pFileFlag)
	{
		ExFreePoolWithTag(g_pFileFlag, FILEFLAG_POOL_TAG);
		g_pFileFlag = NULL;
	}

	return STATUS_SUCCESS;
}


