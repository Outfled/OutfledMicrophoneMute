#pragma once

#define MICROPHONE_MUTED_EVENT			L"Global\\OutfledMicrophoneMute_MutedEvent"
#define MICROPHONE_UNMUTED_EVENT		L"Global\\OutfledMicrophoneMute_UnmutedEvent"

// Global Events 
extern HANDLE g_hTrayIconMicrophoneMuted;
extern HANDLE g_hTrayIconMicrophoneUnmuted;

DWORD WINAPI MicrophoneHandleThread( LPVOID );