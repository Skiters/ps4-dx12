param(
    [Parameter(Mandatory)][string]$Destination,
    [string]$OriginalUserRoot="$env:APPDATA\shadPS4"
)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $Destination){throw 'Destination must be new; existing profiles/results are never overwritten.'}
$originalFull=[IO.Path]::GetFullPath($OriginalUserRoot).TrimEnd('\')
$destinationFull=[IO.Path]::GetFullPath($Destination).TrimEnd('\')
if($destinationFull.StartsWith($originalFull+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Destination must be outside the original profile.'}
New-Item -ItemType Directory -Path $destinationFull | Out-Null
$user=Join-Path $destinationFull 'user'
New-Item -ItemType Directory -Path $user | Out-Null
# Portable-directory lookup happens in static initialization, before CLI parsing.
# Copy local state only; never redirect a development run to the installed profile.
$names=@('config.json','config.toml','users.json','keys.json','custom_configs','input_config','savedata','home','sys_modules','custom_modules','fonts','patches','cheats','licenses','trophy','custom_trophy')
foreach($name in $names){
    $source=Join-Path $originalFull $name
    if(Test-Path -LiteralPath $source){
        $links=@(Get-ChildItem -LiteralPath $source -Recurse -Force | Where-Object {$_.Attributes -band [IO.FileAttributes]::ReparsePoint})
        if($links.Count){throw ('Refusing linked state tree: '+$source)}
        Copy-Item -LiteralPath $source -Destination (Join-Path $user $name) -Recurse
    }
}
$configPath=Join-Path $user 'config.json'
$config=Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json
foreach($setting in @{home_dir=(Join-Path $user 'home');sys_modules_dir=(Join-Path $user 'sys_modules');font_dir=(Join-Path $user 'fonts');addon_install_dir=(Join-Path $user 'addcont')}.GetEnumerator()){
    $config.General | Add-Member -NotePropertyName $setting.Key -NotePropertyValue $setting.Value -Force
    New-Item -ItemType Directory -Path $setting.Value -Force | Out-Null
}
$config | ConvertTo-Json -Depth 40 | Set-Content -LiteralPath $configPath -Encoding utf8
$manifest=@(Get-ChildItem -LiteralPath $user -File -Recurse | ForEach-Object {@{path=$_.FullName.Substring($user.Length+1);bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}})
@{created_utc=[DateTime]::UtcNow.ToString('o');source=$originalFull;portable_user=$user;cache_policy='empty initial cache; warmup required';dlc_policy='isolated empty addcont; copy matching title DLC separately if needed';files=$manifest}|ConvertTo-Json -Depth 10|Set-Content -LiteralPath (Join-Path $destinationFull 'profile-manifest.json') -Encoding utf8
Write-Output $destinationFull
