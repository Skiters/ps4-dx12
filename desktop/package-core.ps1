param(
    [Parameter(Mandatory)][string]$SourceDirectory,
    [Parameter(Mandatory)][string]$DestinationRoot,
    [Parameter(Mandatory)][string]$Id,
    [Parameter(Mandatory)][string]$Name,
    [Parameter(Mandatory)][string]$Commit,
    [Parameter(Mandatory)][string]$Description,
    [string]$Branch = 'fix/mixed-cmask',
    [string]$Executable = 'shadPS4-DX12-Core.exe',
    [ValidateSet('Экспериментальное','Тесты пройдены','Проверено в игре')][string]$Status = 'Экспериментальное',
    [string]$Results = '',
    [switch]$Dirty
)
$ErrorActionPreference = 'Stop'
if ($Id -notmatch '^[a-zA-Z0-9][a-zA-Z0-9._-]+$') { throw 'Invalid version ID' }
$destination = Join-Path ([IO.Path]::GetFullPath($DestinationRoot)) $Id
if (Test-Path -LiteralPath $destination) { throw 'Version already exists; refusing overwrite' }
if (!(Test-Path -LiteralPath (Join-Path $SourceDirectory $Executable))) { throw 'Executable missing' }
$staging = $destination + '.staging-' + [Guid]::NewGuid().ToString('N')
if (![IO.Path]::GetFullPath($staging).StartsWith([IO.Path]::GetFullPath($DestinationRoot).TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Staging path outside destination root' }
New-Item -ItemType Directory -Path $staging -Force | Out-Null
try {
    Copy-Item -LiteralPath (Join-Path $SourceDirectory $Executable) -Destination $staging
    Get-ChildItem -LiteralPath $SourceDirectory -Filter '*.dll' -File | Copy-Item -Destination $staging
    # Preserve runtime subdirectories (for example D3D12 Agility SDK).
    Get-ChildItem -LiteralPath $SourceDirectory -Directory | Copy-Item -Destination $staging -Recurse
    $files = @{}
    Get-ChildItem -LiteralPath $staging -File -Recurse | ForEach-Object {
        $relative = [IO.Path]::GetRelativePath($staging, $_.FullName)
        $files[$relative] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    $manifest = @{ Id=$Id; Name=$Name; BuildDate=(Get-Item -LiteralPath (Join-Path $SourceDirectory $Executable)).LastWriteTimeUtc.ToString('o'); Branch=$Branch;
        Commit=$Commit; Dirty=[bool]$Dirty; Backend='dx12'; Description=$Description; Status=$Status; Executable=$Executable;
        Sha256=$files[$Executable]; Files=$files; Results=$Results }
    $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $staging 'core-version.json') -Encoding utf8NoBOM
    Move-Item -LiteralPath $staging -Destination $destination
    Write-Output $destination
} finally {
    if (Test-Path -LiteralPath $staging) { Remove-Item -LiteralPath $staging -Recurse -Force }
}
