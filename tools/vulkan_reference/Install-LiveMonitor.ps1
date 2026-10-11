param(
    [Parameter(Mandatory=$true)][string]$Shortcut,
    [Parameter(Mandatory=$true)][string]$ExpectedSha,
    [string]$OutputRoot='F:\PS4Dev\handoff\vulkan-live-monitor',
    [string]$Python='F:\PS4Dev\toolchain\python\cpython-3.11.16-windows-x86_64-none\python.exe',
    [string]$PresentMon='F:\tmp\PresentMon-2.6.0-x64.exe',
    [string]$Revision='974f8c211c5c6b277f8da2fadcd30c4d770cb27b-dirty',
    [string]$BuildLabel='DLSS test 974f8c21*'
)
$ErrorActionPreference='Stop'
$bytes=[IO.File]::ReadAllBytes($Shortcut)
if(([BitConverter]::ToUInt32($bytes,20) -band 0x2000) -ne 0){throw 'RunAs shortcut requires separate preservation review; original not changed.'}
$shell=New-Object -ComObject WScript.Shell;$s=$shell.CreateShortcut($Shortcut)
if($s.TargetPath -notmatch 'shadPS4.exe$' -or $s.Arguments -notmatch 'CUSA03173'){throw 'Select the existing direct Bloodborne shortcut; external launchers are not automatically rewritten.'}
if((Get-FileHash -LiteralPath $s.TargetPath).Hash.ToLowerInvariant() -ne $ExpectedSha){throw 'Executable identity mismatch.'}
$program=Join-Path $OutputRoot 'program';$backup=Join-Path $OutputRoot ('integration-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ'))
New-Item -ItemType Directory -Path $program,$backup,(Join-Path $OutputRoot 'commands') -Force | Out-Null
$original=Join-Path $backup 'original.lnk';[IO.File]::WriteAllBytes($original,$bytes)
if((Get-FileHash -LiteralPath $original).Hash -ne (Get-FileHash -LiteralPath $Shortcut).Hash){throw 'Shortcut backup verification failed.'}
$cfg=@{shortcut=$Shortcut;original_shortcut=$original;original_shortcut_sha256=(Get-FileHash -LiteralPath $original).Hash.ToLowerInvariant();target=$s.TargetPath;arguments=$s.Arguments;working_directory=$s.WorkingDirectory;window_style=$s.WindowStyle;icon=$s.IconLocation;description=$s.Description;hotkey=$s.Hotkey;link_flags=[BitConverter]::ToUInt32($bytes,20);profile_root=(Join-Path $s.WorkingDirectory 'user');expected_sha=$ExpectedSha;revision=$Revision;build_label=$BuildLabel;output_root=$OutputRoot;python=$Python;presentmon=$PresentMon;environment='Inherited from Explorer through original ShellExecute shortcut; no game environment edits';installed_utc=[DateTime]::UtcNow.ToString('o')}
$manifest=Join-Path $OutputRoot 'integration.json';$cfg|ConvertTo-Json -Depth 4|Set-Content -LiteralPath $manifest -Encoding UTF8
foreach($name in @('mode_a.py','live_metrics.py','report.py','snapshot.py','adapter_identity.py','LiveHud.cs','Launch-LiveMonitor.ps1','Capture-CPU-Stacks.ps1','xinput_control.py')){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $program $name) -Force}
& "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /nologo /target:winexe ("/out:"+(Join-Path $program 'LiveHud.exe')) /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.Web.Extensions.dll (Join-Path $program 'LiveHud.cs')
if($LASTEXITCODE){throw 'HUD compilation failed; shortcut unchanged.'}
$vbs=Join-Path $program 'Launch.vbs'
$launch=Join-Path $program 'Launch-LiveMonitor.ps1'
$command='"'+$env:WINDIR+'\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "'+$launch+'" -Manifest "'+$manifest+'"'
[IO.File]::WriteAllText($vbs,'CreateObject("WScript.Shell").Run "'+$command.Replace('"','""')+'", 0, False'+[Environment]::NewLine)
$s.TargetPath=Join-Path $env:WINDIR 'System32\wscript.exe';$s.Arguments='"'+$vbs+'"';$s.Save()
$new=$shell.CreateShortcut($Shortcut)
if($new.TargetPath -ne $s.TargetPath -or $new.Arguments -ne $s.Arguments){Copy-Item -LiteralPath $original -Destination $Shortcut -Force;throw 'Shortcut verification failed; original restored.'}
Get-ChildItem -LiteralPath $program -File | ForEach-Object {[pscustomobject]@{name=$_.Name;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()}} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $program 'component-hashes.json') -Encoding UTF8
$cfg|ConvertTo-Json -Depth 4
