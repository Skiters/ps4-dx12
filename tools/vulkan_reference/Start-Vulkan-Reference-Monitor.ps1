param(
    [int]$TargetProcessId=0,
    [string]$OutputRoot='F:\PS4Dev\handoff\vulkan-reference-profiler-20261011',
    [string]$Python='F:\PS4Dev\toolchain\python\cpython-3.11.16-windows-x86_64-none\python.exe',
    [string]$ExpectedSha='91eb69a59cb3ea3c35290fbd03a69718d2f6254c202ba86c6acbe1d1fc8b95f1',
    [int]$Seconds=28800,
    [ValidateRange(0.2,60)][double]$IntervalSeconds=1,
    [ValidateRange(1,4096)][int]$MaxMiB=1024,
    [double]$PrivateGrowthMiB=512,
    [double]$FaultRate=10000
)
$ErrorActionPreference='Stop'
$processes=@(Get-CimInstance Win32_Process -Filter "Name='shadPS4.exe'" | Where-Object {$_.ExecutablePath -and (!$TargetProcessId -or $_.ProcessId -eq $TargetProcessId)})
$matching=@($processes | Where-Object {(Get-FileHash -LiteralPath $_.ExecutablePath -Algorithm SHA256).Hash.ToLowerInvariant() -eq $ExpectedSha})
if($matching.Count -ne 1){throw 'Exactly one matching Vulkan executable is required; supply TargetProcessId to disambiguate.'}
$target=$matching[0]
if($target.CommandLine -notmatch 'CUSA03173'){throw 'Selected process command line does not identify Bloodborne CUSA03173.'}
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null
$active=Join-Path $OutputRoot 'active-session.json'
if(Test-Path -LiteralPath $active){
    $old=Get-Content -LiteralPath $active -Raw | ConvertFrom-Json
    $existing=Get-CimInstance Win32_Process -Filter "ProcessId=$($old.collector_pid)"
    if($existing -and $existing.CommandLine -like '*mode_a.py*' -and $existing.CommandLine.Contains($old.session)){throw 'A collector is already active. Stop it through Stop-Vulkan-Reference-Monitor.ps1.'}
}
$name='mode-a-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$session=Join-Path $OutputRoot $name
$stdout=Join-Path $OutputRoot ($name+'.stdout.txt');$stderr=Join-Path $OutputRoot ($name+'.stderr.txt')
$script=Join-Path $PSScriptRoot 'mode_a.py'
$arguments=@(('"'+$script+'"'),'--pid',$target.ProcessId,'--output',('"'+$session+'"'),'--expected-sha',$ExpectedSha,'--seconds',$Seconds,'--interval',$IntervalSeconds.ToString([Globalization.CultureInfo]::InvariantCulture),'--max-mb',$MaxMiB,'--private-growth-mb',$PrivateGrowthMiB.ToString([Globalization.CultureInfo]::InvariantCulture),'--fault-rate',$FaultRate.ToString([Globalization.CultureInfo]::InvariantCulture))
$collector=Start-Process -FilePath $Python -ArgumentList $arguments -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
@{collector_pid=$collector.Id;target_pid=$target.ProcessId;session=$session;stderr=$stderr;stdout=$stdout}|ConvertTo-Json|Set-Content -LiteralPath $active -Encoding UTF8
Start-Sleep -Milliseconds 1000
$collector.Refresh()
if($collector.HasExited -and !(Test-Path -LiteralPath (Join-Path $session 'status.json'))){throw ('Collector failed before initialization. See '+$stderr)}
Get-Content -LiteralPath $active
