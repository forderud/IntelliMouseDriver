Filter drivers for Microsoft Pro IntelliMouse that implements safety checks and exposes WMI interfaces. Based on the [KMDF filter driver for a HID device](https://github.com/microsoft/Windows-driver-samples/tree/main/hid/firefly) sample from Microsoft that have been updated to work with more recent IntelliMouse models ([backport request](https://github.com/microsoft/Windows-driver-samples/issues/1022)). The mouse should report itself with `VendorID=045E` (Microsoft) and `ProductID=082A` (Pro IntelliMouse).

### Projects
* `MouseMirror`: An upper device filter driver for the Mouse class for Microsoft Pro Intellimouse. Registers a [MouseMirrorDeviceInformation](MouseMirror/MouseMirror.mof) WMI class that can be accessed from user mode to mirror mouse movement. Can easily be modified to also work with other mouse models.
* `MouseMirror.ps1`: PowerShell script for enabling mirroring of mouse movement through the WMI interface.
* `TailLight`: An upper device filter driver for the HID class for Microsoft Pro Intellimouse. Registers a [TailLightDeviceInformation](TailLight/TailLight.mof) WMI class that can be accessed from user mode to control the tail-light.
* `TailLight.ps1`: PowerShell script for updating the tail-light through the WMI interface.
* `flicker`: Application for causing the mouse to blink by sending commands through the WMI interface.
* `HidUtil`: Command-line utility for querying and communicating with HID devices.

### Prerequisites
* Microsoft [Pro IntelliMouse](https://www.microsoft.com/en/accessories/products/mice/microsoft-pro-intellimouse)
* Separate computer for driver testing. Needed to avoid crashing or corrupting your main computer in case of driver problems.
* USB 3 A/A crossover cable for kernel debugging. Can also use a network connection if the machine has a compatible network adapter.

**Getting started information for [driver testing](../../wiki/Driver-testing)**.

<img alt="Prerequisites" src="Prerequisites.png" width="50%" height="50%" />  
