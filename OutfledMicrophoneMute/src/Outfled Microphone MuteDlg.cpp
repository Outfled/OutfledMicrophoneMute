
// OutfledMicrophoneMuteDlg.cpp : implementation file
//

#include "pch.h"
#include "framework.h"
#include "Outfled Microphone Mute.h"
#include "Outfled Microphone MuteDlg.h"
#include "afxdialogex.h"
#include "appreg.h"
#include "microphone_handler.h"
#include "apptray.h"
#include "overlay/game_overlay.h"
#include <strsafe.h>
#include "microphone_thread.h"
#include <TlHelp32.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


#define OUTFLED_OVERLAY_MGR_32_EXE_NAME	L"OutfledOverlayMgr.exe"
#define OUTFLED_OVERLAY_MGR_32_EXE_PATH	L"C:\\Outfled\\AppData\\Outfled-Overlay-Manager\\Binaries\\OutfledOverlayMgr.exe"
#define OUTFLED_OVERLAY_MGR_64_EXE_NAME	L"OutfledOverlayMgr.exe"
#define OUTFLED_OVERLAY_MGR_64_EXE_PATH	L"C:\\Outfled\\AppData\\Outfled-Overlay-Manager\\Binaries\\OutfledOverlayMgr64.exe"

COutfledMicrophoneMuteDlg *g_pAppWindow = NULL;

//
// Check if the app/process is elevated (admin)
//
static BOOL IsAppElevated()
{
	HANDLE	hProcessToken;
	BOOL	bElevated;

	bElevated = FALSE;

	/* Open the current process token */
	if (OpenProcessToken( GetCurrentProcess(), TOKEN_QUERY, &hProcessToken ))
	{
		TOKEN_ELEVATION tkElevation;
		DWORD			cbSize;

		//
		// Get the TokenElevation property of the current process
		//
		if (GetTokenInformation( hProcessToken, TokenElevation, &tkElevation, sizeof( tkElevation ), &cbSize ))
		{
			bElevated = tkElevation.TokenIsElevated;
		}

		CloseHandle( hProcessToken );
	}

	return bElevated;
}

//
// Populate the Microphone Device drop down
static VOID PopulateMicrophoneDevices(COutfledMicrophoneMuteDlg *pWnd, const CString &szDefault)
{
	UINT		nDevCount;
	CComboBox	*pDeviceDropBox;

	pDeviceDropBox = (CComboBox *)pWnd->GetDlgItem(IDC_COMBO_DEVICES);
	pDeviceDropBox->ResetContent();

	/* Get the number of microphone devices */
	HRESULT hResult = GetMicrophoneDevCount(&nDevCount);
	if (hResult != S_OK)
	{
		_com_error cError(hResult);
		DisplayAppError(cError.ErrorMessage());

		PostQuitMessage(0);
	}

	/* Populate the device names dropdown box */
	for (UINT iDev = 0; iDev < nDevCount; ++iDev)
	{
		WCHAR szDevName[256];

		/* Get the current device name */
		hResult = EnumerateMicrophoneDevs(iDev, szDevName, ARRAYSIZE(szDevName));
		if (SUCCEEDED(hResult))
		{
			pDeviceDropBox->AddString(szDevName);
		}
	}

	/* If the default micropone exists, it must be selected in the dropdown box */
	if (wcslen(szDefault) > 0)
	{
		for (UINT iDev = 0; iDev < nDevCount; ++iDev)
		{
			CString szDevice;

			pDeviceDropBox->GetLBText(iDev, szDevice);
			if (0 == wcscmp(szDevice, szDefault))
			{
				pDeviceDropBox->SetCurSel(iDev);
			}
		}
	}
}


