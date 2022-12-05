#pragma once

#define	CheckKeyPressedStatus(keys)			( GetKeyState(keys) & 0x8000 )

// Thread to listen for toggle keys and then mute/unmute microphone
DWORD WINAPI MicrophoneStatusThread( LPVOID lpParam );