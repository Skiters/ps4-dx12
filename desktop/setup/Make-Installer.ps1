param(
    [Parameter(Mandatory=$true)][string]$InnoCompiler,
    [Parameter(Mandatory=$true)][string]$NativeRuntime,
    [Parameter(Mandatory=$true)][string]$PackageWorker,
    [string]$OutputDir = (Join-Path $PSScriptRoot 'packages')
)
$ErrorActionPreference = 'Stop'
$desktopRoot = Split-Path $PSScriptRoot -Parent
$payload = Join-Path $PSScriptRoot 'payload'
foreach ($path in @($InnoCompiler, (Join-Path $NativeRuntime 'shadPS4-DX12.exe'),
    (Join-Path $NativeRuntime 'core\shadPS4-DX12-Core.exe'),
    (Join-Path $PackageWorker 'PS4PackageInstaller.exe'))) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Required build input missing: $path" }
}
if (Test-Path -LiteralPath $payload) {
    throw 'Use a fresh setup/payload directory; preserve or rename the prior build before rebuilding.'
}
New-Item -ItemType Directory -Path $payload, $OutputDir -Force | Out-Null
& dotnet publish (Join-Path $desktopRoot 'PS4Desktop.csproj') -c Release -r win-x64 --self-contained true -p:PublishSingleFile=false -o $payload
if ($LASTEXITCODE -ne 0) { throw 'Menu publish failed' }
Copy-Item -LiteralPath $NativeRuntime -Destination (Join-Path $payload 'native') -Recurse
Copy-Item -LiteralPath $PackageWorker -Destination (Join-Path $payload 'installer') -Recurse
Copy-Item -LiteralPath (Join-Path $desktopRoot 'profile-template') -Destination $payload -Recurse
Copy-Item -LiteralPath (Join-Path $desktopRoot '..\LICENSE') -Destination (Join-Path $payload 'LICENSE-shadPS4.txt')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'application-readme.txt') -Destination (Join-Path $payload 'README.txt')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'THIRD-PARTY.txt') -Destination $payload
if (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'licenses')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'licenses') -Destination $payload -Recurse
}
# Runtime sources must contain the app-local CRT and converter DLLs supplied by
# the installed package, or copied from official redistributable build outputs.
foreach ($file in @('msvcp140.dll','msvcp140_atomic_wait.dll','vcruntime140.dll',
    'vcruntime140_1.dll','spirv_to_dxil.dll','dxcompiler.dll','dxil.dll')) {
    if (-not (Test-Path -LiteralPath (Join-Path $payload "native\core\$file"))) {
        throw "Required redistributable missing: $file"
    }
}
& $InnoCompiler ("/DPayloadDir=" + $payload) ("/DPackageOutput=" + [IO.Path]::GetFullPath($OutputDir)) (Join-Path $PSScriptRoot 'PS4Desktop.iss')
if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed' }
