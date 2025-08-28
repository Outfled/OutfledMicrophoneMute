#include "pch.h"
#include "apptray.h"
#include "Outfled Microphone MuteDlg.h"
#include "microphone_thread.h"
#include <strsafe.h>


#define TRAY_HOVER_MESSAGE		TRAY_WINDOW_NAME

static HWND							g_hTrayWindow;
static WNDCLASSEX					g_WindowClass;
static HMODULE						g_hInstance;
static COutfledMicrophoneMuteDlg	*g_pAppWindow;
static PNOTIFYICONDATA				g_pIconData;
static HMENU						g_hPopupMenu;
static UINT							g_nTaskbarCreatedMsg;

static LRESULT TrayIconProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static int ShowTrayContextMenu();

DWORD WINAPI AppTrayIconThread( LPVOID lpData )
{
	MSG	WindowMsg;

	g_pAppWindow	= (COutfledMicrophoneMuteDlg *)lpData;
	g_hTrayWindow	= NULL;
	g_hInstance		= GetModuleHandle( NULL );
	g_pIconData		= NULL;
	g_WindowClass	= { 0 };

	//
	// Create the tray window class
	//
	g_WindowClass.cbSize		= sizeof( WNDCLASSEX );
	g_WindowClass.style			= CS_HREDRAW | CS_VREDRAW;
	g_WindowClass.lpfnWndProc	= TrayIconProcedure;
	g_WindowClass.hInstance		= g_hInstance;
	g_WindowClass.lpszClassName = TRAY_WINDOW_CLASS_NAME;
	//g_WindowClass.cbClsExtra	= cchLongestDeviceName * sizeof( WCHAR );
	if ( !RegisterClassEx( &g_WindowClass ) )
	{
		ExitThread( GetLastError() );
	}

	/* Create the tray shell icon window (hidden) */
	g_hTrayWindow = CreateWindow(TRAY_WINDOW_CLASS_NAME,
		TRAY_WINDOW_NAME,
		WS_OVERLAPPED,
		CW_USEDEFAULT,
		0,
		CW_USEDEFAULT,
		0,
		NULL,
		NULL,
		g_hInstance,
		NULL
	);
	if ( g_hTrayWindow == NULL )
	{
		DWORD dwError = GetLastError();

		UnregisterClass( TRAY_WINDOW_CLASS_NAME, g_hInstance );
		DisplayAppError( dwError );

		ExitProcess( 0 );
	}

	//
	// Whenever Windows Explorer is restarted, all tray icons are destroyed
	// RegisterWindowMessage(L"TaskbarCreated") will allow the WndProc to receive a message when this occurs
	g_nTaskbarCreatedMsg = RegisterWindowMessage(L"TaskbarCreated");
	ChangeWindowMessageFilterEx(g_hTrayWindow, g_nTaskbarCreatedMsg, MSGFLT_ALLOW, NULL);
	ChangeWindowMessageFilterEx(g_hTrayWindow, WM_COMMAND, MSGFLT_ALLOW, NULL);

	/* Get all tray icon messages */
	while ( GetMessage( &WindowMsg, NULL, 0, 0 ) )
	{
		Sleep( 0 );

		TranslateMessage( &WindowMsg );
		DispatchMessageW( &WindowMsg );

		if ( g_pAppWindow->m_bTrayEnabled == FALSE )
		{
			PostQuitMessage( 0 );
			break;
		}
	}

	/* Cleanup */
	UnregisterClassW( TRAY_WINDOW_CLASS_NAME, g_hInstance );
	LocalFree( g_pIconData );

	ExitProcess( 0 );
	ExitThread( 0 );
}

BOOL SendTrayNotification( LPCWSTR lpszMessage )
{
	if ( g_pIconData )
	{
		StringCbCopy( g_pIconData->szInfo, sizeof( g_pIconData->szInfo ), lpszMessage );
		return Shell_NotifyIcon( NIM_MODIFY, g_pIconData );
	}

	return FALSE;
}
BOOL SetTrayIconImage( HICON hIcon )
{
	if ( g_pIconData )
	{
		/* Clear any previous notification string */
		ZeroMemory( g_pIconData->szInfo, sizeof( g_pIconData ) );

		g_pIconData->hIcon = hIcon;
		return Shell_NotifyIcon( NIM_MODIFY, g_pIconData );
	}

	return FALSE;
}
BOOL SetTrayIconTitle( LPCWSTR lpszTitle )
{
	if ( g_pIconData )
	{
		/* Clear any previous notification string */
		ZeroMemory( g_pIconData->szInfo, sizeof( g_pIconData ) );

		StringCbCopy( g_pIconData->szTip, sizeof( g_pIconData->szTip ), lpszTitle );
		return Shell_NotifyIcon( NIM_MODIFY, g_pIconData );
	}

	return FALSE;
}



