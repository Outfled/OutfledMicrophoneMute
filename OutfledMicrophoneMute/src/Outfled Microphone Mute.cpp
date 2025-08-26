
// OutfledMicrophoneMute.cpp : Defines the class behaviors for the application.
//

#include "pch.h"
#include "framework.h"
#include "Outfled Microphone Mute.h"
#include "Outfled Microphone MuteDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

COutfledMicrophoneMuteApp theApp;

static VOID TerminatePreviousInstance(DWORD dwProcessId)
{
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessId);
	TerminateProcess(hProcess, 0);
	CloseHandle(hProcess);
}

BEGIN_MESSAGE_MAP(COutfledMicrophoneMuteApp, CWinApp)
END_MESSAGE_MAP()

COutfledMicrophoneMuteApp::COutfledMicrophoneMuteApp()
{
	// support Restart Manager
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;
}

BOOL COutfledMicrophoneMuteApp::InitInstance()
{
	LPWSTR *pCommandLineArgs;
	INT		nArgs;

	pCommandLineArgs = ::CommandLineToArgvW( ::GetCommandLine(), &nArgs );

	// InitCommonControlsEx() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// Set this to include all the common control classes you want to use
	// in your application.
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();


	AfxEnableControlContainer();

	// Create the shell manager, in case the dialog contains
	// any shell tree view or shell list view controls.
	CShellManager *pShellManager = new CShellManager;

	// Activate "Windows Native" visual manager for enabling themes in MFC controls
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove from the following
	// the specific initialization routines you do not need
	// Change the registry key under which our settings are stored
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization
	SetRegistryKey(_T("Outfled Microphone Mute"));

	COutfledMicrophoneMuteDlg	dlg;
	INT_PTR						nResponse;

	m_pMainWnd = &dlg;

	// Check command line args
	if ( nArgs > 1 )
	{
		if ( 0 == _wcsicmp( pCommandLineArgs[1], L"--no-wnd" ) )
		{
			dlg.Create( IDD_OUTFLEDMICROPHONEMUTE_DIALOG );
			dlg.ShowWindow( SW_HIDE );
			nResponse = dlg.RunModalLoop();
		}
		else if (0 == _wcsicmp(pCommandLineArgs[1], L"--prev_instance_pid"))
		{
			if (nArgs >= 3)
			{
				DWORD pid = _wtoi(pCommandLineArgs[2]);
				TerminatePreviousInstance(pid);
			}

			nResponse = dlg.DoModal();
		}
	}
	else
	{
		nResponse = dlg.DoModal();
	}

	if (nResponse == IDOK)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with OK
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with Cancel
	}
	else if (nResponse == -1)
	{
		TRACE(traceAppMsg, 0, "Warning: dialog creation failed, so application is terminating unexpectedly.\n");
		TRACE(traceAppMsg, 0, "Warning: if you are using MFC controls on the dialog, you cannot #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS.\n");
	}

	// Delete the shell manager created above.
	if (pShellManager != nullptr)
	{
		delete pShellManager;
	}

#if !defined(_AFXDLL) && !defined(_AFX_NO_MFC_CONTROLS_IN_DIALOGS)
	ControlBarCleanUp();
#endif

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}

