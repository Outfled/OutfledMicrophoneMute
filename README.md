# Microphone-Mute
Windows 10/11 Program to mute/unmute your microphone using key binds

<img width="331" height="365" alt="Capture" src="https://github.com/user-attachments/assets/ce509cb2-8f6a-426a-8836-a34a918c133f" />

## Features

-Mizimize to Tray

-Mute/Unmute Using Custom Key Binds

-Send Windows Notifications on Mute/Unmute (Optional)

-Play Sound Effect on Mute/Unmute (Optional)

-Automatically Starts up with Windows

-Mute/Unmute Using the Tray Icon

-Game overlay icon to show microphone mute status

## In-Game Overlay

To enable the in-game overlay, check the Enabled' box on the main window

Here's an example with Overwatch with my mic currently unmuted; **the image size is set to top left & the image size is set to 450x450**:

![top_left](https://github.com/user-attachments/assets/b1f15b30-641c-4563-aa86-08e0e2192295)


    Currently, only games that use DirectX 11 are supported.

### Icon Location:

Once enabled, you can choose where to display the overlay icon in the game

<img width="328" height="183" alt="Untitled2" src="https://github.com/user-attachments/assets/ab0d151f-ab91-4066-8ea8-f64cf654829f" />


### Icon Size:

The default size is 450x450 (pixels).

**You can change the size to one of the preset dimensions**

<img width="328" height="204" alt="size" src="https://github.com/user-attachments/assets/c8a5b504-0289-4bd3-9a7d-0d0896c18d15" /><br /><br />



**Additionally, you can change the dimensions to a custom size if none of the preset dimensions are desirable.**
**It requires modifying 2 registry values**

Navigate to the registry key:

    HKEY_CURRENT_USER\SOFTWARE\Outfled\Microphone Mute

To modify the image size width (in pixels):
    
    Right click 'OverlayIconWidth'. Underneath 'Base' make sure 'Decimal' is selected. Then enter the desired width in the value data

To modify the image size height (in pixels):

    Right click 'OverlayIconHeight'. Underneath 'Base' make sure 'Decimal' is selected. Then enter the desired height in the value data

<br /><br />


# Help/Contact
For any questions or concerns, my discord is ***outfled***
