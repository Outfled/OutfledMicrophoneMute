#include <Windows.h>
#include <Shlwapi.h>
#include <strsafe.h>
#include <TlHelp32.h>
#include <overlay/game_overlay.h>

#pragma comment( lib, "Shlwapi.lib" )


//
// Mapping Data

#define DOS_IMAGE_MAGIC			0x5A4D
#define RELOC_FLAG32(r)			((r >> 0x0C) == IMAGE_REL_BASED_HIGHLOW)
#define RELOC_FLAG64(r)			((r >> 0x0C) == IMAGE_REL_BASED_DIR64)
#ifdef _WIN64
#define RELOC_FLAG RELOC_FLAG64
#else
#define RELOC_FLAG RELOC_FLAG32
#endif // _WIN64

typedef HINSTANCE(WINAPI *pfnLoadLibraryA)(LPCSTR lpszFileName);
typedef UINT_PTR(WINAPI *pfnGetProcAddress)(HINSTANCE, LPCSTR);
typedef BOOL(WINAPI *pfnDllEntryPoint)(void *hModule, DWORD dwReason, LPVOID lpReserved);

typedef enum InjectMode
{
    IM_NORMAL_INJECT,
    IM_MANUAL_MAP
}InjectMode;
typedef struct MANUAL_MAPPING_DATA
{
    pfnLoadLibraryA		pLoadLibrary;
    pfnGetProcAddress	pGetProcAddress;
    HINSTANCE			hModule;
}MANUAL_MAPPING_DATA;

static DWORD InjectOverlayModule(HANDLE hProcess, LPCWSTR lpszModuleFile);
static DWORD ManualMapOverlayModule(HANDLE hProcess, LPCWSTR lpszModuleFile);
static DWORD __stdcall RemoteShellCode(MANUAL_MAPPING_DATA *pMapData);

int wmain(int argc, wchar_t *argv[])
{
    DWORD       dwProcessId;
    LPWSTR      lpszOverlayModuleFile;
    InjectMode  iMode;
    HANDLE      hInjectProcess;

    dwProcessId             = 0;
    lpszOverlayModuleFile   = NULL;
    iMode                   = IM_NORMAL_INJECT;

    for (size_t i = 1; i < argc; ++i)
    {
        LPCWSTR lpszArg = argv[i];
        if (lpszArg[0] != L'-')
        {
            lpszOverlayModuleFile = lpszArg;
            continue;
        }
        else
        {
            if (0 == _wcsicmp(lpszArg, OVERLAY_HELPER_PROCESSID_ARG))
            {
                if (i + 1 < argc)
                {
                    dwProcessId = (DWORD)_wtoi(argv[i + 1]);
                    ++i;
                    continue;
                }     
            }
            else if (0 == _wcsicmp(lpszArg, OVERLAY_HELPER_MANUAL_MAP_ARG))
            {
                iMode = IM_MANUAL_MAP;
                continue;
            }
            else if (0 == _wcsicmp(lpszArg, OVERLAY_HELPER_DEFAULT_INJ_ARG))
            {
                iMode = IM_NORMAL_INJECT;
                continue;
            }
        }
    }
    if (!dwProcessId || !lpszOverlayModuleFile)
    {
        return EXIT_FAILURE;
    }

    /* Open the target process */
    hInjectProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessId);
    if (!hInjectProcess)
    {
        return EXIT_FAILURE;
    }

	wprintf(L"%s\t%d", lpszOverlayModuleFile, dwProcessId);

    /* Inject the dll */
    DWORD dwResult = 0;
    switch (iMode)
    {
		default:
			dwResult = InjectOverlayModule(hInjectProcess, lpszOverlayModuleFile);
			break;
       // case IM_NORMAL_INJECT:
       //     dwResult = InjectOverlayModule(hInjectProcess, lpszOverlayModuleFile);
       //     break;
       // case IM_MANUAL_MAP:
       //     dwResult = ManualMapOverlayModule(hInjectProcess, lpszOverlayModuleFile);
       //    break;
    }

	wprintf(L"%d", dwResult);

    CloseHandle(hInjectProcess);
    return (int)dwResult;
}

