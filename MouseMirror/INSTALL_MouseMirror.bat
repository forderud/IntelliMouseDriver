@echo off
:: Goto current directory
cd /d "%~dp0"

:: Trust driver certificate
certutil.exe -addstore root MouseMirror.cer
certutil.exe -addstore trustedpublisher MouseMirror.cer

:: Use PnpUtil for installation
PNPUTIL /add-driver MouseMirror.inf /install

pause
