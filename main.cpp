#include "stdafx.h"
#include <Ole2.h>
#include "DownshaEncoder.h"
#include "Log.h"

CDownshaEncoder theEncoder;   // Global unique encoder instance
HANDLE hCtrlEvent   = NULL;   // Ctrl event to notify main thread that user is closing the window.
HANDLE hExitEvent   = NULL;   // Exit event to notify system thread that encoder is exited now.
TCHAR  szTitle[256] = _T(""); // Original console title

void RunEncoder(LPCSTR szParamFile)
{
	DWORD dwRet = 0;

	if (!theEncoder.Start(szParamFile))
	{
		theEncoder.Stop();
		printf("DownshaEncoder is running failed.\n");
		return ;
	}
	printf("DownshaEncoder is running successfully. Click close button to terminate.\n");

	hCtrlEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	hExitEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	// Save console title before running encoder
	GetConsoleTitle(szTitle, sizeof(szTitle) / sizeof(szTitle[0]));

	while (true)
	{
		theEncoder.PrintStat();
		dwRet = WaitForSingleObject(hCtrlEvent, 1000);
		if (dwRet == WAIT_OBJECT_0)
			break;
	}

	// Restore console title after running encoder
	SetConsoleTitle(szTitle);

	printf("DownshaEncoder is terminating...\n");
	theEncoder.Stop();
	printf("DownshaEncoder is terminated.\n");

	SetEvent(hExitEvent);
	CloseHandle(hExitEvent);
	CloseHandle(hCtrlEvent);
}

void StopEncoder()
{
	SetEvent(hCtrlEvent);
	WaitForSingleObject(hExitEvent, 5000);
}

/********************************************************************************
 * http://msdn.microsoft.com/en-us/library/ms686016(v=vs.85).aspx
 * An application-defined function used with the SetConsoleCtrlHandler function. 
 * A console process uses this function to handle control signals received by 
 * the process. When the signal is received, the system creates a new thread in 
 * the process to execute the function.
 *
 * When a CTRL+C signal is received, the control handler returns TRUE, indicating 
 * that it has handled the signal. Doing this prevents other control handlers from 
 * being called. When a CTRL_CLOSE_EVENT signal is received, the control handler 
 * returns TRUE and the process terminates.
 *
 * When a CTRL_BREAK_EVENT, CTRL_LOGOFF_EVENT, or CTRL_SHUTDOWN_EVENT signal is 
 * received, the control handler returns FALSE. Doing this causes the signal to be 
 * passed to the next control handler function. If no other control handlers have 
 * been registered or none of the registered handlers returns TRUE, the default 
 * handler will be used, resulting in the process being terminated.
 *
 * If a console process is being debugged and CTRL+C signals have not been disabled, 
 * the system generates a DBG_CONTROL_C exception. This exception is raised only for 
 * the benefit of the debugger, and an application should never use an exception 
 * handler to deal with it. If the debugger handles the exception, an application will 
 * not notice the CTRL+C, with one exception: alertable waits will terminate. If the 
 * debugger passes the exception on unhandled, CTRL+C is passed to the console process 
 * and treated as a signal, as previously discussed.
 **************************************************************************/
BOOL ConsoleCtrlHandler(DWORD dwCtrlType)
{
	printf("ConsoleCtrlHandler received %s.\t\t\t\t\n",
		(dwCtrlType == CTRL_C_EVENT)        ? "CTRL_C_EVENT"        : 
		(dwCtrlType == CTRL_CLOSE_EVENT)    ? "CTRL_CLOSE_EVENT"    : 
		(dwCtrlType == CTRL_BREAK_EVENT)    ? "CTRL_BREAK_EVENT"    : 
		(dwCtrlType == CTRL_LOGOFF_EVENT)   ? "CTRL_LOGOFF_EVENT"   : 
		(dwCtrlType == CTRL_SHUTDOWN_EVENT) ? "CTRL_SHUTDOWN_EVENT" : "UNKNOWN_EVENT");

	switch (dwCtrlType)
	{
	case CTRL_C_EVENT:
	case CTRL_CLOSE_EVENT:
		StopEncoder();
		return TRUE;
	case CTRL_BREAK_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		StopEncoder();
		return FALSE;
	}

	return FALSE;
}

int _tmain(int argc, _TCHAR* argv[])
{
	char szParamFile[64] = "DownshaEncoder.conf";

	if (argc > 1)
		WideCharToMultiByte(CP_ACP, 0, argv[1], -1, szParamFile, sizeof(szParamFile), NULL, NULL);

	// This function provides a similar notification for console application and services 
	// that WM_QUERYENDSESSION provides for graphical applications with a message pump.
	if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleCtrlHandler, TRUE))
		printf("SetConsoleCtrlHandler failed. Error: %d\n", GetLastError());

	// OleInitialize calls CoInitializeEx internally to initialize the COM library on the current apartment. 
	// Because OLE operations are not thread-safe, OleInitialize specifies the concurrency model as single-thread apartment.
	::OleInitialize(NULL);
	LOG_INIT(LOG_LEVEL_DEFAULT, LOG_FILE | LOG_FILE_PATH_CURRENT | LOG_FILE_NAME_DATE);

	// Run encoder now, the main thread will block until Ctrl+C or Close signal is received.
	RunEncoder(szParamFile);

	LOG_FINI();
	::OleUninitialize();
	return 0;
}
