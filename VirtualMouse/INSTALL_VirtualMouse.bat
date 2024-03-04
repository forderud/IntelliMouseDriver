@echo off
:: Goto current directory
cd /d "%~dp0"

:: Install driver certificate
certmgr.exe /add UDEFX2.cer /s /r localMachine root
certmgr.exe /add UDEFX2.cer /s /r localMachine trustedpublisher

:: Install driver
PNPUTIL /add-driver UDEFX2.inf /install /reboot

:: Create virtual mouse
devgen /add /instanceid 1 /hardwareid "Root\UDEFX2"

pause
