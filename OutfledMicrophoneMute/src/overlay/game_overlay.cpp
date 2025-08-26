#include "pch.h"
#include "appreg.h"
#include "overlay/game_overlay.h"
#include <string>
#include <vector>
#include <tuple>
#include <TlHelp32.h>
#include <DbgHelp.h>
#include <psapi.h>
#include <strsafe.h>
#include "Outfled Microphone MuteDlg.h"

#pragma comment( lib, "Dbghelp.lib" )



template<class ...T>	using Tuple_t	= std::tuple<T...>;
template<class T>		using Vector_t	= std::vector<T>;
using					String_t		= std::wstring;

static HKEY g_hOverlayKey;

static const LPCWSTR g_rgExcludeDirectories[] = {
	L"C:\\WINDOWS\\"
};
static const LPCWSTR g_rgExcludeFiles[] = {
	L"steam.exe",
	L"steamwebhelper.exe",
	L"NVIDIA Share.exe"
};

static DWORD GetRegisteredPrograms(Vector_t<Tuple_t<String_t, String_t>> *prgRegistered);
static DWORD GetRegisteredPrograms(Vector_t<Tuple_t<String_t, String_t>> *prgRegistered);
static DWORD RemoveRegisteredProgram(LPCWSTR lpszImageName);
static DWORD FindProcessModuleAddress(DWORD dwProcessId, LPCWSTR lpszModule, ULONG64 *pAddress);
static BOOL FullscreenWndCallback(HWND hWnd, LPARAM pFullscreenWnd);
static DWORD GetModuleFileHeaders(LPCWSTR lpszFile, IMAGE_NT_HEADERS *pNtHeader);
static BOOL IsModule64Bit(LPCWSTR lpszModulePath);
static LPWSTR GetProcessFullPath(HANDLE hProcess);
static DWORD InjectOverlayModule(DWORD dwProcessId, LPCWSTR lpszBinaryExe, LPCWSTR lpszOverlayModule);
static DWORD InjectOverlayModule(HANDLE hTargetProcess, LPCWSTR lpszBinaryExe, LPCWSTR lpszOverlayModule);
static DWORD AddRegisteredProgram(LPCWSTR lpszImageExeFile, LPCWSTR lpszOverlayModuleName);


DWORD FindProcessIdByName(LPCWSTR lpszProcessName)
{
    HANDLE          hSnapshot;
    PROCESSENTRY32W snapData;
    DWORD           dwProcessId;

    hSnapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    snapData.dwSize = sizeof( PROCESSENTRY32W );
    if (!Process32FirstW( hSnapshot, &snapData ))
    {
        CloseHandle( hSnapshot );
        return 0;
    }

    dwProcessId = 0;
    do
    {
        if (0 == _wcsicmp( snapData.szExeFile, lpszProcessName )) {
            dwProcessId = snapData.th32ProcessID;
        }
    } while (!dwProcessId && Process32NextW( hSnapshot, &snapData ));

    CloseHandle( hSnapshot );
    return dwProcessId;
}

DWORD SetInGameOverlayStatus(DWORD dwStatus, DWORD dwImageAnchor)
{
	DWORD dwResult;

	switch (dwStatus)
	{
	case INGAME_OVERLAY_STATUS_ENABLED:
		//
		// Set the in-game overlay properties & status
		//
		dwResult = SetAppRegistryValue( APP_REG_VALUE_OVERLAY_IMAGE_ANCHOR, &dwImageAnchor, sizeof( DWORD ) );
		if (dwResult == NO_ERROR) {
			dwResult = SetAppRegistryValue( APP_REG_VALUE_INGAME_OVERLAY_ENABLED, &dwStatus, sizeof( DWORD ) );
		}

		break;
	case INGAME_OVERLAY_STATUS_DISABLED:
	default:
		/* Set the in-game overlay status to disabled */
		dwResult = SetAppRegistryValue( APP_REG_VALUE_INGAME_OVERLAY_ENABLED, &dwImageAnchor, sizeof( DWORD ) );
		break;
	}

	return dwResult;
}

