::The default location for certmgr.exe is at 
::C:\Program Files (x86)\Windows Kits\10\bin\<sdk version>\x64
::It is added automatically to the include path in a Visual Studio 
::developer command prompt.
::Do no confuse with certmgr.msc.

certmgr.exe /c /add TailLight.cer /s root /r localMachine /v
certmgr.exe /c /add TailLight.cer /s trustedpublisher /r localMachine /v