DWORD InjectOverlayModule(HANDLE hProcess, LPCWSTR lpszModuleFile)
{
    LPVOID  pLoadLibrary;
    LPVOID  pAllocated;
    HANDLE  hThread;
    DWORD   cbModulePath;
    DWORD   dwThreadId;

    pLoadLibrary = GetProcAddress(GetModuleHandleW(L"Kernel32.dll"), "LoadLibraryW");
    cbModulePath = (wcslen(lpszModuleFile) + 1) * sizeof(WCHAR);

    //
    // Allocate memory in the process to hold the module (dll) file path string
    pAllocated = VirtualAllocEx(hProcess, NULL, cbModulePath, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pAllocated)
    {
        DWORD dwError = GetLastError();
        return dwError;
    }

    //
    // Write the module file path to the allocated memory
    BOOL bSuccess = WriteProcessMemory(hProcess, pAllocated, lpszModuleFile, cbModulePath, NULL);
    if (!bSuccess)
    {
        DWORD dwError = GetLastError();
        VirtualFreeEx(hProcess, pAllocated, 0, MEM_RELEASE);

        return dwError;
    }

    //
    // Call LoadLibraryW in the process to load the module
    hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLibrary, pAllocated, 0, &dwThreadId);
    if (!hThread)
    {
        DWORD dwError = GetLastError();
        VirtualFreeEx(hProcess, pAllocated, 0, MEM_RELEASE);

        return dwError;
    }
    if (WAIT_TIMEOUT != WaitForSingleObject(hThread, 1000))
    {
        VirtualFreeEx(hProcess, pAllocated, 0, MEM_RELEASE);
    }

    return NO_ERROR;
}

