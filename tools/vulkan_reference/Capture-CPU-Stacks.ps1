param([Parameter(Mandatory=$true)][string]$OutputDirectory,[ValidateRange(5,60)][int]$Seconds=15)
# Manual only. Never invokes elevation; invoke explicitly in an approved elevated terminal.
$ErrorActionPreference='Stop'
$admin=([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if(!$admin){throw 'CPU ETW stacks unavailable in this token. This script does not auto-elevate.'}
$status=& wpr.exe -status 2>&1
if($LASTEXITCODE -ne 0 -or ($status -notmatch 'not recording|не ведется|не ведётся')){throw 'Cannot verify WPR is idle; will not interfere with another capture.'}
New-Item -ItemType Directory -Path $OutputDirectory -Force|Out-Null
& wpr.exe -start GeneralProfile -filemode
if($LASTEXITCODE){throw 'WPR start failed.'}
Start-Sleep -Seconds $Seconds
& wpr.exe -stop (Join-Path $OutputDirectory 'cpu-stacks.etl') 'Bloodborne short controlled CPU stacks, not a performance baseline'
if($LASTEXITCODE){throw 'WPR stop failed; inspect own WPR session before retry.'}
