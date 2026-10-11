param([Parameter(Mandatory=$true)][string]$Manifest)
$ErrorActionPreference='Stop'
# Shell launch may inherit a PS7 module search path. Resolve Windows PowerShell
# built-ins explicitly without changing the environment of the original game.
foreach($module in @('Microsoft.PowerShell.Utility','Microsoft.PowerShell.Management','CimCmdlets')){
    Import-Module (Join-Path $PSHOME ('Modules\'+$module)) -ErrorAction Stop
}
$cfg=Get-Content -LiteralPath $Manifest -Raw | ConvertFrom-Json
$root=$cfg.output_root
$mutex=New-Object Threading.Mutex($false,('Local\VulkanResearch-'+$cfg.expected_sha.Substring(0,16)))
$locked=$false
try {
    try {$locked=$mutex.WaitOne(0)} catch [Threading.AbandonedMutexException] {$locked=$true}
    if(!$locked){exit 0}
    if((Get-FileHash -LiteralPath $cfg.target -Algorithm SHA256).Hash.ToLowerInvariant() -ne $cfg.expected_sha){throw 'Installed executable changed: reinstall monitor integration to verify its identity.'}
    $cores=@(Get-CimInstance Win32_Process -Filter "Name='shadPS4.exe'")
    $matches=@($cores | Where-Object {$_.ExecutablePath -eq $cfg.target -and $_.CommandLine -match 'CUSA03173'})
    if($cores.Count -and $matches.Count -ne 1){throw 'Another emulator is running; no duplicate game was launched.'}
    if(!$matches.Count){
        # Launch the immutable ORIGINAL shortcut: preserves arguments, CWD, environment
        # inheritance and ShellExecute semantics, and cannot recurse into this wrapper.
        Start-Process -FilePath $cfg.original_shortcut
        $deadline=[DateTime]::UtcNow.AddSeconds(120)
        do {
            Start-Sleep -Milliseconds 200
            $matches=@(Get-CimInstance Win32_Process -Filter "Name='shadPS4.exe'" | Where-Object {$_.ExecutablePath -eq $cfg.target -and $_.CommandLine -match 'CUSA03173'})
        } while(!$matches.Count -and [DateTime]::UtcNow -lt $deadline)
        if($matches.Count -ne 1){throw 'Original game did not expose exactly one Bloodborne process within 120 seconds.'}
    }
    $target=Get-Process -Id $matches[0].ProcessId
    # Retain the native process handle before exit; PID lookup after exit loses status.
    $targetHandle=$target.Handle
    $startUtc=$target.StartTime.ToUniversalTime().ToString('o')
    $targetId=$target.Id
    $exitFlag=Join-Path $root ('hud-exit-'+$targetId)
    if(Test-Path -LiteralPath $exitFlag){Remove-Item -LiteralPath $exitFlag}
    $collector=$null;$hud=$null;$segment=$null;$hudRestarts=0
    function Start-Capture {
        $name='session-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8)
        $script:segment=Join-Path $root $name
        $args=@(('"'+(Join-Path $PSScriptRoot 'mode_a.py')+'"'),'--pid',$targetId,'--output',('"'+$script:segment+'"'),'--expected-sha',$cfg.expected_sha,'--profile-root',('"'+$cfg.profile_root+'"'),'--revision',$cfg.revision,'--build-label',('"'+$cfg.build_label+'"'),'--hud-health',('"'+(Join-Path $root 'hud-health.json')+'"'))
        if($cfg.presentmon){$args+=@('--presentmon',('"'+$cfg.presentmon+'"'))}
        $script:collector=Start-Process -FilePath $cfg.python -ArgumentList $args -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $root ($name+'.stdout.txt')) -RedirectStandardError (Join-Path $root ($name+'.stderr.txt'))
        @{session=$script:segment;target_pid=$targetId;target_start_utc=$startUtc;collector_pid=$script:collector.Id;supervisor_pid=$PID;started_utc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'active-session.tmp') -Encoding UTF8
        Move-Item -LiteralPath (Join-Path $root 'active-session.tmp') -Destination (Join-Path $root 'active-session.json') -Force
    }
    Start-Capture
    $hud=Start-Process -FilePath (Join-Path $PSScriptRoot 'LiveHud.exe') -ArgumentList @(('"'+$root+'"'),$targetId) -WindowStyle Hidden -PassThru
    $moduleCaptured=$false;$moduleNext=[DateTime]::UtcNow
    while(!$target.HasExited){
        if([DateTime]::UtcNow -ge $moduleNext -and (Test-Path -LiteralPath (Join-Path $segment 'manifest.json'))){
            $modules=@($target.Modules | ForEach-Object { $p=$_.FileName; [pscustomobject]@{name=$_.ModuleName;path=$p;version=$_.FileVersionInfo.FileVersion;sha256=if($_.ModuleName -match 'dlss|^sl\.|vulkan|graphics-hook|nvspcap|Nexus'){(Get-FileHash -LiteralPath $p).Hash.ToLowerInvariant()}else{$null}} })
            $modules | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $segment 'loaded-modules.json') -Encoding UTF8
            $moduleCaptured=$true;$moduleNext=[DateTime]::UtcNow.AddSeconds(60)
        }
        foreach($f in @(Get-ChildItem -LiteralPath (Join-Path $root 'commands') -Filter *.json -ErrorAction SilentlyContinue)){
            $c=Get-Content -LiteralPath $f.FullName -Raw | ConvertFrom-Json
            if($c.pid -ne $targetId){Move-Item -LiteralPath $f.FullName -Destination ($f.FullName+'.ignored');continue}
            switch($c.command){
                'stop' {if(!$collector.HasExited){Set-Content -LiteralPath (Join-Path $segment 'STOP') -Value 'user control panel'}}
                'start' {if($collector.HasExited){Start-Capture;$moduleCaptured=$false;$moduleNext=[DateTime]::UtcNow}}
                'pause' {if(!$collector.HasExited){$pause=Join-Path $segment 'PAUSE';if(Test-Path -LiteralPath $pause){Remove-Item -LiteralPath $pause}else{Set-Content -LiteralPath $pause -Value 'user capture pause'}}}
                {$_ -in 'snapshot','export'} {
                    $dest=Join-Path $root ('snapshot-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8))
                    & $cfg.python (Join-Path $PSScriptRoot 'snapshot.py') $segment $dest *> (Join-Path $root 'last-export.txt')
                }
            }
            if(Test-Path -LiteralPath (Join-Path $segment 'manifest.json')){
                $m=Get-Content -LiteralPath (Join-Path $segment 'manifest.json') -Raw | ConvertFrom-Json
                @{session_id=$m.session_id;process_start_utc=$startUtc;pid=$targetId;command=$c.command;label=$c.label;utc=$c.utc;qpc_s=$c.qpc_s;observed_utc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json -Compress | Add-Content -LiteralPath (Join-Path $segment 'markers.jsonl') -Encoding UTF8
            }
            Move-Item -LiteralPath $f.FullName -Destination ($f.FullName+'.handled')
        }
        $hud.Refresh()
        if($hud.HasExited -and $hudRestarts -lt 2){$hud=Start-Process -FilePath (Join-Path $PSScriptRoot 'LiveHud.exe') -ArgumentList @(('"'+$root+'"'),$targetId) -WindowStyle Hidden -PassThru;$hudRestarts++}
        Start-Sleep -Milliseconds 1000;$target.Refresh();$collector.Refresh()
    }
    $target.WaitForExit()
    $exitCode=$target.ExitCode
    @{pid=$targetId;start_utc=$startUtc;observed_exit_utc=[DateTime]::UtcNow.ToString('o');exit_code=$exitCode;normal_exit_claim=($null -ne $exitCode -and $exitCode -eq 0)} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $segment 'process-exit.json') -Encoding UTF8
    if(!$collector.HasExited){$collector.WaitForExit(10000)|Out-Null}
    if(!$collector.HasExited){Set-Content -LiteralPath (Join-Path $segment 'STOP') -Value 'process exited, supervisor finalization';$collector.WaitForExit(10000)|Out-Null}
    if($collector.HasExited){& $cfg.python (Join-Path $PSScriptRoot 'report.py') $segment *> (Join-Path $segment 'report-finalization.txt')}
    Set-Content -LiteralPath $exitFlag -Value 'target exited'
} catch {
    @{utc=[DateTime]::UtcNow.ToString('o');error=$_.Exception.ToString()} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'supervisor-error.json') -Encoding UTF8
    # Do not leave a hidden modal holding the single-instance mutex.
    Write-Error $_.Exception.Message -ErrorAction Continue
} finally {if($locked){$mutex.ReleaseMutex()};$mutex.Dispose()}
