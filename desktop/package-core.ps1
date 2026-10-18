param(
    [Parameter(Mandatory)][string]$SourceDirectory,
    [Parameter(Mandatory)][string]$DestinationRoot,
    [Parameter(Mandatory)][string]$Id,
    [Parameter(Mandatory)][string]$Name,
    [Parameter(Mandatory)][string]$Commit,
    [Parameter(Mandatory)][string]$Description,
    [Parameter(Mandatory)][string]$RuntimeFileList,
    [Parameter(Mandatory)][string]$SourceIdentityFile,
    [Parameter(Mandatory)][string]$BuildIdentityFile,
    [ValidateSet('dx12','vulkan')][string]$Backend = 'dx12',
    [string]$Branch = 'fix/mixed-cmask',
    [string]$Executable = 'shadPS4-DX12-Core.exe',
    [ValidateSet('Экспериментальное','Тесты пройдены','Проверено в игре')][string]$Status = 'Экспериментальное',
    [string]$Results = '',
    [string]$ProfileId = '',
    [switch]$IsolatedProfile,
    [switch]$Dirty
)
$ErrorActionPreference = 'Stop'
if ($Id -notmatch '^[a-zA-Z0-9][a-zA-Z0-9._-]+$') { throw 'Invalid version ID' }
if ($Backend -eq 'dx12' -and $Id -ne 'dx12-current') { throw 'DX12 uses the single stable Id dx12-current; do not create version archives.' }
if ($ProfileId -and ($ProfileId -in @('.','..') -or $ProfileId.IndexOfAny([IO.Path]::GetInvalidFileNameChars()) -ge 0)) { throw 'Invalid profile identity' }
$root = [IO.Path]::GetFullPath($DestinationRoot).TrimEnd('\') + '\'
$source = [IO.Path]::GetFullPath($SourceDirectory).TrimEnd('\') + '\'
function Inside([string]$base, [string]$relative) {
    if ([IO.Path]::IsPathRooted($relative)) { throw 'Absolute runtime path' }
    $resolved = [IO.Path]::GetFullPath((Join-Path $base $relative))
    if (!$resolved.StartsWith($base, [StringComparison]::OrdinalIgnoreCase)) { throw 'Runtime path outside package' }
    return $resolved
}
function Hex([string]$value, [int]$size) { return $value -match ('^[0-9a-fA-F]{' + $size + '}$') }
$runtime = @(Get-Content -LiteralPath $RuntimeFileList -Raw | ConvertFrom-Json | ForEach-Object {
    if ($_ -isnot [string]) { throw 'Runtime paths must be strings' }
    $_.Replace('\','/')
})
if (!$runtime.Count -or $Executable -notin $runtime -or ($runtime | Select-Object -Unique).Count -ne $runtime.Count) { throw 'Invalid runtime file list' }
$sourceIdentity = Get-Content -LiteralPath $SourceIdentityFile -Raw | ConvertFrom-Json -AsHashtable
$buildIdentity = Get-Content -LiteralPath $BuildIdentityFile -Raw | ConvertFrom-Json -AsHashtable
foreach ($field in @('PatchFile')) { if ($sourceIdentity[$field]) { $sourceIdentity[$field] = $sourceIdentity[$field].Replace('\','/') } }
foreach ($field in @('CompilerProfileFile','NativeCompilerContractFile','ReferenceCompletionContractFile')) {
    if ($buildIdentity[$field]) { $buildIdentity[$field] = $buildIdentity[$field].Replace('\','/') }
}
if (!$sourceIdentity.Repository -or $null -eq $sourceIdentity.Submodules -or !(Hex $sourceIdentity.Revision 40) -or $sourceIdentity.Revision -cne $Commit -or
    [bool]$sourceIdentity.Dirty -ne [bool]$Dirty -or ($Dirty -and !(Hex $sourceIdentity.PatchSha256 64))) { throw 'Invalid source identity' }
foreach ($entry in $sourceIdentity.Submodules.GetEnumerator()) {
    if (!$entry.Key -or !(Hex $entry.Value 40)) { throw 'Invalid submodule identity' }
}
if ($sourceIdentity.ReferenceRevision -and !(Hex $sourceIdentity.ReferenceRevision 40)) { throw 'Invalid reference revision' }
if (!$buildIdentity.Configuration -or $buildIdentity.Architecture -ne 'x64' -or !$buildIdentity.Compiler -or !$buildIdentity.CompilerVersion -or
    !(Hex $buildIdentity.CompilerProfileSha256 64) -or $buildIdentity.CompilerProfileFile -notin $runtime) { throw 'Invalid build identity' }
if ($Dirty -and $sourceIdentity.PatchFile -notin $runtime) { throw 'Source patch missing from runtime file list' }
if ($Backend -eq 'dx12' -and (!$buildIdentity.NativeCompilerContractFile -or $buildIdentity.NativeCompilerContractFile -notin $runtime)) { throw 'Native compiler contract missing' }
if ($buildIdentity.ReferenceCompletionContractFile -and ($Backend -ne 'vulkan' -or $buildIdentity.ReferenceCompletionContractFile -notin $runtime)) { throw 'Invalid reference completion contract' }
$destination = Inside $root $Id
function Assert-NoReparse([string]$path) {
    $item = Get-Item -LiteralPath $path
    while ($item) {
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Reparse path is not a package target: $path" }
        $item = if ($item -is [IO.DirectoryInfo]) { $item.Parent } else { $item.Directory }
    }
    if ((Get-ChildItem -LiteralPath $path -Recurse -Force | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint } | Select-Object -First 1)) {
        throw 'Package contains a reparse point'
    }
}
function Assert-NotRunning([string]$path) {
    $prefix = [IO.Path]::GetFullPath($path).TrimEnd('\') + '\'
    foreach ($process in Get-CimInstance Win32_Process) {
        if ($process.ExecutablePath -and $process.ExecutablePath.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)) {
            throw "Core is still in use by PID $($process.ProcessId); leave it unchanged."
        }
    }
    # Refuse replacement if another reader holds a package file, even if it is
    # not itself the core process. These probes never terminate a process.
    foreach ($file in Get-ChildItem -LiteralPath $path -File -Recurse) {
        $probe = [IO.File]::Open($file.FullName,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::None)
        $probe.Dispose()
    }
}
function Assert-OwnedPackage([string]$path) {
    Assert-NoReparse $path
    $old = Get-Content -LiteralPath (Join-Path $path 'core-version.json') -Raw | ConvertFrom-Json -AsHashtable
    if ($old.Backend -ne 'dx12' -or $old.Id -ne $Id) { throw 'Not an owned current DX12 package' }
    $expected = @($old.Files.Keys) + @('core-version.json')
    foreach ($file in Get-ChildItem -LiteralPath $path -File -Recurse) {
        $relative = [IO.Path]::GetRelativePath($path,$file.FullName).Replace('\','/')
        if ($relative -notin $expected) { throw "Unowned file in current package: $relative" }
    }
    foreach ($entry in $old.Files.GetEnumerator()) {
        if ((Get-FileHash -LiteralPath (Inside ($path.TrimEnd('\')+'\') $entry.Key)).Hash -ine $entry.Value) { throw 'Old package changed; preserve it for review' }
    }
    return $old
}
New-Item -ItemType Directory -Path $root -Force | Out-Null
Assert-NoReparse $root
$mutexKey = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($destination.TrimEnd('\').ToUpperInvariant())))
$publication = [Threading.Mutex]::new($false,('Local\PS4Desktop-Core-'+$mutexKey))
$acquired = $false
try { $acquired = $publication.WaitOne(0) } catch [Threading.AbandonedMutexException] { $acquired = $true }
if (!$acquired) { $publication.Dispose(); throw 'A core launch or publication is in progress; keep the current package unchanged.' }
try {
$oldManifest = $null
if (Test-Path -LiteralPath $destination) {
    if ($Backend -ne 'dx12') { throw 'Reference versions are immutable' }
    $oldManifest = Assert-OwnedPackage $destination
    Assert-NotRunning $destination
    if (!$ProfileId) { $ProfileId = $oldManifest.ProfileId }
}
if ($Backend -eq 'dx12') {
    $pdb = [IO.Path]::ChangeExtension($Executable,'.pdb')
    if ($pdb -notin $runtime) { throw 'Current DX12 package requires corresponding PDB in runtime list' }
    foreach ($directory in Get-ChildItem -LiteralPath $root -Directory) {
        $manifestPath = Join-Path $directory.FullName 'core-version.json'
        if ($directory.FullName -ne $destination -and (Test-Path -LiteralPath $manifestPath)) {
            $other = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
            if ($other.Backend -eq 'dx12') { throw 'Retire old DX12 packages before publishing one current package' }
        }
    }
}
if (!(Test-Path -LiteralPath (Join-Path $SourceDirectory $Executable))) { throw 'Executable missing' }
$staging = $destination + '.staging-' + [Guid]::NewGuid().ToString('N')
$previous = $destination + '.replacing'
if (Test-Path -LiteralPath $previous) { throw 'Previous replacement needs recovery; refusing to discard it' }
if (![IO.Path]::GetFullPath($staging).StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) { throw 'Staging path outside destination root' }
New-Item -ItemType Directory -Path $staging -Force | Out-Null
try {
    $stageRoot = [IO.Path]::GetFullPath($staging).TrimEnd('\') + '\'
    foreach ($relative in $runtime) {
        $inputFile = Inside $source $relative
        $outputFile = Inside $stageRoot $relative
        if (!(Test-Path -LiteralPath $inputFile -PathType Leaf)) { throw ('Runtime file missing: ' + $relative) }
        New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($outputFile)) -Force | Out-Null
        Copy-Item -LiteralPath $inputFile -Destination $outputFile
    }
    $files = @{}
    Get-ChildItem -LiteralPath $staging -File -Recurse | ForEach-Object {
        $relative = [IO.Path]::GetRelativePath($staging, $_.FullName).Replace('\','/')
        $files[$relative] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    if ($files[$buildIdentity.CompilerProfileFile] -cne $buildIdentity.CompilerProfileSha256 -or
        ($Dirty -and $files[$sourceIdentity.PatchFile] -cne $sourceIdentity.PatchSha256)) { throw 'Provenance file hash mismatch' }
    if ($buildIdentity.ReferenceCompletionContractFile) {
        $completion = Get-Content -LiteralPath (Inside $stageRoot $buildIdentity.ReferenceCompletionContractFile) -Raw | ConvertFrom-Json -AsHashtable
        if ($completion.referenceRevision -cne $Commit -or $completion.executableSha256 -cne $files[$Executable] -or
            $completion.layerName -cne 'VK_LAYER_CODEX_guest_completion' -or !$completion.flipRva -or !$completion.flipSize -or
            $completion.flipPresentReturnRva -le $completion.flipRva -or $completion.flipPresentReturnRva -ge ([uint64]$completion.flipRva+[uint64]$completion.flipSize) -or
            $completion.requestRegister -cne 'rbx' -or $completion.requestIndexOffset -ne 24) { throw 'Reference completion identity mismatch' }
        foreach ($item in @(@('layerManifestFile','layerManifestSha256'),@('layerLibraryFile','layerLibrarySha256'),@('mapFile','mapSha256'),@('pdbFile','pdbSha256'))) {
            if (!(Hex $completion[$item[1]] 64) -or $files[$completion[$item[0]]] -cne $completion[$item[1]]) { throw 'Reference completion file hash mismatch' }
        }
    }
    $manifest = @{ ManifestVersion=2; Id=$Id; Name=$Name; BuildDate=(Get-Item -LiteralPath (Inside $source $Executable)).LastWriteTimeUtc.ToString('o'); Branch=$Branch;
        Commit=$Commit; Dirty=[bool]$Dirty; Backend=$Backend; Description=$Description; Status=$Status; Executable=$Executable;
        Sha256=$files[$Executable]; Files=$files; Results=$Results; Source=$sourceIdentity; Build=$buildIdentity; IsolatedProfile=[bool]$IsolatedProfile; ProfileId=$ProfileId }
    $manifest | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath (Join-Path $staging 'core-version.json') -Encoding utf8NoBOM
    if ($oldManifest) {
        $null = Assert-OwnedPackage $destination
        Assert-NotRunning $destination
        Move-Item -LiteralPath $destination -Destination $previous
    }
    try { Move-Item -LiteralPath $staging -Destination $destination }
    catch {
        if ($oldManifest) { Move-Item -LiteralPath $previous -Destination $destination }
        throw
    }
    if ($oldManifest) {
        $null = Assert-OwnedPackage $previous
        Assert-NotRunning $previous
        if ([IO.Path]::GetFullPath($previous) -ne ([IO.Path]::GetFullPath($destination)+'.replacing')) { throw 'Replacement cleanup outside owned target' }
        Remove-Item -LiteralPath $previous -Recurse -Force
    }
    Write-Output $destination
} finally {
    if (Test-Path -LiteralPath $staging) {
        $cleanup = [IO.Path]::GetFullPath($staging)
        if (!$cleanup.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) { throw 'Cleanup path outside destination root' }
        Assert-NoReparse $cleanup
        Remove-Item -LiteralPath $cleanup -Recurse -Force
    }
}
} finally {
    $publication.ReleaseMutex()
    $publication.Dispose()
}
