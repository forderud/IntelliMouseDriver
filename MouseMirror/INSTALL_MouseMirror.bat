@echo off
:: Goto current directory
cd /d "%~dp0"

:: Install driver certificate
certmgr.exe /add MouseMirror.cer /s /r localMachine root
certmgr.exe /add MouseMirror.cer /s /r localMachine trustedpublisher

:: Use PnpUtil for installation
PNPUTIL /add-driver MouseMirror.inf /install /reboot

pause
