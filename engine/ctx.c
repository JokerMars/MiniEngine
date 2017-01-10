#include "ctx.h"

NPAGED_LOOKASIDE_LIST Pre2PostContextList;


static NTSTATUS iCtx_CreateStreamContext(PFLT_RELATED_OBJECTS FltObjects, PSTREAM_CONTEXT *StreamContext);

VOID SC_iLOCK(PERESOURCE pResource)
{
	try
	{
		ASSERT(KeGetCurrentIrql() <= APC_LEVEL);
		ASSERT(ExIsResourceAcquiredExclusiveLite(pResource) || !ExIsResourceAcquiredSharedLite(pResource));

		KeEnterCriticalRegion();
		(VOID)ExAcquireResourceExclusiveLite(pResource, TRUE);
	}
	finally
	{
	}
}



VOID SC_iUNLOCK(PERESOURCE pResource)
{
	try
	{
		ASSERT(KeGetCurrentIrql() <= APC_LEVEL);
		ASSERT(ExIsResourceAcquiredExclusiveLite(pResource) || ExIsResourceAcquiredSharedLite(pResource));

		ExReleaseResourceLite(pResource);
		KeLeaveCriticalRegion();
	}
	finally
	{
	}
}


VOID SC_LOCK(PSTREAM_CONTEXT SC, PKIRQL OldIrql)
{
	if (KeGetCurrentIrql() <= APC_LEVEL)
	{
		SC_iLOCK(SC->resource);
	}
	else
	{
		KeAcquireSpinLock(&SC->resource1, OldIrql);
	}
}


VOID SC_UNLOCK(PSTREAM_CONTEXT SC, KIRQL OldIrql)
{
	if (KeGetCurrentIrql() <= APC_LEVEL)
	{
		SC_iUNLOCK(SC->resource);
	}
	else
	{
		KeReleaseSpinLock(&SC->resource1, OldIrql);
	}
}



NTSTATUS
Ctx_FindOrCreateStreamContext(
	__in PFLT_CALLBACK_DATA Data,
	__in PFLT_RELATED_OBJECTS FltObjects,
	__in BOOLEAN CreateIfNotFound,
	__deref_out PSTREAM_CONTEXT *StreamContext,
	__out_opt BOOLEAN* ContextCreated
)
{
	NTSTATUS status;
	PSTREAM_CONTEXT streamContext;
	PSTREAM_CONTEXT oldStreamContext;

	PAGED_CODE();

	*StreamContext = NULL;
	if (ContextCreated != NULL) *ContextCreated = FALSE;

	//  First try to get the stream context.
	status = FltGetStreamContext(Data->Iopb->TargetInstance,
		Data->Iopb->TargetFileObject, &streamContext);

	if (!NT_SUCCESS(status) &&
		(status == STATUS_NOT_FOUND) &&
		CreateIfNotFound)
	{
		status = iCtx_CreateStreamContext(FltObjects, &streamContext);
		if (!NT_SUCCESS(status))
			return status;

		status = FltSetStreamContext(Data->Iopb->TargetInstance,
			Data->Iopb->TargetFileObject,
			FLT_SET_CONTEXT_KEEP_IF_EXISTS,
			streamContext,
			&oldStreamContext);

		if (!NT_SUCCESS(status))
		{
			FltReleaseContext(streamContext);

			if (status != STATUS_FLT_CONTEXT_ALREADY_DEFINED)
			{
				//  FltSetStreamContext failed for a reason other than the context already
				//  existing on the stream. So the object now does not have any context set
				//  on it. So we return failure to the caller.
				return status;
			}
			streamContext = oldStreamContext;
			status = STATUS_SUCCESS;
		}
		else
		{
			if (ContextCreated != NULL) *ContextCreated = TRUE;
		}
	}
	*StreamContext = streamContext;

	return status;

}

NTSTATUS
Ctx_UpdateNameInStreamContext(
	__in PUNICODE_STRING DirectoryName,
	__inout PSTREAM_CONTEXT StreamContext
)
{
	NTSTATUS status = STATUS_SUCCESS;

	PAGED_CODE();

	//Free any existing name
	if (StreamContext->uniFileName.Buffer != NULL)
	{
		ExFreePoolWithTag(StreamContext->uniFileName.Buffer, STRING_TAG);

		StreamContext->uniFileName.Length = StreamContext->uniFileName.MaximumLength = 0;
		StreamContext->uniFileName.Buffer = NULL;
	}

	//Allocate and copy off the directory name
	StreamContext->uniFileName.MaximumLength = DirectoryName->Length;
	StreamContext->uniFileName.Buffer = ExAllocatePoolWithTag(PagedPool,
		StreamContext->uniFileName.MaximumLength,
		STRING_TAG);
	if (StreamContext->uniFileName.Buffer == NULL)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlCopyUnicodeString(&StreamContext->uniFileName, DirectoryName);

	return status;
}

NTSTATUS
iCtx_CreateStreamContext(
	__in PFLT_RELATED_OBJECTS FltObjects,
	__deref_out PSTREAM_CONTEXT *StreamContext
)
{
	NTSTATUS status;
	PSTREAM_CONTEXT streamContext;

	PAGED_CODE();

	status = FltAllocateContext(FltObjects->Filter,
		FLT_STREAM_CONTEXT,
		sizeof(STREAM_CONTEXT),
		NonPagedPool,
		&streamContext);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	//  Initialize the newly created context
	RtlZeroMemory(streamContext, sizeof(STREAM_CONTEXT));

	streamContext->resource = ExAllocatePoolWithTag(NonPagedPool, sizeof(ERESOURCE), RESOURCE_TAG);
	if (streamContext->resource == NULL)
	{
		FltReleaseContext(streamContext);
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	ExInitializeResourceLite(streamContext->resource);

	KeInitializeSpinLock(&streamContext->resource1);

	*StreamContext = streamContext;

	return STATUS_SUCCESS;
}




VOID CleanupStreamContext(
	_In_ PFLT_CONTEXT Context,
	_In_ FLT_CONTEXT_TYPE ContextType
)
{
	PSTREAM_CONTEXT streamCtx = NULL;
	streamCtx = (PSTREAM_CONTEXT)Context;

	if (streamCtx->uniFileName.Buffer != NULL)
	{
		ExFreePoolWithTag(streamCtx->uniFileName.Buffer, STRING_TAG);

		streamCtx->uniFileName.Length = streamCtx->uniFileName.MaximumLength = 0;
		streamCtx->uniFileName.Buffer = NULL;
	}

	if (NULL != streamCtx->resource)
	{
		ExDeleteResourceLite(streamCtx->resource);
		ExFreePoolWithTag(streamCtx->resource, RESOURCE_TAG);
	}

	FltReleaseContext(streamCtx);
}

