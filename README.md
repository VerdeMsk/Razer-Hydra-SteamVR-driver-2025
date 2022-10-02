[![EN](https://user-images.githubusercontent.com/9499881/33184537-7be87e86-d096-11e7-89bb-f3286f752bc6.png)](https://github.com/r57zone/Razer-Hydra-SteamVR-driver) 
[![RU](https://user-images.githubusercontent.com/9499881/27683795-5b0fbac6-5cd8-11e7-929c-057833e01fb1.png)](https://github.com/r57zone/Razer-Hydra-SteamVR-driver/blob/master/README.RU.md) 
# Razer Hydra Driver for SteamVR
The driver emulates Valve Index or HTC Vive controllers using Razer Hydra controllers. The type of controllers is switched in the settings. Changeable keyboard button press and crouch are supported.

![](https://user-images.githubusercontent.com/9499881/191363340-17717ab4-8825-4904-aadd-1b8f606df23a.gif) ![](https://user-images.githubusercontent.com/9499881/191363360-531c6bc8-e294-43f4-a523-79aa4a6bb16e.gif)

## Index controller layout
Razer Hydra | Left Index Controller | Right Index Controller
------------ | ------------- | -------------
Button 1 | Button A | Grip button
Button 3 | Button B | Crouch
Button 2, bumper | Grip Button | Button A
Button 4 | Pressing the touchpad of the right controller | Button B
Start button | System button | System button

### Stick modes
Stick and touchpad mode | Hot key
------------ | -------------
Standard mode, the touchpad is not emulated. | `ALT` + `1`
The touchpad is emulated with a stick, the stick is disabled. | `ALT` + `2`
The touchpad is duplicated by a stick. | `ALT` + `3`
The touchpad is emulated with a stick, pressing dpad left and right, on the right controller, is inverted. | `ALT` + `4`
The touchpad is emulated with a stick, dpad up and up on the right controller are inverted. | `ALT` + `5`

## Vive controller layout
Razer Hydra | Left Vive controller | Right Vive Controller
------------ | ------------- | -------------
Button 1 | Menu button | Grip button
Button 3 | Pressing dpad down on the right controller | Crouch
Button 2, bumper | Grip Button | Menu button
Button 4 | Changeable keyboard button press, by default, this is the `V` | Pressing dpad up on the right controller.
Start button | System button | System button

### Stick modes
Stick and touchpad mode | Hot key
------------ | -------------
Standard mode. | `ALT` + `1`
Presses on dpad left and right on the right controller are inverted. | `ALT` + `2`
All presses are inverted, except for dpad up and up on the right controller. | `ALT` + `3`
All clicks are inverted. | `ALT` + `4`

### Other features
Description | Razer Hydra Button
------------ | -------------
Turning on, off crouch | `ALT` + `9` and `ALT` + `0` (replaced to touchpad press)

- For HMD, you can use any driver that supports crouch by button. For example, you can use [OpenVR-ArduinoHMD Driver](https://github.com/r57zone/OpenVR-ArduinoHMD) or [TrueOpenVR and SteamVR Bridge Driver](https://github.com/TrueOpenVR) for HMD (FreeTrack for HMD from smartphones or ArduinoHMD for [full-fledged DIY headsets](https://github.com/TrueOpenVR/TrueOpenVR-DIY/blob/master/HMD/HMD.RU.md)).

- You can change the type of controllers from Valve Index to HTC Vive by changing the value `true` to `false`, parameter `IndexControllers`, in the configuration file "default.vrsettings", parameter `CustomPressKey`

- While pressing button 3, on the right Razer Hydra controller, the keyboard button is also pressed (the button is configurable). Crouch settings can be found in the configuration file "default.vrsettings".

- Supports pressing the keyboard button, on button 4, of the left controller. By default, this is the `V` button, you can change it in the "default.vrsettings" configuration file, the `CustomPressKey` parameter, the code for the desired button can be found [here](https://github.com/r57zone/Half-Life-Alyx-novr/blob/master/BINDINGS.md#codes). You can turn it on in the configuration file by changing the value of `false` to `true`, the `EnableCustomKey` parameter and it will replace pressing the controller touchpad.

- You can also edit the controller layout in the "SteamVR Bindings UI" by opening the SteamVR settings, selecting "Advance Settings" -> "Show" and going to the controllers item.

## Installation

1. [Download](https://github.com/r57zone/Razer-Hydra-SteamVR-driver/releases/) the latest driver.
2. Unpack archive to "..\Steam\steamapps\common\SteamVR\drivers".
3. [Add option](https://youtu.be/QCA3m4_3IJM?t=197) `"activateMultipleDrivers" : true,` to config "...\Steam\config\steamvr.vrsettings", to `steamvr` section.
4. Change the dead zone if your stick goes to the side in config "..\Steam\steamapps\common\SteamVR\drivers\razer_hydra\hydra\resources\settings\default.vrsettings", option `JoyStickDeadZone`. To determine the value of the dead zone can use [this program](https://github.com/r57zone/Sixence-Razer-Hydra-sample/releases).

For more detailed instructions please visit the [wiki](https://github.com/betavr/steamvr_driver_hydra/wiki).

## Problems solving
**• The stick is tilted to one side and does not move to the opposite side**<br>
Close SteamVR, unplug the controllers USB cable, wait 5-10 seconds and plug it back in.


**• Driver don't work:**
1. Uninstall the previous installed driver on Steam or folder.
2. Download the [MotionCreator utility](https://github.com/r57zone/Razer-Hydra-SteamVR-driver/releases/tag/1) (official utility by Sixence), switch "Controller Mode" to "Motion controller" mode.
3. Remove MotionCreator.

If it doesn’t help, try another utility RazerHydra [[1]](https://support.razer.com/console/razer-hydra)[[2]](https://github.com/r57zone/Razer-Hydra-SteamVR-driver/releases/tag/1) (official utility by Razer).


**• The cursor moves:**<br>
Uninstall MotionCreator or RazerHydra utility.


**• Controllers spin insanely when pushed away from the base station [(like that)](https://twitter.com/r57zone/status/1467868670609305600)**<br>
The main coil contacts going into the circuit are oxidized and need to be stripped, scratched or soldered directly without a connector.

## Building
1. Download sources and unpack.
2. [Download "openvr"](https://github.com/ValveSoftware/openvr) and unpack to "C:\openvr".
3. [Download "SixenseSDK_102215.zip"](https://github.com/r57zone/Razer-Hydra-SteamVR-driver/releases/tag/1) and unpack to "C:\SixenseSDK_102215".
4. [Download Microsoft Visual Studio Code 2017](https://code.visualstudio.com/download) and compile.
5. Change the SDK version and toolset to yours in the project properties, and then select the "Release" build type and "x86" or "x64" architecture.