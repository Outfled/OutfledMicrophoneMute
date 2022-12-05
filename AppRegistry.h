#pragma once

#include "pch.h"

#define APP_REG_VALUE_DEFAULT_MIC			L""
#define APP_REG_VALUE_DEFAULT_TOGGLEKEYS	L"ToggleKeys"
#define APP_REG_VALUE_DEFAULT_MODIFIERS		L"Modifiers"
#define APP_REG_VALUE_PLAYSOUND				L"SoundEffects"
#define APP_REG_VALUE_SENDNOTIFICATIONS		L"Notifications"
#define APP_REG_VALUE_STARTUPAPP			L"StartupProgram"

#define STATUS_ENABLED						0x00000001
#define STATUS_DISABLED						0x00000000

LRESULT GetAppRegistryValue( LPCWSTR lpszValueName, LPVOID lpResult, DWORD cbSize );
LRESULT SetAppRegistryValue( LPCWSTR lpszValueName, LPCVOID lpValue, DWORD cbSize );
LRESULT SetAppStartupProgram( BOOL bStartup );
