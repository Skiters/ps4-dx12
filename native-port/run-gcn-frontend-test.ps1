param([string]$BuildDir,[string]$Mode='base',
    [ValidateSet('gcn_frontend','gcn_ordered_compile','shader_reload','pipeline_storage','wave64','gcn_arithmetic','gcn_winding','gcn_attachment_alias','address_space_protections','guest_frame_trace','packed_float','emulator_shutdown')][string]$Fixture='gcn_frontend',
    [switch]$CompileOnly,[ValidateRange(1,64)][int]$Jobs=20)
$ErrorActionPreference='Stop'
$build=(Resolve-Path -LiteralPath $BuildDir).Path
$source=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot ($Fixture+'_test.cpp'))).Path.Replace('\','/')
$output=$Fixture.Replace('_','-')+'-test'
$ninja='C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
$compiler='F:\PS4Dev\toolchain\llvm-22.1.3\bin\clang-cl.exe'
$linker='F:\PS4Dev\toolchain\llvm-22.1.3\bin\lld-link.exe'
Push-Location $build
try {
    & F:\Codex-Shared\devtools\bin\codex-dev.cmd run $ninja shadPS4-DX12-Core.exe -j $Jobs
    if($LASTEXITCODE){throw 'Core build failed; refusing stale objects'}
    # Reuse the current core's production objects and libraries with a test-only main.
    # All outputs are separate: the core executable, main object and PDB are untouched.
    $command=(& $ninja -t commands CMakeFiles/shadps4.dir/src/main.cpp.obj)[-1]
    if($LASTEXITCODE){throw 'Cannot obtain current compiler command'}
    $args=$command.Substring($command.IndexOf('  ')+2)
    $args=$args.Replace('CMakeFiles/shadps4.dir/src/main.cpp.obj',$output+'.obj')
    $args=$args.Replace('CMakeFiles\shadps4.dir\src\main.cpp.obj.d',$output+'.obj.d')
    $args=$args.Replace('/FdCMakeFiles\shadps4.dir\','/Fd'+$output+'-compile.pdb')
    $args=[regex]::Replace($args,'-c -- .*?/src/main.cpp$','-c -- '+$source)
    if(!$args.EndsWith($source)){throw 'Unexpected compiler command structure'}
    [IO.File]::WriteAllText((Join-Path $build ($output+'-compile.rsp')),$args)
    & F:\Codex-Shared\devtools\bin\codex-dev.cmd run $compiler ('@'+$output+'-compile.rsp')
    if($LASTEXITCODE){throw 'GCN fixture compilation failed'}
    $manifest=[IO.File]::ReadAllText((Join-Path $build 'build.ninja'))
    $block=[regex]::Match($manifest,'(?ms)^build shadPS4-DX12-Core.exe: (.*?)(?=\r?\n\r?\n)').Value
    $first=($block -split '\r?\n')[0]
    $objects=($first -replace '^build shadPS4-DX12-Core.exe: \S+ ','') -split ' \|' | Select-Object -First 1
    $libraries=[regex]::Match($block,'(?m)^  LINK_LIBRARIES = (.*)$').Groups[1].Value
    if(!$objects.Contains('CMakeFiles/shadps4.dir/src/main.cpp.obj') -or !$libraries){throw 'Unexpected core link manifest'}
    $objects=$objects.Replace('CMakeFiles/shadps4.dir/src/main.cpp.obj',$output+'.obj')
    [IO.File]::WriteAllText((Join-Path $build ($output+'-link.rsp')),($objects+' '+$libraries).Replace('$:',':').Replace('$ ',' '))
    & F:\Codex-Shared\devtools\bin\codex-dev.cmd run $linker ('@'+$output+'-link.rsp') ('/out:'+$output+'.exe') ('/map:'+$output+'.map') /debug ('/pdb:'+$output+'.pdb') /subsystem:console /stack:0x400000,0x400000
    if($LASTEXITCODE){throw 'GCN fixture link failed'}
    if($CompileOnly){return}
    $converter=(Resolve-Path -LiteralPath (Join-Path $build 'native-compiler/spirv_to_dxil.dll')).Path
    $dxc=(Resolve-Path -LiteralPath (Join-Path $build 'native-compiler/dxcompiler.dll')).Path
    & F:\Codex-Shared\devtools\bin\codex-dev.cmd run (Join-Path $build ($output+'.exe')) $converter $dxc $Mode
    if($LASTEXITCODE){throw "GCN fixture failed: $LASTEXITCODE"}
} finally {Pop-Location}
