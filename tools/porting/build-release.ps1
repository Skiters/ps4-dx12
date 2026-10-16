param(
    [Parameter(Mandatory)][string]$ExternalsRoot,
    [Parameter(Mandatory)][string]$DxcRoot,
    [Parameter(Mandatory)][string]$LlvmRoot,
    [Parameter(Mandatory)][string]$VsDevCmd,
    [Parameter(Mandatory)][string]$BuildDir,
    [int]$Jobs = 12,
    [string]$SourceRoot
)
$ErrorActionPreference = 'Stop'
if (-not $SourceRoot) {
    $SourceRoot = Join-Path (Split-Path $MyInvocation.MyCommand.Path) '../..'
}
$SourceRoot = (Resolve-Path -LiteralPath $SourceRoot).Path
$ExternalsRoot = (Resolve-Path -LiteralPath $ExternalsRoot).Path
$BuildDir = [IO.Path]::GetFullPath($BuildDir)
function Git-Value([string]$Root, [string[]]$GitArgs) {
    $value = & git -C $Root @GitArgs
    if ($LASTEXITCODE -ne 0) { throw "git failed in $Root" }
    return ($value -join "`n").Trim()
}
if (Git-Value $SourceRoot @('status','--porcelain','--untracked-files=no')) {
    throw 'Build a committed clean source checkout; do not overlay working files.'
}
$commit = Git-Value $SourceRoot @('rev-parse','HEAD')
$lock = Get-Content -Raw "$SourceRoot/native-port/vendor/release-dependencies.json" | ConvertFrom-Json
$referenceRoot = Split-Path $ExternalsRoot
if ((Git-Value $referenceRoot @('rev-parse','HEAD')) -ne $lock.reference_sha) {
    throw 'External source root is not the pinned shadPS4 revision.'
}
foreach ($entry in $lock.submodules) {
    $path = Join-Path $referenceRoot $entry.path
    if ((Git-Value $path @('rev-parse','HEAD')) -ne $entry.sha -or
        (Git-Value $path @('status','--porcelain','--untracked-files=no'))) {
        throw "Dependency revision or tracked contents differ: $path"
    }
}
# Quote only paths/arguments accepted literally by cmd, without expansion characters.
function Cmd-Quote([string]$Value) {
    if ($Value -match '["%\r\n]') { throw 'Unsupported command argument character.' }
    return '"' + $Value + '"'
}
$common7 = Split-Path (Split-Path $VsDevCmd)
$cmakeBin = Join-Path $common7 "IDE/CommonExtensions/Microsoft/CMake/CMake/bin"
$ninjaBin = Join-Path $common7 "IDE/CommonExtensions/Microsoft/CMake/Ninja"
$compiler = Join-Path $LlvmRoot 'bin/clang-cl.exe'
$linker = Join-Path $LlvmRoot 'bin/lld-link.exe'
if ((& $compiler --version | Select-Object -First 1) -notmatch '22\.1\.3') {
    throw 'Use the pinned LLVM 22.1.3 toolchain.'
}
$options = @('-S', $SourceRoot, '-B', $BuildDir, '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=Release', "-DCMAKE_C_COMPILER=$compiler", "-DCMAKE_CXX_COMPILER=$compiler",
    "-DCMAKE_LINKER=$linker", '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON', '-DENABLE_TESTS=OFF',
    '-DENABLE_DISCORD_RPC=OFF', '-DENABLE_UPDATER=OFF', '-DENABLE_SYSTEM_LIBRARIES=OFF',
    '-DTRACY_ENABLE=OFF', '-DSDL_VULKAN=OFF', '-DSDL_RENDER_VULKAN=OFF',
    "-DSHADPS4_EXTERNALS_DIR=$ExternalsRoot", "-DSHADPS4_DXC_INCLUDE_DIR=$DxcRoot/inc",
    "-DSHADPS4_DXC_EXECUTABLE=$DxcRoot/bin/x64/dxc.exe")
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
$configure = 'cmake ' + (($options | ForEach-Object { Cmd-Quote $_ }) -join ' ')
$build = 'cmake --build ' + (Cmd-Quote $BuildDir) + " --target shadps4 -j $Jobs"
$batch = @('@echo off', ('call ' + (Cmd-Quote $VsDevCmd) + ' -arch=x64 -host_arch=x64 >nul'),
    'if errorlevel 1 exit /b %errorlevel%',
    ('set "PATH=' + $LlvmRoot + '\bin;' + $cmakeBin + ';' + $ninjaBin + ';%PATH%"'), $configure,
    'if errorlevel 1 exit /b %errorlevel%', $build)
$commandPath = Join-Path $BuildDir 'release-build.cmd'
[IO.File]::WriteAllLines($commandPath, $batch, [Text.Encoding]::Default)
& cmd /d /c $commandPath
if ($LASTEXITCODE -ne 0) { throw "Release build failed with exit $LASTEXITCODE" }
if (Git-Value $SourceRoot @('status','--porcelain','--untracked-files=no')) {
    throw 'Source checkout changed during build.'
}
$files = @($compiler, $linker) + @(Get-ChildItem -Recurse -File $BuildDir |
    Where-Object { $_.Name -eq 'shadPS4-DX12-Core.exe' -or $_.Name -eq 'shadPS4-DX12-Core.pdb' -or
        $_.DirectoryName -eq (Join-Path $BuildDir 'native-compiler') } | Select-Object -ExpandProperty FullName)
function File-Sha256([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','').ToLowerInvariant() }
    finally { $sha.Dispose(); $stream.Dispose() }
}
$identity = [ordered]@{ commit=$commit; source=$SourceRoot; build=$BuildDir;
    utc=[DateTime]::UtcNow.ToString('o'); configuration='Release'; runtime_verification='not_run';
    configure=$configure; build_command=$build; dependencies=$lock;
    files=@($files | ForEach-Object { @{ path=$_; sha256=(File-Sha256 $_) } }) }
$identity | ConvertTo-Json -Depth 12 | Set-Content -Encoding utf8 (Join-Path $BuildDir 'release-identity.json')
