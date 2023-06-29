Filter driver for Microsoft Pro IntelliMouse that exposes a WMI interface. Tested on [Pro IntelliMouse](https://www.microsoft.com/en/accessories/products/mice/microsoft-pro-intellimouse) Shadow with `VendorID=045E` (Microsoft) and `ProductID=082A` (Pro IntelliMouse).

Projects:
* `firefly.sys`: An upper device filter driver for the HID class for Microsoft Pro Intellimouse. Based on the [KMDF filter driver for a HID device](https://github.com/microsoft/Windows-driver-samples/tree/main/hid/firefly) sample from Microsoft. During start device, the driver registers a [FireflyDeviceInformation](firefly/firefly.mof) WMI class. The user mode application connects to the WMI namespace (root\\wmi) and opens this class using COM interfaces. Then the application can make requests to read ("get") or change ("set") the current value of the TailLight data value from this class. In response to a set WMI request, the driver opens the HID collection using IoTarget and creates a feature report to update the feature that controls the light.
* `IntelliMouse.ps1`: PowerShell script for sending commands through the WMI interface.
* `flicker.exe`: Application for causing the mouse to blink by sending commands through the WMI interface.
* `HidUtil.exe`: Command-line utility for querying and communicating with HID devices.

Please see the [wiki](../../wiki) for documentation content.
