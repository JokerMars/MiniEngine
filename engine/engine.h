#pragma once
#ifndef _ENGINE_H
#define _ENGINE_H

#include "common.h"
#include "CallbackRoutines.h"

#define PTDBG_TRACE_ROUTINES            0x00000001
#define PTDBG_TRACE_OPERATION_STATUS    0x00000002

#define PT_DBG_PRINT( _dbgLevel, _string )          \
    (FlagOn(gTraceFlags,(_dbgLevel)) ?              \
        DbgPrint _string :                          \
        ((int)0))


/*************************************************************************
Prototypes
*************************************************************************/

EXTERN_C_START

DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry(
	_In_ PDRIVER_OBJECT DriverObject,
	_In_ PUNICODE_STRING RegistryPath
);

NTSTATUS
engineInstanceSetup(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_SETUP_FLAGS Flags,
	_In_ DEVICE_TYPE VolumeDeviceType,
	_In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
);

VOID
engineInstanceTeardownStart(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
);

VOID
engineInstanceTeardownComplete(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
);

NTSTATUS
engineUnload(
	_In_ FLT_FILTER_UNLOAD_FLAGS Flags
);

NTSTATUS
engineInstanceQueryTeardown(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
);



EXTERN_C_END

//
//  Assign text sections for each routine.
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, engineUnload)
#pragma alloc_text(PAGE, engineInstanceQueryTeardown)
#pragma alloc_text(PAGE, engineInstanceSetup)
#pragma alloc_text(PAGE, engineInstanceTeardownStart)
#pragma alloc_text(PAGE, engineInstanceTeardownComplete)
#endif





#endif