DWORD WINAPI InGameOverlayManager(LPVOID lpParam)
{
	LRESULT						lResult;
	DWORD						dwDisposition;
	COutfledMicrophoneMuteDlg	*pApp;

	pApp = (COutfledMicrophoneMuteDlg *)lpParam;

	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
	
	/* Open/create the overlay registry key */
	lResult = RegCreateKeyEx(HKEY_CURRENT_USER,
		APP_REG_PATH_OVERLAY,
		0,
		NULL,
		0,
		KEY_ALL_ACCESS,
		NULL,
		&g_hOverlayKey,
		&dwDisposition
	);
	if (lResult != ERROR_SUCCESS)
	{
		CloseHandle(pApp->m_hOverlayThread);
		pApp->m_hOverlayThread = NULL;
		return 1;
	}

	while (TRUE)
	{
		DWORD									bOverlayEnabled;
		Vector_t<Tuple_t<String_t, String_t>>   rgRegistered;
		DWORD									dwResult;
		Vector_t<DWORD>							rgdwProcessIds;

		Sleep(7500);

		/* Ensure the overlay is enabled from the registry value */
		GetAppRegistryValue(APP_REG_VALUE_INGAME_OVERLAY_ENABLED, &bOverlayEnabled, sizeof(DWORD));
		if (!bOverlayEnabled)
		{
			continue;
		}

		/* Get list of registered programs */
		GetRegisteredPrograms(&rgRegistered);

		//
		// Check if any of the registered programs are running
		for (LONGLONG i = 0; i < rgRegistered.size(); ++i)
		{
			LPCWSTR lpszProgramExePath	= std::get<0>(rgRegistered[i]).c_str();
			LPCWSTR lpszOverlayModule	= std::get<1>(rgRegistered[i]).c_str();
			LPCWSTR lpszProgramName		= PathFindFileName(lpszProgramExePath);

			if (!PathFileExists(lpszProgramExePath))
			{
				continue;
			}

			/* Find running process that matches the program exe name */
			DWORD dwProcessId = FindProcessIdByName(lpszProgramName);
			if (dwProcessId != 0)
			{
				UINT64 ModuleAddr;

				/* Ensure the overlay module is not already located in the process */
				dwResult = FindProcessModuleAddress(dwProcessId, lpszOverlayModule, &ModuleAddr);
				if (ModuleAddr != ULLONG_MAX)
				{
					continue;
				}

				/* Inject the overlay module */
				InjectOverlayModule(dwProcessId, lpszProgramExePath, lpszOverlayModule);
			}

			continue;
		}

		//
		// Try to determine if a full screen application is opened
		if (rgRegistered.size() == 0)
		{
			QUERY_USER_NOTIFICATION_STATE	qState;
			UINT64							ModuleAddr;

			/* Check the computer notification state to determine if a fullscreen app is running */
			SHQueryUserNotificationState(&qState);
			if (qState == QUNS_RUNNING_D3D_FULL_SCREEN)
			{
				//
				// Enumerate all windows and find the fullscreen app window
				//
				HWND hFullscreenWnd = NULL;
				if (!EnumWindows(FullscreenWndCallback, (LPARAM)&hFullscreenWnd) && hFullscreenWnd)
				{
					/* Get the process ID of the fullscreen process */
					DWORD dwGameProcessId;
					GetWindowThreadProcessId(hFullscreenWnd, &dwGameProcessId);

					//
					// Open the program & get the exe path
					HANDLE hGameProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwGameProcessId);
					if (hGameProcess)
					{
						LPWSTR	lpszProcessBinaryPath	= GetProcessFullPath(hGameProcess);
						LPCWSTR lpszModuleName			= NULL;

						CloseHandle(hGameProcess);

						//
						// Ensure the overlay module is not already located in the process
						BOOL bInjected = TRUE;
						if (IsModule64Bit(lpszProcessBinaryPath))
						{
							lpszModuleName = OUTFLED_OVERLAY_MODULE_NAME64;
							if (!FindProcessModuleAddress(dwGameProcessId, OUTFLED_OVERLAY_MODULE_NAME64, &ModuleAddr))
							{
								bInjected = FALSE;
								InjectOverlayModule(hGameProcess, lpszProcessBinaryPath, OUTFLED_OVERLAY_MODULE_NAME64);
							}
						}
						else
						{
							lpszModuleName = OUTFLED_OVERLAY_MODULE_NAME32;
							if (!FindProcessModuleAddress(dwGameProcessId, OUTFLED_OVERLAY_MODULE_NAME32, &ModuleAddr))
							{
								bInjected = FALSE;
								InjectOverlayModule(hGameProcess, lpszProcessBinaryPath, OUTFLED_OVERLAY_MODULE_NAME32);
							}
						}

						/* Register the prgram */
						AddRegisteredProgram(lpszProcessBinaryPath, lpszModuleName);
						LocalFree(lpszProcessBinaryPath);
					}
				}
			}
		}
	}

	CloseHandle(pApp->m_hOverlayThread);
	pApp->m_hOverlayThread = NULL;
	return 1;
}