#pragma warning( push )
#pragma warning( disable : 28182 )	// Dereferencing NULL pointer
#pragma warning( disable : 28183 )	// (Pointer) could be 0
static LRESULT TrayIconProcedure( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	if (uMsg == g_nTaskbarCreatedMsg) {
		uMsg = WM_CREATE;
	}

	switch (uMsg)
	{
	// Create tray icon
		case WM_CREATE:
		{
			g_hPopupMenu = NULL;

			/* Initialize the tray icon data */
			g_pIconData						= (PNOTIFYICONDATA)LocalAlloc( LPTR, sizeof( NOTIFYICONDATA ) );
			g_pIconData->cbSize				= sizeof( NOTIFYICONDATA );
			g_pIconData->hWnd				= hWnd;
			g_pIconData->uID				= 0;
			g_pIconData->uFlags				= NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP | NIF_INFO;
			g_pIconData->uCallbackMessage	= TRAY_ICON_CALLBACK;
			g_pIconData->uVersion			= NOTIFYICON_VERSION_4;
			g_pIconData->dwInfoFlags		= NIIF_INFO;
			g_pIconData->hIcon				= g_pAppWindow->GetIcon( TRUE );

			/* Notification title */
			StringCbCopy(g_pIconData->szInfoTitle, sizeof(g_pIconData->szInfoTitle), L"Outfled Microphone Mute");

			/* Add icon to shell */
			Shell_NotifyIcon(NIM_ADD, g_pIconData);
			Shell_NotifyIcon(NIM_SETVERSION, g_pIconData);

			/* Set the hover message */
			if (g_pAppWindow->m_bMuted)
			{
				SetTrayIconTitle(L"Outfled Microphone Mute (Muted)");
			}
			else
			{
				SetTrayIconTitle(L"Outfled Microphone Mute (Un-muted)");
			}

			return DefWindowProc(hWnd, uMsg, wParam, lParam);
		}

		// Tray icon message/command
		case TRAY_ICON_CALLBACK:
		{
			switch (lParam)
			{
				// Left click on icon
				case WM_LBUTTONUP:

					/* Show the main app window */
					g_pAppWindow->ShowWindow(SW_SHOW);
					SetForegroundWindow(g_pAppWindow->GetSafeHwnd());
					break;

				// Display the context menu
				case WM_CONTEXTMENU:
				{
					int iResult = ShowTrayContextMenu();
					switch (iResult)
					{
						case TRAY_MENU_ID_OPEN_APP:
							g_pAppWindow->ShowWindow(SW_SHOW);
							break;

						case TRAY_MENU_ID_EXIT_APP:
							Shell_NotifyIcon(NIM_DELETE, g_pIconData);
							PostQuitMessage(0);

							return 0;

						case TRAY_MENU_ID_MUTE_MIC:
							SetEvent(g_hTrayIconMicrophoneMuted);
							break;

						case TRAY_MENU_ID_UNMUTE_MIC:
							SetEvent(g_hTrayIconMicrophoneUnmuted);
							break;
					}

					break;
				}
			}
			break;
		}

		// Destroy tray icon
		case WM_DESTROY:
			Shell_NotifyIcon(NIM_DELETE, g_pIconData);
			PostQuitMessage(0);

			return 0;
	}

	return DefWindowProcW( hWnd, uMsg, wParam, lParam );
}
#pragma warning( pop )

int ShowTrayContextMenu()
{
	POINT Cursor{};

	if (g_hPopupMenu == NULL)
	{
		MENUITEMINFO miiSeparator = { 0 };
		miiSeparator.cbSize = sizeof(MENUITEMINFO);
		miiSeparator.fMask	= MIIM_FTYPE;
		miiSeparator.fType	= MFT_SEPARATOR;

		//
		// Create & initialize the menu
		g_hPopupMenu = CreatePopupMenu();

		InsertMenu(g_hPopupMenu,
			0,
			(g_pAppWindow->IsWindowVisible()) ? MF_GRAYED : 0 | MF_STRING | MF_BYCOMMAND,
			TRAY_MENU_ID_OPEN_APP,
			L"Show Window"
		);
		InsertMenuItem(g_hPopupMenu, 1, TRUE, &miiSeparator);

		InsertMenu(g_hPopupMenu,
			2,
			(g_pAppWindow->m_bMuted) ? MF_GRAYED | MF_GRAYED : MF_BYCOMMAND,
			TRAY_MENU_ID_MUTE_MIC,
			L"Mute Microphone"
		);
		InsertMenuItem(g_hPopupMenu, 3, TRUE, &miiSeparator);

		InsertMenu(g_hPopupMenu,
			4,
			(g_pAppWindow->m_bMuted) ? MF_BYCOMMAND | MF_BYCOMMAND : MF_GRAYED,
			TRAY_MENU_ID_UNMUTE_MIC,
			L"Unmute Microphone"
		);
		InsertMenuItem(g_hPopupMenu, 5, TRUE, &miiSeparator);

		InsertMenu(g_hPopupMenu, 6, MF_BYCOMMAND, TRAY_MENU_ID_EXIT_APP, L"Exit");
	}

	//
	// Display the popup menu & get the clicked item id
	SetForegroundWindow(g_hTrayWindow);
	GetCursorPos(&Cursor);

	return TrackPopupMenu(g_hPopupMenu,
		TPM_CENTERALIGN | TPM_RIGHTBUTTON | TPM_VERNEGANIMATION | TPM_VERPOSANIMATION | TPM_RETURNCMD,
		Cursor.x,
		Cursor.y,
		0,
		g_hTrayWindow,
		NULL
	);
}