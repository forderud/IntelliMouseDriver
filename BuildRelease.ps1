# Script for building and packaging releases of this project.
# Run from "Developer PowerShell for VS 2022"

# stop script on first error
$ErrorActionPreference = "Stop"

### Build solution in x64|Release and ARM64|Release
msbuild /nologo /verbosity:minimal /property:Configuration="Release"`;Platform="x64" IntelliMouse.sln
if ($LastExitCode -ne 0) {
    throw "msbuild failure"
}
msbuild /nologo /verbosity:minimal /property:Configuration="Release"`;Platform="ARM64" IntelliMouse.sln
if ($LastExitCode -ne 0) {
    throw "msbuild failure"
}

### Package drivers, test-utilities and scripts
New-Item -Name dist -ItemType "directory"

# Compress TailLight driver
Compress-Archive -Path x64\Release\TailLight   -DestinationPath dist\TailLight_x64.zip
Compress-Archive -Path ARM64\Release\TailLight -DestinationPath dist\TailLight_arm64.zip
# Compress MouseMirror driver
Compress-Archive -Path x64\Release\MouseMirror   -DestinationPath dist\MouseMirror_x64.zip
Compress-Archive -Path ARM64\Release\MouseMirror -DestinationPath dist\MouseMirror_arm64.zip
# Compress VirtualMouse driver
Compress-Archive -Path x64\Release\VirtualMouse   -DestinationPath dist\VirtualMouse_x64.zip
Compress-Archive -Path ARM64\Release\VirtualMouse -DestinationPath dist\VirtualMouse_arm64.zip

# Copy MouseMove utility
Copy-Item -Path x64\Release\MouseMove.exe   -Destination dist\MouseMove_x64.zip
Copy-Item -Path arm64\Release\MouseMove.exe -Destination dist\MouseMove_arm64.zip
# Copy HidUtil utility
Copy-Item -Path x64\Release\HidUtil.exe   -Destination dist\HidUtil_x64.zip
Copy-Item -Path arm64\Release\HidUtil.exe -Destination dist\HidUtil_arm64.zip
# Copy flicker utility
Copy-Item -Path x64\Release\flicker.exe   -Destination dist\flicker_x64.zip
Copy-Item -Path arm64\Release\flicker.exe -Destination dist\flicker_arm64.zip

# Copy test scripts
Copy-Item -Path x64\Release\*.ps1 -Destination dist
