param([string]$BuildDir,[string]$Mode='base')
$ErrorActionPreference='Stop'
$build=(Resolve-Path -LiteralPath $BuildDir).Path
$source=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'gcn_frontend_test.cpp')).Path.Replace('\','/')
$ninja='C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
$compiler='F:\PS4Dev\toolchain\llvm-22.1.3\bin\clang-cl.exe'
$linker='F:\PS4Dev\toolchain\llvm-22.1.3\bin\lld-link.exe'
Push-Location $build
try {
    & $ninja shadPS4-DX12-Core.exe -j 12
    if($LASTEXITCODE){throw 'Core build failed; refusing stale objects'}
    # Reuse the current core's production objects and libraries with a test-only main.
    # All outputs are separate: the core executable, main object and PDB are untouched.
    $command=(& $ninja -t commands CMakeFiles/shadps4.dir/src/main.cpp.obj)[-1]
    if($LASTEXITCODE){throw 'Cannot obtain current compiler command'}
    $args=$command.Substring($command.IndexOf('  ')+2)
    $args=$args.Replace('CMakeFiles/shadps4.dir/src/main.cpp.obj','gcn-frontend-test.obj')
    $args=$args.Replace('CMakeFiles\shadps4.dir\src\main.cpp.obj.d','gcn-frontend-test.obj.d')
    $args=$args.Replace('/FdCMakeFiles\shadps4.dir\','/Fdgcn-frontend-compile.pdb')
    $args=[regex]::Replace($args,'-c -- .*?/src/main.cpp$','-c -- '+$source)
    if(!$args.EndsWith($source)){throw 'Unexpected compiler command structure'}
    [IO.File]::WriteAllText((Join-Path $build 'gcn-frontend-compile.rsp'),$args)
    & $compiler '@gcn-frontend-compile.rsp'
    if($LASTEXITCODE){throw 'GCN fixture compilation failed'}
    $manifest=[IO.File]::ReadAllText((Join-Path $build 'build.ninja'))
    $block=[regex]::Match($manifest,'(?ms)^build shadPS4-DX12-Core.exe: (.*?)(?=\r?\n\r?\n)').Value
    $first=($block -split '\r?\n')[0]
    $objects=($first -replace '^build shadPS4-DX12-Core.exe: \S+ ','') -split ' \|' | Select-Object -First 1
    $libraries=[regex]::Match($block,'(?m)^  LINK_LIBRARIES = (.*)$').Groups[1].Value
    if(!$objects.Contains('CMakeFiles/shadps4.dir/src/main.cpp.obj') -or !$libraries){throw 'Unexpected core link manifest'}
    $objects=$objects.Replace('CMakeFiles/shadps4.dir/src/main.cpp.obj','gcn-frontend-test.obj')
    [IO.File]::WriteAllText((Join-Path $build 'gcn-frontend-link.rsp'),($objects+' '+$libraries).Replace('$:',':').Replace('$ ',' '))
    & $linker '@gcn-frontend-link.rsp' /out:gcn-frontend-test.exe /subsystem:console /stack:0x200000,0x200000
    if($LASTEXITCODE){throw 'GCN fixture link failed'}
    $converter=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../mesa-dxil-bary-build/src/microsoft/spirv_to_dxil/spirv_to_dxil.dll')).Path
    & (Join-Path $build 'gcn-frontend-test.exe') $converter 'F:\PS4Dev\work\dxc-sdk\bin\x64\dxcompiler.dll' $Mode
    if($LASTEXITCODE){throw "GCN fixture failed: $LASTEXITCODE"}
} finally {Pop-Location}