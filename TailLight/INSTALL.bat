@echo off
:: Goto current directory
cd /d "%~dp0"

:: create devnode
devgen /add /bus ROOT /hardwareid "HID\VID_045E&PID_082A&MI_01&Col05"

:: install driver
PNPUTIL /add-driver TailLight.inf /install /reboot

pause
