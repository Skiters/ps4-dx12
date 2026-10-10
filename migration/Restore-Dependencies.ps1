param([switch]$SkipMesa)
$ErrorActionPreference = 'Stop'
$workRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$lock = Get-Content (Join-Path $PSScriptRoot 'dependency-lock.json') -Raw | ConvertFrom-Json
function Run-Git([string[]]$Arguments) {
    & git @Arguments
    if ($LASTEXITCODE -ne 0) { throw "git failed: $($Arguments -join ' ')" }
}
function Clone-Pinned([string]$Url, [string]$Commit, [string]$Target) {
    if (Test-Path -LiteralPath $Target) {
        $head = & git -C $Target rev-parse HEAD
        if ($LASTEXITCODE -ne 0 -or $head -ne $Commit) {
            throw "Existing dependency differs; inspect before continuing: $Target"
        }
        return
    }
    Run-Git -Arguments @('clone','--no-checkout',$Url,$Target)
    Run-Git -Arguments @('-C',$Target,'checkout','--detach',$Commit)
}
$reference = Join-Path $workRoot 'shadps4-xess-src'
Clone-Pinned $lock.reference.url $lock.reference.commit $reference
# Only native build dependencies. Removed Vulkan/glslang/DLSS bridges are not
# initialized or linked into the fork. The reference checkout is read-only input.
foreach ($module in $lock.submodules) {
    Run-Git -Arguments @('-C',$reference,'submodule','update','--init','--recursive','--',$module.path)
    $actual = & git -C (Join-Path $reference $module.path) rev-parse HEAD
    if ($actual -ne $module.commit) { throw "Dependency pin mismatch: $($module.path)" }
}
$reader = Join-Path $workRoot 'pkg-installer'
Clone-Pinned $lock.pkg_reader.url $lock.pkg_reader.commit $reader
$desktop = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\desktop'))
New-Item -ItemType Directory -Path (Join-Path $desktop 'reader') -Force | Out-Null
foreach ($file in @('ps4_pkg.py','pkg_keys.py')) {
    Copy-Item -LiteralPath (Join-Path $reader $file) -Destination (Join-Path $desktop 'reader')
}
# Native checks retain the historical fmt include location. Source comes from
# the pinned dependency, not from a copied binary/build cache.
$fmt = Join-Path $reference 'build-xess\_deps\fmt-src'
New-Item -ItemType Directory -Path $fmt -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $reference 'externals\fmt\include') -Destination $fmt -Recurse -Force
if (-not $SkipMesa) {
    $mesa = Join-Path $workRoot 'mesa-dxil-source'
    Clone-Pinned $lock.mesa.url $lock.mesa.commit $mesa
    $overlay = Join-Path $PSScriptRoot 'mesa-source-overlay'
    foreach ($file in Get-ChildItem -LiteralPath $overlay -Recurse -File) {
        $relative = $file.FullName.Substring($overlay.Length + 1)
        $target = Join-Path $mesa $relative
        New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
        Copy-Item -LiteralPath $file.FullName -Destination $target -Force
    }
    $linkedHeaders = Join-Path $workRoot 'mesa-dxil-bary-overlay\src\microsoft\spirv_to_dxil'
    New-Item -ItemType Directory -Path $linkedHeaders -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $mesa 'src\microsoft\spirv_to_dxil\spirv_to_dxil_linked.h') -Destination $linkedHeaders -Force
}
Write-Output 'Pinned dependency source trees restored. Install DXC headers as described in READ-ME-FIRST.txt.'
