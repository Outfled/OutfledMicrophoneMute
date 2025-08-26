// OutfledMicrophoneMuteDlg.h : header file
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
	enum { IDD = IDD_OUTFLEDMICROPHONEMUTE_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

private:
	HANDLE	m_hAppTrayThread;

private:
	VOID SelectCurrentOverlayImagePositionCB();
	VOID SelectCurrentOverlayImageSizeCB();
public:
	BOOL		m_bNotificationsEnabled;
	BOOL		m_bSoundEnabled;
	BOOL		m_bTrayEnabled;
	BOOL		m_bOverlayEnabled;
	DWORD		m_dwHotkeys;
	WORD		m_wModifiers;
	BOOL		m_bMuted;
	HANDLE		m_hOverlayThread;
	CString		m_szMicrophoneDevice;
	CString		m_szAppModulePath;
	CComboBox	*m_pOverlayPositionDropBox;
	CComboBox	*m_pOverlaySizeDropBox;

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg HBRUSH OnCtlColor( CDC *pDC, CWnd *pWnd, UINT nCtlColor );
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnCbnSelchangeComboDevices();
	afx_msg void OnBnClickedCheckNotifications();
	afx_msg void OnBnClickedCheckPlaysound();
	afx_msg void OnBnClickedCheckStartupprogram();
	afx_msg void OnBnClickedCheckClosetotray();
	afx_msg void OnBnClickedCheckOverlayenable();
	afx_msg void OnBnClickedRestartappButton();
	afx_msg void OnCbnSelchangeComboImageposition();
	afx_msg void OnCbnSelchangeComboImagesize();
};

extern COutfledMicrophoneMuteDlg *g_pAppWindow;

// Display Error
inline void DisplayAppError( LPCWSTR lpszMessage );
inline void DisplayAppError( DWORD dwError );