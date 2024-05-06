# Get IntelliMouse WMI object
# HW drivers reside in the "root\wmi" namespace (https://learn.microsoft.com/en-us/windows/win32/wmicoreprov/wdm-provider)
$mouse = Get-CimInstance -Namespace root\WMI -Class TailLightDeviceInformation

Write-Host("IntelliMouse device:")
Write-Host("  InstanceName: {0}" -f $mouse.InstanceName)
Write-Host("  Active: {0}" -f $mouse.Active)

# get current tail-light color
Write-Host("  Color: {0:x}" -f $mouse.TailLight) # display as hex string

# change tail-light color
$mouse.TailLight = 0xFF0000 # 0xBBGGRR format
Write-Host("  Changing color to {0:x}" -f $mouse.TailLight) # display as hex string

Write-Host("Storing changes.")
Set-CimInstance -CimInstance $mouse

Write-Host("Running HW self-test...")
$res = Invoke-CimMethod -InputObject $mouse -MethodName SelfTest
Write-Host("  result: {0}" -f $res.result)
Write-Host("  ReturnValue: {0}" -f $res.ReturnValue)
