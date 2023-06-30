# Must be run with admin privileges

# get IntelliMouse WMI object
$mouse = Get-CimInstance -Namespace root\WMI -Class FireflyDeviceInformation

Write-Host "IntelliMouse device:"

$name = $mouse.InstanceName
Write-Host "  InstanceName: $name"

# check if mouse is active
$active = $mouse.Active
Write-Host "  Active: $active"

# get current tail-light color
$color = $mouse.TailLight
$colorStr = "{0:x}" -f $color # convert to hex string
Write-Host "  Color: $colorStr"

# set tail-light to blue
$color = 0xFF0000 # 0xBBGGRR format
$colorStr = "{0:x}" -f $color # convert to hex string
Write-Host "  Changing color to $colorStr"
$mouse.TailLight = $color

Write-Host "Storing changes..."
Set-CimInstance -CimInstance $mouse -PassThru
