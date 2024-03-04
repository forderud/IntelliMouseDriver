@echo off
:: Goto current directory
cd /d "%~dp0"

:: Uninstall driver
pnputil.exe /delete-driver VirtualMouse.inf /uninstall /force /reboot

:: Delete virtual mouse
devgen /remove "SWD\DEVGEN\1"
:: TODO: Switch to PNPUTIL in Win11 21H2
::pnputil.exe /remove-device /deviceid "SWD\DEVGEN\1"


pause
