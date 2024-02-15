@echo off
:: Goto current directory
cd /d "%~dp0"

:: delete the devnode
devcon /r remove "HID\VID_045E&PID_082A&MI_00&Col01"
:: TODO: Switch to PNPUTIL in Win11 21H2
::PNPUTIL /remove-device /deviceid "HID\VID_045E&PID_082A&MI_00&Col01"

:: uninstall driver
PNPUTIL /delete-driver MouseMirror.inf /uninstall /force /reboot

:: Delete MouseMirrorDeviceInformation WMI class security descriptor
reg delete "HKLM\SYSTEM\CurrentControlSet\Control\WMI\Security" /v "97754ED3-63DF-485E-A7B9-E4BDD92C8BF3" /f

pause
