#pragma once

DWORD WINAPI AppTrayIconThread( LPVOID );

BOOL SendTrayNotification( LPCWSTR lpszMessage );
BOOL UpdateTrayIconImage( HICON hIcon );
BOOL UpdateTrayIconTitle( LPCWSTR lpszTitle );