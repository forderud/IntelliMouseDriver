@echo off
:: Goto current directory
cd /d "%~dp0"

:: install driver
PNPUTIL /add-driver TailLight.inf /install /reboot

pause
