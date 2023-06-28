# Must be run with admin privileges

# get IntelliMouse WMI object
$mouse = Get-CimInstance -Namespace root\WMI -Class FireflyDeviceInformation

# check if mouse is active
$mouse.Active

# get current tail-light color
$mouse.TailLight

# set tail-light to blue
$mouse.TailLight = 0xFF0000 # 0xBBGGRR format
Set-CimInstance -CimInstance $mouse -PassThru
