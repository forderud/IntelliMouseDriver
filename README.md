Filter driver for Microsoft Pro IntelliMouse that exposes a WMI interface. Tested on [Pro IntelliMouse](https://www.microsoft.com/en/accessories/products/mice/microsoft-pro-intellimouse) Shadow with `VendorID=045E` (Microsoft) and `ProductID=082A` (Pro IntelliMouse).

Projects:
* `firefly.sys`: An upper device filter driver for the HID class for Microsoft Pro Intellimouse. Based on the [KMDF filter driver for a HID device](https://github.com/microsoft/Windows-driver-samples/tree/main/hid/firefly) sample from Microsoft. During start device, the driver registers a [FireflyDeviceInformation](firefly/firefly.mof) WMI class. The user mode application connects to the WMI namespace (root\\wmi) and opens this class using COM interfaces. Then the application can make requests to read ("get") or change ("set") the current value of the TailLight data value from this class. In response to a set WMI request, the driver opens the HID collection using IoTarget and creates a feature report to update the feature that controls the light.
* `IntelliMouse.ps1`: PowerShell script for sending commands through the WMI interface.
* `flicker.exe`: Application for causing the mouse to blink by sending commands through the WMI interface.
* `HidUtil.exe`: Command-line utility for querying and communicating with HID devices.


## Driver development
Prerequisite: Built the driver, so that you already have `firefly.sys` and `firefly.inf` available.

Relevant documentation:
* [Windows Driver Kit (WDK)](https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk) installation.

### Target computer configuration
Steps to configure the *target* computer for driver testing:
* Disable Secure Boot in UEFI/BIOS.
* Enable test-signed drivers: [`bcdedit /set testsigning on`](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/the-testsigning-boot-configuration-option).
* Configuration of [kernel-mode debugging over a USB 3.0 cable](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/setting-up-a-usb-3-0-debug-cable-connection) with a USB 3 A/A crossover cable:
  - `bcdedit /debug on`
  - `bcdedit /dbgsettings usb targetname:KernelUSBConn`
* From the host computer, connect with the WinDbg over USB to the `KernelUSBConn` target.
* Restart the target computer.
* Reconnect to the target computer using WinDbg.
* Break execution and send the following command to enable display of debug messages: `kd>ed nt!Kd_DEFAULT_Mask 0xff`.

### Driver testing
Driver installation:
* Open "Device Manager".
* Switch to "Devices by connection" view.
* Navigate to the "HID-compliant vendor-defined device" with `HID\VID_045E&PID_082A&MI_01&Col05` hardware ID.
![DeviceManager](DeviceManager.png)
* Right-click on the relevant device, and select "Update driver".
* Click on "Browse my computer for drivers".
* Click on "Let me pick from a list...".
* Click on "Have Disk..." and select `firefly.inf` in the file system:
![ManualDriverPick](ManualDriverPick.png)
* Click on "Install this driver software anyway" when being warned about the publisher:
![UnsignedDriverConfirm](UnsignedDriverConfirm.png)

Driver uninstallation:
* Either uninstall the driver from "Device Manager",
* or run `PNPUTIL /delete-driver firefly.inf /uninstall` from an admin command-prompt.
