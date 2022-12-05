
// Outfled Microphone MuteDlg.h : header file
//

#pragma once


// COutfledMicrophoneMuteDlg dialog
class COutfledMicrophoneMuteDlg : public CDialogEx
{
// Construction
public:
	COutfledMicrophoneMuteDlg(CWnd* pParent = nullptr);	// standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_OUTFLED_MICROPHONE_MUTE_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

public:
	CString m_szMicrophoneDevice;
	BOOL	m_bSendNotifications;
	BOOL	m_bPlaySoundEffects;
	DWORD	m_dwToggleKeys;
	WORD	m_wModifiers;

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnWindowPosChanging( WINDOWPOS * );
	DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnCbnSelchangeComboDevicenames();
	afx_msg void OnBnClickedCheckSendnotifications();
	afx_msg void OnBnClickedCheckPlaysound();
	

private:
	BOOL	m_bTrayEnabled;
	HANDLE	m_hTrayThread;
	HANDLE	m_hMicrophoneHandlerThread;
	BOOL	m_bStartHidden;
public:
	afx_msg void OnBnClickedCheckTrayenabled();
	afx_msg void OnBnClickedCheckStartupprogram();
};

#include <strsafe.h>
inline static LPWSTR ErrorCodeToString( HRESULT hResult )
{
	static WCHAR szBuffer[1024];

	StringCbPrintf( szBuffer, sizeof( szBuffer ), L"Unknwon Error Occurred [0xlx", hResult );
	return szBuffer;
}

inline static LPWSTR ErrorCodeToString( DWORD dwResult = GetLastError() )
{
	static LPWSTR lpszBuffer = nullptr;

	FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dwResult,
		MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
		(LPWSTR)&lpszBuffer,
		0,
		NULL
	);
	return lpszBuffer;
}