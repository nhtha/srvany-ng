#include <Windows.h>

#define MAX_DATA_LENGTH 8192
#define MAX_KEY_LENGTH MAX_PATH
#define SERVICE_NAME TEXT("srvany-ng")

SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE                g_ServiceStopEvent = INVALID_HANDLE_VALUE;
PROCESS_INFORMATION   g_Process;

DWORD WINAPI ServiceWorkerThread(LPVOID lpParam)
{
	while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0)
	{
		Sleep(1000);
	}

	return ERROR_SUCCESS;
}

void ServiceSetState(DWORD acceptedControls, DWORD newState, DWORD exitCode)
{
	SERVICE_STATUS serviceStatus;
	ZeroMemory(&serviceStatus, sizeof(SERVICE_STATUS));
	serviceStatus.dwCheckPoint = 0;
	serviceStatus.dwControlsAccepted = acceptedControls;
	serviceStatus.dwCurrentState = newState;
	serviceStatus.dwServiceSpecificExitCode = 0;
	serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	serviceStatus.dwWaitHint = 0;
	serviceStatus.dwWin32ExitCode = exitCode;

	if (SetServiceStatus(g_StatusHandle, &serviceStatus) == FALSE)
	{
		OutputDebugString(TEXT("SetServiceStatus failed\n"));
	}
}

void WINAPI ServiceCtrlHandler(DWORD CtrlCode)
{
	switch (CtrlCode)
	{
	case SERVICE_CONTROL_STOP:
		SetEvent(g_ServiceStopEvent); //Kill the worker thread
		TerminateProcess(g_Process.hProcess, 0); //Kill the target process
		ServiceSetState(0, SERVICE_STOPPED, 0);
		break;

	case SERVICE_CONTROL_PAUSE:
		ServiceSetState(0, SERVICE_PAUSED, 0);
		break;

	case SERVICE_CONTROL_CONTINUE:
		ServiceSetState(0, SERVICE_RUNNING, 0);
		break;

	default:
		break;
	}
}

void WINAPI ServiceMain(DWORD argc, TCHAR *argv[])
{
	Sleep(10000);

	g_StatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);

	if (g_StatusHandle == NULL)
	{
		ServiceSetState(0, SERVICE_STOPPED, GetLastError());
		return;
	}

	g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (g_ServiceStopEvent == NULL)
	{
		ServiceSetState(0, SERVICE_STOPPED, GetLastError());
		return;
	}

	HKEY openedKey;
	DWORD cbData = MAX_DATA_LENGTH;

	TCHAR keyPath[MAX_KEY_LENGTH];
	wsprintf(keyPath, TEXT("%s%s%s"), TEXT("SYSTEM\\CurrentControlSet\\Services\\"), argv[0], TEXT("\\Parameters\\"));

	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyPath, 0, KEY_READ, &openedKey) != ERROR_SUCCESS)
	{
		OutputDebugString(TEXT("Faileed to open service parameters key\n"));
		ServiceSetState(0, SERVICE_STOPPED, 0);
		return;
	}

	TCHAR applicationString[MAX_DATA_LENGTH] = TEXT("");
	if (RegQueryValueEx(openedKey, TEXT("Application"), NULL, NULL, (LPBYTE)applicationString, &cbData) != ERROR_SUCCESS)
	{
		OutputDebugString(TEXT("Failed to open Application value\n"));
		ServiceSetState(0, SERVICE_STOPPED, 0);
		return;
	}

	TCHAR applicationParameters[MAX_DATA_LENGTH] = TEXT("");
	if (RegQueryValueEx(openedKey, TEXT("AppParameters"), NULL, NULL, (LPBYTE)applicationParameters, &cbData) != ERROR_SUCCESS)
	{
		OutputDebugString(TEXT("AppParameters key not found. Non fatal.\n"));
	}

	TCHAR applicationEnvironment[MAX_DATA_LENGTH] = TEXT("");
	if (RegQueryValueEx(openedKey, TEXT("AppParameters"), NULL, NULL, (LPBYTE)applicationEnvironment, &cbData) != ERROR_SUCCESS)
	{
		OutputDebugString(TEXT("AppEnvironment key not found. Non fatal.\n"));
	}

	TCHAR applicationDirectory[MAX_DATA_LENGTH] = TEXT("");
	GetCurrentDirectory(MAX_DATA_LENGTH, applicationDirectory);
	if (RegQueryValueEx(openedKey, TEXT("AppParameters"), NULL, NULL, (LPBYTE)applicationDirectory, &cbData) != ERROR_SUCCESS)
	{
		OutputDebugString(TEXT("AppDirectory key not found. Non fatal.\n"));
	}

	STARTUPINFO startupInfo;
	ZeroMemory(&startupInfo, sizeof(STARTUPINFO));
	startupInfo.cb = sizeof(STARTUPINFO);
	startupInfo.wShowWindow = 0;
	startupInfo.lpReserved = NULL;
	startupInfo.cbReserved2 = 0;
	startupInfo.lpReserved2 = NULL;

	if (CreateProcess(NULL, applicationString, NULL, NULL, FALSE, CREATE_NO_WINDOW, applicationEnvironment, applicationDirectory, &startupInfo, &g_Process))
	{
		ServiceSetState(SERVICE_ACCEPT_STOP, SERVICE_RUNNING, 0);
		HANDLE hThread = CreateThread(NULL, 0, ServiceWorkerThread, NULL, 0, NULL);
		WaitForSingleObject(hThread, INFINITE); //Wait here for a stop signal
		CloseHandle(g_ServiceStopEvent);
	}

	ServiceSetState(0, SERVICE_STOPPED, 0);
}


int main(int argc, TCHAR *argv[])
{
	SERVICE_TABLE_ENTRY ServiceTable[] =
	{
		{ SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain },
		{ NULL, NULL }
	};

	if (StartServiceCtrlDispatcher(ServiceTable) == FALSE)
	{
		return GetLastError();
	}

	return 0;
}