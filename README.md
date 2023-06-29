Filter driver for Microsoft Pro IntelliMouse that exposes a WMI interface. Tested on [Pro IntelliMouse](https://www.microsoft.com/en/accessories/products/mice/microsoft-pro-intellimouse) Shadow with `VendorID=045E` (Microsoft) and `ProductID=082A` (Pro IntelliMouse).

Projects:
* `firefly.sys`: An upper device filter driver for the HID class for Microsoft Pro Intellimouse. Based on the [KMDF filter driver for a HID device](https://github.com/microsoft/Windows-driver-samples/tree/main/hid/firefly) sample from Microsoft. Registers a [FireflyDeviceInformation](firefly/firefly.mof) WMI class that can be accessed from user mode to control the tail light.
* `IntelliMouse.ps1`: PowerShell script for sending commands through the WMI interface.
* `flicker.exe`: Application for causing the mouse to blink by sending commands through the WMI interface.
* `HidUtil.exe`: Command-line utility for querying and communicating with HID devices.

Please see the [wiki](../../wiki) for documentation content.
