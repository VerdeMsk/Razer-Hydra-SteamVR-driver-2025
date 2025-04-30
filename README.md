# Razer Hydra Driver for SteamVR 2025
The driver emulates Valve Index or Oculus Touch controllers using Razer Hydra controllers

## Overview
This version is based on the r57zone Hydra SteamVR driver, but heavily modified with the help of ChatGPT.
Vive Wand emulation has been replaced with Oculus Touch emulation, while Valve Index support is preserved.
The Sixense filtering system was integrated — this is the same filter you might have seen in MotionCreator2.
Originally, it caused high latency and poor responsiveness. Its logic has been improved to behave more dynamically:
The filter strength increases when the controller is idle or far from the base.
It decreases when the controller is moving or close to the base station.
The Hydra magnetic base station is also now displayed in SteamVR (custom-built 3D model).
To reduce conflicts with other drivers, it currently behaves as a SteamVR tracker, not as a base station.
However, this workaround hasn’t eliminated all potential issues...
Its position is recalculated during calibration, based on controller position.
Rotation is currently not calculated.

[https://youtu.be/ugt8mn3hZBU?si=OYFKQwaMa-GF-Qem]

## Features
- Oculus Touch emulation replaces Vive Wand emulation
- Dynamic filtering added
	Based on Sixense filtering, similar to MotionCreator2, but reworked to dynamically respond to distance and motion.
	Stronger filtering when idle or far from the base; lighter filtering when moving or near the base.
- Toggle and hold modes for grip buttons
	No need to hold the bumper button constantly — you can toggle grip with a single press
- Configurable velocity multiplier
	Helps increase throw speed in games. Adjustable via the config.
- Hydra base station visualization (as a SteamVR tracker)
	The magnetic base is shown in VR. Its position is auto-calculated during calibration. Rotation is not yet implemented.
- Alternative IMU emulation logic added
	The original IMU smoothing relied on Sixense Utils.
	The new version uses direct position deltas for calculating velocity and acceleration.
	Actually not a much of a difference, but the seems like velocity multiplier causes less jitter with the new IMU in some cases.
- Updated to the latest OpenVR SDK
- Various small improvements and bugfixes

## Issues and Limitations
If Hydra base station visualization is enabled, it may conflict with other SteamVR drivers, like Driver4VR.
If you experience issues — disable the base station visualization in the config file.
The base station's position is estimated relative to the controllers and may be slightly inaccurate.
Its rotation is not currently calculated, so the station might appear facing the wrong direction.
The dynamic filter only uses the velocity of the fastest controller.
Angular velocity is not yet accounted for, which may cause a slight delay during quick rotations when the filter is strong.

## Index controller layout
Razer Hydra | Left Index Controller | Right Index Controller
------------ | ------------- | -------------
Button 1 | Button A | Filter on
Button 2 | Filter off | Button A
Button 3 | Button B | Crouch (or touchpad press)
Button 4 | Pressing the touchpad of the right controller | Button B
Bumper | Grip Button | Grip Button
Start button | System button | System button

### Stick modes (Index only)
Stick and touchpad mode | Hot key
------------ | -------------
Standard mode, the touchpad is not emulated. | `ALT` + `1`
The touchpad is emulated with a stick, the stick is disabled. | `ALT` + `2`
The touchpad is duplicated by a stick. | `ALT` + `3`
The touchpad is emulated with a stick, pressing dpad left and right, on the right controller, is inverted. | `ALT` + `4`
The touchpad is emulated with a stick, dpad up and up on the right controller are inverted. | `ALT` + `5`

## Oculus Touch layout
Razer Hydra | Left Oculus Touch | Right Oculus Touch
------------ | ------------- | -------------
Button 1 | Button X | Filter on
Button 2 | Filter off | Button A
Button 3 | Button Y | Crouch (or Thumbrest Touch)
Button 4 | Thumbrest Touch | Button B
Bumper | Grip Button | Grip Button
Start button | System button | System button

## Installation

1. [Download](https://github.com/VerdeMsk/Razer-Hydra-SteamVR-driver-2025/releases) the latest driver.
2. Unpack archive to "..\Steam\steamapps\common\SteamVR\drivers".
3. [Add option](https://youtu.be/QCA3m4_3IJM?t=197) `"activateMultipleDrivers" : true,` to config "...\Steam\config\steamvr.vrsettings", to `steamvr` section.

## Config
### default.vrsettings file in Steam\steamapps\common\SteamVR\drivers\hydra\resources\settings
IndexControllers — true - Valve Knuckles; false - Oculus Touch
GripToggle — true - toggle mode; false - hold mode
JoyStickDeadZone — To determine the value of the dead zone can use [this program](https://github.com/r57zone/Sixence-Razer-Hydra-sample/releases)
CrouchPressKey — the name of the desired button can be found [here](https://github.com/r57zone/DualShock4-emulator/blob/master/BINDINGS.md).
CrouchOffset — self explanatory. Recommended 0.7. For HMD, you can use any driver that supports crouch by button. For example, you can use [OpenVR-ArduinoHMD Driver](https://github.com/r57zone/OpenVR-ArduinoHMD) or [OpenVR-OpenTrack](https://github.com/r57zone/OpenVR-OpenTrack) drivers for HMD (FreeTrack for HMD from smartphones or ArduinoHMD for [full-fledged DIY headsets](https://github.com/TrueOpenVR/TrueOpenVR-DIY/blob/master/HMD/HMD.md)).
EnableCustomKey — self explanatory.
CustomPressKey — the name for the desired button can be found [here](https://github.com/r57zone/DualShock4-emulator/blob/master/BINDINGS.md)
ShowBaseStation — true - on; false - off
EnableImu — true - on, recommended; false - off, throwing might not work in some games
AlternativeImuVersion — true - new; false - original. P.S. similar results
ThrowMultiplier — more - higher velocity and acceleration. Recommended 2.0 - 3.0
SixenseFilterEnabled — true - on; false - off. P.S. you can still use buttons to turn it on/off
DynamicFilterPower — higher - more power (closer to MaxFilteringValue)
MinFilteringValue — Minimum filtering. When close to the base station or when controller is in motion. Higher - more power. Recommended 0.75 - 0.85
MaxFilteringValue — Maximum filtering. When far from the base station or when controller is standing still. Higher - more power. Recommended 0.9

##Troubleshooting
**• Hydra overlay doesn’t show up in SteamVR, even though controllers are connected:
Launch MotionCreator2, wait for controllers to appear, then close it.
**• One controller rotates wildly:
Try placing it back on the base station — often it will reset.
If the issue persists [(like that)](https://twitter.com/r57zone/status/1467868670609305600)**<br>, the main coil contacts going into the circuit are oxidized and need to be stripped, scratched or soldered directly without a connector.
**• Controller tracking breaks and one flies off into space:
Place both controllers on the base station — tracking should recalibrate. This can happen near magnetic interference sources (even HMD).
**• Trigger not working in SteamVR dashboard:
Likely a grip button is pressed — release it.
**• Ingame menu not opening:
Check your current input binding in SteamVR settings. You can also switch controller emulation type (Touch vs Knuckles).
**• Noticeable motion delay:
Reduce or disable the filter settings in the config file.
**• Controllers feel bouncy or too fast:
Likely due to a high velocity multiplier — reduce the "ThrowMultiplier" value.
**• Controllers move in a “steppy” way:
Likely caused by high "MaxFilteringValue". Try lowering it to reduce this snapping motion.
**• The stick is tilted to one side and does not move to the opposite side**<br>
Close SteamVR, unplug the controllers USB cable, wait 5-10 seconds and plug it back in.
**• Driver don't work:**
1. Uninstall the previous installed driver on Steam or folder.
2. Download the [MotionCreator utility](https://github.com/r57zone/Razer-Hydra-SteamVR-driver/releases/tag/1) (official utility by Sixence), switch "Controller Mode" to "Motion controller" mode.
3. Remove MotionCreator.
If it doesn’t help, try another utility RazerHydra [[1]](https://support.razer.com/console/razer-hydra)[[2]](https://github.com/r57zone/Razer-Hydra-SteamVR-driver/releases/tag/1) (official utility by Razer).
**• The cursor moves:**<br>
Close or uninstall MotionCreator or RazerHydra utility.

## Special Thanks
- To Valve for providing the original driver code and the OpenVR.
- To BetaVR for bringing the first Razer Hydra driver to Steam.
- To r57zone for the modified version, which became the starting point for this one.
- To Sixense and Razer for creating the Razer Hydra and SDK.
- To OpenAI for ChatGPT, which bring this driver to life.

## Building
1. Download the sources and unzip them.
2. [Download "openvr"](https://github.com/ValveSoftware/openvr) and unpack to "C:\openvr".
3. [Download "SixenseSDK_102215.zip"](https://github.com/r57zone/Razer-Hydra-SteamVR-driver/releases/tag/1) and unpack to "C:\SixenseSDK_102215".
2. [Download](https://code.visualstudio.com/download) and [install](https://github.com/r57zone/RE4ExtendedControl/assets/9499881/69dafce6-fd57-4768-83eb-c1bb69901f07) Microsoft Visual Studio Code 2017+.
5. Change the SDK version and toolset to yours in the project properties, then select the "Release" build type and "x86" or "x64" architecture and compile.