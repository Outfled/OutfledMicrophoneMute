#include "pch.h"
#include "AppRegistry.h"
#include <strsafe.h>

#define HKEY_BASE_PATH		L"SOFTWARE\\Outfled\\Microphone Mute"
#define HKEY_STARUP_PATH	L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"
#define HKEY_STARTUP_VALUE	L"Outfled Microphone Mute"


LRESULT GetAppRegistryValue( 
	LPCWSTR lpszValueName,
	LPVOID	lpResult, 
	DWORD	cbSize
)
{
	LRESULT lResult;
	HKEY	hBaseKey;
	DWORD	cbValue;

	/* Open/create HKEY_CURRENT_USER\SOFTWARE\Outfled key */
	lResult = RegOpenKeyEx(HKEY_CURRENT_USER, HKEY_BASE_PATH, 0, KEY_ALL_ACCESS, &hBaseKey );
	if (FAILED( lResult ))
	{
		return lResult;
	}

	/* Query key value */
	lResult = RegQueryValueEx( hBaseKey, lpszValueName, NULL, NULL, (LPBYTE)lpResult, &(cbValue = cbSize) );

	RegCloseKey( hBaseKey );
	return lResult;
}

LRESULT SetAppRegistryValue( LPCWSTR lpszValueName, LPCVOID lpValue, DWORD cbSize )
{
	LRESULT lResult;
	HKEY	hBaseKey;

	/* Open/create HKEY_CURRENT_USER\SOFTWARE\Outfled key */
	lResult = RegOpenKeyEx( HKEY_CURRENT_USER, HKEY_BASE_PATH, 0, KEY_ALL_ACCESS, &hBaseKey );
	if (FAILED( lResult ))
	{
		return lResult;
	}

	/* Set key value */
	if (0 == wcscmp( APP_REG_VALUE_DEFAULT_MIC, lpszValueName )) {
		lResult = RegSetValueEx( hBaseKey, lpszValueName, 0, REG_SZ, (LPBYTE)lpValue, cbSize );
	}
	else {
		lResult = RegSetValueEx( hBaseKey, lpszValueName, 0, REG_DWORD, (LPBYTE)lpValue, cbSize );
	}

	RegCloseKey( hBaseKey );
	return lResult;
}

LRESULT SetAppStartupProgram( BOOL bStartup )
{
	HKEY	hStartupKey;
	LRESULT lResult;

	lResult = RegOpenKey( HKEY_CURRENT_USER, HKEY_STARUP_PATH, &hStartupKey );
	if (SUCCEEDED( lResult ))
	{
		WCHAR szCurrentPath[1024];

		switch (bStartup)
		{
		case TRUE:
			WCHAR szValue[1024];

			GetModuleFileName( NULL, szCurrentPath, ARRAYSIZE( szCurrentPath ) );
			StringCbPrintf( szValue, sizeof( szValue ), L"\"%s\" -silent", szCurrentPath );

			lResult = RegSetValueEx( hStartupKey, HKEY_STARTUP_VALUE, 0, REG_SZ, (LPBYTE)szValue, (wcslen( szValue ) + 1) * sizeof( WCHAR ) );
			break;

		case FALSE:
			lResult = RegDeleteValue( hStartupKey, HKEY_STARTUP_VALUE );
			break;
		}

		RegCloseKey( hStartupKey );
	}

	return lResult;
}