/*++

Module Name:

    engine.c

Abstract:

    This is the main module of the engine miniFilter driver.

Environment:

    Kernel mode

--*/

#include "engine.h"

PFLT_FILTER gFilterHandle;

//
//  operation registration
//

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {

	{
		IRP_MJ_CREATE,
		FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_IO,
		PreCreate,
		PostCreate
	},

	{
		IRP_MJ_CLEANUP,
		FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_IO,
		PreCleanup,
		NULL
	},

	//{
	//	IRP_MJ_CLOSE,
	//	0,
	//	PreClose,
	//	NULL
	//},

	//{
	//	IRP_MJ_QUERY_INFORMATION,
	//	0,
	//	PreQueryInfo,
	//	PostQueryInfo
	//},

	//{
	//	IRP_MJ_SET_INFORMATION,
	//	0,
	//	PreSetInfo,
	//	PostSetInfo
	//},


	//{
	//	IRP_MJ_READ,
	//	0,
	//	PreRead,
	//	PostRead
	//},

	//{
	//	IRP_MJ_WRITE,
	//	0,
	//	PreWrite,
	//	PostWrite
	//},

    { IRP_MJ_OPERATION_END }
};

//
//Context definitions we currently care about 
//
CONST FLT_CONTEXT_REGISTRATION ContextNotifications[] = {

	{
		FLT_STREAM_CONTEXT,
		0,
		CleanupStreamContext,
		sizeof(STREAM_CONTEXT),
		CONTEXT_TAG
	},


	{ FLT_CONTEXT_END }
};

//
//  This defines what we want to filter with FltMgr
//

CONST FLT_REGISTRATION FilterRegistration = {

    sizeof( FLT_REGISTRATION ),         //  Size
    FLT_REGISTRATION_VERSION,           //  Version
    0,                                  //  Flags

	ContextNotifications,                               //  Context
    Callbacks,                          //  Operation callbacks

    engineUnload,                           //  MiniFilterUnload

    engineInstanceSetup,                    //  InstanceSetup
    engineInstanceQueryTeardown,            //  InstanceQueryTeardown
    engineInstanceTeardownStart,            //  InstanceTeardownStart
    engineInstanceTeardownComplete,         //  InstanceTeardownComplete

    NULL,                               //  GenerateFileName
    NULL,                               //  GenerateDestinationFileName
    NULL                                //  NormalizeNameComponent

};





/*************************************************************************
    MiniFilter initialization and unload routines.
*************************************************************************/

NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    This is the initialization routine for this miniFilter driver.  This
    registers with FltMgr and initializes all global data structures.

Arguments:

    DriverObject - Pointer to driver object created by the system to
        represent this driver.

    RegistryPath - Unicode string identifying where the parameters for this
        driver are located in the registry.

Return Value:

    Routine can return non success error codes.

--*/
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER( RegistryPath );

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("engine!DriverEntry: Entered\n") );

	//
	// Initialize the variable
	//

	g_curProcessNameOffset = GetProcessNameOffset();
	InitMonitorVariable();
#if DBG
	KdPrint(("    Process Name: %d", g_curProcessNameOffset));
#endif

	File_InitFileFlag();

    //
    //  Register with FltMgr to tell it our callback routines
    //

    status = FltRegisterFilter( DriverObject,
                                &FilterRegistration,
                                &gFilterHandle );

    FLT_ASSERT( NT_SUCCESS( status ) );

    if (NT_SUCCESS( status )) {

        //
        //  Start filtering i/o
        //

        status = FltStartFiltering( gFilterHandle );

        if (!NT_SUCCESS( status )) {

            FltUnregisterFilter( gFilterHandle );
        }
    }

    return status;
}

NTSTATUS
engineUnload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    )
/*++

Routine Description:

    This is the unload routine for this miniFilter driver. This is called
    when the minifilter is about to be unloaded. We can fail this unload
    request if this is not a mandatory unload indicated by the Flags
    parameter.

Arguments:

    Flags - Indicating if this is a mandatory unload.

Return Value:

    Returns STATUS_SUCCESS.

--*/
{
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("engine!engineUnload: Entered\n") );

    FltUnregisterFilter( gFilterHandle );
	File_UninitFileFlag();

    return STATUS_SUCCESS;
}

