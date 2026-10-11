# Invoke only after the user explicitly authorizes a controlled game run.
param(
    [Parameter(Mandatory)][string]$ProfileRoot,
    [Parameter(Mandatory)][string]$GamePath,
    [Parameter(Mandatory)][string]$TraceDirectory,
    [Parameter(Mandatory)][string]$Executable,
    [Parameter(Mandatory)][string]$ExpectedSha,
    [switch]$GpuTimestamps
)
$ErrorActionPreference='Stop'
if(!(Test-Path -LiteralPath (Join-Path $ProfileRoot 'user')) -or !(Test-Path -LiteralPath (Join-Path $ProfileRoot 'profile-manifest.json'))){throw 'Prepare an isolated portable profile first.'}
if(Test-Path -LiteralPath $TraceDirectory){throw 'Trace directory must be new.'}
if(!(Test-Path -LiteralPath $GamePath)){throw 'Game path does not exist.'}
if((Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash.ToLowerInvariant() -ne $ExpectedSha.ToLowerInvariant()){throw 'Instrumented executable SHA mismatch.'}
if(Get-CimInstance Win32_Process -Filter "Name='shadPS4.exe'"){throw 'Close the other core normally before a controlled run.'}
New-Item -ItemType Directory -Path $TraceDirectory | Out-Null
$oldTrace=$env:SHADPS4_REFERENCE_TRACE;$oldTiming=$env:SHADPS4_REF_GPU_TIMING
try{
    $env:SHADPS4_REFERENCE_TRACE=[IO.Path]::GetFullPath($TraceDirectory)
    if($GpuTimestamps){$env:SHADPS4_REF_GPU_TIMING='1'}else{Remove-Item Env:SHADPS4_REF_GPU_TIMING -ErrorAction SilentlyContinue}
    $started=Start-Process -FilePath $Executable -WorkingDirectory $ProfileRoot -ArgumentList @('-g',('"'+$GamePath+'"')) -WindowStyle Hidden -PassThru
    @{mode='B';pid=$started.Id;started_utc=[DateTime]::UtcNow.ToString('o');executable=$Executable;sha256=$ExpectedSha;source_revision='c7e065d1b415be16c23e260a21f1dd8bbfc4cb57';profile=$ProfileRoot;gpu_timestamps=[bool]$GpuTimestamps;matched_benchmark=$false}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $TraceDirectory 'launch-manifest.json') -Encoding utf8
    Write-Output ('Started authorized controlled run, PID '+$started.Id)
}finally{
    $env:SHADPS4_REFERENCE_TRACE=$oldTrace;$env:SHADPS4_REF_GPU_TIMING=$oldTiming
}
