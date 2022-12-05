#include "pch.h"
#include "AppTrayIcon.h"
#include "Outfled Microphone MuteDlg.h"

#define TRAY_WNDCLASS_NAME	L"__hidden__"
#define TRAY_WND_NAME		L"Outfled Microphone Mute"
#define TRAY_HOVER_MSG		TRAY_WND_NAME
#define TRAY_ICON_CALLBACK	WM_APP + 1
#define MENU_ID_OPENAPP		101
#define MENU_ID_EXITAPP		102


//--------------------
// Global Variables
static HWND		g_hTrayWnd;
static WNDCLASSEX	g_WndClass;
static HMODULE		g_hInstance;
static CWnd		*g_pAppWnd;
static PNOTIFYICONDATA	g_lpIconData;

static LRESULT TrayIconProcedure( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
static BOOL CreateTrayWndClass();


DWORD WINAPI AppTrayIconThread( LPVOID lpParam )
{
	MSG				WndMsg;
	COutfledMicrophoneMuteDlg	*DlgWnd;

	DlgWnd		= (COutfledMicrophoneMuteDlg*)lpParam;
	g_hTrayWnd	= NULL;
	g_hInstance	= GetModuleHandle( nullptr );
	g_pAppWnd	= DlgWnd;
	g_lpIconData	= nullptr;

	/* Create hidden window class */
	CreateTrayWndClass();

	/* Create shell icon window */
	g_hTrayWnd = CreateWindow( TRAY_WNDCLASS_NAME,
		TRAY_WND_NAME,
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
	if (g_hTrayWnd == NULL)
	{
		LPWSTR lpszMessage = ErrorCodeToString();
		DlgWnd->MessageBox( lpszMessage, L"Outfled Microphone Mute Error", MB_OK );

		UnregisterClass( TRAY_WNDCLASS_NAME, g_hInstance );
		LocalFree( lpszMessage );

		ExitProcess( 0 );
	}

	/* Get tray icon window messages */
	while (GetMessage( &WndMsg, NULL, 0, 0 ))
	{
		Sleep(0);
		TranslateMessage( &WndMsg );
		DispatchMessageW( &WndMsg );
	}

	/* Cleanup */
	UnregisterClass( TRAY_WNDCLASS_NAME, g_hInstance);
	delete g_lpIconData;

	ExitProcess( 0 );
	return 0;
}

BOOL SendTrayNotification( LPCWSTR lpszMessage )
{
	if (g_lpIconData == NULL) {
		return FALSE;
	}

	StringCbCopy( g_lpIconData->szInfo, sizeof( g_lpIconData->szInfo ), lpszMessage );
	return Shell_NotifyIcon( NIM_MODIFY, g_lpIconData );
}

BOOL UpdateTrayIconImage( HICON hIcon )
{
	if (g_lpIconData == NULL) {
		return FALSE;
	}
	
	/* Clear the previous notification string */
	ZeroMemory( g_lpIconData->szInfo, sizeof( g_lpIconData ) );

	g_lpIconData->hIcon = hIcon;
	return Shell_NotifyIcon( NIM_MODIFY, g_lpIconData );
}

BOOL UpdateTrayIconTitle( LPCWSTR lpszTitle )
{
	if (g_lpIconData == NULL) {
		return FALSE;
	}

	/* Clear the previous notification string */
	ZeroMemory( g_lpIconData->szInfo, sizeof( g_lpIconData ) );

	StringCbCopy( g_lpIconData->szTip, sizeof( g_lpIconData->szTip ), lpszTitle );
}


static LRESULT TrayIconProcedure( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch (uMsg)
	{
		/* Create icon */
	case WM_CREATE:
		{
			g_lpIconData			= new NOTIFYICONDATA{};
			g_lpIconData->cbSize		= sizeof( *g_lpIconData );
			g_lpIconData->hWnd		= hWnd;
			g_lpIconData->uID		= 0;
			g_lpIconData->uFlags		= NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP | NIF_INFO;
			g_lpIconData->uCallbackMessage	= TRAY_ICON_CALLBACK;
			g_lpIconData->uVersion		= NOTIFYICON_VERSION_4;
			g_lpIconData->dwInfoFlags	= NIIF_INFO;
			g_lpIconData->hIcon		= g_pAppWnd->GetIcon( TRUE );

			/* Notification title */
			StringCbCopy( g_lpIconData->szInfoTitle, sizeof( g_lpIconData->szInfoTitle ), L"Outfled Microphone Mute" ); 

			/* Add icon to shell */
			Shell_NotifyIcon( NIM_ADD, g_lpIconData );
			Shell_NotifyIconW( NIM_SETVERSION, g_lpIconData );
	
			break;
		}

		/* Icon event */
	case TRAY_ICON_CALLBACK:
		{
			/* Left click */
			if (LOWORD( lParam ) == WM_LBUTTONUP)
			{
				/* Show the app window */
				g_pAppWnd->ShowWindow( SW_SHOW );
				SetForegroundWindow( g_pAppWnd->GetSafeHwnd() );
			}

			/* Popup menu */
			if (LOWORD( lParam ) == WM_CONTEXTMENU)
			{
				POINT		Cursor;
				HMENU		PopupMenu;
				MENUITEMINFO	ItemInfo;
				UINT		Result;

				ItemInfo.cbSize = sizeof( ItemInfo );
				ItemInfo.fMask	= MIIM_FTYPE;
				ItemInfo.fType	= MFT_SEPARATOR;
				PopupMenu	= CreatePopupMenu();

				/* Create popup menu options */
				InsertMenu( PopupMenu,
					0,		// Index Position
					g_pAppWnd->IsWindowVisible() ? MF_GRAYED : 0 | MF_STRING | MF_BYCOMMAND,
					MENU_ID_OPENAPP,
					L"Show Window"
				);
				InsertMenuItem( PopupMenu, 1, TRUE, &ItemInfo );
				InsertMenu( PopupMenu,
					2,
					MF_BYPOSITION,
					MENU_ID_EXITAPP,
					L"Exit"
				);

				SetForegroundWindow( g_hTrayWnd );
				GetCursorPos( &Cursor );

				/* Display popupmenu & get clicked item */
				Result = TrackPopupMenu( PopupMenu,
					TPM_CENTERALIGN | TPM_RIGHTBUTTON | TPM_VERNEGANIMATION | TPM_VERPOSANIMATION | TPM_RETURNCMD,
					Cursor.x,
					Cursor.y,
					0,
					g_hTrayWnd,
					NULL
				);
				switch (Result)
				{
				case MENU_ID_OPENAPP:
					g_pAppWnd->ShowWindow( SW_SHOW );
					break;

				case MENU_ID_EXITAPP:
					Shell_NotifyIcon( NIM_DELETE, g_lpIconData );
					PostQuitMessage( 0 );

					return 0;

				default:
					break;
				}
			}
			break;
		}

	case WM_DESTROY:
		Shell_NotifyIcon( NIM_DELETE, g_lpIconData );
		PostQuitMessage( 0 );

		return 0;

	default:
		break;
	}

	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

static BOOL	CreateTrayWndClass()
{
	g_WndClass			= { 0 };
	g_WndClass.cbSize		= sizeof( WNDCLASSEX );
	g_WndClass.style		= CS_HREDRAW | CS_VREDRAW;
	g_WndClass.lpfnWndProc		= TrayIconProcedure;
	g_WndClass.hInstance		= g_hInstance;
	g_WndClass.lpszClassName	= TRAY_WNDCLASS_NAME;
	return RegisterClassEx( &g_WndClass );
}
