
// Outfled Microphone MuteDlg.cpp : implementation file
//

#include "pch.h"
#include "framework.h"
#include "Outfled Microphone Mute.h"
#include "Outfled Microphone MuteDlg.h"
#include "afxdialogex.h"
#include <mmhandler.h>
#include "AppRegistry.h"
#include "MicrophoneThread.h"
#include "AppTrayIcon.h"

#pragma comment ( lib, "mmhandler.lib" )


#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// COutfledMicrophoneMuteDlg dialog



COutfledMicrophoneMuteDlg::COutfledMicrophoneMuteDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_OUTFLED_MICROPHONE_MUTE_DIALOG, pParent)
{
	m_bSendNotifications		= 1; // Default
	m_bPlaySoundEffects			= 0; // Default
	m_bTrayEnabled				= 1; // Default
	m_dwToggleKeys				= 0;
	m_wModifiers				= 0;
	m_bStartHidden				= 0;
	m_hTrayThread				= NULL;
	m_hMicrophoneHandlerThread	= NULL;

	// Set the default icon to unmuted
	m_hIcon = AfxGetApp()->LoadIcon( IDI_ICON_UNMUTED );

	// Check the default microphone status
	if (SUCCEEDED( CoInitialize( NULL ) ))
	{
		WCHAR	szMicrophone[MICROPHONE_BUF_MAX]{};
		BOOL	bMuted;

		bMuted = 0;
		if (ERROR_SUCCESS == GetAppRegistryValue( APP_REG_VALUE_DEFAULT_MIC, szMicrophone, sizeof( szMicrophone ) ))
		{
			GetMicrophoneValue( szMicrophone, MIC_DEVICE_VALUE_ISMUTE, &bMuted );

			/* Update icon */
			m_hIcon = AfxGetApp()->LoadIcon( (bMuted) ? IDI_ICON_MUTED : IDI_ICON_UNMUTED );
		}

		CoUninitialize();
	}
}

void COutfledMicrophoneMuteDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(COutfledMicrophoneMuteDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_WINDOWPOSCHANGED()
	ON_CBN_SELCHANGE( IDC_COMBO_DEVICENAMES, &COutfledMicrophoneMuteDlg::OnCbnSelchangeComboDevicenames )
	ON_BN_CLICKED( IDC_CHECK_SENDNOTIFICATIONS, &COutfledMicrophoneMuteDlg::OnBnClickedCheckSendnotifications )
	ON_BN_CLICKED( IDC_CHECK_PLAYSOUND, &COutfledMicrophoneMuteDlg::OnBnClickedCheckPlaysound )
	ON_BN_CLICKED( IDC_CHECK_TRAYENABLED, &COutfledMicrophoneMuteDlg::OnBnClickedCheckTrayenabled )
	ON_BN_CLICKED( IDC_CHECK_STARTUPPROGRAM, &COutfledMicrophoneMuteDlg::OnBnClickedCheckStartupprogram )
END_MESSAGE_MAP()


// COutfledMicrophoneMuteDlg message handlers

BOOL COutfledMicrophoneMuteDlg::OnInitDialog()
{
	CComboBox	*pDropdownBox;
	CHotKeyCtrl *pHotkeys;

	pDropdownBox		= (CComboBox *)GetDlgItem( IDC_COMBO_DEVICENAMES );
	pHotkeys			= (CHotKeyCtrl *)GetDlgItem( IDC_HOTKEY_SHORTCUTKEYS );

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

	/* Create tray icon thread */
	m_hTrayThread = CreateThread( NULL, 0, AppTrayIconThread, this, 0, NULL );
	if (m_hTrayThread == NULL)
	{
		MessageBox( ErrorCodeToString(), L"Outfled Microphone Mute Error" );
		ExitProcess( 0 );
	}

	/* Populate the device names dropdown */
	HRESULT hResult = CoInitialize( NULL );
	if (SUCCEEDED( hResult ))
	{
		DWORD nDevices;
		WCHAR szDefaultDevice[256]{};

		/* Get number of microphone devices */
		nDevices = GetMicrophoneDeviceCount();
		for (DWORD iDevice = 0; iDevice < nDevices; ++iDevice)
		{
			WCHAR szName[MICROPHONE_BUF_MAX];

			/* Get device name */
			hResult = EnumerateMicrophoneDevices( iDevice, szName, ARRAYSIZE( szName ) );
			if (SUCCEEDED( hResult ))
			{
				pDropdownBox->AddString( szName );
			}
		}

		/* Select the default microphone from list */
		if (ERROR_SUCCESS == GetAppRegistryValue( APP_REG_VALUE_DEFAULT_MIC, szDefaultDevice, sizeof( szDefaultDevice ) ))
		{
			if (0 != wcslen( szDefaultDevice ))
			{
				for (DWORD iDevice = 0; iDevice < nDevices; ++iDevice)
				{
					CString szName;

					pDropdownBox->GetLBText( iDevice, szName );
					if (0 == wcscmp( szName, szDefaultDevice ))
					{
						BOOL bMuted = 0;

						m_szMicrophoneDevice = szName;
						pDropdownBox->SetCurSel( iDevice );

						if (SUCCEEDED( GetMicrophoneValue( m_szMicrophoneDevice, MIC_DEVICE_VALUE_ISMUTE, &bMuted ) )) {
							UpdateTrayIconTitle( (bMuted) ? L"Outfled Microphone Mute (Muted)" : L"Outfled Microphone Mute (Un-muted)" );
						}

						break;
					}				
				}		
			}
		}

		CoUninitialize();
	}

	/* Set the default hotkeys */
	if (ERROR_SUCCESS == GetAppRegistryValue( APP_REG_VALUE_DEFAULT_TOGGLEKEYS, &m_dwToggleKeys, sizeof( DWORD ) ))
	{
		if (m_dwToggleKeys != 0)
		{
			GetAppRegistryValue( APP_REG_VALUE_DEFAULT_MODIFIERS, &m_wModifiers, sizeof( DWORD ) );
			pHotkeys->SetHotKey( m_dwToggleKeys, m_wModifiers );
		}
	}

	/* Get the default check box values */
	DWORD dwValue;
	if (ERROR_SUCCESS == GetAppRegistryValue( APP_REG_VALUE_SENDNOTIFICATIONS, &dwValue, (sizeof( DWORD )) ))
	{
		m_bSendNotifications = (dwValue == STATUS_ENABLED);
		((CButton *)GetDlgItem( IDC_CHECK_SENDNOTIFICATIONS ))->SetCheck( dwValue );
	}
	if (ERROR_SUCCESS == GetAppRegistryValue( APP_REG_VALUE_PLAYSOUND, &dwValue, (sizeof(DWORD)))) 
	{
		m_bPlaySoundEffects = (dwValue == STATUS_ENABLED);
		((CButton *)GetDlgItem( IDC_CHECK_PLAYSOUND ))->SetCheck( dwValue );
	}
	if (ERROR_SUCCESS == GetAppRegistryValue( APP_REG_VALUE_STARTUPAPP, &dwValue, (sizeof( DWORD )) ))
	{
		((CButton *)GetDlgItem( IDC_CHECK_STARTUPPROGRAM ))->SetCheck( dwValue );
		SetAppStartupProgram( (dwValue == STATUS_ENABLED));
	}

	/* Set the 'Close to Tray' checkbox to true */
	((CButton *)GetDlgItem( IDC_CHECK_TRAYENABLED ))->SetCheck( 1 );
	m_bTrayEnabled = TRUE;

	/* Create microphone status thread */
	m_hMicrophoneHandlerThread = CreateThread( NULL, 0, MicrophoneStatusThread, this, 0, NULL );
	if (m_hMicrophoneHandlerThread == NULL)
	{
		MessageBox( ErrorCodeToString(), L"Outfled Microphone Mute Error" );
		ExitProcess( 0 );
	}

	SetForegroundWindow();
	::SetFocus( NULL );
	return TRUE;  // return TRUE  unless you set the focus to a control
}

void COutfledMicrophoneMuteDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	if (nID == SC_CLOSE)
	{
		// Check if 'Close to Tray' checkbox is checked
		if (m_bTrayEnabled)
		{
			static BOOL bFirstTime = TRUE;

			CHotKeyCtrl *Hotkeys;

			// Update the toggle keys variable 
			Hotkeys = (CHotKeyCtrl *)GetDlgItem( IDC_HOTKEY_SHORTCUTKEYS );
			Hotkeys->GetHotKey( (WORD &)m_dwToggleKeys, m_wModifiers );
			
			// Update the registry default toggle keys
			SetAppRegistryValue( APP_REG_VALUE_DEFAULT_TOGGLEKEYS, &m_dwToggleKeys, sizeof(DWORD));
			SetAppRegistryValue( APP_REG_VALUE_DEFAULT_MODIFIERS, &m_wModifiers, sizeof( DWORD ) );

			// Start the tray thread if not already created
			if (m_hTrayThread == NULL)
			{
				//m_hTrayThread = CreateThread( NULL, 0, AppTrayIconThread, this, 0, NULL );
			}

			// Hide the window
			ShowWindow( SW_HIDE );

			// Send notification
			if (bFirstTime)
			{
				bFirstTime = FALSE;
				SendTrayNotification( L"Minimized to Tray" );
			}
		}

		// 'Close to Tray' checkbox is unchecked -- Close the application
		else
		{
			TerminateThread( m_hMicrophoneHandlerThread, 0 );
			TerminateThread( m_hTrayThread, 0 );

			Sleep( 10 );

			// Call the default OnSysCommand -- Which will close & terminate the process
			CDialogEx::OnSysCommand( nID, lParam ); 
		}
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void COutfledMicrophoneMuteDlg::OnPaint()
{
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



// The system calls this function whenever a new value is selected
//  from the microphone device name dropdown box
void COutfledMicrophoneMuteDlg::OnCbnSelchangeComboDevicenames()
{
	CComboBox *DeviceList = (CComboBox *)GetDlgItem( IDC_COMBO_DEVICENAMES );

	/* Remove focus from the selected name */
	::SetFocus( NULL );

	/* Check if no text is selected */
	if (DeviceList->GetCurSel() == CB_ERR)
	{
		return;
	}

	/* Get the selected text */
	DeviceList->GetLBText( DeviceList->GetCurSel(), m_szMicrophoneDevice );

	/* Set the default microphone name in the registry */
	SetAppRegistryValue( APP_REG_VALUE_DEFAULT_MIC, m_szMicrophoneDevice, (wcslen( m_szMicrophoneDevice ) + 1) * sizeof( wchar_t ) );

	// Update the app icon 
	if (SUCCEEDED( CoInitialize( NULL ) ))
	{
		BOOL bMuted = FALSE;
		GetMicrophoneValue( m_szMicrophoneDevice, MIC_DEVICE_VALUE_ISMUTE, &bMuted );

		m_hIcon = AfxGetApp()->LoadIcon( (bMuted) ? IDI_ICON_MUTED : IDI_ICON_UNMUTED );

		SetIcon( m_hIcon, TRUE ); // Set taskbar icon
		SetIcon( m_hIcon, FALSE );// Set app window icon
		if (m_hTrayThread)
		{
			UpdateTrayIconImage( m_hIcon );
			UpdateTrayIconTitle( (bMuted) ? L"Outfled Microphone Mute (Muted)" : L"Outfled Microphone Mute (Un-muted)" );
		}

		UpdateWindow();
		CoUninitialize();
	}
}


void COutfledMicrophoneMuteDlg::OnBnClickedCheckSendnotifications()
{
	DWORD dwStatus;

	CButton *SendNotifications = (CButton *)GetDlgItem( IDC_CHECK_SENDNOTIFICATIONS );
	switch (SendNotifications->GetCheck())
	{
	case 1:
		dwStatus				= STATUS_ENABLED;
		m_bSendNotifications	= TRUE;

		break;
	case 0:
		dwStatus				= STATUS_DISABLED;
		m_bSendNotifications	= FALSE;
		break;
	}

	SetAppRegistryValue( APP_REG_VALUE_SENDNOTIFICATIONS, &dwStatus, sizeof( DWORD ) );
}

void COutfledMicrophoneMuteDlg::OnBnClickedCheckPlaysound()
{
	DWORD dwStatus;

	CButton *SendNotifications = (CButton *)GetDlgItem( IDC_CHECK_PLAYSOUND );
	switch (SendNotifications->GetCheck())
	{
	case 1:
		dwStatus			= STATUS_ENABLED;
		m_bPlaySoundEffects	= TRUE;

		break;
	case 0:
		dwStatus			= STATUS_DISABLED;
		m_bPlaySoundEffects = FALSE;
		break;
	}

	SetAppRegistryValue( APP_REG_VALUE_PLAYSOUND, &dwStatus, sizeof( DWORD ) );
}

void COutfledMicrophoneMuteDlg::OnBnClickedCheckTrayenabled()
{
	CButton *SendNotifications = (CButton *)GetDlgItem( IDC_CHECK_PLAYSOUND );
	switch (SendNotifications->GetCheck())
	{
	case 1:
		m_bTrayEnabled = TRUE;
		break;
	case 0:
		m_bTrayEnabled = FALSE;
		break;
	}
}

void COutfledMicrophoneMuteDlg::OnBnClickedCheckStartupprogram()
{
	DWORD dwStatus;

	CButton *SendNotifications = (CButton *)GetDlgItem( IDC_CHECK_STARTUPPROGRAM );
	switch (SendNotifications->GetCheck())
	{
	case 1:
		dwStatus = STATUS_ENABLED;
		break;
	case 0:
		dwStatus = STATUS_DISABLED;
		break;
	}

	SetAppRegistryValue( APP_REG_VALUE_STARTUPAPP, &dwStatus, sizeof( DWORD ) );
	SetAppStartupProgram( (dwStatus == STATUS_ENABLED) );
}

void COutfledMicrophoneMuteDlg::OnWindowPosChanging( WINDOWPOS *pwp )
{
	if (m_bStartHidden)
	{
		m_bStartHidden	= FALSE;
		pwp->flags		&= ~SWP_SHOWWINDOW;
	}

	CDialog::OnWindowPosChanged( pwp );
}
