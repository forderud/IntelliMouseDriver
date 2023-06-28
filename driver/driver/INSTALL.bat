@echo off
:: Goto current directory
cd /d "%~dp0"

:: Use DevCon for installation (leads to an malfunctioning _additional_ device)
::devcon install usbsamp.inf "HID\VID_045E&PID_082A"

:: Use PnpUtil for installation (succeeds by driver must install in device manager afterwards)
PNPUTIL /add-driver firefly.inf /install /reboot

pause