// Display Error
inline void DisplayAppError( LPCWSTR lpszMessage )
{
	MessageBox( NULL, lpszMessage, L"Error", MB_OK );
}
inline void DisplayAppError( DWORD dwError )
{
	WCHAR szErrorBuf[100];

	LPWSTR lpszErrorBuffer = NULL;
	if ( 0 == FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dwError,
		MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
		(LPWSTR)&lpszErrorBuffer,
		0,
		NULL
	) )
	{
		StringCbPrintf( szErrorBuf, sizeof( szErrorBuf ), L"An unknown error occurred: %d", szErrorBuf );

		DisplayAppError( szErrorBuf );
		return;
	}
	else
	{
		StringCbPrintf( szErrorBuf, sizeof( szErrorBuf ), L"An unknown error occurred: %s", lpszErrorBuffer );
	}

	DisplayAppError( lpszErrorBuffer );
	if ( lpszErrorBuffer ) {
		LocalFree( lpszErrorBuffer );
	}
}

//---------------------------------------------------------------------------------------
// 
// COutfledMicrophoneMuteDlg Dialog
//
//---------------------------------------------------------------------------------------
COutfledMicrophoneMuteDlg::COutfledMicrophoneMuteDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_OUTFLEDMICROPHONEMUTE_DIALOG, pParent)
{
	WCHAR szBuffer[512]{};

	g_pAppWindow = this;

	// Set the default options until the registry is read
	m_bNotificationsEnabled = 1;
	m_bSoundEnabled			= 0; 
	m_bOverlayEnabled		= 0;
	m_bTrayEnabled			= 0;
	m_dwHotkeys				= 0;
	m_wModifiers			= 0;
	m_bMuted				= 0;
	m_hAppTrayThread		= NULL;
	m_hOverlayThread		= NULL;
	/* Load the muted icon (default) */
	m_hIcon = AfxGetApp()->LoadIcon(IDI_ICON_MUTED);

	/* Get the default microphone name from the registry */
	LRESULT lResult = GetAppRegistryValue( APP_REG_VALUE_DEFAULT_DEVICE, szBuffer, sizeof( szBuffer ) );
	if ( lResult == ERROR_SUCCESS && wcslen( szBuffer ) > 0 )
	{
		/* Check if the default mic. device is current unmuted */
		HRESULT hResult = GetMicrophoneMuted( szBuffer, &m_bMuted );
		if ( SUCCEEDED( hResult ) && m_bMuted == 0 )
		{
			/* Update the icon to the unmuted icon */
			m_hIcon = AfxGetApp()->LoadIconW( IDI_ICON_UNMUTED );
		}

		/* Check if the default mic. device doesn't exist */
		if ( hResult == E_INVALIDARG )
		{
			ZeroMemory( szBuffer, sizeof( szBuffer ) );
			SetAppRegistryValue( APP_REG_VALUE_DEFAULT_DEVICE, szBuffer, sizeof( szBuffer ) );
		}
	}

	m_szMicrophoneDevice = szBuffer;
}

void COutfledMicrophoneMuteDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(COutfledMicrophoneMuteDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_CBN_SELCHANGE( IDC_COMBO_DEVICES, &COutfledMicrophoneMuteDlg::OnCbnSelchangeComboDevices )
	ON_BN_CLICKED( IDC_CHECK_NOTIFICATIONS, &COutfledMicrophoneMuteDlg::OnBnClickedCheckNotifications )
	ON_BN_CLICKED( IDC_CHECK_PLAYSOUND, &COutfledMicrophoneMuteDlg::OnBnClickedCheckPlaysound )
	ON_BN_CLICKED( IDC_CHECK_STARTUPPROGRAM, &COutfledMicrophoneMuteDlg::OnBnClickedCheckStartupprogram )
	ON_BN_CLICKED( IDC_CHECK_CLOSETOTRAY, &COutfledMicrophoneMuteDlg::OnBnClickedCheckClosetotray )
	ON_WM_CTLCOLOR()
	ON_BN_CLICKED( IDC_CHECK_OVERLAYENABLE, &COutfledMicrophoneMuteDlg::OnBnClickedCheckOverlayenable )
	ON_CBN_SELCHANGE( IDC_COMBO_IMAGEPOSITION, &COutfledMicrophoneMuteDlg::OnCbnSelchangeComboImageposition )
	ON_BN_CLICKED(IDC_RESTARTAPP_BUTTON, &COutfledMicrophoneMuteDlg::OnBnClickedRestartappButton)
	ON_CBN_SELCHANGE(IDC_COMBO_IMAGESIZE, &COutfledMicrophoneMuteDlg::OnCbnSelchangeComboImagesize)
END_MESSAGE_MAP()


//---------------------------------------------------------------------------------------
// 
// COutfledMicrophoneMuteDlg Message Handles
//
//---------------------------------------------------------------------------------------
BOOL COutfledMicrophoneMuteDlg::OnInitDialog()
{
	CHotKeyCtrl *pHotKeyCtl;
	HREFTYPE	hResult;
	DWORD		dwStatus;
	HANDLE		hThread;
	RECT		rcWnd;
	WCHAR		szFileName[MAX_PATH];

	GetModuleFileName(GetModuleHandle(NULL), szFileName, MAX_PATH);

	pHotKeyCtl			= (CHotKeyCtrl *)GetDlgItem( IDC_HOTKEY_SHORTCUTKEYS );
	m_szAppModulePath	= szFileName;

	//
	// Check for duplicate process

	HWND hOpenedAppWnd = ::FindWindow(TRAY_WINDOW_CLASS_NAME, TRAY_WINDOW_NAME);
	if (hOpenedAppWnd)
	{
		::PostMessage(hOpenedAppWnd, TRAY_ICON_CALLBACK, NULL, (LPARAM)WM_LBUTTONUP);
		//if (!pOpenedAppWnd->IsWindowVisible())
		{
			//pOpenedAppWnd->ShowWindow(SW_SHOW);
		}

		//COutfledMicrophoneMuteDlg *P = (COutfledMicrophoneMuteDlg *)pOpenedAppWnd;
		ExitProcess(0);
	}

	CDialogEx::OnInitDialog();

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	//
	// Populate the ingame-overlay anchor positions dropdown box
	m_pOverlayPositionDropBox = (CComboBox *)GetDlgItem( IDC_COMBO_IMAGEPOSITION );
	m_pOverlayPositionDropBox->AddString( L"Top Left" );
	m_pOverlayPositionDropBox->AddString( L"Top Right" );
	m_pOverlayPositionDropBox->AddString( L"Bottom Left" );
	m_pOverlayPositionDropBox->AddString( L"Bottom Right" );

	//
	// Populate the ingame-overlay image size dropdown box
	m_pOverlaySizeDropBox = (CComboBox *)GetDlgItem(IDC_COMBO_IMAGESIZE);
	m_pOverlaySizeDropBox->AddString(L"25x25");
	m_pOverlaySizeDropBox->AddString(L"45x45");
	m_pOverlaySizeDropBox->AddString(L"75x75");
	m_pOverlaySizeDropBox->AddString(L"100x100");

	//
	// Set the default hotkey
	//
	GetAppRegistryValue( APP_REG_VALUE_DEFAULT_HOTKEYS, &m_dwHotkeys, sizeof( DWORD ) );
	GetAppRegistryValue( APP_REG_VALUE_HK_MODIFIERS, &m_wModifiers, sizeof( DWORD ) );
	if ( m_dwHotkeys != 0 )
	{
		pHotKeyCtl->SetHotKey( m_dwHotkeys, m_wModifiers );
	}

	//
	// Set the check box values
	//
	GetAppRegistryValue( APP_REG_VALUE_NOTIFS_ENABLED, &dwStatus, sizeof( DWORD ) );
	( (CButton *)GetDlgItem( IDC_CHECK_NOTIFICATIONS ) )->SetCheck( dwStatus );

	m_bNotificationsEnabled = (BOOL)dwStatus;

	GetAppRegistryValue( APP_REG_VALUE_SOUND_ENABLED, &dwStatus, sizeof( DWORD ) );
	( (CButton *)GetDlgItem( IDC_CHECK_PLAYSOUND ) )->SetCheck( dwStatus );

	m_bSoundEnabled = (BOOL)dwStatus;

	GetAppRegistryValue( APP_REG_VALUE_STARTUP_PROGRAM, &dwStatus, sizeof( DWORD ) );
	( (CButton *)GetDlgItem( IDC_CHECK_STARTUPPROGRAM ) )->SetCheck( dwStatus );

	GetAppRegistryValue( APP_REG_VALUE_INGAME_OVERLAY_ENABLED, &dwStatus, sizeof( DWORD ) );
	( (CButton *)GetDlgItem( IDC_CHECK_OVERLAYENABLE ) )->SetCheck( dwStatus );

	m_bOverlayEnabled = (BOOL)dwStatus;

	BOOL bInGameOverlayEnabled = (dwStatus == 1);

	/* Fetch the microphone devices */
	PopulateMicrophoneDevices(this, m_szMicrophoneDevice);

	//
	// Set control visibility
	if (IsAppElevated() && bInGameOverlayEnabled)
	{
		m_pOverlayPositionDropBox->EnableWindow();
		m_pOverlaySizeDropBox->EnableWindow();

		((CButton *)GetDlgItem(IDC_STATIC_IMAGEPOSITION))->EnableWindow();
		((CButton *)GetDlgItem(IDC_STATIC_IMAGE_SIZE))->EnableWindow();
		
		OnBnClickedCheckOverlayenable();
	}
	if (!IsAppElevated())
	{
		//
		// Modify the In-Game Overlay -> Enabled check box
		((CButton *)GetDlgItem(IDC_CHECK_OVERLAYENABLECB))->SetWindowTextW(L"Enabled (Admin Required)");
		((CButton *)GetDlgItem(IDC_CHECK_OVERLAYENABLECB))->SetCheck(FALSE);

		//
		// Modify the In-Game Overlay -> Restart button
		((CMFCButton *)GetDlgItem(IDC_RESTARTAPP_BUTTON))->ShowWindow(SW_SHOW);
		((CMFCButton *)GetDlgItem(IDC_RESTARTAPP_BUTTON))->EnableWindowsTheming(FALSE);
		((CMFCButton *)GetDlgItem(IDC_RESTARTAPP_BUTTON))->SetFaceColor(RGB(255, 255, 255), 0);
		((CMFCButton *)GetDlgItem(IDC_RESTARTAPP_BUTTON))->SetFont(GetFont());
	}

	//
	// Always set the 'Close to Tray' checkbox to true
	//
	( (CButton *)GetDlgItem( IDC_CHECK_CLOSETOTRAY ) )->SetCheck( TRUE );
	m_bTrayEnabled = TRUE;

	/* Create the microphone handler thread */
	hThread = CreateThread( NULL, 0, MicrophoneHandleThread, this, 0, NULL );
	if ( hThread == NULL )
	{
		DisplayAppError( GetLastError() );
		ExitProcess( 0 );
	}

	CloseHandle(hThread);

	// Create the tray manager thread
	m_hAppTrayThread = CreateThread( NULL, 0, AppTrayIconThread, this, 0, NULL );
	if ( !m_hAppTrayThread )
	{
		DisplayAppError( GetLastError() );
		ExitProcess( 0 );
	}

	SetForegroundWindow();
	::SetFocus( NULL );

	// return TRUE  unless you set the focus to a control
	return TRUE; 
}

void COutfledMicrophoneMuteDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	// Window is closing
	if ( nID == SC_CLOSE )
	{
		static bool bFirstTime = TRUE;

		UpdateHotkeys();

		// Hide the window
		if ( m_bTrayEnabled )
		{
			// Send the notification
			if ( bFirstTime )
			{
				bFirstTime = FALSE;
				SendTrayNotification( L"Minimized to Tray" );
			}

			// Hide the window
			ShowWindow( SW_HIDE );
		}
		
		/* 'Close to Tray' is not checked; exit the application */
		else
		{
			CDialogEx::OnSysCommand( nID, lParam );
		}
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

void COutfledMicrophoneMuteDlg::OnPaint()
{
	PopulateMicrophoneDevices(this, m_szMicrophoneDevice);
	SelectCurrentOverlayImageSizeCB();

	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR COutfledMicrophoneMuteDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

// The system calls this function to obtain the brush to use for the dialog background
HBRUSH COutfledMicrophoneMuteDlg::OnCtlColor( CDC *pDC, CWnd *pWnd, UINT nCtlColor )
{
	//
	// Return white brush for background color
	//
	return (HBRUSH)GetStockObject( WHITE_BRUSH );
}

// The system calls this function whenever a value from the devices combo box (dropdown box) is selected
void COutfledMicrophoneMuteDlg::OnCbnSelchangeComboDevices()
{
	BOOL bMuted;

	CComboBox *pDevListComboBox = ( (CComboBox*)GetDlgItem( IDC_COMBO_DEVICES ) );

	// Remove focus from the control
	::SetFocus( NULL );

	// Check if no text is selected
	if ( pDevListComboBox->GetCurSel() == CB_ERR )
	{
		SetAppRegistryValue( APP_REG_VALUE_DEFAULT_DEVICE, L"", sizeof( L"" ) );
		return;
	}

	// Get the device name
	pDevListComboBox->GetLBText( pDevListComboBox->GetCurSel(), m_szMicrophoneDevice );

	// Update the default microphone name in the registry
	SetAppRegistryValue( APP_REG_VALUE_DEFAULT_DEVICE, m_szMicrophoneDevice, ( wcslen( m_szMicrophoneDevice ) + 1 ) * sizeof( WCHAR ) );

	// Update the app icon
	if ( S_OK == GetMicrophoneMuted( m_szMicrophoneDevice, &bMuted ) )
	{
		if ( m_bMuted != bMuted )
		{
			m_bMuted = bMuted;

			m_hIcon = AfxGetApp()->LoadIcon( ( bMuted ) ? IDI_ICON_MUTED : IDI_ICON_UNMUTED );
			SetIcon( m_hIcon, TRUE );	// Taskbar icon
			SetIcon( m_hIcon, FALSE );	// Dlg window icon

			UpdateWindow();		
		}
	}

	//SetWindowLongPtr( *this, GWLP_USERDATA, (LONG_PTR)m_szMicrophoneDevice.GetBuffer() );
}


// The system calls this function whenever the 'Send Notifications' checkbox is clicked
void COutfledMicrophoneMuteDlg::OnBnClickedCheckNotifications()
{
	DWORD dwStatus;

	CButton *pNotificationsCheckBox = (CButton *)GetDlgItem( IDC_CHECK_NOTIFICATIONS );
	switch ( pNotificationsCheckBox->GetCheck() )
	{
	case 1:
		dwStatus				= REG_STATUS_ENABLED;
		m_bNotificationsEnabled = TRUE;

		break;
	case 0:
		dwStatus				= REG_STATUS_DISABLED;
		m_bNotificationsEnabled = FALSE;
		break;
	}

	// Update the registry
	SetAppRegistryValue( APP_REG_VALUE_NOTIFS_ENABLED, &dwStatus, sizeof( DWORD ) );
}


// The system calls this function whenever the 'Play Sound' checkbox is clicked
void COutfledMicrophoneMuteDlg::OnBnClickedCheckPlaysound()
{
	DWORD dwStatus;

	CButton *pPlaySoundCheckbox = (CButton *)GetDlgItem( IDC_CHECK_PLAYSOUND );
	switch ( pPlaySoundCheckbox->GetCheck() )
	{
	case 1:
		dwStatus		= REG_STATUS_ENABLED;
		m_bSoundEnabled = TRUE;

		break;
	case 0:
		dwStatus		= REG_STATUS_DISABLED;
		m_bSoundEnabled = FALSE;
		break;
	}

	// Update the registry
	SetAppRegistryValue( APP_REG_VALUE_SOUND_ENABLED, &dwStatus, sizeof( DWORD ) );
}


// The system calls this function whenever the 'Startup Program' checkbox is clicked
void COutfledMicrophoneMuteDlg::OnBnClickedCheckStartupprogram()
{
	DWORD dwStatus;

	CButton *pStartupProgCheckbox = (CButton *)GetDlgItem( IDC_CHECK_STARTUPPROGRAM );
	switch ( pStartupProgCheckbox->GetCheck() )
	{
	case 1:
		dwStatus = REG_STATUS_ENABLED;
		break;
	case 0:
		dwStatus = REG_STATUS_DISABLED;
		break;
	}

	// Update the registry
	SetAppRegistryValue( APP_REG_VALUE_STARTUP_PROGRAM, &dwStatus, sizeof( DWORD ) );
	SetAppStartupProgram( dwStatus );
}


// The system calls this function whenever the 'Close to Tray' checkbox is clicked
void COutfledMicrophoneMuteDlg::OnBnClickedCheckClosetotray()
{
	DWORD dwStatus;

	CButton *pPlaySoundCheckbox = (CButton *)GetDlgItem( IDC_CHECK_CLOSETOTRAY );
	switch ( pPlaySoundCheckbox->GetCheck() )
	{
	case 1:
		dwStatus		= REG_STATUS_ENABLED;
		m_bTrayEnabled	= TRUE;

		break;
	case 0:
		dwStatus		= REG_STATUS_DISABLED;
		m_bTrayEnabled	= FALSE;
		break;
	}

	// Update the registry
	SetAppRegistryValue( APP_REG_VALUE_SOUND_ENABLED, &dwStatus, sizeof( DWORD ) );
}


// The system calls this function whenever the 'Enabled' checkbox (for In-Game Overlay) is clicked
void COutfledMicrophoneMuteDlg::OnBnClickedCheckOverlayenable()
{
	DWORD dwStatus;

	CButton *pOverlayPositionsDropBox = (CButton *)GetDlgItem( IDC_CHECK_OVERLAYENABLE );
	switch (pOverlayPositionsDropBox->GetCheck())
	{
	case 1:
		if (!IsAppElevated())
		{
			int iResult = MessageBox(L"The in-game overlay requires administrative privileges. Would you like to restart as admin?",
				L"Outfled Microphone Mute",
				MB_YESNOCANCEL
			);
			switch (iResult)
			{
			case IDYES:
				break;
			case IDCANCEL:
				((CButton *)GetDlgItem(IDC_STATIC_IMAGEPOSITION))->SetCheck(FALSE);
			case IDNO:
				return;
			}

			dwStatus = INGAME_OVERLAY_STATUS_ENABLED;
			SetAppRegistryValue(APP_REG_VALUE_INGAME_OVERLAY_ENABLED, &dwStatus, sizeof(DWORD));

			// Restart app as admin
			OnBnClickedRestartappButton();
			return;
		}

		if (!m_hOverlayThread)
		{
			m_hOverlayThread = CreateThread(NULL, 0, InGameOverlayManager, this, 0, NULL);
			if (!m_hOverlayThread)
			{
				DisplayAppError(L"Error: Failed to create overlay thread");
				return;
			}
		}
		else
		{
			ResumeThread(m_hOverlayThread);
		}

		dwStatus = INGAME_OVERLAY_STATUS_ENABLED;

		m_pOverlayPositionDropBox->EnableWindow();
		m_pOverlaySizeDropBox->EnableWindow();

		((CButton *)GetDlgItem(IDC_STATIC_IMAGEPOSITION))->EnableWindow();
		((CButton *)GetDlgItem(IDC_STATIC_IMAGE_SIZE))->EnableWindow();

		break;
	case 0:
	default:
		dwStatus = INGAME_OVERLAY_STATUS_DISABLED;

		m_pOverlayPositionDropBox->EnableWindow( FALSE );
		m_pOverlaySizeDropBox->EnableWindow(FALSE);

		((CButton *)GetDlgItem(IDC_STATIC_IMAGEPOSITION))->EnableWindow(FALSE);
		((CButton *)GetDlgItem(IDC_STATIC_IMAGE_SIZE))->EnableWindow(FALSE);

		if (m_hOverlayThread)
		{
			SuspendThread(m_hOverlayThread);
		}
		break;
	}

	SetAppRegistryValue( APP_REG_VALUE_INGAME_OVERLAY_ENABLED, &dwStatus, sizeof( DWORD ) );

	SelectCurrentOverlayImagePositionCB();
	SelectCurrentOverlayImageSizeCB();
}


// The system calls this function whenever the 'Restart' checkbox (for In-Game Overlay) is clicked
void COutfledMicrophoneMuteDlg::OnBnClickedRestartappButton()
{
	//
	// Restart app as admin
	WCHAR szParameters[40];
	StringCbPrintf(szParameters, sizeof(szParameters), L"--prev_instance_pid %d", GetCurrentProcessId());

	ShellExecute(NULL, L"runas", m_szAppModulePath, szParameters, NULL, SW_SHOWNORMAL);
}

// The system calls this function whenever a value from the 'Image Position' combo-box (dropdown-box) is selected
void COutfledMicrophoneMuteDlg::OnCbnSelchangeComboImageposition()
{
	CString szImagePosition;
	DWORD	dwAnchorType;

	dwAnchorType = 0;

	//
	// Determine the image anchor type
	//
	m_pOverlayPositionDropBox->GetLBText( m_pOverlayPositionDropBox->GetCurSel(), szImagePosition );
	
	if (0 <= szImagePosition.Find( L"Bottom" )) {
		dwAnchorType |= INGAME_OVERLAY_ANCHOR_BOTTOM;
	}
	else /* (szImagePosition.Find( L"Top" )) */ {
		dwAnchorType |= INGAME_OVERLAY_ANCHOR_TOP;
	}

	if (0 <= szImagePosition.Find( L"Right" )) {
		dwAnchorType |= INGAME_OVERLAY_ANCHOR_RIGHT;
	}
	else /* (szImagePosition.Find( L"Left" )) */ {
		dwAnchorType |= INGAME_OVERLAY_ANCHOR_LEFT;
	}

	//
	// Update the registry
	//
	SetAppRegistryValue( APP_REG_VALUE_OVERLAY_IMAGE_ANCHOR, &dwAnchorType, sizeof( DWORD ) );
}

// The system calls this function whenever a value from the 'Image Size' combo-box (dropdown-box) is selected
void COutfledMicrophoneMuteDlg::OnCbnSelchangeComboImagesize()
{
	CString szImageSize;
	DWORD	dwNewWidth;
	DWORD	dwNewHeight;

	/* Get the selected image size string */
	m_pOverlaySizeDropBox->GetLBText(m_pOverlaySizeDropBox->GetCurSel(), szImageSize);
	if (szImageSize.Find(L"Custom (") == 0)
	{
		szImageSize.Delete(0, wcslen(L"Custom ("));
		szImageSize.Delete(szImageSize.Find(L")"));
	}

	//
	// Get the width & height
	CString szWidth = CString(szImageSize, szImageSize.Find(L"x"));
	dwNewWidth		= (DWORD)_wtoi(szWidth);

	szImageSize.Delete(0, szWidth.GetLength() + 1);
	dwNewHeight = (DWORD)_wtoi(szWidth);

	/* Update the registry */
	SetAppRegistryValue(APP_REG_VALUE_OVERLAY_IMG_WIDTH, &dwNewWidth, sizeof(DWORD));
	SetAppRegistryValue(APP_REG_VALUE_OVERLAY_IMG_HEIGHT, &dwNewHeight, sizeof(DWORD));
}

// Update the mute/unmute hotkeys registry value
VOID COutfledMicrophoneMuteDlg::UpdateHotkeys()
{
	CHotKeyCtrl *pHotKey = ((CHotKeyCtrl *)GetDlgItem(IDC_HOTKEY_SHORTCUTKEYS));

	// Get the current hotkeys
	pHotKey->GetHotKey((WORD &)m_dwHotkeys, m_wModifiers);

	// Update the registry default hotkeys
	SetAppRegistryValue(APP_REG_VALUE_DEFAULT_HOTKEYS, &m_dwHotkeys, sizeof(DWORD));
	SetAppRegistryValue(APP_REG_VALUE_HK_MODIFIERS, &m_wModifiers, sizeof(WORD));
}

// Select the overlay image position text from the 'Image Position' combo-box (dropdown-box) =
VOID COutfledMicrophoneMuteDlg::SelectCurrentOverlayImagePositionCB()
{
	DWORD	dwAnchorType;
	LPCWSTR lpszSelection;

	if (m_pOverlayPositionDropBox->IsWindowEnabled())
	{
		GetAppRegistryValue( APP_REG_VALUE_OVERLAY_IMAGE_ANCHOR, &dwAnchorType, sizeof( DWORD ) );
		if (dwAnchorType & INGAME_OVERLAY_ANCHOR_BOTTOM)
		{
			if (dwAnchorType & INGAME_OVERLAY_ANCHOR_RIGHT) {
				lpszSelection = L"Bottom Right";
			}
			else {
				lpszSelection = L"Bottom Left";
			}
		}
		else /* dwAnchorType & INGAME_OVERLAY_ANCHOR_TOP */
		{
			if (dwAnchorType & INGAME_OVERLAY_ANCHOR_RIGHT) {
				lpszSelection = L"Top Right";
			}
			else {
				lpszSelection = L"Top Left";
			}
		}

		for (int i = 0; i < m_pOverlayPositionDropBox->GetCount(); ++i)
		{
			CString szEnumSelection;
			m_pOverlayPositionDropBox->GetLBText( i, szEnumSelection );

			if (0 == szEnumSelection.Compare( lpszSelection ))
			{
				m_pOverlayPositionDropBox->SetCurSel( i );
				break;
			}
		}
	}
	else
	{
		//m_pOverlayPositionDropBox->Clear();
	}
}

// Select the overlay image size text from the 'Image size' combo-box (dropdown-box) =
VOID COutfledMicrophoneMuteDlg::SelectCurrentOverlayImageSizeCB()
{
	if (m_pOverlaySizeDropBox->IsWindowEnabled())
	{
		DWORD dwImageWidth	= OVERLAY_IMAGE_DEFAULT_WIDTH;
		DWORD dwImageHeight = OVERLAY_IMAGE_DEFAULT_HEIGHT;

		GetAppRegistryValue(APP_REG_VALUE_OVERLAY_IMG_WIDTH, &dwImageWidth, sizeof(DWORD));
		GetAppRegistryValue(APP_REG_VALUE_OVERLAY_IMG_HEIGHT, &dwImageHeight, sizeof(DWORD));

		WCHAR szSelection[25];
		StringCbPrintf(szSelection, sizeof(szSelection), L"%dx%d", dwImageWidth, dwImageHeight);

		for (int i = 0; i < m_pOverlaySizeDropBox->GetCount(); ++i)
		{
			CString szEnumSelection;
			m_pOverlaySizeDropBox->GetLBText(i, szEnumSelection);

			if (0 == szEnumSelection.Compare(szSelection))
			{
				m_pOverlaySizeDropBox->SetCurSel(i);
				return;
			}
		}

		DWORD dwSelection = m_pOverlaySizeDropBox->GetCount();

		StringCbPrintf(szSelection, sizeof(szSelection), L"Custom (%dx%d)", dwImageWidth, dwImageHeight);

		m_pOverlaySizeDropBox->AddString(szSelection);
		m_pOverlaySizeDropBox->SetCurSel(dwSelection);
		OnCbnSelchangeComboImagesize();
	}
}




