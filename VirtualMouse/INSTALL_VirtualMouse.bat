@echo off
:: Goto current directory
cd /d "%~dp0"

:: Install driver certificate
certmgr.exe /add VirtualMouse.cer /s /r localMachine root
certmgr.exe /add VirtualMouse.cer /s /r localMachine trustedpublisher

:: Install driver
PNPUTIL /add-driver VirtualMouse.inf /install /reboot

:: Create virtual mouse
devgen /add /instanceid 1 /hardwareid "Root\VirtualMouse"

pause
