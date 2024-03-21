@echo off
:: Goto current directory
cd /d "%~dp0"

:: Trust driver certificate
certutil.exe -addstore root VirtualMouse.cer
certutil.exe -addstore trustedpublisher VirtualMouse.cer

:: Install driver
PNPUTIL /add-driver VirtualMouse.inf /install /reboot

:: Create virtual mouse
devgen /add /instanceid 1 /hardwareid "Root\VirtualMouse"

pause
