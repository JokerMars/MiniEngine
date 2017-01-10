#include "common.h"


ULONG_PTR OperationStatusCtx = 1;
ULONG gTraceFlags = 0;

PCHAR MonitoredProcess[MAXNUM] = { NULL };
UNICODE_STRING Ext[MAXNUM] = { 0 };
UNICODE_STRING MonitoredDirectory = { 0 };

ULONG g_curProcessNameOffset;

//
// initialize the above process and ext 
//
VOID InitMonitorVariable()
{
	MonitoredProcess[0] = "notepad.exe";
	//MonitoredProcess[0] = "acad.exe";
	//MonitoredProcess[1] = "WINWORD.EXE";
	//MonitoredProcess[2] = "ugraf.exe";
	//MonitoredProcess[3] = "explorer.exe";
	//MonitoredProcess[4] = "System";
	//MonitoredProcess[5] = "EXCEL.EXE";
	/*MonitoredProcess[1] = "Connect.Service.ContentService.exe";
	MonitoredProcess[2] = "AdAppMgr.exe";
	MonitoredProcess[3] = "AdAppMgrSvc.exe";
	MonitoredProcess[4] = "WSCommCntr4.exe";
	*/
	RtlInitUnicodeString(Ext + 0, L"txt");
	//RtlInitUnicodeString(Ext + 1, L"tmp");
	/*RtlInitUnicodeString(Ext+2, L"dwg");
	RtlInitUnicodeString(Ext+3, L"dwl2");*/

	RtlInitUnicodeString(&MonitoredDirectory, L"\\Device\\HarddiskVolume1\\Test");
}


BOOLEAN IsMonitoredProcess(PCHAR procName)
{
	int i;
	for (i = 0; i < MAXNUM; i++)
	{
		if (MonitoredProcess[i] == NULL)continue;
		if (strncmp(MonitoredProcess[i], procName, strlen(procName)) == 0)
		{
			return TRUE;
		}
	}

	return FALSE;
}


BOOLEAN IsMonitoredFileExt(PUNICODE_STRING ext)
{
	int i;
	for (i = 0; i < MAXNUM; i++)
	{
		if (Ext[i].Length == 0)continue;
		if (RtlCompareUnicodeString(&Ext[i], ext, TRUE) == 0)
		{
			return TRUE;
		}
	}

	return FALSE;
}


BOOLEAN IsMonitored(PCHAR procName, PUNICODE_STRING ext)
{
	if (IsMonitoredProcess(procName) && IsMonitoredFileExt(ext))
	{
		return TRUE;
	}

	return FALSE;
}



ULONG GetProcessNameOffset()
{
	PEPROCESS curproc = NULL;
	int i = 0;

	curproc = PsGetCurrentProcess();

	for (i = 0; i<3 * PAGE_SIZE; i++)
	{
		if (!strncmp("System", (PCHAR)curproc + i, strlen("System")))
		{
			return i;
		}
	}

	return 0;
}

PCHAR GetProcessName()
{
	PEPROCESS curproc = NULL;
	curproc = PsGetCurrentProcess();

	if (curproc != NULL)
	{
		return (PCHAR)curproc + g_curProcessNameOffset;
	}

	return NULL;
}

INT UnicodeStringIndexOf(UNICODE_STRING *sour, UNICODE_STRING *val)
{

	if ((sour->Length)<(val->Length))
	{

		return -1;
	}

	int i;

	int sour_len = sour->Length / 2;
	int val_len = val->Length / 2;
	int len = sour_len - val_len + 1;

	/*DbgPrint("sour is %wZ and val is %wZ",sour,val);
	DbgPrint("sour_len is %d and val_len is %d and len is %d",sour_len,val_len,len);*/
	for (i = 0; i<len; i++)
	{
		BOOLEAN flag = TRUE;
		for (int j = 0; j<val_len; j++)
		{

			WCHAR c1 = sour->Buffer[i + j];
			WCHAR c2 = val->Buffer[j];

			if (c1 != c2)
			{
				flag = FALSE;
				break;
			}
		}
		if (flag)
		{

			return i;
		}
	}
	return -1;
}

BOOLEAN IsMonitoredDirectory(PUNICODE_STRING path)
{
	if (UnicodeStringIndexOf(path, &MonitoredDirectory) == -1)
	{
		return FALSE;
	}

	return TRUE;
}