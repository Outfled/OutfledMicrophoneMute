#pragma once


//
// Names of Registry Values for Holding the Previous App Instance Data

#define APP_REG_VALUE_DEFAULT_DEVICE			L""
#define APP_REG_VALUE_DEFAULT_HOTKEYS			L"HotKeys"
#define APP_REG_VALUE_HK_MODIFIERS				L"Modifiers"
#define APP_REG_VALUE_SOUND_ENABLED				L"SoundEffects"
#define APP_REG_VALUE_NOTIFS_ENABLED			L"Notifications"
#define APP_REG_VALUE_STARTUP_PROGRAM			L"StartWithWindows"
#define APP_REG_VALUE_INGAME_OVERLAY_ENABLED	L"In-GameOverlay"
#define APP_REG_VALUE_OVERLAY_IMAGE_ANCHOR		L"OverlayAnchor"
#define APP_REG_VALUE_OVERLAY_IMG_WIDTH			L"OverlayIconWidth"
#define APP_REG_VALUE_OVERLAY_IMG_HEIGHT		L"OverlayIconHeight"

#define REG_STATUS_ENABLED						(DWORD)0x00000001
#define REG_STATUS_DISABLED						(DWORD)0x00000000

DWORD GetAppRegistryValue( LPCWSTR lpszValueName, LPVOID lpResult, DWORD cbSize, HKEY hKey = NULL );
DWORD SetAppRegistryValue( LPCWSTR lpszValueName, LPCVOID lpData, DWORD cbSize, HKEY hKey = NULL);
DWORD SetAppStartupProgram( DWORD dwStatus );
