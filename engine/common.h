#pragma once

#ifndef _COMMON_H
#define _COMMON_H


#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")

extern ULONG_PTR OperationStatusCtx;
extern ULONG gTraceFlags;

//
// define the number of monitored process
//
#define MAXNUM 10
extern PCHAR MonitoredProcess[MAXNUM];
extern UNICODE_STRING Ext[MAXNUM];
extern UNICODE_STRING MonitoredDirectory;

extern ULONG g_curProcessNameOffset;


//
// The pool tag
//


#define BUFFER_SWAP_TAG     'bdBS'
#define CONTEXT_TAG         'xcBS'
#define NAME_TAG            'mnBS'
#define PRE_2_POST_TAG      'ppBS'

#define STRING_TAG                        'tSxC'
#define RESOURCE_TAG                      'cRxC'
#define STREAM_CONTEXT_TAG                'cSxC'
#define FILEFLAG_POOL_TAG				  'CVXC'

#define SECTOR_SIZE 0X200
#define FILE_GUID_LENGTH 16
#define HASH_SIZE 20
#define MAX_PATH 260



VOID InitMonitorVariable();

BOOLEAN IsMonitoredProcess(PCHAR procName);

BOOLEAN IsMonitoredFileExt(PUNICODE_STRING ext);

BOOLEAN IsMonitored(PCHAR procName, PUNICODE_STRING ext);

BOOLEAN IsMonitoredDirectory(PUNICODE_STRING path);

ULONG GetProcessNameOffset();

PCHAR GetProcessName();





#endif