@echo off
:: Goto current directory
cd /d "%~dp0"

:: uninstall driver
PNPUTIL /delete-driver MouseMirror.inf /uninstall /force

:: Delete MouseMirrorDeviceInformation WMI class security descriptor
reg delete "HKLM\SYSTEM\CurrentControlSet\Control\WMI\Security" /v "97754ED3-63DF-485E-A7B9-E4BDD92C8BF3" /f

pause
