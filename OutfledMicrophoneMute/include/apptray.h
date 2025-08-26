#pragma once

#define TRAY_WINDOW_CLASS_NAME	L"__hidden__"
#define TRAY_WINDOW_NAME		L"Outfled Microphone Mute"

#define TRAY_ICON_CALLBACK		WM_APP + 1
#define TRAY_MENU_ID_OPEN_APP	100 + 1
#define TRAY_MENU_ID_EXIT_APP	100 + 2
#define TRAY_MENU_ID_MUTE_MIC	100 + 3
#define TRAY_MENU_ID_UNMUTE_MIC	100 + 4


DWORD WINAPI AppTrayIconThread( LPVOID );
BOOL SendTrayNotification( LPCWSTR );
BOOL SetTrayIconImage( HICON );
BOOL SetTrayIconTitle( LPCWSTR );