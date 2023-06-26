# Must be run with admin privileges!

# query IntelliMouse WMI class
$mouse = Get-CimInstance -Namespace root\WMI -Class FireflyDeviceInformation

# check if mouse is active
$mouse.Active

# get tail-light status
$mouse.TailLit

# update tail-light
$mouse.TailLit = 0