//
// TODO: Fix manual mapping
DWORD ManualMapOverlayModule(HANDLE hProcess, LPCWSTR lpszModuleFile)
{
	PBYTE					pbModuleData;
	DWORD					cbModuleData;
	IMAGE_NT_HEADERS		*pOldNtHeader;
	IMAGE_OPTIONAL_HEADER	*pOldOptHeader;
	IMAGE_FILE_HEADER		*pOldFileHeader;
	PBYTE					pbTargetBase;
	HANDLE					hModuleFile;
	MANUAL_MAPPING_DATA		mapData;

	pbModuleData			= NULL;
	mapData.pLoadLibrary	= LoadLibraryA;
	mapData.pGetProcAddress = (pfnGetProcAddress)GetProcAddress;

	ZeroMemory(&mapData, sizeof(mapData));

	//
	// Read the DLL file data
	hModuleFile = CreateFileW(lpszModuleFile, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hModuleFile != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER liFileSize;
		BOOL			bSuccess;
		DWORD			dwError;

		dwError = NO_ERROR;

		bSuccess = GetFileSizeEx(hModuleFile, &liFileSize);
		if (bSuccess)
		{
			if (liFileSize.QuadPart >= 0x1000)
			{
				pbModuleData = HeapAlloc(GetProcessHeap(), 0, liFileSize.QuadPart);
				
				bSuccess = ReadFile(hModuleFile, pbModuleData, liFileSize.QuadPart, &cbModuleData, NULL);
				if (!bSuccess)
				{
					dwError = GetLastError();
					HeapFree(GetProcessHeap(), 0, pbModuleData);
				}
			}
			else
			{
				dwError = ERROR_FILE_CORRUPT;
			}
		}
		else
		{
			dwError = GetLastError();
		}

		CloseHandle(hModuleFile);
		if (dwError != NO_ERROR)
		{
			return dwError;
		}
	}

	/* Validate the DOS header */
	IMAGE_DOS_HEADER *pDOSHeader = (IMAGE_DOS_HEADER *)pbModuleData;
	if (pDOSHeader->e_magic != DOS_IMAGE_MAGIC)
	{
		HeapFree(GetProcessHeap(), 0, pbModuleData);
		return ERROR_FILE_INVALID;
	}

	//
	// Get the NT headers
	pOldNtHeader	= (IMAGE_NT_HEADERS *)(pbModuleData + pDOSHeader->e_lfanew);
	pOldOptHeader	= &pOldNtHeader->OptionalHeader;
	pOldFileHeader	= &pOldNtHeader->FileHeader;

	//
	// Validate the architecture type
#ifdef _WIN64
	if (pOldFileHeader->Machine != IMAGE_FILE_MACHINE_AMD64)
	{
		HeapFree(GetProcessHeap(), 0, pbModuleData);
		return ERROR_BAD_EXE_FORMAT;
	}
#else
	if (pOldFileHeader->Machine != IMAGE_FILE_MACHINE_I386)
	{
		HeapFree(GetProcessHeap(), 0, pbModuleData);
		return ERROR_BAD_EXE_FORMAT;
	}
#endif //_WIN64

	//
	// Allocate memory in the target process
	pbTargetBase = (PBYTE)VirtualAllocEx(hProcess,
		(LPVOID)pOldOptHeader->ImageBase,
		pOldOptHeader->SizeOfImage,
		MEM_COMMIT | MEM_RESERVE,
		PAGE_EXECUTE_READWRITE
	);
	if (pbTargetBase == NULL)
	{
		pbTargetBase = (PBYTE)VirtualAllocEx(hProcess, NULL, pOldOptHeader->SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	}

	if (!pbTargetBase)
	{
		DWORD dwError = GetLastError();

		HeapFree(GetProcessHeap(), 0, pbModuleData);
		return (dwError == NO_ERROR) ? ERROR_ACCESS_DENIED : dwError;
	}

	//
	// Set the mapping data
	IMAGE_SECTION_HEADER *pSectionHeader = IMAGE_FIRST_SECTION(pOldNtHeader);
	for (UINT i = 0; i != pOldFileHeader->NumberOfSections; ++i, ++pSectionHeader)
	{
		if (pSectionHeader->SizeOfRawData)
		{
			BOOL bSuccess = WriteProcessMemory(hProcess,
				pbTargetBase + pSectionHeader->VirtualAddress,
				pbModuleData + pSectionHeader->PointerToRawData,
				pSectionHeader->SizeOfRawData,
				NULL
			);
			if (!bSuccess)
			{
				DWORD dwError = GetLastError();
				VirtualFreeEx(hProcess, (LPVOID)pbTargetBase, pOldOptHeader->SizeOfImage, MEM_RELEASE);
				HeapFree(GetProcessHeap(), 0, pbModuleData);

				return (dwError == NO_ERROR) ? ERROR_ACCESS_DENIED : dwError;
			}
		}
	}

	//
	// Write the map to the target base
	CopyMemory(pbModuleData, &mapData, sizeof(mapData));
	WriteProcessMemory(hProcess, pbTargetBase, pbModuleData, 0x1000, NULL);

	HeapFree(GetProcessHeap(), 0, pbModuleData);

	//
	// Allocate shellcode into process memory
	LPVOID pShellCode = VirtualAllocEx(hProcess, NULL, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (pShellCode)
	{
		SIZE_T h;
		WriteProcessMemory(hProcess, pShellCode, RemoteShellCode, 0x1000, &h);

		HANDLE hThread = CreateRemoteThread(hProcess, NULL,0, (LPTHREAD_START_ROUTINE)pShellCode, pbTargetBase, 0, NULL);
		if (hThread)
		{
			CloseHandle(hThread);
			//VirtualFreeEx(hTargetProcess, pShellCode, 0, MEM_RELEASE);
			return TRUE;
		}
	}

	DWORD dwError = GetLastError();

	VirtualFreeEx(hProcess, pbTargetBase, 0, MEM_RELEASE);
	if (pShellCode)
	{
		VirtualFreeEx(hProcess, pShellCode, 0, MEM_RELEASE);
	}

	return (dwError == NO_ERROR) ? ERROR_ACCESS_DENIED : dwError;
}

//
// This method will be called from inside the remote process
DWORD __stdcall RemoteShellCode(MANUAL_MAPPING_DATA *pMapData)
{
	PBYTE pBaseMemory = (PBYTE)pMapData;
	if (!pMapData)
	{
		return ERROR_INVALID_PARAMETER;
	}

	IMAGE_OPTIONAL_HEADER64 *pOptHeader		= &((IMAGE_NT_HEADERS *)(pBaseMemory + ((IMAGE_DOS_HEADER *)pMapData)->e_lfanew))->OptionalHeader;
	pfnLoadLibraryA			_LoadLibrary	= pMapData->pLoadLibrary;
	pfnGetProcAddress		_GetProcAddress = pMapData->pGetProcAddress;
	pfnDllEntryPoint		_DllMain		= (pfnDllEntryPoint)(pBaseMemory + pOptHeader->AddressOfEntryPoint);

	//
	// Relocate the DLL base
	PBYTE pbLocationDelta = pBaseMemory - pOptHeader->ImageBase;
	if (pbLocationDelta)
	{
		if (!pOptHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size)
		{
			//
			// Cant relocate
			return ERROR_EMPTY;
		}

		IMAGE_BASE_RELOCATION *pRelocData = (IMAGE_BASE_RELOCATION *)(pBaseMemory + pOptHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
		while (pRelocData->VirtualAddress)
		{
			UINT EntryCount		= (pRelocData->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
			WORD *pRelativeInfo = (WORD *)(pRelocData + 1);

			for (UINT i = 0; i != EntryCount; ++i, ++pRelativeInfo)
			{
				if (RELOC_FLAG(*pRelativeInfo))
				{
					UINT_PTR *pPatch = (UINT_PTR *)(pBaseMemory + pRelocData->VirtualAddress + ((*pRelativeInfo) & 0xFFF));
					*pPatch += (UINT_PTR)(pbLocationDelta);
				}
			}

			pRelocData = (IMAGE_BASE_RELOCATION *)((PBYTE)pRelocData + pRelocData->SizeOfBlock);
		}
	}

	//
	// All references modules/dll's in the injection Dll need to be imported/loaded into the target process
	if (pOptHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size)
	{
		IMAGE_IMPORT_DESCRIPTOR *pImportDesc = (IMAGE_IMPORT_DESCRIPTOR *)(pBaseMemory + pOptHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
		while (pImportDesc->Name)
		{
			LPSTR		lpszModule	= (LPSTR)(pBaseMemory + pImportDesc->Name);
			HINSTANCE	hModule		= _LoadLibrary(lpszModule);

			ULONG_PTR *pThunkRef = (ULONG_PTR *)(pBaseMemory + pImportDesc->OriginalFirstThunk);
			ULONG_PTR *pFuncRef  = (ULONG_PTR *)(pBaseMemory + pImportDesc->FirstThunk);
			if (!pThunkRef) {
				pThunkRef = pFuncRef;
			}

			for (; *pThunkRef; ++pThunkRef, ++pFuncRef)
			{
				if (IMAGE_SNAP_BY_ORDINAL(*pThunkRef))
				{
					//
					// Import by ordinal
					*pFuncRef = _GetProcAddress(hModule, (LPSTR)(*pThunkRef & 0xFFFF));
				}
				else
				{
					//
					// Import by name
					*pFuncRef = _GetProcAddress(hModule, ((IMAGE_IMPORT_BY_NAME *)(pBaseMemory + (*pThunkRef)))->Name);
				}
			}

			++pImportDesc;
		}
	}

	//
	// Execute the module TLS callbacks
	if (pOptHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].Size)
	{
		IMAGE_TLS_DIRECTORY *pThreadLocalStorage	= (IMAGE_TLS_DIRECTORY *)(pBaseMemory + pOptHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress);
		PIMAGE_TLS_CALLBACK *pCallback				= (PIMAGE_TLS_CALLBACK*)(pThreadLocalStorage->AddressOfCallBacks);

		for (; pCallback && *pCallback; ++pCallback)
		{
			(*pCallback)(pBaseMemory, DLL_PROCESS_ATTACH, NULL);
		}
	}

	/* Execute DllMain */
	_DllMain(pBaseMemory, DLL_PROCESS_ATTACH, NULL);

	pMapData->hModule = (HINSTANCE)pBaseMemory;
	return 0;
}