DWORD GetRegisteredPrograms( Vector_t<Tuple_t<String_t, String_t>> *prgRegistered )
{
    BOOL    bSuccess;
    DWORD   dwRegisteredCount;
	LRESULT lResult;

    //
    // Enumerate the 'Overlay' subkeys 
	lResult = RegQueryInfoKey(g_hOverlayKey,
		NULL, NULL, NULL,
		&dwRegisteredCount,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL
	);
	if (lResult != ERROR_SUCCESS)
	{
		return GetLastError();
	}
 
    for (DWORD dwEnumProgram = 0; dwEnumProgram < dwRegisteredCount; ++dwEnumProgram)
    {
		HKEY	hEnumKey;
		WCHAR	szKeyName[256];
        WCHAR	szModuleFile[100]{};
		WCHAR	szBinaryPath[MAX_PATH + 1]{};
		DWORD	cbBuffer;

		hEnumKey = NULL;

		//
		// Open the next key 
		lResult = RegEnumKeyExW(g_hOverlayKey,
			dwEnumProgram,
			szKeyName,
			&(cbBuffer = MAX_PATH + 1),
			NULL,
			NULL,
			NULL,
			NULL
		);
		if (lResult == ERROR_SUCCESS) {
			lResult = RegOpenKeyEx(g_hOverlayKey, szKeyName, 0, KEY_ALL_ACCESS, &hEnumKey);
		}

		if (lResult != ERROR_SUCCESS)
		{
			continue;
		}
				
		//
		// Fetch the dll module name & the exe file path
		lResult = RegQueryValueExW(hEnumKey, APP_REG_VALUE_BINARY_PATH, NULL, NULL, (PBYTE)szModuleFile, &(cbBuffer = sizeof(szModuleFile)));
		if (lResult == ERROR_SUCCESS) {
			lResult = RegQueryValueExW(hEnumKey, APP_REG_VALUE_SELECTED_MODULE, NULL, NULL, (PBYTE)szBinaryPath, &(cbBuffer = sizeof(szBinaryPath)));
		}

		RegCloseKey(hEnumKey);
		if (lResult == ERROR_SUCCESS)
		{
			PathAddExtension(szModuleFile, L".exe");
			prgRegistered->push_back(std::make_tuple(szModuleFile, szBinaryPath));
		}
    }

    return NO_ERROR;
}
DWORD RemoveRegisteredProgram(LPCWSTR lpszImageName)
{
	return (DWORD)RegDeleteKeyW(g_hOverlayKey, lpszImageName);
}
DWORD FindProcessModuleAddress(DWORD dwProcessId, LPCWSTR lpszModule, ULONG64 *pAddress)
{
	HANDLE			hModuleSnapshot;
	MODULEENTRY32W	meSnapshotData;

	*pAddress = ULLONG_MAX;

	//
	// Take snapshot of all loaded modules in process
	//
	hModuleSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwProcessId);
	if (hModuleSnapshot == INVALID_HANDLE_VALUE) {
		return GetLastError();
	}

	meSnapshotData.dwSize = sizeof(MODULEENTRY32W);
	if (!Module32First(hModuleSnapshot, &meSnapshotData))
	{
		DWORD dwError = GetLastError();
		CloseHandle(hModuleSnapshot);

		return dwError;
	}


	//
	// Locate the module
	//
	do
	{
		if (0 == _wcsicmp(PathFindFileName(lpszModule), meSnapshotData.szModule)) {
			*pAddress = (ULONG64)meSnapshotData.modBaseAddr;
			break;
		}
	} while (Module32Next(hModuleSnapshot, &meSnapshotData));

	CloseHandle(hModuleSnapshot);
	return (*pAddress == ULLONG_MAX) ? ERROR_MOD_NOT_FOUND : NO_ERROR;
}
BOOL FullscreenWndCallback( HWND hWnd, LPARAM pFullscreenWnd )
{
    HWND        *phFullscreenWnd;
    RECT        rcWnd;
   
    phFullscreenWnd     = (HWND *)pFullscreenWnd;
    *phFullscreenWnd    = NULL;

    GetWindowRect( hWnd, &rcWnd );

    /* Ensure the window is visible and not minimized */
    if (IsWindowVisible( hWnd ) && !IsIconic( hWnd ))
    {
        //
        // Get the monitor size that the window is 
        //
        MONITORINFO mScreenInfo = { sizeof( MONITORINFO ) };
        if (GetMonitorInfo( MonitorFromWindow( hWnd, MONITOR_DEFAULTTOPRIMARY ), &mScreenInfo ))
        {
            if (mScreenInfo.rcMonitor.left == rcWnd.left && mScreenInfo.rcMonitor.right == rcWnd.right &&
                mScreenInfo.rcMonitor.top == rcWnd.top && mScreenInfo.rcMonitor.bottom == rcWnd.bottom)
            {
                *phFullscreenWnd = hWnd;
            }
        }
    }

    return (*phFullscreenWnd == NULL);
}
DWORD GetModuleFileHeaders(LPCWSTR lpszFile, IMAGE_NT_HEADERS *pNtHeader)
{
	HANDLE	hModuleFile;
	HANDLE	hFileMap;
	DWORD	dwError;

	dwError = NO_ERROR;

	hModuleFile = CreateFileW(lpszFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hModuleFile == INVALID_HANDLE_VALUE) {
		return GetLastError();
	}

	hFileMap = CreateFileMapping(
		hModuleFile,
		NULL,
		PAGE_READONLY,
		0,
		0,
		NULL
	);
	if (hFileMap)
	{
		LPVOID lpMappedFile = MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 0);
		if (lpMappedFile)
		{
			PIMAGE_NT_HEADERS pHeader = ImageNtHeader(lpMappedFile);
			if (pHeader) {
				CopyMemory(pNtHeader, pHeader, sizeof(IMAGE_NT_HEADERS));
			}

			UnmapViewOfFile(lpMappedFile);
		}
		else {
			dwError = GetLastError();
		}

		CloseHandle(hFileMap);
	}
	else {
		dwError = GetLastError();
	}

	CloseHandle(hModuleFile);
	return dwError;
}
BOOL IsModule64Bit(LPCWSTR lpszModulePath)
{
	DWORD				dwResult;
	IMAGE_NT_HEADERS	ntFileHeader;

	dwResult = GetModuleFileHeaders(lpszModulePath, &ntFileHeader);
	if (dwResult != NO_ERROR) {
		return FALSE;
	}

	return !(ntFileHeader.FileHeader.Machine == IMAGE_FILE_MACHINE_I386);
}
LPWSTR GetProcessFullPath(HANDLE hProcess)
{
	WCHAR szFileName[512]{};
	GetModuleFileNameExW(hProcess, NULL, szFileName, ARRAYSIZE(szFileName));

	return StrDup(szFileName);
}
DWORD InjectOverlayModule(DWORD dwProcessId, LPCWSTR lpszBinaryExe, LPCWSTR lpszOverlayModule)
{
	WCHAR				szOverlayModFullPath[MAX_PATH];
	WCHAR				szOverlayHelperPath[MAX_PATH];
	WCHAR				szBuffer[MAX_PATH];
	WCHAR				szProcessCmdLine[512];
	STARTUPINFO			StartupInfo;
	PROCESS_INFORMATION ProcessInfo;
	BOOL				bSuccess;

	ZeroMemory(&StartupInfo, sizeof(STARTUPINFO));

	StartupInfo.cb = sizeof(StartupInfo);

	GetCurrentDirectory(MAX_PATH, szBuffer);

	/* Format the full path of the overlay module (dll) file */
	StringCbPrintf(szOverlayModFullPath,
		sizeof(szOverlayModFullPath),
		L"%s\\%s\\%s",
		szBuffer,
		OUTFLED_OVERLAY_DIRECTORY,
		lpszOverlayModule
	);
	PathQuoteSpaces(szOverlayModFullPath);

	//
	// Format the full path of the overlay helper exe file
	if (0 == _wcsicmp(lpszOverlayModule, OUTFLED_OVERLAY_MODULE_NAME32))
	{
		StringCbPrintf(szOverlayHelperPath, sizeof(szOverlayHelperPath), L"%s\\%s\\OverlayHelper.exe", szBuffer,OUTFLED_OVERLAY_DIRECTORY);
	}
	else
	{
		StringCbPrintf(szOverlayHelperPath, sizeof(szOverlayHelperPath), L"%s\\%s\\OverlayHelper64.exe", szBuffer, OUTFLED_OVERLAY_DIRECTORY);
	}

	PathQuoteSpaces(szOverlayHelperPath);

	/* Format the arguments for overlay helper */
	StringCbPrintf(szProcessCmdLine,
		sizeof(szProcessCmdLine),
		L"%s %s %d %s",
		szOverlayHelperPath,
		OVERLAY_HELPER_PROCESSID_ARG,
		dwProcessId,
		szOverlayModFullPath
	);

	/* Launch the overlay helper to inject the module */
	bSuccess = CreateProcess(0,//szOverlayHelperPath,
		szProcessCmdLine,
		NULL,
		NULL,
		FALSE,
		CREATE_NEW_CONSOLE,
		NULL,
		NULL,
		&StartupInfo,
		&ProcessInfo
	);
	if (bSuccess)
	{
		DWORD dwResult = WaitForSingleObject(ProcessInfo.hProcess, 4000);
		if (dwResult != WAIT_OBJECT_0) {
			TerminateProcess(ProcessInfo.hProcess, OVERLAY_ERROR_UNKNOWN);
		}

		GetExitCodeProcess(ProcessInfo.hProcess, &dwResult);
		CloseHandle(ProcessInfo.hProcess);
		CloseHandle(ProcessInfo.hThread);

		return dwResult;
	}

	return GetLastError();
}
DWORD InjectOverlayModule(HANDLE hTargetProcess, LPCWSTR lpszBinaryExe, LPCWSTR lpszOverlayModule)
{
	return InjectOverlayModule(GetProcessId(hTargetProcess), lpszBinaryExe, lpszOverlayModule);
}
DWORD AddRegisteredProgram(LPCWSTR lpszImageExeFile, LPCWSTR lpszOverlayModuleName)
{
	WCHAR	szSubkeyName[MAX_PATH];
	LRESULT lResult;
	HKEY	hRegisteredKey;

	StringCbCopy(szSubkeyName, sizeof(szSubkeyName), PathFindFileName(lpszImageExeFile));
	PathRemoveExtension(szSubkeyName);

	//
	// Add a subkey of the exe name
	lResult = RegCreateKeyEx(g_hOverlayKey,
		szSubkeyName,
		0,
		NULL,
		0,
		KEY_ALL_ACCESS,
		NULL,
		&hRegisteredKey,
		NULL
	);
	if (lResult == ERROR_SUCCESS)
	{
		lResult = RegSetValueEx(hRegisteredKey, APP_REG_VALUE_BINARY_PATH, 0, REG_SZ, (PBYTE)lpszImageExeFile, (wcslen(lpszImageExeFile) + 1) * sizeof(WCHAR));
		if (lResult == ERROR_SUCCESS) {
			lResult = RegSetValueEx(hRegisteredKey,
				APP_REG_VALUE_BINARY_PATH,
				0,
				REG_SZ,
				(PBYTE)lpszOverlayModuleName,
				(wcslen(lpszOverlayModuleName) + 1) * sizeof(WCHAR)
			);
		}

		RegCloseKey(hRegisteredKey);
		if (lResult != ERROR_SUCCESS)
		{
			RemoveRegisteredProgram(szSubkeyName);
		}
	}

	return lResult;
}
