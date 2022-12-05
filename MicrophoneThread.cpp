#include "pch.h"
#include "MicrophoneThread.h"
#include "Outfled Microphone MuteDlg.h"
#include "AppTrayIcon.h"
#include <mmhandler.h>
#include <strsafe.h>
#include <Mmsystem.h>
#include "resource.h"

#pragma comment ( lib, "Winmm.lib" )

#define MUTED_SOUND_EFFECT		L"sound\\muted.wav"
#define UNMUTED_SOUND_EFFECT		L"sound\\unmuted.wav"

DWORD WINAPI MicrophoneStatusThread( LPVOID lpParam )
{
	HRESULT				Result;
	COutfledMicrophoneMuteDlg	*DlgWnd;

	DlgWnd = (COutfledMicrophoneMuteDlg *)lpParam;

	/* Initialize COM on thread */
	Result = CoInitialize( NULL );
	if (Result == S_OK)
	{
		BOOL bMuted = FALSE;

		while (TRUE)
		{
			// Add sleep to prevent high cpu usage
			Sleep( 30 );
			
			/* Microphone not set */
			if (0 == wcslen( DlgWnd->m_szMicrophoneDevice ))
			{
				continue;
			}

			/* Check if toggle key is pressed */
			if (0 == CheckKeyPressedStatus( DlgWnd->m_dwToggleKeys ))
			{
				continue;
			}

			/* Check if modifier keys pressed */
			if (DlgWnd->m_wModifiers & HOTKEYF_ALT)
			{
				if (0 == CheckKeyPressedStatus( VK_MENU ))
				{
					continue;
				}
			}
			if (DlgWnd->m_wModifiers & HOTKEYF_CONTROL)
			{
				if (0 == CheckKeyPressedStatus( VK_CONTROL )) {
					continue;
				}
			}
			if (DlgWnd->m_wModifiers & HOTKEYF_SHIFT)
			{
				if (0 == CheckKeyPressedStatus( VK_SHIFT )) {
					continue;
				}
			}

			/* Get the microphone status */
			Result = MMGetMicrophoneValue( DlgWnd->m_szMicrophoneDevice, MIC_DEVICE_VALUE_ISMUTE, &bMuted );
			if (FAILED( Result ))
			{
				if (DlgWnd->m_bSendNotifications) {
					SendTrayNotification( ErrorCodeToString( Result ) );
				}
				else {
					DlgWnd->MessageBox( ErrorCodeToString( Result ), L"Outfled Microphone Mute");
				}
				
				continue;
			}

			/* Set the new microphone status */
			Result = MMSetMicrophoneValue( DlgWnd->m_szMicrophoneDevice, MIC_DEVICE_VALUE_ISMUTE, &(bMuted = !bMuted) );
			if (FAILED( Result ))
			{
				if (DlgWnd->m_bSendNotifications) {
					SendTrayNotification( ErrorCodeToString( Result ) );
				}
				else {
					DlgWnd->MessageBox( ErrorCodeToString( Result ), L"Outfled Microphone Mute" );
				}

				continue;
			}

			/* Send notification & play sound effect */
			switch (bMuted)
			{
			case TRUE:
				if (DlgWnd->m_bSendNotifications) {
					SendTrayNotification( L"Muted" );
				}
				if (DlgWnd->m_bPlaySoundEffects) {
					PlaySound( MUTED_SOUND_EFFECT, NULL, SND_FILENAME | SND_NODEFAULT );
				}

				/* Update tray icon hover message & icon image */
				UpdateTrayIconTitle( L"Outfled Microphone Mute (Muted) " );
				UpdateTrayIconImage( AfxGetApp()->LoadIcon( IDI_ICON_MUTED ) );

				/* Update app icon */
				DlgWnd->SetIcon( AfxGetApp()->LoadIcon( IDI_ICON_MUTED ), TRUE );
				DlgWnd->SetIcon( AfxGetApp()->LoadIcon( IDI_ICON_MUTED ), FALSE );
				
				break;
			case FALSE:
				if (DlgWnd->m_bSendNotifications) {
					SendTrayNotification( L"Un-Muted" );
				}
				if (DlgWnd->m_bPlaySoundEffects) {
					PlaySound( UNMUTED_SOUND_EFFECT, NULL, SND_FILENAME | SND_NODEFAULT );
				}

				/* Update tray icon hover message & icon image */
				UpdateTrayIconTitle( L"Outfled Microphone Mute (Un-muted) " );
				UpdateTrayIconImage( AfxGetApp()->LoadIcon( IDI_ICON_UNMUTED ) );

				/* Update app icon */
				DlgWnd->SetIcon( AfxGetApp()->LoadIcon( IDI_ICON_UNMUTED ), TRUE );
				DlgWnd->SetIcon( AfxGetApp()->LoadIcon( IDI_ICON_UNMUTED ), FALSE );
				
				break;
			}

			DlgWnd->UpdateWindow();

			/* Wait for toggle key to be unpressed */
			while (CheckKeyPressedStatus( DlgWnd->m_dwToggleKeys ))
			{
				Sleep( 1 );
			}
		}

		CoUninitialize();
	}

	return 1;
}
